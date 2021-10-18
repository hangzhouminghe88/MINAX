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
 * Bacula User Agent specific configuration and defines
 *
 *     Kern Sibbald, Sep MM
 *
 */

/*
 * Resource codes -- they must be sequential for indexing
 */

bool parse_cons_config(CONFIG *config, const char *configfile, int exit_code);

enum {
   R_CONSOLE   = 1001,
   R_DIRECTOR,
   R_FIRST     = R_CONSOLE,
   R_LAST      = R_DIRECTOR           /* Keep this updated */
};

/*
 * Some resource attributes
 */
enum {
   R_NAME     = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};


/* Definition of the contents of each Resource */

/* Console "globals" */
struct CONRES {
   RES   hdr;
   char *rc_file;                     /* startup file */
   char *password;                    /* UA server password */
   bool comm_compression;             /* Enable comm line compression */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS on all connections */
   bool tls_psk_enable;               /* Enable TLS-PSK on all connections */
   bool tls_require;                  /* Require TLS on all connections */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   char *director;                    /* bind to director */
   utime_t heartbeat_interval;        /* Interval to send heartbeats to Dir */
   bool require_fips;                  /* Check for FIPS module */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   TLS_CONTEXT *psk_ctx;              /* Shared TLS-PSK Context */
};

/* Director */
struct DIRRES {
   RES   hdr;
   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   char *password;                    /* UA server password */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_psk_enable;               /* Enable TLS-PSK */
   bool tls_require;                  /* Require TLS */
   bool require_fips;                  /* Check for FIPS module */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   utime_t heartbeat_interval;        /* Interval to send heartbeats to Dir */
   char *hist_file;                   /* command history file */
   int32_t hist_file_size;            /* command history file size */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   TLS_CONTEXT *psk_ctx;              /* Shared TLS-PSK Context */
};


/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CONRES res_cons;
   RES hdr;
};

/* Get the size of a give resource */
int get_resource_size(int type);
