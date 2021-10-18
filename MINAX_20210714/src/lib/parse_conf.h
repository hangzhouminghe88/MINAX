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
 *     Kern Sibbald, January MM
 *
 */
#ifndef PARSE_CONF_H
#define PARSE_CONF_H

/* Used for certain keyword tables */
struct s_kw {
   const char *name;
   int token;
};

/* Collector Type keyword structure */
struct s_collt {
   const char *type_name;
   int32_t coll_type;
};

struct RES_ITEM;                   /* Declare forward referenced structure */
struct RES_ITEM1;
struct RES_ITEM2;                  /* Declare forward referenced structure */
class RES;                         /* Declare forward referenced structure */
struct HPKT;                       /* Declare forward referenced structure */
typedef void (RES_HANDLER)(HPKT &hpkt);
typedef void (MSG_RES_HANDLER)(LEX *lc, RES_ITEM *item, int index, int pass);
/* The INC_RES handler has an extra argument */
typedef void (INC_RES_HANDLER)(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);

/* This is the structure that defines
 * the record types (items) permitted within each
 * resource. It is used to define the configuration
 * tables.
 */
struct RES_ITEM {
   const char *name;                  /* Resource name i.e. Director, ... */
   MSG_RES_HANDLER *handler;          /* Routine storing the resource item */
   union {
      char **value;                   /* Where to store the item */
      char **charvalue;
      uint32_t ui32value;
      int32_t i32value;
      uint64_t ui64value;
      int64_t i64value;
      bool boolvalue;
      utime_t utimevalue;
      RES *resvalue;
      RES **presvalue;
   };
   int32_t  code;                     /* item code/additional info */
   uint32_t  flags;                   /* flags: default, required, ... */
   int32_t  default_value;            /* default value */
};

/*
 * This handler takes only the RPKT as an argument
 */
struct RES_ITEM1 {
   const char *name;                  /* Resource name i.e. Director, ... */
   RES_HANDLER *handler;              /* Routine storing/displaying the resource */
   union {
      char **value;                   /* Where to store the item */
      char **charvalue;
      uint32_t ui32value;
      int32_t i32value;
      uint64_t ui64value;
      int64_t i64value;
      bool boolvalue;
      utime_t utimevalue;
      RES *resvalue;
      RES **presvalue;
   };
   int32_t  code;                     /* item code/additional info */
   uint32_t  flags;                   /* flags: default, required, ... */
   int32_t  default_value;            /* default value */
};

/* INC_RES_HANDLER has exclude argument */
struct RES_ITEM2 {
   const char *name;                  /* Resource name i.e. Director, ... */
   INC_RES_HANDLER *handler;          /* Routine storing the resource item */
   union {
      char **value;                   /* Where to store the item */
      char **charvalue;
      uint32_t ui32value;
      int32_t i32value;
      uint64_t ui64value;
      int64_t i64value;
      bool boolvalue;
      utime_t utimevalue;
      RES *resvalue;
      RES **presvalue;
   };
   int32_t  code;                     /* item code/additional info */
   uint32_t  flags;                   /* flags: default, required, ... */
   int32_t  default_value;            /* default value */
};


/* For storing name_addr items in res_items table */
#define ITEM(x) {(char **)&res_all.x}

#define MAX_RES_ITEMS 100             /* maximum resource items per RES */

class RES_HEAD {
public:
   rblist *res_list;                  /* Resource list */
   RES *first;                        /* First RES item in list */
   RES *last;                         /* Last RES item inserted */
};

/*
 * This is the universal header that is
 * at the beginning of every resource
 * record.
 */
class RES {
public:
   rblink link;                       /* red-black link */
   RES *res_next;                     /* pointer to next resource of this type */
   char *name;                        /* resource name */
   char *desc;                        /* resource description */
   uint32_t rcode;                    /* resource id or type */
   int32_t  refcnt;                   /* reference count for releasing */
   char  item_present[MAX_RES_ITEMS]; /* set if item is present in conf file */
};


/*
 * Master Resource configuration structure definition
 * This is the structure that defines the
 * resources that are available to this daemon.
 */
struct RES_TABLE {
   const char *name;                  /* resource name */
   RES_ITEM *items;                   /* list of resource keywords */
   uint32_t rcode;                    /* code if needed */
};

/* Job Level keyword structure */
struct s_jl {
   const char *level_name;                 /* level keyword */
   int32_t  level;                         /* level */
   int32_t  job_type;                      /* JobType permitting this level */
};

