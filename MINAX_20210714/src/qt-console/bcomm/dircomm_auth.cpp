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
 *
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI   adapted to bat, Jan MMVI
 *
 */


#include "bat.h"

/*
 * Version at end of Hello
 *   prior to 06Aug13 no version
 *   1 21Oct13 - added comm line compression
 */
#define BAT_VERSION 1


/* Commands sent to Director */
static char hello[]    = "Hello %s calling %d tlspsk=%d\n";

/* Response from Director */
static char oldOKhello[]   = "1000 OK:";
static char newOKhello[]   = "1000 OK: %d";
static char FDOKhello[]   = "2000 OK Hello %d";
class AuthenticateBase;


class DirCommAuthenticate: public AuthenticateBase
{
   char *errmsg;
   int errmsg_len;

public:
   DirCommAuthenticate(BSOCK *bsock, char *a_errmsg, int a_errmsg_len);
   ~DirCommAuthenticate();
   virtual void TLSFailure();
   virtual bool CheckTLSRequirement();
   bool authenticate_director(DIRRES *director, CONRES *cons, bool dump_ok);
};

DirCommAuthenticate::DirCommAuthenticate(BSOCK *bsock, char *a_errmsg, int a_errmsg_len):
AuthenticateBase(NULL, bsock, dtCli, dcCON, dcDIR),
errmsg(a_errmsg),
errmsg_len(a_errmsg_len)
{
}

DirCommAuthenticate::~DirCommAuthenticate()
{
};

void DirCommAuthenticate::TLSFailure()
{
   bsnprintf(errmsg, errmsg_len,
      _("TLS negotiation failed with Director at \"%s:%d\"\n"),
      bsock->host(), bsock->port());
}

bool DirCommAuthenticate::CheckTLSRequirement()
{
   /* Verify that the connection is willing to meet our TLS requirements */
   switch (TestTLSRequirement()) {
   case TLS_REQ_ERR_LOCAL:
     bsnprintf(errmsg, errmsg_len, _("Authorization problem with Director at \"%s:%d\":"
                     " Remote server requires TLS.\n"),
            bsock->host(), bsock->port());
     return false;

   case TLS_REQ_ERR_REMOTE:
      bsnprintf(errmsg, errmsg_len, _("Authorization problem:"
             " Remote server at \"%s:%d\" did not advertise required TLS support.\n"),
             bsock->host(), bsock->port());
      return false;
   case TLS_REQ_OK:
      break;
   }
   return true;
}

bool DirCommAuthenticate::authenticate_director(DIRRES *director, CONRES *cons, bool dump_ok)
{
   BSOCK *dir = bsock;
   int dir_version = 0;
   char bashed_name[MAX_NAME_LENGTH];

   errmsg[0] = 0;
   /*
    * Send my name to the Director then do authentication
    */
   if (cons) {
      bstrncpy(bashed_name, cons->hdr.name, sizeof(bashed_name));
      bash_spaces(bashed_name);
      CalcLocalTLSNeedFromRes(cons->tls_enable, cons->tls_require,
                  cons->tls_authenticate, false, NULL, cons->tls_ctx,
                  cons->tls_psk_enable, cons->psk_ctx, cons->password);

   } else {
      bstrncpy(bashed_name, "*UserAgent*", sizeof(bashed_name));
      CalcLocalTLSNeedFromRes(director->tls_enable, director->tls_require,
            director->tls_authenticate, false, NULL, director->tls_ctx,
            director->tls_psk_enable, director->psk_ctx, director->password);
   }

   /* Timeout Hello after 15 secs */
   StartAuthTimeout(15);
   dir->fsend(hello, bashed_name, BAT_VERSION, tlspsk_local_need);

   if (!ClientCramMD5Authenticate(password)) {
      goto bail_out;
   }

   if (!HandleTLS()) {
      return false;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->stop_timer();
      bsnprintf(errmsg, errmsg_len, _("Bad response to Hello command: ERR=%s\n"
                      "The Director at \"%s:%d\" is probably not running.\n"),
                    dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, oldOKhello, sizeof(oldOKhello)-1) == 0) {
      /* If Dir version exists, get it */
      sscanf(dir->msg, newOKhello, &dir_version);

      /* We do not check the last %d */
   } else if (strncmp(dir->msg, FDOKhello, sizeof(FDOKhello)-3) == 0) {
      sscanf(dir->msg, FDOKhello, &dir_version);
      // TODO: Keep somewhere that we need a proxy command, or run it directly?
   } else {
      bsnprintf(errmsg, errmsg_len, _("Director at \"%s:%d\" rejected Hello command\n"),
                dir->host(), dir->port());
      return false;
   }

   /* Turn on compression for newer Directors */
   if (dir_version >= 1 && (!cons || cons->comm_compression)) {
      dir->set_compress();
   }

   if (dump_ok) {
      bsnprintf(errmsg, errmsg_len, "%s", dir->msg);
   }
   return true;

bail_out:
   bsnprintf(errmsg, errmsg_len, _("Authorization problem with Director at \"%s:%d\"\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "For help, please see " MANUAL_AUTH_URL "\n"),
             dir->host(), dir->port());
   return false;
}

/*
 * Authenticate Director
 */
bool DirComm::authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons,
                                    char *errmsg, int errmsg_len)
{
   DirCommAuthenticate auth(jcr->dir_bsock, errmsg, errmsg_len);
   return auth.authenticate_director(director, cons, m_conn == 0);
}

