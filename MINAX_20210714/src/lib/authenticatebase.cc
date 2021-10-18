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
 * authenticateb.c
 *
 * AuthenticateBase is the base class to handles authentication for all daemons
 *
 */

#include "bacula.h"
#include "jcr.h"

static const int authdl = 50;

const char *AuthenticateBase::dc_short_name[6]={ "UNK", "CON", "FD", "SD", "DIR", "GUI" };
const char *AuthenticateBase::dc_long_name[6]={
      "Unknown", "Console", "File Daemon", "Storage Daemon", "Director", "Gui"
};

AuthenticateBase::AuthenticateBase(JCR *jcr, BSOCK *bsock, int loc_typ, int loc_cls, int rm_cls):
jcr(jcr),
bsock(bsock),
local_type(loc_typ),
local_class(loc_cls),
remote_class(rm_cls),
tls_local_need(BNET_TLS_NONE),
tls_remote_need(BNET_TLS_NONE),
tls_authenticate(false),
tls_verify_peer(false),
tls_verify_list(NULL),
verify_list(NULL),
tls_ctx(NULL),
psk_ctx(NULL),
ctx(NULL),
password(NULL),
psk_local_need(BNET_TLS_NONE),
psk_remote_need(BNET_TLS_NONE),
tlspsk_local_need(0),
tid(NULL),
auth_success(false),
check_early_tls(false),
tls_started(false)
{
   local_name[0]='\0';
   remote_name[0]='\0';
}

AuthenticateBase::~AuthenticateBase()
{
   StopAuthTimeout();
}

