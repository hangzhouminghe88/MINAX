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
 * Bacula File Daemon specific configuration
 *
 *     Kern Sibbald, Sep MM
 */

/*
 * Resource codes -- they must be sequential for indexing
 * TODO: Check if we can change R_codes to enum like for other daemons.
 */
#define R_FIRST                       1001

#define R_DIRECTOR                    1001
#define R_CLIENT                      1002
#define R_MSGS                        1003
#define R_CONSOLE                     1004
#define R_COLLECTOR                   1005
#define R_SCHEDULE                    1006
#define R_LAST                        R_SCHEDULE

/*
 * Some resource attributes
 */
#define R_NAME                        1020
#define R_ADDRESS                     1021
#define R_PASSWORD                    1022
#define R_TYPE                        1023

/* Cipher/Digest keyword structure */
struct s_ct {
   const char *type_name;
   int32_t type_value;
};

struct DIRINFO
{
   char *password;                    /* Director password */
   char *address;                     /* Director address or zero */
   int   heartbeat_interval;
   int   comm_compression;
   int32_t DIRport;
   bool tls_authenticate;             /* Authenticate with TSL */
   bool tls_enable;                   /* Enable TLS */
   bool tls_psk_enable;               /* Enable TLS-PSK */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Client Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   TLS_CONTEXT *psk_ctx;              /* Shared TLS-PSK Context */
};

/* Definition of the contents of each Resource */
struct CONSRES {
   RES   hdr;
   DIRINFO dirinfo;
};

/* Run structure contained in Schedule Resource */
class RUNRES: public RUNBASE {
public:
   RUNRES *next;                     /* points to next run record */
   utime_t MaxConnectTime;           /* max connect time in sec from Sched time */
   bool MaxConnectTime_set;          /* MaxConnectTime given */

   void copyall(RUNRES *src);
   void clearall();
};

/*
 *   Schedule Resource
 */
class SCHEDRES {
public:
   RES   hdr;
   RUNRES *run;
   bool Enabled;                      /* set if enabled */

   /* Methods */
   char *name() const { return hdr.name; };
   bool is_enabled() { return Enabled;};
   void setEnabled(bool val) { Enabled = val;};
};

/* Definition of the contents of each Resource */
struct DIRRES {
   RES   hdr;
   DIRINFO dirinfo;
   bool monitor;                      /* Have only access to status and .status functions */
   bool remote;                       /* Remote console, can run and control jobs */
   bool connect_to_dir;               /* Connect the Director to get jobs */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (per director) */
   alist *disable_cmds;               /* Commands to disable */
   bool *disabled_cmds_array;         /* Disabled commands array */
   CONSRES *console;
   SCHEDRES *schedule;                /* Know when to connect the Director */
   int reconnection_time;             /* Reconnect after a given time */
};

struct CLIENT {
   RES   hdr;
   dlist *FDaddrs;
   dlist *FDsrc_addr;                 /* address to source connections from */
   char *working_directory;
   char *pid_directory;
   char *subsys_directory;
   char *plugin_directory;            /* Plugin directory */
   char *scripts_directory;
   char *snapshot_command;
   char *dedup_index_dir;             /* Directory for local dedup cache (deprecated) */
   MSGS *messages;                    /* daemon message handler */
   uint32_t MaxConcurrentJobs;
   utime_t SDConnectTimeout;          /* timeout in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   uint32_t max_network_buffer_size;  /* max network buf size */
   bool comm_compression;             /* Enable comm line compression */
   bool pki_sign;                     /* Enable Data Integrity Verification via Digital Signatures */
   bool pki_encrypt;                  /* Enable Data Encryption */
   bool local_dedup;                  /* Enable Client (local) deduplication */
   char *pki_keypair_file;            /* PKI Key Pair File */
   alist *pki_signing_key_files;      /* PKI Signing Key Files */
   alist *pki_master_key_files;       /* PKI Master Key Files */
   int32_t pki_cipher;               /* PKI Cipher type */
   int32_t pki_digest;               /* PKI Digest type */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_psk_enable;               /* Enable TLS-PSK */
   bool tls_require;                  /* Require TLS */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */

   X509_KEYPAIR *pki_keypair;         /* Shared PKI Public/Private Keypair */
   alist *pki_signers;                /* Shared PKI Trusted Signers */
   alist *pki_recipients;             /* Shared PKI Recipients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   TLS_CONTEXT *psk_ctx;              /* Shared TLS-PSK Context */
   char *verid;                       /* Custom Id to print in version command */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (global) */
   bool require_fips;                  /* Check for FIPS module */
   bool allow_dedup_cache;            /* allow the use of dedup cache for rehydration */
   alist *disable_cmds;               /* Commands to disable */
   bool *disabled_cmds_array;         /* Disabled commands array */
};

/* Get the size of a resource object */
int get_resource_size(int type);

/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES       res_dir;
   CLIENT       res_client;
   MSGS         res_msgs;
   CONSRES      res_cons;
   RES          hdr;
   COLLECTOR    res_collector;
   SCHEDRES     res_sched;
};
