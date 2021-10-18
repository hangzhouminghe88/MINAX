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
 * authenticatebase.h
 *
 * AuthenticateBase is the base class to handles authentication for all daemons
 *
 */

enum {
   TLS_REQ_OK,
   TLS_REQ_ERR_LOCAL,
   TLS_REQ_ERR_REMOTE
};

class AuthenticateBase
{
   static const char *dc_short_name[6];
   static const char *dc_long_name[6];

protected:
   JCR *jcr;
   BSOCK *bsock;
   int local_type;
   int local_class, remote_class;

   enum constants { max_name_length = 128 };
   char local_name[max_name_length];
   char remote_name[max_name_length];

   int tls_local_need;
   int tls_remote_need;
   bool tls_authenticate;
   bool tls_verify_peer;
   alist *tls_verify_list;
   alist *verify_list;
   TLS_CONTEXT *tls_ctx;
   TLS_CONTEXT *psk_ctx;
   TLS_CONTEXT *ctx;
   const char *password;
   int psk_local_need;
   int psk_remote_need;
   int tlspsk_local_need;
   btimer_t *tid;
   bool auth_success;
   bool check_early_tls; // already did a recv() before the cram-md5
   bool tls_started;

public:

   enum DaemonClass { dcUnknown=0, dcCON, dcFD, dcSD, dcDIR, dcGUI };
   enum DaemonType { dtUnknown=0, dtCli, dtSrv };

   AuthenticateBase(JCR *jcr, BSOCK *bsock, int loc_typ, int loc_cls, int rm_cls);
   virtual ~AuthenticateBase();
   void StartAuthTimeout(int auth_timeout=AUTH_TIMEOUT);
   void StopAuthTimeout();
   void CalcLocalTLSNeedFromRes(bool tls_enable, bool tls_require,
         bool atls_authenticate, bool atls_verify_peer,
         alist *atls_verify_list, TLS_CONTEXT *atls_ctx,
         bool tls_psk_enable, TLS_CONTEXT *apsk_ctx, const char *apassword);
   void DecodeRemoteTLSPSKNeed(int remote_need);
   int TestTLSRequirement();
   const char *GetLocalClassShortName();
   const char *GetLocalClassLongName();
   const char *GetRemoteClassShortName();
   const char *GetRemoteClassLongName();
   int GetTLSPSKLocalNeed();

   bool ClientEarlyTLS();
   bool ServerEarlyTLS();

   bool ClientCramMD5Authenticate(const char *password);
   bool ServerCramMD5Authenticate(const char *password);

   bool HandleTLS();

   virtual void TLSFailure();
   virtual bool CheckTLSRequirement();

};
