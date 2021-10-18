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

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

FDAuthenticateDIR::FDAuthenticateDIR(JCR *jcr):
AuthenticateBase(jcr, jcr->dir_bsock, dtSrv, dcFD, dcDIR)
{
}

/*********************************************************************
 *
 * Validate hello from the Director
 *
 * Returns: true  if Hello is good.
 *          false if Hello is bad.
 */
bool FDAuthenticateDIR::validate_dir_hello()
{
   POOLMEM *dirname;
   DIRRES *director = NULL;
   int dir_version = 0;
   BSOCK *dir = bsock;
   int tlspsk_remote = 0;
   bool auth_successful = false;

   if (dir->msglen < 25 || dir->msglen > 500) {
      Dmsg2(dbglvl, "Bad Hello command from Director at %s. Len=%d.\n",
            dir->who(), dir->msglen);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s. Len=%d.\n"),
            dir->who(), dir->msglen);
      return false;
   }
   dirname = get_pool_memory(PM_MESSAGE);
   dirname = check_pool_memory_size(dirname, dir->msglen);

   Dmsg1(dbglvl, "authenticate dir: %s", dir->msg);
   if (scan_string(dir->msg, "Hello Director %127s calling %d tlspsk=%d", dirname,
            &dir_version, &tlspsk_remote) != 3 &&
       scan_string(dir->msg, "Hello Director %127s calling %d", dirname,
            &dir_version) != 2 &&
       scan_string(dir->msg, "Hello Director %127s calling", dirname) != 1 &&
       scan_string(dir->msg, "Hello %127s calling %d tlspsk=%d", dirname,
                   &dir_version, &tlspsk_remote) != 3 &&
       scan_string(dir->msg, "Hello %127s calling %d", dirname, &dir_version) != 2 ) {
      char addr[64];
      char *who = dir->get_peer(addr, sizeof(addr)) ? dir->who() : addr;
      dir->msg[100] = 0;
      Dmsg2(dbglvl, "Bad Hello command from Director at %s: %s\n",
            dir->who(), dir->msg);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"),
            who, dir->msg);
      goto auth_fatal;
   }
   DecodeRemoteTLSPSKNeed(tlspsk_remote);
   if (beef && dir_version >= 1 && me->comm_compression) {
      dir->set_compress();
   } else {
      dir->clear_compress();
      Dmsg0(050, "*** No FD compression to DIR\n");
   }
   unbash_spaces(dirname);
   foreach_res(director, R_DIRECTOR) {
      if (strcmp(director->hdr.name, dirname) == 0)
         break;
   }
   if (!director) {
      char addr[64];
      char *who = dir->get_peer(addr, sizeof(addr)) ? dir->who() : addr;
      Jmsg2(jcr, M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"),
            dirname, who);
      goto auth_fatal;
   }
   auth_successful = true;

auth_fatal:
   free_pool_memory(dirname);
   jcr->director = director;
   /* Single thread all failures to avoid DOS */
   if (!auth_successful) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }
   return auth_successful;
}

/*
 * Authenticated the Director
 */
bool FDAuthenticateDIR::authenticate_director()
{
   BSOCK *dir = jcr->dir_bsock;
   DIRRES *director = jcr->director;

   CalcLocalTLSNeedFromRes(director->dirinfo.tls_enable, director->dirinfo.tls_require,
         director->dirinfo.tls_authenticate, director->dirinfo.tls_verify_peer,
         director->dirinfo.tls_allowed_cns, director->dirinfo.tls_ctx,
         director->dirinfo.tls_psk_enable, director->dirinfo.psk_ctx, director->dirinfo.password);

   /* Timeout authentication after 10 mins */
   StartAuthTimeout();

   /* Challenge the director */
   if (!ServerCramMD5Authenticate(director->dirinfo.password)) {
      goto auth_fatal;
   }

   this->auth_success = HandleTLS();

auth_fatal:
   if (auth_success) {
      return send_hello_ok(dir);
   }
   send_sorry(dir);
   /* Single thread all failures to avoid DOS */
   P(mutex);
   bmicrosleep(6, 0);
   V(mutex);
   return false;
}

FDAuthenticateSD::FDAuthenticateSD(JCR *jcr):
AuthenticateBase(jcr, jcr->store_bsock, dtCli, dcSD, dcSD)
{
   /* TLS Requirement must be done before the send "hello" */
   CalcLocalTLSNeedFromRes(me->tls_enable, me->tls_require, me->tls_authenticate,
         false, NULL, me->tls_ctx,
         me->tls_psk_enable, me->psk_ctx, jcr->sd_auth_key);
}

/*
 * First prove our identity to the Storage daemon, then
 * make him prove his identity.
 */
bool FDAuthenticateSD::authenticate_storagedaemon()
{
   BSOCK *sd = jcr->store_bsock;
   int sd_version = 0;

   /* Timeout authentication after 10 mins */
   StartAuthTimeout();

   /* Challenge the FD */
   if (!ClientCramMD5Authenticate(jcr->sd_auth_key)) {
      goto auth_fatal;
   }

   this->auth_success = HandleTLS();

   if (!auth_success) {
      goto auth_fatal;
   }

   if (sd->recv() <= 0) {
      auth_success = false;
      goto auth_fatal;
   }
   sscanf(sd->msg, "3000 OK Hello %d", &sd_version);
   if (sd_version >= 1 && me->comm_compression) {
      sd->set_compress();
   } else {
      sd->clear_compress();
      Dmsg0(050, "*** No FD compression with SD\n");
   }

   /* At this point, we have successfully connected */

auth_fatal:
   /* Destroy session key */
   memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   /* Single thread all failures to avoid DOS */
   if (!auth_success) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }
   return auth_success;
}