/*
 * 
 * LOCAL                    REMOTE
 * {TLS,PSK}                {TLS,PSK}
 * --------------------------------------------------
 * {OK,OK}                 {OK,OK}               => t   TLS
 * {OK,OK}                 {OK,NONE}             => t   TLS
 * {OK,OK}                 {OK,REQUIRED}         => t   TLS
 * {OK,OK}                 {NONE,OK}             => t   PSK
 * {OK,OK}                 {NONE,NONE}           => t   NONE
 * {OK,OK}                 {NONE,REQUIRED}       => t   PSK
 * {OK,OK}                 {REQUIRED,OK}         => t   TLS
 * {OK,OK}                 {REQUIRED,NONE}       => t   TLS
 * {OK,OK}                 {REQUIRED,REQUIRED}   => t   TLS
 *
 * {OK,NONE}               {OK,OK}               => t
 * {OK,NONE}               {OK,NONE}             => t
 * {OK,NONE}               {OK,REQUIRED}         => t
 * {OK,NONE}               {NONE,OK}             => t
 * {OK,NONE}               {NONE,NONE}           => t
 * {OK,NONE}               {NONE,REQUIRED}       => F   -- Remote
 * {OK,NONE}               {REQUIRED,OK}         => t
 * {OK,NONE}               {REQUIRED,NONE}       => t
 * {OK,NONE}               {REQUIRED,REQUIRED}   => t
 *
 * Not possible, it means TLS keys is defined + TLS Requires = yes
 * {OK,REQUIRED}           {OK,OK}               => t
 * {OK,REQUIRED}           {OK,NONE}             => t
 * {OK,REQUIRED}           {OK,REQUIRED}         => t
 * {OK,REQUIRED}           {NONE,OK}             => t
 * {OK,REQUIRED}           {NONE,NONE}           => F  -- Local
 * {OK,REQUIRED}           {NONE,REQUIRED}       => t
 * {OK,REQUIRED}           {REQUIRED,OK}         => t
 * {OK,REQUIRED}           {REQUIRED,NONE}       => t
 * {OK,REQUIRED}           {REQUIRED,REQUIRED}   => t
 *
 * {NONE,OK}               {OK,OK}               => t
 * {NONE,OK}               {OK,NONE}             => t
 * {NONE,OK}               {OK,REQUIRED}         => t
 * {NONE,OK}               {NONE,OK}             => t
 * {NONE,OK}               {NONE,NONE}           => t
 * {NONE,OK}               {NONE,REQUIRED}       => t
 * {NONE,OK}               {REQUIRED,OK}         => t
 * {NONE,OK}               {REQUIRED,NONE}       => F  -- Remote
 * {NONE,OK}               {REQUIRED,REQUIRED}   => t
 *
 * {NONE,NONE}             {OK,OK}               => t
 * {NONE,NONE}             {OK,NONE}             => t
 * {NONE,NONE}             {OK,REQUIRED}         => F
 * {NONE,NONE}             {NONE,OK}             => t
 * {NONE,NONE}             {NONE,NONE}           => t
 * {NONE,NONE}             {NONE,REQUIRED}       => F  -- Remote
 * {NONE,NONE}             {REQUIRED,OK}         => F  -- Remote
 * {NONE,NONE}             {REQUIRED,NONE}       => F  -- Remote
 * {NONE,NONE}             {REQUIRED,REQUIRED}   => F
 *
 * {NONE,REQUIRED}         {OK,OK}               => t
 * {NONE,REQUIRED}         {OK,NONE}             => F  -- Local
 * {NONE,REQUIRED}         {OK,REQUIRED}         => t
 * {NONE,REQUIRED}         {NONE,OK}             => t
 * {NONE,REQUIRED}         {NONE,NONE}           => F  -- Local
 * {NONE,REQUIRED}         {NONE,REQUIRED}       => t
 * {NONE,REQUIRED}         {REQUIRED,OK}         => t
 * {NONE,REQUIRED}         {REQUIRED,NONE}       => F  -- Local
 * {NONE,REQUIRED}         {REQUIRED,REQUIRED}   => t
 *
 * {REQUIRED,OK}           {OK,OK}               => t
 * {REQUIRED,OK}           {OK,NONE}             => t
 * {REQUIRED,OK}           {OK,REQUIRED}         => t
 * {REQUIRED,OK}           {NONE,OK}             => t
 * {REQUIRED,OK}           {NONE,NONE}           => F  -- Local
 * {REQUIRED,OK}           {NONE,REQUIRED}       => t
 * {REQUIRED,OK}           {REQUIRED,OK}         => t
 * {REQUIRED,OK}           {REQUIRED,NONE}       => t
 * {REQUIRED,OK}           {REQUIRED,REQUIRED}   => t
 *
 * {REQUIRED,NONE}         {OK,OK}               => t
 * {REQUIRED,NONE}         {OK,NONE}             => t
 * {REQUIRED,NONE}         {OK,REQUIRED}         => t
 * {REQUIRED,NONE}         {NONE,OK}             => F  -- Local
 * {REQUIRED,NONE}         {NONE,NONE}           => F  -- Local
 * {REQUIRED,NONE}         {NONE,REQUIRED}       => F  -- Local
 * {REQUIRED,NONE}         {REQUIRED,OK}         => t
 * {REQUIRED,NONE}         {REQUIRED,NONE}       => t
 * {REQUIRED,NONE}         {REQUIRED,REQUIRED}   => t
 *
 * {REQUIRED,REQUIRED}     {OK,OK}               => t
 * {REQUIRED,REQUIRED}     {OK,NONE}             => t
 * {REQUIRED,REQUIRED}     {OK,REQUIRED}         => t
 * {REQUIRED,REQUIRED}     {NONE,OK}             => t
 * {REQUIRED,REQUIRED}     {NONE,NONE}           => F  -- Local
 * {REQUIRED,REQUIRED}     {NONE,REQUIRED}       => t
 * {REQUIRED,REQUIRED}     {REQUIRED,OK}         => t
 * {REQUIRED,REQUIRED}     {REQUIRED,NONE}       => t
 * {REQUIRED,REQUIRED}     {REQUIRED,REQUIRED}   => t
 * 
 */

/* OK
 * Remote requirement not ok
 * Local requirement not ok
 */