/* Run structure contained in Schedule Resource */
class RUNBASE {
public:
   uint32_t minute;                   /* minute to run job */
   time_t last_run;                   /* last time run */
   time_t next_run;                   /* next time to run */
   bool last_day_set;                 /* set if last_day is used */
   char hour[nbytes_for_bits(24)];    /* bit set for each hour */
   char mday[nbytes_for_bits(32)];    /* bit set for each day of month */
   char month[nbytes_for_bits(12)];   /* bit set for each month */
   char wday[nbytes_for_bits(7)];     /* bit set for each day of the week */
   char wom[nbytes_for_bits(6)];      /* week of month */
   char woy[nbytes_for_bits(54)];     /* week of year */

   void clear() {
      minute = 0;
      last_run = next_run = 0;
      last_day_set = false;
      memset(hour, 0, sizeof(hour));
      memset(mday, 0, sizeof(mday));
      memset(month, 0, sizeof(month));
      memset(wday, 0, sizeof(wday));
      memset(wom, 0, sizeof(wom));
      memset(woy, 0, sizeof(woy));
   };

   void copy(RUNBASE *src);
   void store_runbase(LEX *lc, int token);
};

/* Common Resource definitions */

#define MAX_RES_NAME_LENGTH MAX_NAME_LENGTH-1       /* maximum resource name length */

/* Permitted bits in Flags field */
#define ITEM_REQUIRED    (1<<0)       /* item required */
#define ITEM_DEFAULT     (1<<1)       /* default supplied */
#define ITEM_NO_EQUALS   (1<<2)       /* Don't scan = after name */
#define ITEM_LAST        (1<<3)       /* Last item in list */
#define ITEM_ALLOW_DUPS  (1<<4)       /* Allow duplicate directives */

/* Message Resource */
class MSGS {
public:
   RES   hdr;
   char *mail_cmd;                    /* mail command */
   char *operator_cmd;                /* Operator command */
   DEST *dest_chain;                  /* chain of destinations */
   char  send_msg[MAX_BITS_FOR_INT];  /* bit array of types */

   int32_t  custom_type_current_index; /* current index for custom index */
   rblist  *custom_type;               /* custom type */

private:
   bool m_in_use;                     /* set when using to send a message */
   bool m_closing;                    /* set when closing message resource */

public:
   /* Methods */
   char *name() const;
   void clear_in_use() { lock(); m_in_use=false; unlock(); }
   void set_in_use() { wait_not_in_use(); m_in_use=true; unlock(); }
   void set_closing() { m_closing=true; }
   bool get_closing() { return m_closing; }
   void clear_closing() { lock(); m_closing=false; unlock(); }
   bool is_closing() { lock(); bool rtn=m_closing; unlock(); return rtn; }

   void wait_not_in_use();            /* in message.c */
   void lock();                       /* in message.c */
   void unlock();                     /* in message.c */

   int get_custom_type(char *name);
   int add_custom_type(bool is_not, char *name);
   bool is_custom_type(int type) { return type > M_MAX && type <= custom_type_current_index; };
};

inline char *MSGS::name() const { return hdr.name; }

/* just for reference */
class bstatcollect;

/* Statistics Resource */
class COLLECTOR {
public:
   RES   hdr;                          /* standard resource header */
   char *file;                         /* stat file if required */
   char *prefix;                       /* metric prefix */
   const char *daemon;                 /* the Daemon type string for spooling */
   const char *spool_directory;        /* a working directory of the Daemon */
   utime_t interval;                   /* interval in seconds between metrics collection */
   uint32_t port;                      /* TCP port when using network backend */
   char *host;                         /* remote host address when using network backend */
   int32_t type;                       /* the Collector backend type */
   alist *metrics;                     /* the list for Metrics parameters in resource */
   /* private */
   JCR *jcr;                           /* JCR resource */
   bstatcollect *statcollector;        /* the reference to daemon's bstatcollect'or class */
   time_t timestamp;                   /* the last collection time */
   bool valid;                         /* when set to false the collector thread should involuntary exit */
   bool running;                       /* set when a collector thread is running */
   bool mangle_name;                   /* when set metrics name will be mangled by replacing dot '.' for "%32" */
   int spooled;                        /* the spooling status of the collector */
   POOLMEM *errmsg;                    /* error message if any */
   pthread_t thid;                     /* thread id for collector thread */
   pthread_mutex_t mutex;              /* when accessing collector resource data you should lock it first */

public:
   /* Methods */
   char *name() const;
   void lock();                        /* in bcollector.c */
   void unlock();                      /* in bcollector.c */
   void setspooled(int s);             /* in bcollector.c */
   int getspooled();                   /* in bcollector.c */
   void updatetimestamp();             /* in bcollector.c */
};

inline char *COLLECTOR::name() const { return hdr.name; }

/*
 * New C++ configuration routines
 */

