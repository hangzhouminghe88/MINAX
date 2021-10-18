/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/
/*
 * Authenticate Director who is attempting to connect.
 *
 *   Kern Sibbald, October 2000
 *
 */

#include "bacula.h"
#include "filed.h"

extern CLIENT *me;                 /* my resource */
extern int beef;

const int dbglvl = 50;

static char hello_sd[]  = "Hello Bacula SD: Start Job %s %d tlspsk=%d\n";
static char hello_dir[] = "2000 OK Hello %d\n";
static char sorry_dir[] = "2999 Authentication failed.\n";

/*
 * Note, we handle the initial connection request here.
 *   We only get the jobname and the SD version, then we
 *   return, authentication will be done when the Director
 *   sends the storage command -- as is usually the case.
 *   This should be called only once by the SD.
 */
void *handle_storage_connection(BSOCK *sd)
{
   char job_name[500];
   char tbuf[150];
   int sd_version = 0;
   JCR *jcr;

   // Don't expect a TLS-PSK header in this "dummy" hello
   if (sscanf(sd->msg, "Hello FD: Bacula Storage calling Start Job %127s %d",
       job_name, &sd_version) != 2) {
      Jmsg(NULL, M_FATAL, 0, _("SD connect failed: Bad Hello command\n"));
      return NULL;
   }
   Dmsg1(110, "Got a SD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf),
         (utime_t)time(NULL)));
   Dmsg1(50, "authenticate storage: %s", sd->msg);

   if (!(jcr=get_jcr_by_full_name(job_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("SD connect failed: Job name not found: %s\n"), job_name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", job_name);
      sd->destroy();
      return NULL;
   }
   set_jcr_in_tsd(jcr);
   Dmsg1(150, "Found Job %s\n", job_name);

   jcr->lock_auth();
   /* We already have a socket connected, just discard it */
   if (jcr->sd_calls_client_bsock) {
      Qmsg1(jcr, M_WARNING, 0, _("SD \"%s\" tried to connect two times.\n"), sd->who());
      free_bsock(sd);
      /* will exit just after the unlock() */

   } else {
      /* If we have a previous socket in store_bsock, we are in multi restore mode */
      jcr->sd_calls_client_bsock = sd;
      sd->set_jcr(jcr);
   }
   jcr->unlock_auth();

   if (!sd) {                   /* freed by free_bsock(), connection already done */
      free_jcr(jcr);
      return NULL;
   }

   /* Turn on compression for newer FDs */
   if (sd_version >= 1 && me->comm_compression) {
      sd->set_compress();             /* set compression allowed */
   } else {
      sd->clear_compress();
      Dmsg2(050, "******** No FD compression to SD. sd_ver=%d compres=%d\n",
            sd_version, me->comm_compression);
   }

   if (!jcr->max_bandwidth) {
      if (jcr->director->max_bandwidth_per_job) {
         jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;

      } else if (me->max_bandwidth_per_job) {
         jcr->max_bandwidth = me->max_bandwidth_per_job;
      }
   }
   sd->set_bwlimit(jcr->max_bandwidth);

   Dmsg2(200, "beef=%ld sd_version=%ld\n", beef, sd_version);

   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   free_jcr(jcr);
   return NULL;
}


/*
 * Send Hello OK to DIR
 */
bool send_hello_ok(BSOCK *bs)
{
   return bs->fsend(hello_dir, FD_VERSION);
}

bool send_sorry(BSOCK *bs)
{
   return bs->fsend(sorry_dir);
}

/*
 * Send Hello to SD
 */
bool send_hello_sd(JCR *jcr, char *Job, int tlspsk)
{
   bool rtn;
   BSOCK *sd = jcr->store_bsock;

   bash_spaces(Job);
   rtn = sd->fsend(hello_sd, Job, FD_VERSION, tlspsk);
   unbash_spaces(Job);
   Dmsg1(100, "Send to SD: %s\n", sd->msg);
   return rtn;
}

/* ======================== */

/*
 */
bool send_fdcaps(JCR *jcr, BSOCK *sd)
{
   int do_dedup=0;
   int rehydration = 0; /* 0 : the SD do rehydration */

#if BEEF
   if (jcr->dedup_use_cache) {
      rehydration = 1; /* 1 : the FD do rehydration */
   }
   do_dedup=1;
#endif

   Dmsg3(200, "Send caps to SD dedup=%d rehydration=%d proxy=%d\n",
         do_dedup, rehydration, jcr->director->remote);

   return sd->fsend("fdcaps: dedup=%d rehydration=%d proxy=%d\n",
                    do_dedup, rehydration,
                    jcr->director->remote);
}

bool recv_sdcaps(JCR *jcr)
{
   int stat;
   int32_t dedup = (jcr->dedup != NULL);
   int32_t hash = 0;
   uint32_t block_size = 0;
   uint32_t min_block_size = 0;
   uint32_t max_block_size = 0;
   BSOCK *sd = jcr->store_bsock;

   Dmsg0(200, "Recv caps from SD.\n");
   stat = sd->recv();
   if (stat <= 0) {
      berrno be;
      Jmsg1(jcr, M_FATAL, 0, _("Recv caps from SD failed. ERR=%s\n"),
         be.bstrerror());
      Dmsg1(050, _("Recv caps from SD failed. ERR=%s\n"), be.bstrerror());
      return false;
   }
   Dmsg1(200, ">stored: %s\n", sd->msg);
   if (sscanf(sd->msg, "sdcaps: dedup=%ld hash=%ld dedup_block=%lu min_dedup_block=%lu max_dedup_block=%lu",
        &dedup, &hash, &block_size, &min_block_size, &max_block_size) != 5) {
      Jmsg1(jcr, M_FATAL, 0, _("Bad caps from SD: %s.\n"), sd->msg);
      Dmsg1(050, _("Bad caps from SD: %s\n"), sd->msg);
      return false;
   }
#if BEEF
   Dmsg5(200, "Recv sdcaps: dedup=%ld hash=%ld dedup_block=%lu min_dedup_block=%lu max_dedup_block=%lu\n",
        dedup, hash, block_size, min_block_size, max_block_size);
   jcr->sd_dedup = dedup;
   jcr->sd_hash = hash;
   jcr->sd_hash_size = bhash_info(jcr->sd_hash, NULL);
   jcr->dedup_block_size = block_size;
   jcr->min_dedup_block_size = min_block_size;
   jcr->max_dedup_block_size = max_block_size;
#endif
   return true;
}

/* Commands sent to Director */
static char hello[]    = "Hello %s calling %d tlspsk=%d\n";

/* fdcallsdir */
static char hello_fdcallsdir[]    = "Hello %s fdcallsdir %d tlspsk=%d\n";

/* Response from Director */
static char DirOKhello[] = "1000 OK: %d";
#define UA_VERSION 1

class FDUAAuthenticateDir: public AuthenticateBase
{
public:
   FDUAAuthenticateDir(JCR *jcr, BSOCK *UA_sock):
   AuthenticateBase(jcr, UA_sock, dtCli, dcFD, dcDIR) {
   }
   virtual ~FDUAAuthenticateDir() {};

   void TLSFailed() {
      Mmsg(jcr->errmsg, _("TLS negotiation failed\n"));
   }
   bool authenticate_director(const char *name, DIRINFO *dir, connect_dir_mode_t mode);
};

bool FDUAAuthenticateDir::authenticate_director(const char *name, DIRINFO *dir, connect_dir_mode_t mode)
{
   BSOCK *UA_sock = bsock;
   int dir_version = 0;
   char bashed_name[MAX_NAME_LENGTH];
   char *hello_cmd = hello;

   /*
    * Send my name to the Director then do authentication
    */
   bstrncpy(bashed_name, name, sizeof(bashed_name));
   bash_spaces(bashed_name);

   /* TLS Requirement */
   CalcLocalTLSNeedFromRes(dir->tls_enable, dir->tls_require,
         dir->tls_authenticate, false, NULL, dir->tls_ctx,
         dir->tls_psk_enable, dir->psk_ctx, dir->password);

   /* Timeout Hello after 15 secs */
   StartAuthTimeout(15);
   if (mode == CONNECT_FDCALLSDIR_MODE) {
      hello_cmd = hello_fdcallsdir;
   }
   UA_sock->fsend(hello_cmd, bashed_name, UA_VERSION, tlspsk_local_need);

   if (!ClientCramMD5Authenticate(dir->password)) {
      return false;
   }

   if (!HandleTLS()) {
      return false;
   }

   /*
    * It's possible that the TLS connection will
    * be dropped here if an invalid client certificate was presented
    */
   Dmsg1(dbglvl, ">dird: %s", UA_sock->msg);
   if (UA_sock->recv() <= 0) {
      Mmsg(jcr->errmsg, _("Bad response to Hello command: ERR=%s\n"),
                         UA_sock->bstrerror());
      return false;
   }

   Dmsg1(dbglvl, "<dird: %s", UA_sock->msg);
   if (strncmp(UA_sock->msg, DirOKhello, sizeof(DirOKhello)-3) == 0) {
      sscanf(UA_sock->msg, DirOKhello, &dir_version);
   } else {
      Mmsg(jcr->errmsg, _("Director rejected Hello command\n"));
      return false;
   }
   /* Turn on compression for newer Directors */
   if (dir_version >= 1 && (!dir || dir->comm_compression)) {
      UA_sock->set_compress();
   } else {
      UA_sock->clear_compress();
   }
   return true;

}

BSOCK *connect_director(JCR *jcr, const char *name, DIRINFO *dir, connect_dir_mode_t mode)
{
   BSOCK *UA_sock = NULL;
   int heart_beat;

   if (!dir || !dir->password || !dir->address) {
      return NULL;
   }

   if (dir) {
      heart_beat = dir->heartbeat_interval;
   } else {
      heart_beat = 0;
   }
   UA_sock = new_bsock();
   if (!UA_sock->connect(NULL, 5, 15, heart_beat, "Director daemon", dir->address,
                          NULL, dir->DIRport, 0)) {
      free_bsock(UA_sock);
      return NULL;
   }

   if (FDUAAuthenticateDir(jcr, UA_sock).authenticate_director(name, dir, mode)) {
      return UA_sock;
   }

   free_bsock(UA_sock);
   Mmsg(jcr->errmsg,
        ( _("Director authorization problem.\n"
            "Most likely the passwords do not agree.\n"
            "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
            "For help, please see " MANUAL_AUTH_URL "\n")));
   return NULL;
}
