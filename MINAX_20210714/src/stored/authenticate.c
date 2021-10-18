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
 * Authenticate caller
 *
 *   Written by Kern Sibbald, October 2000
 *
 */


#include "bacula.h"
#include "stored.h"

extern STORES *me;               /* our Global resource */

const int dbglvl = 50;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

SDAuthenticateDIR::SDAuthenticateDIR(JCR *jcr):
AuthenticateBase(jcr, jcr->dir_bsock, dtSrv, dcSD, dcDIR)
{
}

/*
 * Authenticate the Director
 */
bool SDAuthenticateDIR::authenticate_director()
{
   BSOCK *dir = jcr->dir_bsock;
   DIRRES *director = jcr->director;

   DecodeRemoteTLSPSKNeed(bsock->tlspsk_remote);

   /* TLS Requirement */
   CalcLocalTLSNeedFromRes(director->tls_enable, director->tls_require,
         director->tls_authenticate, director->tls_verify_peer,
         director->tls_allowed_cns, director->tls_ctx,
         director->tls_psk_enable, director->psk_ctx, director->password);

   /* Timeout authentication after 10 mins */
   StartAuthTimeout();

   /* Challenge the director */
   if (!ServerCramMD5Authenticate(password)) {
      goto auth_fatal;
   }

   this->auth_success = HandleTLS();

auth_fatal:
   if (auth_success) {
      return send_hello_ok(dir);
   }
   send_sorry(dir);
   bmicrosleep(5, 0);
   return false;
}

class SDAuthenticateFD: public AuthenticateBase
{
public:
   SDAuthenticateFD(JCR *jcr, BSOCK *bsock):
   AuthenticateBase(jcr, bsock, dtSrv, dcSD, dcFD)
   {
   }
   virtual ~SDAuthenticateFD() {};
   int authenticate_filed(int FDVersion);
};

int authenticate_filed(JCR *jcr, BSOCK *fd, int FDVersion)
{
   return SDAuthenticateFD(jcr, fd).authenticate_filed(FDVersion);
}

int SDAuthenticateFD::authenticate_filed(int FDVersion)
{
   BSOCK *fd = bsock;

   DecodeRemoteTLSPSKNeed(bsock->tlspsk_remote);
   /* TLS Requirement */
   CalcLocalTLSNeedFromRes(me->tls_enable, me->tls_require, me->tls_authenticate,
         me->tls_verify_peer, me->tls_allowed_cns, me->tls_ctx,
         me->tls_psk_enable, me->psk_ctx, jcr->sd_auth_key);

   /* Timeout authentication after 10 mins */
   StartAuthTimeout();

   /* Challenge the FD */
   if (!ServerCramMD5Authenticate(jcr->sd_auth_key)) {
      goto auth_fatal;
   }

   this->auth_success = HandleTLS();

auth_fatal:
   /* Version 5 of the protocol is a bit special, it is used by both 6.0.0
    * Enterprise version and 7.0.x Community version, but do not support the
    * same level of features. As nobody is using the 6.0.0 release, we can
    * be pretty sure that the FD in version 5 is a community FD.
    */
   if (auth_success && (FDVersion >= 9 || FDVersion == 5)) {
      send_hello_ok(fd);
   }
   return auth_success;
}

class SDAuthenticateSD: public AuthenticateBase
{
public:
   SDAuthenticateSD(JCR *jcr):
   AuthenticateBase(jcr, jcr->store_bsock, dtCli, dcSD, dcSD) {
      /* TLS Requirement */
      CalcLocalTLSNeedFromRes(me->tls_enable, me->tls_require, me->tls_authenticate,
            me->tls_verify_peer, me->tls_allowed_cns, me->tls_ctx,
            me->tls_psk_enable, me->psk_ctx, jcr->sd_auth_key);
   }
   virtual ~SDAuthenticateSD() {};
   bool authenticate_storagedaemon();
};


bool send_hello_and_authenticate_sd(JCR *jcr, char *Job)
{
   SDAuthenticateSD auth(jcr);
   /* TODO disable PSK/TLS when the SD talk to itself */
   if (!send_hello_sd(jcr, Job, auth.GetTLSPSKLocalNeed())) {
      return false;
   }
   return auth.authenticate_storagedaemon();
}

/*
 * First prove our identity to the Storage daemon, then
 * make him prove his identity.
 */
bool SDAuthenticateSD::authenticate_storagedaemon()
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