int AuthenticateBase::TestTLSRequirement()
{
   /* {OK,NONE}               {NONE,REQUIRED}       => F   -- Remote */
   if (tls_local_need == BNET_TLS_OK && psk_local_need == BNET_TLS_NONE
       &&
       tls_remote_need == BNET_TLS_NONE && psk_remote_need == BNET_TLS_REQUIRED)
   {
      return TLS_REQ_ERR_REMOTE;
   }

   /* {OK,REQUIRED}           {NONE,NONE}           => F  -- Local */
   if (tls_local_need == BNET_TLS_OK && psk_local_need == BNET_TLS_REQUIRED
       &&
       tls_remote_need == BNET_TLS_NONE && psk_remote_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_LOCAL;
   }

   /* {NONE,OK}               {REQUIRED,NONE}       => F  -- Remote */
   if (tls_local_need == BNET_TLS_NONE && psk_local_need == BNET_TLS_OK
       &&
       tls_remote_need == BNET_TLS_REQUIRED && psk_remote_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_REMOTE;
   }

   /* {NONE,NONE}             {OK,REQUIRED}         => F
    * {NONE,NONE}             {NONE,REQUIRED}       => F  -- Remote
    * {NONE,NONE}             {REQUIRED,OK}         => F  -- Remote
    * {NONE,NONE}             {REQUIRED,NONE}       => F  -- Remote
    * {NONE,NONE}             {REQUIRED,REQUIRED}   => F  
    */
   if (tls_local_need == BNET_TLS_NONE && psk_local_need == BNET_TLS_NONE
       &&
      (tls_remote_need == BNET_TLS_REQUIRED || psk_remote_need == BNET_TLS_REQUIRED))
    {
       return TLS_REQ_ERR_REMOTE;
    }

   /* {NONE,REQUIRED}         {OK,NONE}             => F  -- Local
    * {NONE,REQUIRED}         {NONE,NONE}           => F  -- Local
    * {NONE,REQUIRED}         {REQUIRED,NONE}       => F  -- Local
    */
   if (tls_local_need == BNET_TLS_NONE && psk_local_need == BNET_TLS_REQUIRED
       &&
       psk_remote_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_LOCAL;
   }

   /* {REQUIRED,OK}           {NONE,NONE}           => F  -- Local */
   if (tls_local_need == BNET_TLS_REQUIRED && psk_local_need == BNET_TLS_OK
       &&
       tls_local_need == BNET_TLS_NONE && psk_remote_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_LOCAL;
   }

   /* {REQUIRED,NONE}         {NONE,OK}             => F  -- Local
    * {REQUIRED,NONE}         {NONE,NONE}           => F  -- Local
    * {REQUIRED,NONE}         {NONE,REQUIRED}       => F  -- Local
    */
   if (tls_local_need == BNET_TLS_REQUIRED && psk_local_need == BNET_TLS_NONE
       &&
       tls_local_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_LOCAL;
   }

   /* {REQUIRED,REQUIRED}     {NONE,NONE}           => F  -- Local */
   if (tls_local_need == BNET_TLS_REQUIRED && psk_local_need == BNET_TLS_REQUIRED
       &&
       tls_local_need == BNET_TLS_NONE && psk_local_need == BNET_TLS_NONE)
   {
      return TLS_REQ_ERR_LOCAL;
   }
   return TLS_REQ_OK;
}

const char *AuthenticateBase::GetLocalClassShortName()
{
   return dc_short_name[local_class];
};

const char *AuthenticateBase::GetLocalClassLongName()
{
   return dc_long_name[local_class];
};

const char *AuthenticateBase::GetRemoteClassShortName()
{
   return dc_short_name[remote_class];
};

const char *AuthenticateBase::GetRemoteClassLongName()
{
   return dc_long_name[remote_class];
};

int AuthenticateBase::GetTLSPSKLocalNeed()
{
   return tlspsk_local_need;
};

void AuthenticateBase::StartAuthTimeout(int auth_timeout)
{
   tid = start_bsock_timer(bsock, auth_timeout);
}

void AuthenticateBase::StopAuthTimeout()
{
   if (tid != NULL) {
      stop_bsock_timer(tid);
      tid = NULL;
   }
}

/* calculate this->tls_local_need from RES->tls_enable and tls_require */
/*
 * psk_local_need
 *   TLS  |   TLS    |  TLS PSK  |
 * Enable | Required |  Enable   |
 * ------------------------------+
 *    n        n           n          Clear Text
 *    n        n           y          PSK is welcome
 *    n        y           n          --
 *    n        y           y          PSK is REQUIRED
 *    y        n           n          TLS is welcome
 *    y        n           y          TLS and PSK are welcome
 *    y        y           n          TLS is REQUIRED
 *    y        y           y          TLS or PSK are required
 */