class CONFIG: public SMARTALLOC {
public:
   const char *m_cf;                   /* config file */
   LEX_ERROR_HANDLER *m_scan_error;    /* error handler if non-null */
   int32_t m_err_type;                 /* the way to terminate on failure */
   void *m_res_all;                    /* pointer to res_all buffer */
   int32_t m_res_all_size;             /* length of buffer */
   bool  m_encode_pass;                /* Encode passwords with MD5 or not */

   /* The below are not yet implemented */
   int32_t m_r_first;                  /* first daemon resource type */
   int32_t m_r_last;                   /* last daemon resource type */
   RES_TABLE *m_resources;             /* pointer to table of permitted resources */
   RES_HEAD **m_res_head;              /* pointer to list of resources this type */
   brwlock_t m_res_lock;               /* resource lock */
   POOLMEM *m_errmsg;

   /* functions */
   void init(
      const char *cf,
      LEX_ERROR_HANDLER *scan_error,
      int32_t err_type,
      void *vres_all,
      int32_t res_all_size,
      int32_t r_first,
      int32_t r_last,
      RES_TABLE *resources,
      RES_HEAD ***res_head);

   CONFIG();
   ~CONFIG();
   void encode_password(bool encode);
   bool parse_config();
   void free_all_resources();
   bool insert_res(int rindex, int size);
   bool insert_res(int rindex, RES *res);
   RES_HEAD **save_resources();
   RES_HEAD **new_res_head();
   void init_res_head(RES_HEAD ***rhead, int32_t first, int32_t last);
};

/* Resource routines */
int res_compare(void *item1, void *item2);
RES *GetResWithName(int rcode, const char *name);
RES *GetNextRes(int rcode, RES *res);
RES *GetNextRes(RES_HEAD **rhead, int rcode, RES *res);
void b_LockRes(const char *file, int line);
void b_UnlockRes(const char *file, int line);
void dump_resource(int type, RES *res, void sendmsg(void *sock, const char *fmt, ...), void *sock);
void dump_each_resource(int type, void sendmsg(void *sock, const char *fmt, ...), void *sock);
void free_resource(RES *res, int type);
bool init_resource(CONFIG *config, uint32_t type, void *res, int size);
bool save_resource(CONFIG *config, int type, RES_ITEM *item, int pass);
void unstrip_password(RES_TABLE *resources); /* Used for json stuff */
void strip_password(RES_TABLE *resources);   /* Used for tray monitor */
const char *res_to_str(int rcode);
bool find_config_file(const char *config_file, char *full_path, int max_path);

/* Loop through each resource of type, returning in var */
#ifdef HAVE_TYPEOF
#define foreach_res(var, type) \
        for((var)=NULL; ((var)=(typeof(var))GetNextRes((type), (RES *)var));)
#else
#define foreach_res(var, type) \
    for(var=NULL; (*((void **)&(var))=(void *)GetNextRes((type), (RES *)var));)
#endif


/*
 * Standard global parsers defined in parse_config.c and bcollector.c
 */
void store_str(LEX *lc, RES_ITEM *item, int index, int pass);
void store_dir(LEX *lc, RES_ITEM *item, int index, int pass);
void store_clear_password(LEX *lc, RES_ITEM *item, int index, int pass);
void store_password(LEX *lc, RES_ITEM *item, int index, int pass);
void store_name(LEX *lc, RES_ITEM *item, int index, int pass);
void store_strname(LEX *lc, RES_ITEM *item, int index, int pass);
void store_res(LEX *lc, RES_ITEM *item, int index, int pass);
void store_alist_res(LEX *lc, RES_ITEM *item, int index, int pass);
void store_alist_str(LEX *lc, RES_ITEM *item, int index, int pass);
void store_int32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_pint32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_msgs(LEX *lc, RES_ITEM *item, int index, int pass);
void store_int64(LEX *lc, RES_ITEM *item, int index, int pass);
void store_bit(LEX *lc, RES_ITEM *item, int index, int pass);
void store_bool(LEX *lc, RES_ITEM *item, int index, int pass);
void store_time(LEX *lc, RES_ITEM *item, int index, int pass);
void store_size64(LEX *lc, RES_ITEM *item, int index, int pass);
void store_size32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_speed(LEX *lc, RES_ITEM *item, int index, int pass);
void store_defs(LEX *lc, RES_ITEM *item, int index, int pass);
void store_label(LEX *lc, RES_ITEM *item, int index, int pass);
void store_coll_type(LEX *lc, RES_ITEM *item, int index, int pass);

/* ***FIXME*** eliminate these globals */
extern int32_t r_first;
extern int32_t r_last;
extern RES_TABLE resources[];
extern RES_HEAD **res_head;
extern int32_t res_all_size;

#endif
