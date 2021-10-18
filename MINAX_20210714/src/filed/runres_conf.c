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

#include "bacula.h"
#include "jcr.h"
#include "filed.h"
#include "protos.h"

#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   extern URES res_all;
}
#else
extern URES res_all;
#endif

static RUNRES lrun;

void RUNRES::clearall()
{
   RUNBASE::clear();

   MaxConnectTime=0;
   MaxConnectTime_set = false;
   next = NULL;
}

#define CPY(a) a = src->a

void RUNRES::copyall(RUNRES *src)
{
   clearall();

   RUNBASE::copy(src);
   CPY(MaxConnectTime);
   CPY(MaxConnectTime_set);
}


/*
 * Keywords (RHS) permitted in Run records
 *
 *    name              token
 */
s_kw ConnectFields[] = {
   {"MaxConnectTime",   'm'},
   {NULL,                 0}
};

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
 *   together.
 *
 */
void store_runres(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   bool found;
   utime_t utime;
   int token;
   int options = lc->options;
   RUNRES **run = (RUNRES **)(item->value);

   lc->options |= LOPT_NO_IDENT;      /* want only "strings" */

   /* clear local copy of run record */
   lrun.clearall();

   /* scan for Job level "full", "incremental", ... */
   for (found=true; found; ) {
      found = false;
      token = lex_get_token(lc, T_NAME);
      for (i=0; !found && ConnectFields[i].name; i++) {
         if (strcasecmp(lc->str, ConnectFields[i].name) == 0) {
            found = true;
            if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
               /* NOT REACHED */
            }
            switch (ConnectFields[i].token) {
            case 'm':           /* max run sched time */
               token = lex_get_token(lc, T_QUOTED_STRING);
               if (!duration_to_utime(lc->str, &utime)) {
                  scan_err1(lc, _("expected a time period, got: %s"), lc->str);
                  return;
               }
               lrun.MaxConnectTime = utime;
               lrun.MaxConnectTime_set = true;
               break;
            default:
               scan_err1(lc, _("Expected a keyword name, got: %s"), lc->str);
               /* NOT REACHED */
               break;
            } /* end switch */
         } /* end if strcasecmp */
      } /* end for ConnectFields */
   } /* end for found */

   lrun.store_runbase(lc, token);

   /* Allocate run record, copy new stuff into it,
    * and append it to the list of run records
    * in the schedule resource.
    */
   if (pass == 2) {
      RUNRES *tail;

      /* Create new run record */
      RUNRES *nrun = (RUNRES *)malloc(sizeof(RUNRES));
      nrun->copyall(&lrun);
      nrun ->next = NULL;

      if (!*run) {                    /* if empty list */
         *run = nrun;                 /* add new record */
      } else {
         for (tail = *run; tail->next; tail=tail->next)
            {  }
         tail->next = nrun;
      }
   }

   lc->options = options;             /* restore scanner options */
   set_bit(index, res_all.res_sched.hdr.item_present);
}
