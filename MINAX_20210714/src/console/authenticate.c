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
 *     Kern Sibbald, June MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *
 */

#include "bacula.h"
#include "console_conf.h"

/*
 * Version at end of Hello Enterprise:
 *   prior to 06Aug13 no version
 *   1 06Aug13 - added comm line compression
 * Community
 *   prior to 06Aug13 no version
 *   100  14Feb17 - added comm line compression
 */
#ifndef COMMUNITY
#define UA_VERSION 1   /* Enterprise */
#else
#define UA_VERSION 100 /* Community */
#endif

void senditf(const char *fmt, ...);
void sendit(const char *buf);

/* Commands sent to Director */
static char hello[]    = "Hello %s calling %d tlspsk=%d\n";

/* Response from Director */
static char oldOKhello[]   = "1000 OK:";
static char newOKhello[]   = "1000 OK: %d";
static char FDOKhello[]   = "2000 OK Hello %d";

class ConsoleAuthenticate: public AuthenticateBase
{
public:
   ConsoleAuthenticate(BSOCK *dir):
   AuthenticateBase(NULL, dir, dtCli, dcCON, dcDIR)
   {
   }
   virtual ~ConsoleAuthenticate() {};
   virtual void TLSFailure() { sendit(_("TLS negotiation failed\n")); }
   virtual bool CheckTLSRequirement();

   int authenticate_director(DIRRES *director, CONRES *cons);
};


int authenticate_director(BSOCK *dir, DIRRES *director, CONRES *cons)
{
   ConsoleAuthenticate auth(dir);
   return auth.authenticate_director(director, cons);
}

bool ConsoleAuthenticate::CheckTLSRequirement()
{
   /* Verify that the connection is willing to meet our TLS requirements */
   switch (TestTLSRequirement()) {
   case TLS_REQ_ERR_LOCAL:
     sendit(_("Authorization problem:"
             " Remote server requires TLS.\n"));
      return false;

   case TLS_REQ_ERR_REMOTE:
      sendit(_("Authorization problem:"
             " Remote server did not advertise required TLS support.\n"));
      return false;
   case TLS_REQ_OK:
      break;
   }
   return true;
}

/*
 * Authenticate Director
 */
int ConsoleAuthenticate::authenticate_director(DIRRES *director, CONRES *cons)
{
   BSOCK *dir = bsock;
   int dir_version = 0;
   char bashed_name[MAX_NAME_LENGTH];
   bool skip_msg = false;
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

   dir->fsend(hello, bashed_name, UA_VERSION, tlspsk_local_need);

   if (!ClientCramMD5Authenticate(password)) {
      if (dir->is_timed_out()) {
         sendit(_("The Director is busy or the MaximumConsoleConnections limit is reached.\n"));
         skip_msg = true;
      }
      goto bail_out;
   }

   if (!HandleTLS()) {
      goto bail_out;
   }

   /*
    * It's possible that the TLS connection will
    * be dropped here if an invalid client certificate was presented
    */
   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      senditf(_("Bad response to Hello command: ERR=%s\n"),
         dir->bstrerror());
      goto bail_out;
   }

   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, oldOKhello, sizeof(oldOKhello)-1) == 0) {
      /* If Dir version exists, get it */
      sscanf(dir->msg, newOKhello, &dir_version);
      sendit(dir->msg);

      /* We do not check the last %d */
   } else if (strncmp(dir->msg, FDOKhello, sizeof(FDOKhello)-3) == 0) {
      sscanf(dir->msg, FDOKhello, &dir_version);
      sendit(dir->msg);

   } else {
      sendit(_("Director rejected Hello command\n"));
      goto bail_out;
   }
   /* Turn on compression for newer Directors */
   if (dir_version >= 1 && (!cons || cons->comm_compression)) {
      dir->set_compress();
   } else {
      dir->clear_compress();
   }
   return 1;

bail_out:
   if (!skip_msg) {
      sendit( _("Director authorization problem.\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "For help, please see " MANUAL_AUTH_URL "\n"));
   }
   return 0;
}
