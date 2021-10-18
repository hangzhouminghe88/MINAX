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
 *   Master Configuration routines.
 *
 *   This file contains the common parts of the Bacula
 *   configuration routines.
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_conf.c and
 *      lib/parse_conf.h.
 *      These files contain the parser code, some utility
 *      routines, and the common store routines (name, int,
 *      string, time, int64, size, ...).
 *
 *   3. The daemon specific file, which contains the Resource
 *      definitions as well as any specific store routines
 *      for the resource records.
 *
 *    N.B. This is a two pass parser, so if you malloc() a string
 *         in a "store" routine, you must ensure to do it during
 *         only one of the two passes, or to free it between.
 *         Also, note that the resource record is malloced and
 *         saved in save_resource() during pass 1.  Anything that
 *         you want saved after pass two (e.g. resource pointers)
 *         must explicitly be done in save_resource. Take a look
 *         at the Job resource in src/dird/dird_conf.c to see how
 *         it is done.
 *
 *     Kern Sibbald, January MM
 *
 */


#include "bacula.h"

#if defined(HAVE_WIN32)
#include "shlobj.h"
#else
#define MAX_PATH  1024
#endif

/*
 * Define the Union of all the common resource structure definitions.
 */
union URES {
   MSGS  res_msgs;
   RES hdr;
   COLLECTOR  res_collector;
};

#if defined(_MSC_VER)
// work around visual studio name mangling preventing external linkage since res_all
// is declared as a different type when instantiated.
extern "C" URES res_all;
#else
extern  URES res_all;
#endif

extern brwlock_t res_lock;            /* resource lock */


/* Forward referenced subroutines */
static void scan_types(LEX *lc, MSGS *msg, int dest, char *where, char *cmd);
static const char *get_default_configdir();

/* Common Resource definitions */

/*
 * Message resource directives
 * Note: keep all store_mesgs last in the list as they are all
 *   output in json as a list.
 *   Also, the list store_msgs item must have flags set to ITEM_LAST
 *   so that the list editor (bjson.c) knows when to stop.
 *
 *  name         handler      value       code   flags  default_value
 */
RES_ITEM msgs_items[] = {
   {"Name",        store_name,    ITEM(res_msgs.hdr.name),  0, ITEM_REQUIRED, 0},
   {"Description", store_str,     ITEM(res_msgs.hdr.desc),  0, 0, 0},
   {"MailCommand", store_str,     ITEM(res_msgs.mail_cmd),  0, ITEM_ALLOW_DUPS, 0},
   {"OperatorCommand", store_str, ITEM(res_msgs.operator_cmd), 0, ITEM_ALLOW_DUPS, 0},
   /* See comments above */
   {"Syslog",      store_msgs, ITEM(res_msgs), MD_SYSLOG,   0, 0},
   {"Mail",        store_msgs, ITEM(res_msgs), MD_MAIL,     0, 0},
   {"MailOnError", store_msgs, ITEM(res_msgs), MD_MAIL_ON_ERROR, 0, 0},
   {"MailOnSuccess", store_msgs, ITEM(res_msgs), MD_MAIL_ON_SUCCESS, 0, 0},
   {"File",        store_msgs, ITEM(res_msgs), MD_FILE,     0, 0},
   {"Append",      store_msgs, ITEM(res_msgs), MD_APPEND,   0, 0},
   {"Stdout",      store_msgs, ITEM(res_msgs), MD_STDOUT,   0, 0},
   {"Stderr",      store_msgs, ITEM(res_msgs), MD_STDERR,   0, 0},
   {"Director",    store_msgs, ITEM(res_msgs), MD_DIRECTOR, 0, 0},
   {"Console",     store_msgs, ITEM(res_msgs), MD_CONSOLE,  0, 0},
   {"Operator",    store_msgs, ITEM(res_msgs), MD_OPERATOR, 0, 0},
   {"Catalog",     store_msgs, ITEM(res_msgs), MD_CATALOG,  ITEM_LAST, 0},
   {NULL,          NULL,       {0},       0, 0, 0}
};

/*
 * Statistics resource directives
 *
 *  name         handler      value       code   flags  default_value
 */
RES_ITEM collector_items[] = {
   {"Name",             store_name,       ITEM(res_collector.hdr.name),          0, ITEM_REQUIRED, 0},
   {"Description",      store_str,        ITEM(res_collector.hdr.desc),          0, 0, 0},
   {"Prefix",           store_str,        ITEM(res_collector.prefix),            0, 0, 0},
   {"Metrics",          store_alist_str,  ITEM(res_collector.metrics),           0, 0, 0},   /* default all */
   {"Interval",         store_time,       ITEM(res_collector.interval),          0, ITEM_DEFAULT, 5*60}, /* default 5 min */
   {"Port",             store_pint32,     ITEM(res_collector.port),              0, 0, 0},
   {"Host",             store_str,        ITEM(res_collector.host),              0, 0, 0},
   {"Type",             store_coll_type,  ITEM(res_collector.type),              0, ITEM_REQUIRED, 0},
   {"File",             store_str,        ITEM(res_collector.file),              0, 0, 0},
   {"MangleMetric",     store_bool,       ITEM(res_collector.mangle_name),       0, 0, 0},
   {NULL,               NULL,             {0},                                   0, 0, 0}
};

/* Various message types */
s_kw msg_types[] = {
   {"Debug",         M_DEBUG},  /* Keep 1st place */
   {"Saved",         M_SAVED},  /* Keep 2nd place */
   {"Events",        M_EVENTS}, /* Keep 3rd place */
   {"Abort",         M_ABORT},
   {"Fatal",         M_FATAL},
   {"Error",         M_ERROR},
   {"Warning",       M_WARNING},
   {"Info",          M_INFO},
   {"NotSaved",      M_NOTSAVED},
   {"Skipped",       M_SKIPPED},
   {"Mount",         M_MOUNT},
   {"Terminate",     M_TERM},
   {"Restored",      M_RESTORED},
   {"Security",      M_SECURITY},
   {"Alert",         M_ALERT},
   {"VolMgmt",       M_VOLMGMT},
   {"ErrorTerm",     M_ERROR_TERM},
   {"All",           M_MAX+1},
   {NULL,            0}
};


/*
 * Tape Label types permitted in Pool records
 *
 *   tape label      label code = token
 */
s_kw tapelabels[] = {
   {"Bacula",        B_BACULA_LABEL},
   {"ANSI",          B_ANSI_LABEL},
   {"IBM",           B_IBM_LABEL},
   {NULL,            0}
};

/*
 * Keywords (RHS) permitted in Statistics type records
 *
 *   type_name       backend_type
 */
