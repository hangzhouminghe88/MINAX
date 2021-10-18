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
 *   Bacula Director -- authenticate.c -- handles authorization of
 *     Storage and File daemons.
 *
 *    Written by: Kern Sibbald, May MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 */

#include "bacula.h"
#include "dird.h"

static const int dbglvl = 50;

extern DIRRES *director;

/* Version at end of Hello
 * Note: Enterprise versions now are in 10000 range
 *   prior to 06Aug13 no version
 *       1 06Aug13 - added comm line compression
 *     102 04Jun15 - added jobmedia change
 *     103 14Feb17 - added comm line compression
 *   10002 04Jun15 - added jobmedia batching (from queue in SD)
 */
#define DIR_VERSION 10002


/* Command sent to SD */
static char hello[]    = "Hello %sDirector %s calling %d tlspsk=%d\n";

/* Responses from Storage and File daemons */
static char OKhello[]      = "3000 OK Hello";
static char SDOKnewHello[] = "3000 OK Hello %d";
static char FDOKhello[]    = "2000 OK Hello";
static char FDOKnewHello[] = "2000 OK Hello %d";

/* Sent to User Agent */
static char Dir_sorry[]  = "1999 You are not authorized.\n";

/* Forward referenced functions */

class DIRAuthenticateSD: public AuthenticateBase
{
public:
   DIRAuthenticateSD(JCR *jcr):
   AuthenticateBase(jcr, jcr->store_bsock, dtCli, dcDIR, dcSD)
   {
   }
   virtual ~DIRAuthenticateSD() {};
   bool authenticate_storage_daemon(STORE *store);
};

bool authenticate_storage_daemon(JCR *jcr, STORE *store)
{
   return DIRAuthenticateSD(jcr).authenticate_storage_daemon(store);
}

