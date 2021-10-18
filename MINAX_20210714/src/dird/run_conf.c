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
 *  Configuration parser for Director Run Configuration
 *   directives, which are part of the Schedule Resource
 *
 *     Kern Sibbald, May MM
 *
 */

#include "bacula.h"
#include "dird.h"

#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   extern URES res_all;
}
#else
extern URES res_all;
#endif
extern s_jl joblevels[];

static RUN lrun;

void RUN::clearall()
{
   RUNBASE::clear();

   level=0;
   Priority=0;
   job_type=0;
   MaxRunSchedTime=0;
   MaxRunSchedTime_set = spool_data =  spool_data_set = accurate = accurate_set = false;
   write_part_after_job = write_part_after_job_set = priority_set = level_set = false;

   pool = next_pool = full_pool = inc_pool = diff_pool = vfull_pool = NULL;
   storage  = NULL;
   msgs = NULL;
   since = NULL;
   next = NULL;
}

#define CPY(a) a = src->a

void RUN::copyall(RUN *src)
{
   clearall();

   RUNBASE::copy(src);
   CPY(level);
   CPY(Priority);
   CPY(job_type);
   CPY(MaxRunSchedTime);
   CPY(MaxRunSchedTime_set);
   CPY(spool_data);
   CPY(spool_data_set);
   CPY(accurate);
   CPY(accurate_set);

   CPY(write_part_after_job);
   CPY(write_part_after_job_set);
   CPY(priority_set);
   CPY(level_set);

   CPY(pool);
   CPY(next_pool);
   CPY(full_pool);
   CPY(inc_pool);
   CPY(diff_pool);
   CPY(storage);
   CPY(msgs);
   CPY(since);
}

/*
 * Keywords (RHS) permitted in Run records
 *
 *    name              token
 */
s_kw RunFields[] = {
   {"Pool",              'P'},
   {"FullPool",          'f'},
   {"IncrementalPool",   'i'},
   {"DifferentialPool",  'd'},
   {"Level",             'L'},
   {"Storage",           'S'},
   {"Messages",          'M'},
   {"Priority",          'p'},
   {"SpoolData",         's'},
   {"writepartafterjob", 'W'},
   {"MaxRunSchedTime",   'm'},
   {"Accurate",          'a'},
   {"NextPool",          'N'},
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
void store_run(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i, j;
   bool found;
   utime_t utime;
   int token;
   int options = lc->options;
   RUN **run = (RUN **)(item->value);
   RES *res;


   lc->options |= LOPT_NO_IDENT;      /* want only "strings" */

   /* clear local copy of run record */
   lrun.clearall();

   /* scan for Job level "full", "incremental", ... */
   for (found=true; found; ) {
      found = false;
      token = lex_get_token(lc, T_NAME);
      for (i=0; !found && RunFields[i].name; i++) {
         if (strcasecmp(lc->str, RunFields[i].name) == 0) {
            found = true;
            if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
               /* NOT REACHED */
            }
            switch (RunFields[i].token) {
            case 's':                 /* Data spooling */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.spool_data = true;
                  lrun.spool_data_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.spool_data = false;
                  lrun.spool_data_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            case 'W':                 /* Write part after job */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.write_part_after_job = true;
                  lrun.write_part_after_job_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.write_part_after_job = false;
                  lrun.write_part_after_job_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            case 'L':                 /* level */
               token = lex_get_token(lc, T_NAME);
               for (j=0; joblevels[j].level_name; j++) {
                  if (strcasecmp(lc->str, joblevels[j].level_name) == 0) {
                     lrun.level = joblevels[j].level;
                     lrun.job_type = joblevels[j].job_type;
                     lrun.level_set = true;
                     j = 0;
                     break;
                  }
               }
               if (j != 0) {
                  scan_err1(lc, _("Job level field: %s not found in run record"), lc->str);
                  /* NOT REACHED */
               }
               break;
            case 'p':                 /* Priority */
               token = lex_get_token(lc, T_PINT32);
               if (pass == 2) {
                  lrun.Priority = lc->pint32_val;
                  lrun.priority_set = true;
               }
               break;
            case 'P':                 /* Pool */
            case 'N':                 /* NextPool */
            case 'f':                 /* FullPool */
            case 'v':                 /* VFullPool */
            case 'i':                 /* IncPool */
            case 'd':                 /* DifPool */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_POOL, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Pool Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  switch(RunFields[i].token) {
                  case 'P':
                     lrun.pool = (POOL *)res;
                     break;
                  case 'N':
                     lrun.next_pool = (POOL *)res;
                     break;
                  case 'f':
                     lrun.full_pool = (POOL *)res;
                     break;
                  case 'v':
                     lrun.vfull_pool = (POOL *)res;
                     break;
                  case 'i':
                     lrun.inc_pool = (POOL *)res;
                     break;
                  case 'd':
                     lrun.diff_pool = (POOL *)res;
                     break;
                  }
               }
               break;
            case 'S':                 /* storage */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_STORAGE, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Storage Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.storage = (STORE *)res;
               }
               break;
            case 'M':                 /* messages */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_MSGS, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Messages Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.msgs = (MSGS *)res;
               }
               break;
            case 'm':           /* max run sched time */
               token = lex_get_token(lc, T_QUOTED_STRING);
               if (!duration_to_utime(lc->str, &utime)) {
                  scan_err1(lc, _("expected a time period, got: %s"), lc->str);
                  return;
               }
               lrun.MaxRunSchedTime = utime;
               lrun.MaxRunSchedTime_set = true;
               break;
            case 'a':           /* accurate */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.accurate = true;
                  lrun.accurate_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.accurate = false;
                  lrun.accurate_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            default:
               scan_err1(lc, _("Expected a keyword name, got: %s"), lc->str);
               /* NOT REACHED */
               break;
            } /* end switch */
         } /* end if strcasecmp */
      } /* end for RunFields */

      /* At this point, it is not a keyword. Check for old syle
       * Job Levels without keyword. This form is depreciated!!!
       */
      if (!found) {
         for (j=0; joblevels[j].level_name; j++) {
            if (strcasecmp(lc->str, joblevels[j].level_name) == 0) {
               lrun.level = joblevels[j].level;
               lrun.job_type = joblevels[j].job_type;
               found = true;
               break;
            }
         }
      }
   } /* end for found */

   lrun.store_runbase(lc, token);

   /* Allocate run record, copy new stuff into it,
    * and append it to the list of run records
    * in the schedule resource.
    */
   if (pass == 2) {
      RUN *tail;

      /* Create new run record */
      RUN *nrun = (RUN *)malloc(sizeof(RUN));
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
   set_bit(index, res_all.res_sch.hdr.item_present);
}