s_collt collectortypes[] = {
   {"CSV",           COLLECTOR_BACKEND_CSV},
   {"Graphite",      COLLECTOR_BACKEND_Graphite},
   {NULL,            0}
};


/* Simply print a message */
static void prtmsg(void *sock, const char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vfprintf(stdout, fmt, arg_ptr);
   va_end(arg_ptr);
}

const char *res_to_str(int rcode)
{
   if (rcode < r_first || rcode > r_last) {
      return _("***UNKNOWN***");
   } else {
      return resources[rcode-r_first].name;
   }
}

/*
 * Create a new res_head pointer to a list of res_heads
 */
void CONFIG::init_res_head(RES_HEAD ***rhead, int32_t rfirst, int32_t rlast)
{
   int num = rlast - rfirst + 1;
   RES *res = NULL;
   RES_HEAD **rh;
   rh = *rhead = (RES_HEAD **)malloc(num * sizeof(RES_HEAD));
   for (int i=0; i<num; i++) {
      rh[i] = (RES_HEAD *)malloc(sizeof(RES_HEAD));
      rh[i]->res_list = New(rblist(res, &res->link));
      rh[i]->first = NULL;
      rh[i]->last = NULL;
   }
}

/*
 * Copy the resource in res_all into a new allocated resource
 * an insert it into the resource list.
 */
bool CONFIG::insert_res(int rindex, int size)
{
   RES *res = (RES *)malloc(size);
   memcpy(res, m_res_all, size);
   return insert_res(rindex, res);
}

/*
 * Insert the resource res into the resource list.
 */
bool CONFIG::insert_res(int rindex, RES *res)
{
   rblist *list = m_res_head[rindex]->res_list;
   if (list->empty()) {
      list->insert(res, res_compare);
      m_res_head[rindex]->first = res;
      m_res_head[rindex]->last = res;
   } else {
      RES *item, *prev;
      prev = m_res_head[rindex]->last;
      item = (RES *)list->insert(res, res_compare);
      if (item != res) {
         Mmsg(m_errmsg, _("Attempt to define second \"%s\" resource named \"%s\" is not permitted.\n"),
              resources[rindex].name, ((URES *)res)->hdr.name);
         return false;
      }
      prev->res_next = res;
      m_res_head[rindex]->last = res;
   }
   Dmsg2(900, _("Inserted res: %s index=%d\n"), ((URES *)res)->hdr.name, rindex);
   return true;
}

/*
 * Initialize the static structure to zeros, then
 *  apply all the default values.
 */
void init_resource0(CONFIG *config, int type, RES_ITEM *items, int pass)
{
   int i;
   int rindex = type - r_first;
   
   memset(config->m_res_all, 0, config->m_res_all_size);
   res_all.hdr.rcode = type;
   res_all.hdr.refcnt = 1;

   /* Set defaults in each item */
   for (i=0; items[i].name; i++) {
      Dmsg3(900, "Item=%s def=%s defval=%d\n", items[i].name,
            (items[i].flags & ITEM_DEFAULT) ? "yes" : "no",
            items[i].default_value);
      if (items[i].flags & ITEM_DEFAULT && items[i].default_value != 0) {
         if (items[i].handler == store_bit) {
            *(uint32_t *)(items[i].value) |= items[i].code;
         } else if (items[i].handler == store_bool) {
            *(bool *)(items[i].value) = items[i].default_value != 0;
         } else if (items[i].handler == store_pint32 ||
                    items[i].handler == store_int32 ||
                    items[i].handler == store_size32) {
            *(uint32_t *)(items[i].value) = items[i].default_value;
         } else if (items[i].handler == store_int64) {
            *(int64_t *)(items[i].value) = items[i].default_value;
         } else if (items[i].handler == store_size64) {
            *(uint64_t *)(items[i].value) = (uint64_t)items[i].default_value;
         } else if (items[i].handler == store_speed) {
            *(uint64_t *)(items[i].value) = (uint64_t)items[i].default_value;
         } else if (items[i].handler == store_time) {
            *(utime_t *)(items[i].value) = (utime_t)items[i].default_value;
         } else if (pass == 1 && items[i].handler == store_addresses) {
            init_default_addresses((dlist**)items[i].value, items[i].default_value);
         }
      }
      /* If this triggers, take a look at lib/parse_conf.h */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR_TERM, 0, _("Too many directives in \"%s\" resource\n"), resources[rindex].name);
      }
   }
}

/* Initialize a resouce with default values */
bool init_resource(CONFIG *config, uint32_t type, void *res, int size)
{
   RES_ITEM *items;
   for (int i=0; resources[i].name; i++) {
      if (resources[i].rcode == type) {
         items = resources[i].items;
         if (!items) {
            return false;
         }
         init_resource0(config, type, items, 1);
         memcpy(res, config->m_res_all, size);
         return true;
      }
   }
   return false;
}

/*
 * Dump each resource of type
 */
void dump_each_resource(int type, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   RES *res = NULL;

   if (type < 0) {                    /* no recursion */
      type = -type;
   }
   foreach_res(res, type) {
      dump_resource(-type, res, sendit, sock);
   }
}