bool DIRAuthenticateSD::authenticate_storage_daemon(STORE *store)
{
   BSOCK *sd = bsock;
   char dirname[MAX_NAME_LENGTH];

   /* Calculate tls_local_need from the resource */
   CalcLocalTLSNeedFromRes(store->tls_enable, store->tls_require,
         store->tls_authenticate, false, NULL, store->tls_ctx,
         store->tls_psk_enable, store->psk_ctx, store->password);

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, director->name(), sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 1 min */
   StartAuthTimeout();
   /* Sent Hello SD: Bacula Director <dirname> calling <version> */
   if (!sd->fsend(hello, "SD: Bacula ", dirname, DIR_VERSION, tlspsk_local_need)) {
      Dmsg3(dbglvl, _("Error sending Hello to Storage daemon at \"%s:%d\". ERR=%s\n"),
            sd->host(), sd->port(), sd->bstrerror());
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon at \"%s:%d\". ERR=%s\n"),
            sd->host(), sd->port(), sd->bstrerror());
      return false;
   }

   /* Try to authenticate using cram-md5 */
   if (!ClientCramMD5Authenticate(store->password)) {
      return false;
   }

   if (!HandleTLS()) {
      return false;
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      Jmsg3(jcr, M_FATAL, 0, _("bdird<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
         sd->who(), sd->host(), sd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   jcr->SDVersion = 0;
   if (sscanf(sd->msg, SDOKnewHello, &jcr->SDVersion) != 1 &&
       strncmp(sd->msg, OKhello, sizeof(OKhello)) != 0) {
      Dmsg0(dbglvl, _("Storage daemon rejected Hello command\n"));
      Jmsg2(jcr, M_FATAL, 0, _("Storage daemon at \"%s:%d\" rejected Hello command\n"),
         sd->host(), sd->port());
      return 0;
   }
   /* For newer SD turn on comm line compression */
   if (jcr->SDVersion >= 1 && director->comm_compression) {
      sd->set_compress();
   } else {
      sd->clear_compress();
      Dmsg0(050, "*** No Dir compression to SD\n");
   }
   if (jcr->SDVersion < SD_VERSION) {
      Jmsg2(jcr, M_FATAL, 0, _("Older Storage daemon at \"%s:%d\" incompatible with this Director.\n"),
         sd->host(), sd->port());
      return 0;
   }
   return 1;
}

class DIRAuthenticateFD: public AuthenticateBase
{
public:
   DIRAuthenticateFD(JCR *jcr):
   AuthenticateBase(jcr, jcr->file_bsock, dtCli, dcDIR, dcFD)
   {
   }
   virtual ~DIRAuthenticateFD() {};
   int authenticate_file_daemon();
};

int authenticate_file_daemon(JCR *jcr)
{
   return DIRAuthenticateFD(jcr).authenticate_file_daemon();
}

int DIRAuthenticateFD::authenticate_file_daemon()
{
   BSOCK *fd = bsock;
   CLIENT *client = jcr->client;
   char dirname[MAX_NAME_LENGTH];

   /* Calculate tls_local_need from the resource */
   CalcLocalTLSNeedFromRes(client->tls_enable, client->tls_require, client->tls_authenticate,
   client->tls_allowed_cns, client->tls_allowed_cns, client->tls_ctx,
   client->tls_psk_enable, client->psk_ctx, client->password);

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, director->name(), sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 1 min */
   StartAuthTimeout();
   if (!fd->fsend(hello, "", dirname, DIR_VERSION, tlspsk_local_need)) {
      Dmsg3(dbglvl, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }

   /* Try to authenticate using cram-md5 */
   if (!ClientCramMD5Authenticate(client->password)) {
      return false;
   }

   if (!HandleTLS()) {
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      Dmsg1(dbglvl, _("Bad response from File daemon to Hello command: ERR=%s\n"),
         fd->bstrerror());
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
      return 0;
   }
   Dmsg1(110, "<filed: %s", fd->msg);
   StopAuthTimeout();
   jcr->FDVersion = 0;
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) != 0 &&
       sscanf(fd->msg, FDOKnewHello, &jcr->FDVersion) != 1) {
      Dmsg0(dbglvl, _("File daemon rejected Hello command\n"));
      Jmsg(jcr, M_FATAL, 0, _("File daemon at \"%s:%d\" rejected Hello command\n"),
           fd->host(), fd->port());
      return 0;
   }
   /* For newer FD turn on comm line compression */
   if (jcr->FDVersion >= 9 && jcr->FDVersion != 213 && director->comm_compression) {
      fd->set_compress();
   } else {
      fd->clear_compress();
      Dmsg0(050, "*** No Dir compression to FD\n");
   }
   return 1;
}

class UAAuthenticate: public AuthenticateBase
{
   UAContext *uac;
public:
   UAAuthenticate(UAContext *uac):
   AuthenticateBase(NULL, uac->UA_sock, dtSrv, dcDIR, dcCON),
   uac(uac)
   {
   }
   virtual ~UAAuthenticate() {};
   void TLSFailure() {
      Jmsg(jcr, M_SECURITY, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
            GetRemoteClassShortName(), bsock->host(), bsock->port());
   }

   int authenticate_user_agent();
};

int authenticate_user_agent(UAContext *uac)
{
   return UAAuthenticate(uac).authenticate_user_agent();
}

/*********************************************************************
 *
 */
int UAAuthenticate::authenticate_user_agent()
{
   char name[MAX_NAME_LENGTH];
   CONRES *cons = NULL;
   BSOCK *ua = uac->UA_sock;
   int ua_version = 0;
   int tlspsk_remote = 0;
   bool fdcallsdir=false;
   CLIENT *cli=NULL;

   if (ua->msglen < 16 || ua->msglen >= MAX_NAME_LENGTH + 15) {
      Qmsg3(NULL, M_SECURITY, 0, _("UA Hello from %s:%s is invalid. Len=%d\n"), ua->who(),
            ua->host(), ua->msglen);
      sleep(5);
      return 0;
   }
   Dmsg1(dbglvl, "authenticate user agent: %s", ua->msg);
   if (scan_string(ua->msg, "Hello %127s fdcallsdir %d tlspsk=%d", name, &ua_version, &tlspsk_remote) == 3 ||
       scan_string(ua->msg, "Hello %127s fdcallsdir %d", name, &ua_version) == 2) {
      fdcallsdir = true;
   } else if (scan_string(ua->msg, "Hello %127s calling %d tlspsk=%d", name, &ua_version, &tlspsk_remote) != 3 &&
       scan_string(ua->msg, "Hello %127s calling %d", name, &ua_version) != 2 &&
       scan_string(ua->msg, "Hello %127s calling", name) != 1)
   {
      ua->msg[100] = 0;               /* terminate string */
      Qmsg3(NULL, M_SECURITY, 0, _("UA Hello from %s:%s is invalid. Got: %s\n"), ua->who(),
            ua->host(), ua->msg);
      sleep(5);
      return 0;
   }

   /* Turn on compression for newer consoles */
   if (ua_version >= 1  && director->comm_compression) {
      ua->set_compress();
   } else {
      Dmsg0(050, "*** No Dir compression to UA\n");
   }

   name[sizeof(name)-1] = 0;             /* terminate name */
   if (strcmp(name, "*UserAgent*") == 0) {  /* default console */
      /* TLS Requirement */
      CalcLocalTLSNeedFromRes(director->tls_enable, director->tls_require,
            director->tls_authenticate, director->tls_verify_peer,
            director->tls_allowed_cns, director->tls_ctx,
            director->tls_psk_enable, director->psk_ctx, director->password);

   } else if (fdcallsdir) {
      unbash_spaces(name);
      cli = (CLIENT *)GetResWithName(R_CLIENT, name);
      if (cli && cli->allow_fd_connections) {
         /* TLS Requirement */
         CalcLocalTLSNeedFromRes(cli->tls_enable, cli->tls_require,
                                 cli->tls_authenticate, cli->tls_verify_peer,
                                 cli->tls_allowed_cns, cli->tls_ctx,
                                 cli->tls_psk_enable, cli->psk_ctx, cli->password);
      } else {
         if (cli) {
            Dmsg1(10, "AllowFDConnections not set for %s\n", name);
         }
         auth_success = false;
         goto auth_done;
      }

   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
         /* TLS Requirement */
         CalcLocalTLSNeedFromRes(cons->tls_enable, cons->tls_require,
               cons->tls_authenticate, cons->tls_verify_peer, cons->tls_allowed_cns,
               cons->tls_ctx, cons->tls_psk_enable, cons->psk_ctx, cons->password);
      } else {
         auth_success = false;
         goto auth_done;
      }
   }
   DecodeRemoteTLSPSKNeed(tlspsk_remote);

   if (!ServerCramMD5Authenticate(password)) {
      goto auth_done;
   }

   if (cons) {
      uac->cons = cons;         /* save console resource pointer */
      bstrncpy(uac->name, uac->cons->hdr.name, sizeof(uac->name));
   }

   this->auth_success = HandleTLS();

   if (!auth_success) {
      goto auth_done;
   }

/* Authorization Completed */
auth_done:
   if (!auth_success) {
      ua->fsend("%s", _(Dir_sorry));
      Jmsg4(NULL, M_SECURITY, 0, _("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
            name, ua->who(), ua->host(), ua->port());
      sleep(5);
      return 0;
   }
   ua->fsend(_("1000 OK: %d %s %sVersion: %s (%s)\n"),
      DIR_VERSION, my_name, BDEMO, VERSION, BDATE);

   if (fdcallsdir) {
      Dmsg1(10, "FDCallsDir OK for %s\n", name);
      ua->fsend(_("OK\n"));
      cli->setBSOCK(ua);
      uac->UA_sock = NULL;
      uac->quit = true;
   }
   return 1;
}