void AuthenticateBase::CalcLocalTLSNeedFromRes(bool tls_enable, bool tls_require,
      bool atls_authenticate, bool atls_verify_peer, alist *atls_verify_list,
      TLS_CONTEXT *atls_ctx, bool tls_psk_enable, TLS_CONTEXT *apsk_ctx,
      const char *apassword)
{
   tls_authenticate=atls_authenticate;
   if (tls_enable) {
      if (tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }
   if (tls_psk_enable) {
      if (tls_require) {
         psk_local_need = BNET_TLS_REQUIRED;
      } else if (apsk_ctx != NULL) {
         psk_local_need = BNET_TLS_OK;
      } else {
         psk_local_need = BNET_TLS_NONE; /* TLS PSK not available */
      }
   }
   tls_verify_peer = atls_verify_peer;
   if (tls_verify_peer) {
      tls_verify_list = atls_verify_list;
   } else {
      tls_verify_list = NULL;
   }
   tls_ctx = atls_ctx;
   psk_ctx = apsk_ctx;
   password = apassword;
   tlspsk_local_need = tls_local_need+psk_local_need*100; // Encode TLS-PSK need
   Dmsg1(10, "TLSPSK Local need %d\n", tlspsk_local_need);
   bsock->tlspsk_local = tlspsk_local_need;
}

bool AuthenticateBase::CheckTLSRequirement()
{
   int msg_type = (local_class == dcDIR && remote_class == dcCON)?M_SECURITY:M_FATAL;

   /* Verify that the connection is willing to meet our TLS requirements */
   switch (TestTLSRequirement()) {
   case TLS_REQ_ERR_LOCAL:
      Jmsg(jcr, msg_type, 0, _("Authorization problem: %s \"%s:%s\" did not advertise required TLS support.\n"),
           GetRemoteClassShortName(), bsock->who(), bsock->host());
      return false;

   case TLS_REQ_ERR_REMOTE:
      Jmsg(jcr, msg_type, 0, _("Authorization problem: %s \"%s:%s\" did not advertise required TLS support.\n"),
           GetRemoteClassShortName(), bsock->who(), bsock->host());
      return false;
   case TLS_REQ_OK:
      break;
   }
   return true;
}

/* Convert single "remote_need" from remote hello message into PSK and TLS need */
void AuthenticateBase::DecodeRemoteTLSPSKNeed(int remote_need)
{
   tls_remote_need=remote_need%100;
   psk_remote_need=remote_need/100;
   Dmsg1(10, "TLSPSK Remote need %d\n", remote_need);
}

bool AuthenticateBase::ClientEarlyTLS()
{
   int tlspsk_remote=0;

   check_early_tls=true;
   if (bsock->recv() <= 0) {
      bmicrosleep(5, 0); // original cram_md5_respond() wait for 5s here
      return false;
   }
   if (scan_string(bsock->msg, "starttls tlspsk=%d\n", &tlspsk_remote) != EOF) {
      DecodeRemoteTLSPSKNeed(tlspsk_remote);
      if (!HandleTLS()) {
         return false;
      }
      check_early_tls = false; // "tell" cram_md5_respond to do a recv()
   }
   return true;
}

/* DIR is calling, DIR is the client */
bool AuthenticateBase::ClientCramMD5Authenticate(const char *password)
{
   int compatible = true;

   if (!ClientEarlyTLS()) {
      return false;
   }

   if (((local_class == dcSD && remote_class == dcSD) ||
        (local_class == dcFD && remote_class == dcSD))) {
      if (jcr && job_canceled(jcr)) {
         auth_success = false;
         return false;                   /* quick exit */
      }
   }

   auth_success = cram_md5_respond(bsock, password, &tls_remote_need, &compatible, check_early_tls);

   if (local_class == dcSD && remote_class == dcSD) {
      if (jcr && job_canceled(jcr)) {
         auth_success = false;
         return false;                   /* quick exit */
      }
   }

   if (auth_success) {
      auth_success = cram_md5_challenge(bsock, password, tls_local_need, compatible);
      if (!auth_success) {
         Dmsg2(authdl, "cram_challenge failed for %s: %s\n",
               GetRemoteClassShortName(), bsock->who());
      }
   } else {
      Dmsg2(authdl, "cram_respond failed for %s: %s\n",
            GetRemoteClassShortName(), bsock->who());
   }

   if (!auth_success) {
      if ((local_class == dcFD && remote_class == dcSD) ||
          (local_class == dcSD && remote_class == dcFD) ) {
         Dmsg2(authdl, "Authorization key rejected by %s at %s.\n",
            GetRemoteClassShortName(), bsock->who());
         Jmsg(jcr, M_FATAL, 0, _("Authorization key rejected by %s at %s rejected.\n"
           "For help, please see: " MANUAL_AUTH_URL "\n"),
            GetRemoteClassLongName(), bsock->who());
      } else if ((local_class == dcDIR && (remote_class == dcSD || remote_class == dcFD))) {
         Dmsg2(authdl, _("%s and %s passwords or names not the same.\n"),
               GetLocalClassLongName(), GetRemoteClassLongName());
         Jmsg6(jcr, M_FATAL, 0,
               _("%s unable to authenticate with %s at \"%s:%d\". Possible causes:\n"
               "Passwords or names not the same or\n"
               "Maximum Concurrent Jobs exceeded on the %s or\n"
               "%s networking messed up (restart daemon).\n"
               "For help, please see: " MANUAL_AUTH_URL "\n"),
               GetLocalClassLongName(), GetRemoteClassLongName(),
               bsock->host(), bsock->port(),
               GetRemoteClassShortName(), GetRemoteClassShortName());
      } else {
         // Silent
      }
   }
// TODO check that free_tls() is done at the right place
   if (tls_authenticate) {       /* authentication only? */
      bsock->free_tls();         /* yes, stop tls */
   }

   return auth_success;
}

bool AuthenticateBase::ServerEarlyTLS()
{
   if ((tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) ||
       (psk_local_need >= BNET_TLS_OK && psk_remote_need >= BNET_TLS_OK)) {
      /* If both can speak TLS or PSK then send the "starttls" even if the
       * local requirement is not full filled. The client need to
       * know the server requirements too. Both will terminate the connection
       * in HandleTLS if the requirement are not full filled 
       */
      if (!bsock->fsend("starttls tlspsk=%d\n", tlspsk_local_need)) {
// TODO tweak the error message
         Qmsg3(NULL, M_SECURITY, 0, _("Connection with %s:%s starttls comm error. ERR=%s\n"), bsock->who(),
               bsock->host(), bsock->bstrerror());
         sleep(5);
         return false;
      }
      if (!HandleTLS()) {
         return false;
      }
   }
   return true;
}

bool AuthenticateBase::ServerCramMD5Authenticate(const char *password)
{
   int compatible = true;

   if (!ServerEarlyTLS()) {
      return false;
   }

   /* Challenge the director */
   auth_success = cram_md5_challenge(bsock, password, tls_local_need, compatible);
   if (local_type == dtSrv && local_class == dcFD && remote_class == dcDIR) {
      if (jcr && job_canceled(jcr)) {
         auth_success = false;
         return false;                   /* quick exit */
      }
   }
   if (auth_success) {
      auth_success = cram_md5_respond(bsock, password, &tls_remote_need, &compatible);
      if (!auth_success) {
         char addr[64];
         char *who = bsock->get_peer(addr, sizeof(addr)) ? bsock->who() : addr;
         Dmsg2(authdl, "cram_get_auth respond failed for %s: %s\n",
               GetRemoteClassShortName(), who);
      }
   } else {
      char addr[64];
      char *who = bsock->get_peer(addr, sizeof(addr)) ? bsock->who() : addr;
      Dmsg2(authdl, "cram_auth challenge failed for %s %s\n",
            GetRemoteClassShortName(), who);
   }

   if (!auth_success) {
      if (local_type == dtSrv && local_class == dcDIR && remote_class == dcCON) {
         // let the authenticate_xxxx() react
      } else if (local_class == dcGUI) {
         // let the authenticate_xxxx() react
      } else if (local_type == dtSrv && local_class == dcFD && remote_class == dcDIR) {
         Emsg1(M_FATAL, 0, _("Incorrect password given by Director at %s.\n"),
             bsock->who());
      } else if ((local_class == dcFD && remote_class == dcSD) ||
                 (local_class == dcSD && remote_class == dcFD) ) {
         Jmsg(jcr, M_FATAL, 0, _("Incorrect authorization key from %s at %s rejected.\n"
              "For help, please see: " MANUAL_AUTH_URL "\n"),
               GetRemoteClassLongName(), bsock->who());
      } else {
         Jmsg1(jcr, M_FATAL, 0, _("Incorrect password given by %s.\n"
          "For help, please see: " MANUAL_AUTH_URL "\n"), GetRemoteClassLongName());
      }
   }
   if (tls_authenticate) {       /* authentication only? */
      bsock->free_tls();         /* yes, stop tls */
   }

   return auth_success;
}

void AuthenticateBase::TLSFailure()
{
   Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed with %s at \"%s:%d\"\n"),
         GetRemoteClassShortName(), bsock->host(), bsock->port());
}

bool AuthenticateBase::HandleTLS()
{
   if (tls_started) {
      return true;
   }
   if (!CheckTLSRequirement()) {
      return false;
   }

   /* Is TLS Enabled? */
   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /* Engage TLS! Full Speed Ahead! */
      ctx = tls_ctx;
      Dmsg0(10, "TLSPSK Start TLS\n");
   } else if (psk_local_need >= BNET_TLS_OK && psk_remote_need >= BNET_TLS_OK) {
      ctx = psk_ctx;
      Dmsg0(10, "TLSPSK Start PSK\n");
   } else {
      ctx = NULL;
      Dmsg0(DT_NETWORK, "TLSPSK Start CLEAR\n");
      // Qmsg0(jcr, M_INFO, 0, _("Start connection in CLEAR-TEXT\n"));
   }
   if (ctx != NULL) {
      if ((local_type==dtCli && !bnet_tls_client(ctx, bsock, verify_list, password)) ||
          (local_type==dtSrv && !bnet_tls_server(ctx, bsock, verify_list, password))) {
         TLSFailure();
         return false;
      }
      tls_started = true;
   }
   return true;
}