/* Store Messages Destination information */
void store_msgs(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   char *cmd;
   POOLMEM *dest;
   int dest_len;

   Dmsg2(900, "store_msgs pass=%d code=%d\n", pass, item->code);
   if (pass == 1) {
      switch (item->code) {
      case MD_STDOUT:
      case MD_STDERR:
      case MD_SYSLOG:              /* syslog */
      case MD_CONSOLE:
      case MD_CATALOG:
         scan_types(lc, (MSGS *)(item->value), item->code, NULL, NULL);
         break;
      case MD_OPERATOR:            /* send to operator */
      case MD_DIRECTOR:            /* send to Director */
      case MD_MAIL:                /* mail */
      case MD_MAIL_ON_ERROR:       /* mail if Job errors */
      case MD_MAIL_ON_SUCCESS:     /* mail if Job succeeds */
         if (item->code == MD_OPERATOR) {
            cmd = res_all.res_msgs.operator_cmd;
         } else {
            cmd = res_all.res_msgs.mail_cmd;
         }
         dest = get_pool_memory(PM_MESSAGE);
         dest[0] = 0;
         dest_len = 0;
         /* Pick up comma separated list of destinations */
         for ( ;; ) {
            token = lex_get_token(lc, T_NAME);   /* scan destination */
            dest = check_pool_memory_size(dest, dest_len + lc->str_len + 2);
            if (dest[0] != 0) {
               pm_strcat(dest, " ");  /* separate multiple destinations with space */
               dest_len++;
            }
            pm_strcat(dest, lc->str);
            dest_len += lc->str_len;
            Dmsg2(900, "store_msgs newdest=%s: dest=%s:\n", lc->str, NPRT(dest));
            token = lex_get_token(lc, T_SKIP_EOL);
            if (token == T_COMMA) {
               continue;           /* get another destination */
            }
            if (token != T_EQUALS) {
               scan_err1(lc, _("expected an =, got: %s"), lc->str);
               return;
            }
            break;
         }
         Dmsg1(900, "mail_cmd=%s\n", NPRT(cmd));
         scan_types(lc, (MSGS *)(item->value), item->code, dest, cmd);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;

      case MD_FILE:                /* file */
      case MD_APPEND:              /* append */
         dest = get_pool_memory(PM_MESSAGE);
         /* Pick up a single destination */
         token = lex_get_token(lc, T_NAME);   /* scan destination */
         pm_strcpy(dest, lc->str);
         dest_len = lc->str_len;
         token = lex_get_token(lc, T_SKIP_EOL);
         Dmsg1(900, "store_msgs dest=%s:\n", NPRT(dest));
         if (token != T_EQUALS) {
            scan_err1(lc, _("expected an =, got: %s"), lc->str);
            return;
         }
         scan_types(lc, (MSGS *)(item->value), item->code, dest, NULL);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;

      default:
         scan_err1(lc, _("Unknown item code: %d\n"), item->code);
         return;
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   Dmsg0(900, "Done store_msgs\n");
}

/*
 * Scan for message types and add them to the message
 * destination. The basic job here is to connect message types
 *  (WARNING, ERROR, FATAL, INFO, ...) with an appropriate
 *  destination (MAIL, FILE, OPERATOR, ...)
 */
static void scan_types(LEX *lc, MSGS *msg, int dest_code, char *where, char *cmd)
{
   int i;
   bool found, is_not;
   int msg_type = 0;
   char *str;

   for ( ;; ) {
      lex_get_token(lc, T_NAME);            /* expect at least one type */
      found = false;
      if (lc->str[0] == '!') {
         is_not = true;
         str = &lc->str[1];
      } else {
         is_not = false;
         str = &lc->str[0];
      }
      for (i=0; msg_types[i].name; i++) {
         if (strcasecmp(str, msg_types[i].name) == 0) {
            msg_type = msg_types[i].token;
            found = true;
            break;
         }
      }
      /* Custom event, we add it to the list for this Message */
      if (!found && strncasecmp(str, "events.", 6) == 0) {
         msg_type = msg->add_custom_type(is_not, str+7); /* We can ignore completely if we want */
         Dmsg2(50, "Add events %s => %d\n", str, msg_type);
         if (msg_type < 0) {
            scan_err2(lc, _("message type: Unable to add %s message type. %s"), str,
                      (msg_type == -1) ? "Too much custom type" : "Invalid format");
            return;
         }
         found = true;
      }
      if (!found) {
         scan_err1(lc, _("message type: %s not found"), str);
         return;
      }

      if (msg_type == M_MAX+1) {         /* all? */
         for (i=3; i<=M_MAX; i++) {      /* yes set all types except Debug, Saved and Events */
            add_msg_dest(msg, dest_code, msg_types[i].token, where, cmd);
         }
      } else if (is_not) {
         rem_msg_dest(msg, dest_code, msg_type, where);
      } else {
         add_msg_dest(msg, dest_code, msg_type, where, cmd);
      }
      if (lc->ch != ',') {
         break;
      }
      Dmsg0(900, "call lex_get_token() to eat comma\n");
      lex_get_token(lc, T_ALL);          /* eat comma */
   }
   Dmsg0(900, "Done scan_types()\n");
}


/*
 * This routine is ONLY for resource names
 *  Store a name at specified address.
 */
void store_name(LEX *lc, RES_ITEM *item, int index, int pass)
{
   POOLMEM *msg = get_pool_memory(PM_EMSG);

   lex_get_token(lc, T_NAME);
   if (!is_name_valid(lc->str, &msg)) {
      scan_err1(lc, "%s\n", msg);
      return;
   }
   free_pool_memory(msg);
   /* Store the name both pass 1 and pass 2 */
   if (*(item->value)) {
      scan_err5(lc, _("Attempt to redefine \"%s\" from \"%s\" to \"%s\" referenced on line %d : %s\n"),
         item->name, *(item->value), lc->str, lc->line_no, lc->line);
      return;
   }
   *(item->value) = bstrdup(lc->str);
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/*
 * Store a name string at specified address
 * A name string is limited to MAX_RES_NAME_LENGTH
 */
void store_strname(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   /* Store the name */
   if (pass == 1) {
      if (*(item->value)) {
         scan_err5(lc, _("Attempt to redefine \"%s\" from \"%s\" to \"%s\" referenced on line %d : %s\n"),
            item->name, *(item->value), lc->str, lc->line_no, lc->line);
         return;
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* Store a string at specified address */
void store_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      if (*(item->value) && (item->flags & ITEM_ALLOW_DUPS)) {
         free(*(item->value));
         *(item->value) = NULL;
      }
      if (*(item->value)) {
         scan_err5(lc, _("Attempt to redefine \"%s\" from \"%s\" to \"%s\" referenced on line %d : %s\n"),
            item->name, *(item->value), lc->str, lc->line_no, lc->line);
         return;
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a directory name at specified address. Note, we do
 *   shell expansion except if the string begins with a vertical
 *   bar (i.e. it will likely be passed to the shell later).
 */
void store_dir(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      if (lc->str[0] != '|') {
         do_shell_expansion(lc->str, sizeof_pool_memory(lc->str));
      }
#ifdef STANDARDIZED_DIRECTORY_USAGE
      // TODO ASX we should store all directory without the ending slash to
      // avoid the need of testing its presence
      int len=strlen(lc->str);
      if (len>0 && IsPathSeparator(lc->str[len-1])) {
         lc->str[len-1]='\0';
      }
#endif
      if (*(item->value)) {
         scan_err5(lc, _("Attempt to redefine \"%s\" from \"%s\" to \"%s\" referenced on line %d : %s\n"),
            item->name, *(item->value), lc->str, lc->line_no, lc->line);
         return;
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store a password specified address in MD5 coding */
void store_password(LEX *lc, RES_ITEM *item, int index, int pass)
{
   unsigned int i, j;
   struct MD5Context md5c;
   unsigned char digest[CRYPTO_DIGEST_MD5_SIZE];
   char sig[100];

   if (lc->options & LOPT_NO_MD5) {
      store_str(lc, item, index, pass);

   } else {
      lex_get_token(lc, T_STRING);
      if (pass == 1) {
         MD5Init(&md5c);
         MD5Update(&md5c, (unsigned char *) (lc->str), lc->str_len);
         MD5Final(digest, &md5c);
         for (i = j = 0; i < sizeof(digest); i++) {
            sprintf(&sig[j], "%02x", digest[i]);
            j += 2;
         }
         if (*(item->value)) {
            scan_err5(lc, _("Attempt to redefine \"%s\" from \"%s\" to \"%s\" referenced on line %d : %s\n"),
               item->name, *(item->value), lc->str, lc->line_no, lc->line);
            return;
         }
         *(item->value) = bstrdup(sig);
      }
      scan_to_eol(lc);
      set_bit(index, res_all.hdr.item_present);
   }
}


/* Store a resource at specified address.
 * If we are in pass 2, do a lookup of the
 * resource.
 */
void store_res(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
      res = GetResWithName(item->code, lc->str);
      if (res == NULL) {
         scan_err3(lc, _("Could not find config Resource \"%s\" referenced on line %d : %s\n"),
            lc->str, lc->line_no, lc->line);
         return;
      }
      if (*(item->value)) {
         scan_err3(lc, _("Attempt to redefine resource \"%s\" referenced on line %d : %s\n"),
            item->name, lc->line_no, lc->line);
         return;
      }
      *(item->value) = (char *)res;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a resource pointer in an alist. default_value indicates how many
 *   times this routine can be called -- i.e. how many alists
 *   there are.
 * If we are in pass 2, do a lookup of the
 *   resource.
 */
void store_alist_res(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;
   int count = item->default_value;
   int i = 0;
   alist *list;

   if (pass == 2) {
      if (count == 0) {               /* always store in item->value */
         i = 0;
         if ((item->value)[i] == NULL) {
            list = New(alist(10, not_owned_by_alist));
         } else {
            list = (alist *)(item->value)[i];
         }
      } else {
         /* Find empty place to store this directive */
         while ((item->value)[i] != NULL && i++ < count) { }
         if (i >= count) {
            scan_err4(lc, _("Too many %s directives. Max. is %d. line %d: %s\n"),
               lc->str, count, lc->line_no, lc->line);
            return;
         }
         list = New(alist(10, not_owned_by_alist));
      }

      for (;;) {
         lex_get_token(lc, T_NAME);   /* scan next item */
         res = GetResWithName(item->code, lc->str);
         if (res == NULL) {
            scan_err3(lc, _("Could not find config Resource \"%s\" referenced on line %d : %s\n"),
               lc->str, lc->line_no, lc->line);
            return;
         }
         Dmsg5(900, "Append %p to alist %p size=%d i=%d %s\n",
               res, list, list->size(), i, item->name);
         list->append(res);
         (item->value)[i] = (char *)list;
         if (lc->ch != ',') {         /* if no other item follows */
            if (!lex_check_eol(lc)) {
               /* found garbage at the end of the line */
               scan_err3(lc, _("Found unexpected characters resource list in Directive \"%s\" at the end of line %d : %s\n"),
                  item->name, lc->line_no, lc->line);
            }
            break;                    /* get out */
         }
         lex_get_token(lc, T_ALL);    /* eat comma */
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/*
 * Store a string in an alist.
 */
void store_alist_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
   alist *list;

   if (pass == 2) {
      if (*(item->value) == NULL) {
         list = New(alist(10, owned_by_alist));
         *(item->value) = (char *)list;
      } else {
         list = (alist *)(*(item->value));
      }
      for (;;) {
         lex_get_token(lc, T_STRING);   /* scan next item */
         Dmsg4(900, "Append %s to alist 0x%p size=%d %s\n",
            lc->str, list, list->size(), item->name);
         list->append(bstrdup(lc->str));
         if (lc->ch != ',') {         /* if no other item follows */
            if (!lex_check_eol(lc)) {
               /* found garbage at the end of the line */
               scan_err3(lc, _("Found unexpected characters in resource list in Directive \"%s\" at the end of line %d : %s\n"),
                  item->name, lc->line_no, lc->line);
            }
            break;                    /* get out */
         }
         lex_get_token(lc, T_ALL);    /* eat comma */
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}



/*
 * Store default values for Resource from xxxDefs
 * If we are in pass 2, do a lookup of the
 * resource and store everything not explicitly set
 * in main resource.
 *
 * Note, here item points to the main resource (e.g. Job, not
 *  the jobdefs, which we look up).
 */
void store_defs(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
     Dmsg2(900, "Code=%d name=%s\n", item->code, lc->str);
     res = GetResWithName(item->code, lc->str);
     if (res == NULL) {
        scan_err3(lc, _("Missing config Resource \"%s\" referenced on line %d : %s\n"),
           lc->str, lc->line_no, lc->line);
        return;
     }
   }
   scan_to_eol(lc);
}



/* Store an integer at specified address */
void store_int32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_INT32);
   *(uint32_t *)(item->value) = lc->int32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* Store a positive integer at specified address */
void store_pint32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_PINT32);
   *(uint32_t *)(item->value) = lc->pint32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store an 64 bit integer at specified address */
void store_int64(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_INT64);
   *(int64_t *)(item->value) = lc->int64_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

enum store_unit_type {
   STORE_SIZE,
   STORE_SPEED
} ;

/*
 * This routine stores either a 32 or a 64 bit value (size32)
 *  and either a size (in bytes) or a speed (bytes per second).
 */
static void store_int_unit(LEX *lc, RES_ITEM *item, int index, int pass,
                           bool size32, enum store_unit_type type)
{
   int token;
   uint64_t uvalue;
   char bsize[500];

   Dmsg0(900, "Enter store_unit\n");
   token = lex_get_token(lc, T_SKIP_EOL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      bstrncpy(bsize, lc->str, sizeof(bsize));  /* save first part */
      /* if terminated by space, scan and get modifier */
      while (lc->ch == ' ') {
         token = lex_get_token(lc, T_ALL);
         switch (token) {
         case T_NUMBER:
         case T_IDENTIFIER:
         case T_UNQUOTED_STRING:
            bstrncat(bsize, lc->str, sizeof(bsize));
            break;
         }
      }
      if (type == STORE_SIZE) {
         if (!size_to_uint64(bsize, strlen(bsize), &uvalue)) {
            scan_err1(lc, _("expected a size number, got: %s"), lc->str);
            return;
         }
      } else {
         if (!speed_to_uint64(bsize, strlen(bsize), &uvalue)) {
            scan_err1(lc, _("expected a speed number, got: %s"), lc->str);
            return;
         }
      }
      if (size32) {
         *(uint32_t *)(item->value) = (uint32_t)uvalue;
      } else {
         *(uint64_t *)(item->value) = uvalue;
      }
      break;
   default:
      scan_err2(lc, _("expected a %s, got: %s"),
                (type == STORE_SIZE)?_("size"):_("speed"), lc->str);
      return;
   }
   if (token != T_EOL) {
      scan_to_eol(lc);
   }
   set_bit(index, res_all.hdr.item_present);
   Dmsg0(900, "Leave store_unit\n");
}

/* Store a size in bytes */
void store_size32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, true /* 32 bit */, STORE_SIZE);
}

/* Store a size in bytes */
void store_size64(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, false /* not 32 bit */, STORE_SIZE);
}

/* Store a speed in bytes/s */
void store_speed(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, false /* 64 bit */, STORE_SPEED);
}

/* Store a time period in seconds */
void store_time(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   utime_t utime;
   char period[500];

   token = lex_get_token(lc, T_SKIP_EOL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      bstrncpy(period, lc->str, sizeof(period));  /* get first part */
      /* if terminated by space, scan and get modifier */
      while (lc->ch == ' ') {
         token = lex_get_token(lc, T_ALL);
         switch (token) {
         case T_NUMBER:
         case T_IDENTIFIER:
         case T_UNQUOTED_STRING:
            bstrncat(period, lc->str, sizeof(period));
            break;
         }
      }
      if (!duration_to_utime(period, &utime)) {
         scan_err1(lc, _("expected a time period, got: %s"), period);
         return;
      }
      *(utime_t *)(item->value) = utime;
      break;
   default:
      scan_err1(lc, _("expected a time period, got: %s"), lc->str);
      return;
   }
   if (token != T_EOL) {
      scan_to_eol(lc);
   }
   set_bit(index, res_all.hdr.item_present);
}


/* Store a yes/no in a bit field */
void store_bit(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
      *(uint32_t *)(item->value) |= item->code;
   } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
      *(uint32_t *)(item->value) &= ~(item->code);
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* Store a bool in a bit field */
void store_bool(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
      *(bool *)(item->value) = true;
   } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
      *(bool *)(item->value) = false;
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/*
 * Store Tape Label Type (Bacula, ANSI, IBM)
 *
 */
void store_label(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /* Store the label pass 2 so that type is defined */
   for (i=0; tapelabels[i].name; i++) {
      if (strcasecmp(lc->str, tapelabels[i].name) == 0) {
         *(uint32_t *)(item->value) = tapelabels[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Tape Label keyword, got: %s"), lc->str);
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store Statistics Type (CSV, Graphite - only supported)
 */
void store_coll_type(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /* Store the type both pass 1 and pass 2 */
   for (i = 0; collectortypes[i].type_name; i++) {
      if (strcasecmp(lc->str, collectortypes[i].type_name) == 0) {
         *(int32_t *)(item->value) = collectortypes[i].coll_type;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Statistics backend type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/*
 *
 *  Configuration parser for Director Run Configuration
 *   directives, which are part of the Schedule Resource
 *
 *     Kern Sibbald, May MM
 *
 */

enum e_state {
   s_none = 0,
   s_range,
   s_mday,
   s_month,
   s_time,
   s_at,
   s_wday,
   s_daily,
   s_weekly,
   s_monthly,
   s_hourly,
   s_wom,                           /* 1st, 2nd, ...*/
   s_woy,                           /* week of year w00 - w53 */
   s_ldom                           /* last day of month */
};

struct s_keyw {
  const char *name;                   /* keyword */
  enum e_state state;                 /* parser state */
  int code;                           /* state value */
};

/* Keywords understood by parser */
static struct s_keyw keyw[] = {
  {NT_("on"),         s_none,    0},
  {NT_("at"),         s_at,      0},
  {NT_("lastday"),    s_ldom,    0},

  {NT_("sun"),        s_wday,    0},
  {NT_("mon"),        s_wday,    1},
  {NT_("tue"),        s_wday,    2},
  {NT_("wed"),        s_wday,    3},
  {NT_("thu"),        s_wday,    4},
  {NT_("fri"),        s_wday,    5},
  {NT_("sat"),        s_wday,    6},
  {NT_("jan"),        s_month,   0},
  {NT_("feb"),        s_month,   1},
  {NT_("mar"),        s_month,   2},
  {NT_("apr"),        s_month,   3},
  {NT_("may"),        s_month,   4},
  {NT_("jun"),        s_month,   5},
  {NT_("jul"),        s_month,   6},
  {NT_("aug"),        s_month,   7},
  {NT_("sep"),        s_month,   8},
  {NT_("oct"),        s_month,   9},
  {NT_("nov"),        s_month,  10},
  {NT_("dec"),        s_month,  11},

  {NT_("sunday"),     s_wday,    0},
  {NT_("monday"),     s_wday,    1},
  {NT_("tuesday"),    s_wday,    2},
  {NT_("wednesday"),  s_wday,    3},
  {NT_("thursday"),   s_wday,    4},
  {NT_("friday"),     s_wday,    5},
  {NT_("saturday"),   s_wday,    6},
  {NT_("january"),    s_month,   0},
  {NT_("february"),   s_month,   1},
  {NT_("march"),      s_month,   2},
  {NT_("april"),      s_month,   3},
  {NT_("june"),       s_month,   5},
  {NT_("july"),       s_month,   6},
  {NT_("august"),     s_month,   7},
  {NT_("september"),  s_month,   8},
  {NT_("october"),    s_month,   9},
  {NT_("november"),   s_month,  10},
  {NT_("december"),   s_month,  11},

  {NT_("daily"),      s_daily,   0},
  {NT_("weekly"),     s_weekly,  0},
  {NT_("monthly"),    s_monthly, 0},
  {NT_("hourly"),     s_hourly,  0},

  {NT_("1st"),        s_wom,     0},
  {NT_("2nd"),        s_wom,     1},
  {NT_("3rd"),        s_wom,     2},
  {NT_("4th"),        s_wom,     3},
  {NT_("5th"),        s_wom,     4},
  {NT_("6th"),        s_wom,     5},

  {NT_("first"),      s_wom,     0},
  {NT_("second"),     s_wom,     1},
  {NT_("third"),      s_wom,     2},
  {NT_("fourth"),     s_wom,     3},
  {NT_("fifth"),      s_wom,     4},
  {NT_("sixth"),      s_wom,     5},
  {NULL,         s_none,    0}
};

static bool have_hour, have_mday, have_wday, have_month, have_wom;
static bool have_at, have_woy;

static void set_defaults(RUNBASE *lrun) {
   have_hour = have_mday = have_wday = have_month = have_wom = have_woy = false;
   have_at = false;
   set_bits(0, 23, lrun->hour);
   set_bits(0, 30, lrun->mday);
   set_bits(0, 6,  lrun->wday);
   set_bits(0, 11, lrun->month);
   set_bits(0, 5,  lrun->wom);
   set_bits(0, 53, lrun->woy);
}

void RUNBASE::copy(RUNBASE *src)
{
   minute = src->minute;
   last_run = src->last_run;
   next_run = src->next_run;
   last_day_set = src->last_day_set;
   memcpy(hour, src->hour, sizeof(hour));
   memcpy(mday, src->mday, sizeof(mday));
   memcpy(month,src->month,sizeof(month));
   memcpy(wday, src->wday, sizeof(wday));
   memcpy(wom,  src->wom,  sizeof(wom));
   memcpy(woy,  src->woy,  sizeof(woy));
}

/*
 * Store Schedule Run information
 *
 * Parse Run statement:
 *
 *  Run <keyword=value ...> [on] 2 january at 23:45
 *
 *   Default Run time is daily at 0:0
 *
 *   There can be multiple run statements, they are simply chained
 *   together at the upper level.
 *
 *   Look src/dird/run_conf.c to see how to use this function
 *
 */
void RUNBASE::store_runbase(LEX *lc, int token)
{
   int i;
   int state, state2 = 0, code = 0, code2 = 0;
   int options = lc->options;
   char *p;
   RUNBASE *lrun = this;


   lc->options |= LOPT_NO_IDENT;      /* want only "strings" */

   /* clear local copy of run record */
   clear();

   if (token == T_ERROR) {
      return;
   }

   /*
    * Scan schedule times.
    * Default is: daily at 0:0
    */
   state = s_none;
   set_defaults(lrun);

   for ( ; token != T_EOL; (token = lex_get_token(lc, T_ALL))) {
      int len;
      bool pm = false;
      bool am = false;
      switch (token) {
      case T_NUMBER:
         state = s_mday;
         code = atoi(lc->str) - 1;
         if (code < 0 || code > 30) {
            scan_err0(lc, _("Day number out of range (1-31)"));
         }
         break;
      case T_NAME:                 /* this handles drop through from keyword */
      case T_UNQUOTED_STRING:
         if (strchr(lc->str, (int)'-')) {
            state = s_range;
            break;
         }
         if (strchr(lc->str, (int)':')) {
            state = s_time;
            break;
         }
         if (lc->str_len == 3 && (lc->str[0] == 'w' || lc->str[0] == 'W') &&
             is_an_integer(lc->str+1)) {
            code = atoi(lc->str+1);
            if (code < 0 || code > 53) {
               scan_err0(lc, _("Week number out of range (0-53)"));
              /* NOT REACHED */
            }
            state = s_woy;            /* week of year */
            break;
         }
         /* everything else must be a keyword */
         for (i=0; keyw[i].name; i++) {
            if (strcasecmp(lc->str, keyw[i].name) == 0) {
               state = keyw[i].state;
               code   = keyw[i].code;
               i = 0;
               break;
            }
         }
         if (i != 0) {
            scan_err1(lc, _("Job type field: %s in run record not found"), lc->str);
            /* NOT REACHED */
         }
         break;
      case T_COMMA:
         continue;
      default:
         scan_err2(lc, _("Unexpected token: %d:%s"), token, lc->str);
         /* NOT REACHED */
         break;
      }
      switch (state) {
      case s_none:
         continue;
      case s_mday:                 /* day of month */
         if (!have_mday) {
            clear_bits(0, 30, lrun->mday);
            have_mday = true;
         }
         set_bit(code, lrun->mday);
         break;
      case s_month:                /* month of year */
         if (!have_month) {
            clear_bits(0, 11, lrun->month);
            have_month = true;
         }
         set_bit(code, lrun->month);
         break;
      case s_wday:                 /* week day */
         if (!have_wday) {
            clear_bits(0, 6, lrun->wday);
            have_wday = true;
         }
         set_bit(code, lrun->wday);
         break;
      case s_wom:                  /* Week of month 1st, ... */
         if (!have_wom) {
            clear_bits(0, 5, lrun->wom);
            have_wom = true;
         }
         set_bit(code, lrun->wom);
         break;
      case s_woy:
         if (!have_woy) {
            clear_bits(0, 53, lrun->woy);
            have_woy = true;
         }
         set_bit(code, lrun->woy);
         break;
      case s_time:                 /* time */
         if (!have_at) {
            scan_err0(lc, _("Time must be preceded by keyword AT."));
            /* NOT REACHED */
         }
         if (!have_hour) {
            clear_bits(0, 23, lrun->hour);
         }
//       Dmsg1(000, "s_time=%s\n", lc->str);
         p = strchr(lc->str, ':');
         if (!p)  {
            scan_err0(lc, _("Time logic error.\n"));
            /* NOT REACHED */
         }
         *p++ = 0;                 /* separate two halves */
         code = atoi(lc->str);     /* pick up hour */
         code2 = atoi(p);          /* pick up minutes */
         len = strlen(p);
         if (len >= 2) {
            p += 2;
         }
         if (strcasecmp(p, "pm") == 0) {
            pm = true;
         } else if (strcasecmp(p, "am") == 0) {
            am = true;
         } else if (len != 2) {
            scan_err0(lc, _("Bad time specification."));
            /* NOT REACHED */
         }
         /*
          * Note, according to NIST, 12am and 12pm are ambiguous and
          *  can be defined to anything.  However, 12:01am is the same
          *  as 00:01 and 12:01pm is the same as 12:01, so we define
          *  12am as 00:00 and 12pm as 12:00.
          */
         if (pm) {
            /* Convert to 24 hour time */
            if (code != 12) {
               code += 12;
            }
         /* am */
         } else if (am && code == 12) {
            code -= 12;
         }
         if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
            scan_err0(lc, _("Bad time specification."));
            /* NOT REACHED */
         }
//       Dmsg2(000, "hour=%d min=%d\n", code, code2);
         set_bit(code, lrun->hour);
         lrun->minute = code2;
         have_hour = true;
         break;
      case s_at:
         have_at = true;
         break;
      case s_ldom:
         if (!have_mday) {
            clear_bits(0, 30, lrun->mday);
            have_mday = true;
         }
         lrun->last_day_set = true;
         set_bit(31, lrun->mday);   /* day 32 => last day of month */
         break;
      case s_range:
         p = strchr(lc->str, '-');
         if (!p) {
            scan_err0(lc, _("Range logic error.\n"));
         }
         *p++ = 0;                 /* separate two halves */

         /* Check for day range */
         if (is_an_integer(lc->str) && is_an_integer(p)) {
            code = atoi(lc->str) - 1;
            code2 = atoi(p) - 1;
            if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
               scan_err0(lc, _("Bad day range specification."));
            }
            if (!have_mday) {
               clear_bits(0, 30, lrun->mday);
               have_mday = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun->mday);
            } else {
               set_bits(code, 30, lrun->mday);
               set_bits(0, code2, lrun->mday);
            }
            break;
         }
         /* Check for week of year range */
         if (strlen(lc->str) == 3 && strlen(p) == 3 &&
             (lc->str[0] == 'w' || lc->str[0] == 'W') &&
             (p[0] == 'w' || p[0] == 'W') &&
             is_an_integer(lc->str+1) && is_an_integer(p+1)) {
            code = atoi(lc->str+1);
            code2 = atoi(p+1);
            if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
               scan_err0(lc, _("Week number out of range (0-53)"));
            }
            if (!have_woy) {
               clear_bits(0, 53, lrun->woy);
               have_woy = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun->woy);
            } else {
               set_bits(code, 53, lrun->woy);
               set_bits(0, code2, lrun->woy);
            }
            break;
         }
         /* lookup first half of keyword range (week days or months) */
         lcase(lc->str);
         for (i=0; keyw[i].name; i++) {
            if (strcasecmp(lc->str, keyw[i].name) == 0) {
               state = keyw[i].state;
               code   = keyw[i].code;
               i = 0;
               break;
            }
         }
         if (i != 0 || (state != s_month && state != s_wday && state != s_wom)) {
            scan_err0(lc, _("Invalid month, week or position day range"));
            /* NOT REACHED */
         }

         /* Lookup end of range */
         lcase(p);
         for (i=0; keyw[i].name; i++) {
            if (strcasecmp(p, keyw[i].name) == 0) {
               state2  = keyw[i].state;
               code2   = keyw[i].code;
               i = 0;
               break;
            }
         }
         if (i != 0 || state != state2 || code == code2) {
            scan_err0(lc, _("Invalid month, weekday or position range"));
            /* NOT REACHED */
         }
         if (state == s_wday) {
            if (!have_wday) {
               clear_bits(0, 6, lrun->wday);
               have_wday = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun->wday);
            } else {
               set_bits(code, 6, lrun->wday);
               set_bits(0, code2, lrun->wday);
            }
         } else if (state == s_month) {
            if (!have_month) {
               clear_bits(0, 11, lrun->month);
               have_month = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun->month);
            } else {
               /* this is a bit odd, but we accept it anyway */
               set_bits(code, 11, lrun->month);
               set_bits(0, code2, lrun->month);
            }
         } else {
            /* Must be position */
            if (!have_wom) {
               clear_bits(0, 5, lrun->wom);
               have_wom = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun->wom);
            } else {
               set_bits(code, 5, lrun->wom);
               set_bits(0, code2, lrun->wom);
            }
         }
         break;
      case s_hourly:
         have_hour = true;
         set_bits(0, 23, lrun->hour);
         break;
      case s_weekly:
         have_mday = have_wom = have_woy = true;
         set_bits(0, 30, lrun->mday);
         set_bits(0, 5,  lrun->wom);
         set_bits(0, 53, lrun->woy);
         break;
      case s_daily:
         have_mday = true;
         set_bits(0, 6, lrun->wday);
         break;
      case s_monthly:
         have_month = true;
         set_bits(0, 11, lrun->month);
         break;
      default:
         scan_err0(lc, _("Unexpected run state\n"));
         /* NOT REACHED */
         break;
      }
   }

   lc->options = options;             /* restore scanner options */
}


/* Parser state */
enum parse_state {
   p_none,
   p_resource
};

void CONFIG::init(
   const char *cf,
   LEX_ERROR_HANDLER *scan_error,
   int32_t err_type,
   void *vres_all,
   int32_t res_all_size,
   int32_t r_first,
   int32_t r_last,
   RES_TABLE *resources,
   RES_HEAD ***res_head)
{
   m_cf = cf;
   m_scan_error = scan_error;
   m_err_type = err_type;
   m_res_all = vres_all;
   m_res_all_size = res_all_size;
   m_r_first = r_first;
   m_r_last = r_last;
   m_resources = resources;
   init_res_head(res_head, r_first, r_last);
   m_res_head = *res_head;
}

/*********************************************************************
 *
 * Parse configuration file
 *
 * Return 0 if reading failed, 1 otherwise
 *  Note, the default behavior unless you have set an alternate
 *  scan_error handler is to die on an error.
 */
bool CONFIG::parse_config()
{
   LEX *lc = NULL;
   int token, i, pass;
   int res_type = 0;
   enum parse_state state = p_none;
   RES_ITEM *items = NULL;
   int level = 0;
   static bool first = true;
   int errstat;
   const char *cf = m_cf;
   LEX_ERROR_HANDLER *scan_error = m_scan_error;
   int err_type = m_err_type;
   //HPKT hpkt;

   if (first && (errstat=rwl_init(&res_lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to initialize resource lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }
   first = false;

   char *full_path = (char *)alloca(MAX_PATH + 1);

   if (!find_config_file(cf, full_path, MAX_PATH +1)) {
      Jmsg0(NULL, M_ABORT, 0, _("Config filename too long.\n"));
   }
   cf = full_path;

   /* Make two passes. The first builds the name symbol table,
    * and the second picks up the items.
    */
   Dmsg0(900, "Enter parse_config()\n");
   for (pass=1; pass <= 2; pass++) {
      Dmsg1(900, "parse_config pass %d\n", pass);
      if ((lc = lex_open_file(lc, cf, scan_error)) == NULL) {
         berrno be;
         /* We must create a lex packet to print the error */
         lc = (LEX *)malloc(sizeof(LEX));
         memset(lc, 0, sizeof(LEX));
         lc->str = get_memory(5000);
         if (scan_error) {
            lc->scan_error = scan_error;
         } else {
            lex_set_default_error_handler(lc);
         }
         lex_set_error_handler_error_type(lc, err_type) ;
         pm_strcpy(lc->str, cf);
         lc->fname = lc->str;
         scan_err2(lc, _("Cannot open config file \"%s\": %s\n"),
            lc->str, be.bstrerror());
         free_pool_memory(lc->str);
         free(lc);
         return 0;
      }
      if (!m_encode_pass) {
         lex_store_clear_passwords(lc);
      }
      lex_set_error_handler_error_type(lc, err_type) ;
      while ((token=lex_get_token(lc, T_ALL)) != T_EOF) {
         Dmsg3(900, "parse state=%d pass=%d got token=%s\n", state, pass,
              lex_tok_to_str(token));
         switch (state) {
         case p_none:
            if (token == T_EOL) {
               break;
            } else if (token == T_UTF8_BOM) {
               /* We can assume the file is UTF-8 as we have seen a UTF-8 BOM */
               break;
            } else if (token == T_UTF16_BOM) {
               scan_err0(lc, _("Currently we cannot handle UTF-16 source files. "
                   "Please convert the conf file to UTF-8\n"));
               goto bail_out;
            } else if (token != T_IDENTIFIER) {
               scan_err1(lc, _("Expected a Resource name identifier, got: %s"), lc->str);
               goto bail_out;
            }
            for (i=0; resources[i].name; i++) {
               if (strcasecmp(resources[i].name, lc->str) == 0) {
                  items = resources[i].items;
                  if (!items) {
                     break;
                  }
                  state = p_resource;
                  res_type = resources[i].rcode;
                  init_resource0(this, res_type, items, pass);
                  break;
               }
            }
            if (state == p_none) {
               scan_err1(lc, _("expected resource name, got: %s"), lc->str);
               goto bail_out;
            }
            break;
         case p_resource:
            switch (token) {
            case T_BOB:
               level++;
               break;
            case T_IDENTIFIER:
               if (level != 1) {
                  scan_err1(lc, _("not in resource definition: %s"), lc->str);
                  goto bail_out;
               }
               for (i=0; items[i].name; i++) {
                  //hpkt.pass = pass;
                  //hpkt.ritem = &items[i];
                  //hpkt.edbuf = NULL;
                  //hpkt.index = i;
                  //hpkt.lc = lc;
                  //hpkt.hfunc = HF_STORE;
                  if (strcasecmp(items[i].name, lc->str) == 0) {
                     /* If the ITEM_NO_EQUALS flag is set we do NOT
                      *   scan for = after the keyword  */
                     if (!(items[i].flags & ITEM_NO_EQUALS)) {
                        token = lex_get_token(lc, T_SKIP_EOL);
                        Dmsg1 (900, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
                        if (token != T_EQUALS) {
                           scan_err1(lc, _("expected an equals, got: %s"), lc->str);
                           goto bail_out;
                        }
                     }
                     Dmsg1(800, "calling handler for %s\n", items[i].name);
                     /* Call item handler */
                     items[i].handler(lc, &items[i], i, pass);
                     i = -1;
                     break;
                  }
               }
               if (i >= 0) {
                  Dmsg2(900, "level=%d id=%s\n", level, lc->str);
                  Dmsg1(900, "Keyword = %s\n", lc->str);
                  scan_err1(lc, _("Keyword \"%s\" not permitted in this resource.\n"
                     "Perhaps you left the trailing brace off of the previous resource."), lc->str);
                  goto bail_out;
               }
               break;

            case T_EOB:
               level--;
               state = p_none;
               Dmsg0(900, "T_EOB => define new resource\n");
               if (res_all.hdr.name == NULL) {
                  scan_err0(lc, _("Name not specified for resource"));
                  goto bail_out;
               }
               if (!save_resource(this, res_type, items, pass)) {  /* save resource */
                  scan_err1(lc, "%s", m_errmsg);
                  goto bail_out;
               }
               break;

            case T_EOL:
               break;

            default:
               scan_err2(lc, _("unexpected token %d %s in resource definition"),
                  token, lex_tok_to_str(token));
               goto bail_out;
            }
            break;
         default:
            scan_err1(lc, _("Unknown parser state %d\n"), state);
            goto bail_out;
         }
      }
      if (state != p_none) {
         scan_err0(lc, _("End of conf file reached with unclosed resource."));
         goto bail_out;
      }
      if (chk_dbglvl(900) && pass == 2) {
         int i;
         for (i=m_r_first; i<=m_r_last; i++) {
            dump_each_resource(i, prtmsg, NULL);
         }
      }
      lc = lex_close_file(lc);
   }
   Dmsg0(900, "Leave parse_config()\n");
   return 1;
bail_out:
   if (lc) {
      lc = lex_close_file(lc);
   }
   return 0;
}

const char *get_default_configdir()
{
   return SYSCONFDIR;
}

#ifdef xxx_not_used
   HRESULT hr;
   static char szConfigDir[MAX_PATH + 1] = { 0 };
   if (!p_SHGetFolderPath) {
      bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
      return szConfigDir;
   }
   if (szConfigDir[0] == '\0') {
      hr = p_SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szConfigDir);
      if (SUCCEEDED(hr)) {
         bstrncat(szConfigDir, "\\Bacula", sizeof(szConfigDir));
      } else {
         bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
      }
   }
   return szConfigDir;
#endif


/*
 * Returns false on error
 *         true  on OK, with full_path set to where config file should be
 */
bool
find_config_file(const char *config_file, char *full_path, int max_path)
{
   int file_length = strlen(config_file) + 1;

   /* If a full path specified, use it */
   if (first_path_separator(config_file) != NULL) {
      if (file_length > max_path) {
         return false;
      }
      bstrncpy(full_path, config_file, file_length);
      return true;
   }

   /* config_file is default file name, now find default dir */
   const char *config_dir = get_default_configdir();
   int dir_length = strlen(config_dir);

   if ((dir_length + 1 + file_length) > max_path) {
      return false;
   }

   memcpy(full_path, config_dir, dir_length + 1);

   if (!IsPathSeparator(full_path[dir_length - 1])) {
      full_path[dir_length++] = '/';
   }

   memcpy(&full_path[dir_length], config_file, file_length);

   return true;
}

/*********************************************************************
 *
 *      Free configuration resources
 *
 */
void CONFIG::free_all_resources()
{
   RES *next, *res;
   if (m_res_head == NULL) {
      return;
   }
   /* Walk down chain of res_heads */
   for (int i=m_r_first; i<=m_r_last; i++) {
      if (m_res_head[i-m_r_first]) {
         next = m_res_head[i-m_r_first]->first;
         Dmsg2(500, "i=%d, next=%p\n", i, next);
         /* Walk down resource chain freeing them */
         for ( ; next; ) {
            res = next;
            next = res->res_next;
            free_resource(res, i);
         }
        free(m_res_head[i-m_r_first]->res_list);
        free(m_res_head[i-m_r_first]);
        m_res_head[i-m_r_first] = NULL;
      }
   }
}

CONFIG::CONFIG()
:  m_cf(NULL),
   m_scan_error(NULL),
   m_err_type(0),
   m_res_all(NULL),
   m_res_all_size(0),
   m_encode_pass(true),
   m_r_first(0),
   m_r_last(0),
   m_resources(NULL),
   m_res_head(NULL)
{
   m_errmsg = get_pool_memory(PM_EMSG);
   *m_errmsg = 0;
}

CONFIG::~CONFIG() {
   free_all_resources();
   free_pool_memory(m_errmsg);
}

void CONFIG::encode_password(bool a)
{
   m_encode_pass = a;
}
