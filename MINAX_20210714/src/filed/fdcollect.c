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
 * Radosław Korzeniewski, MMXVIII
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 *
 * This is a Bacula statistics File Daemon collector routines.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bacula.h"
#include "filed.h"

/* imported functions and variables */
extern const char collect_all_cmd[];
extern const char collect_metrics_cmd[];

static bool collector_threads_started = false;

/*
 * Initialize a daemon wide Bacula statcollector.
 *    Creates a global daemon collector and registers all global metrics.
 *    This is a statcollector initialization function specific for FileDaemon.
 */
void initialize_statcollector()
{
   POOL_MEM met(PM_NAME);

   /* create a statistics collector */
   statcollector = New(bstatcollect);
   /* memory utilization */
   Mmsg(met, "bacula.client.%s.memory.bufs", me->hdr.name);
   fdstatmetrics.bacula_client_memory_bufs =
         statcollector->registration(met.c_str(), METRIC_INT, METRIC_UNIT_NUMBER,
            "The number of buffers allocated.");
   Mmsg(met, "bacula.client.%s.memory.heap", me->hdr.name);
   fdstatmetrics.bacula_client_memory_heap =
         statcollector->registration(met.c_str(), METRIC_INT, METRIC_UNIT_BYTE,
            "The size of the heap.");
   Mmsg(met, "bacula.client.%s.memory.maxbufs", me->hdr.name);
   fdstatmetrics.bacula_client_memory_maxbufs =
         statcollector->registration(met.c_str(), METRIC_INT, METRIC_UNIT_NUMBER,
            "The maximum buffers allocated.");
   Mmsg(met, "bacula.client.%s.memory.maxbytes", me->hdr.name);
   fdstatmetrics.bacula_client_memory_maxbytes =
         statcollector->registration(met.c_str(), METRIC_INT, METRIC_UNIT_BYTE,
            "The allocated memory size.");
   Mmsg(met, "bacula.client.%s.memory.smbytes", me->hdr.name);
   fdstatmetrics.bacula_client_memory_smbytes =
         statcollector->registration(met.c_str(), METRIC_INT, METRIC_UNIT_BYTE,
            "The allocated memory size.");
   // statcollector->dump();
};

/*
 * This is a callback function executed by updatecollector_thread() every time some permanent and hard to count
 *    metrics should be collected. This include a memory allocator statistics.
 */
bool update_permanent_stats(void *data)
{
   /* update memory statistics */
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_bufs, sm_buffers);
#ifdef HAVE_WIN32
   uint64_t memused;
   memused = get_memory_info(NULL, 0);
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_heap, memused);
#else
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_heap, heap_used());
#endif
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_maxbufs, sm_max_buffers);
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_maxbytes, sm_max_bytes);
   statcollector->set_value_int64(fdstatmetrics.bacula_client_memory_smbytes, sm_bytes);
   return true;
};

/*
 * Starts a single thread for every defined Statistics resource and one globally single thread for
 *    Update Statistics if at least one Statistics resource is available.
 */
void start_collector_threads()
{
   COLLECTOR *collect;
   utime_t interval = 24*3600;      // the minimal update interval is 1 day
   UPDATE_COLLECTOR_INIT_t initdata;

   foreach_res(collect, R_COLLECTOR){
      interval = MIN(interval, collect->interval);
      collect->statcollector = statcollector;
      collect->spool_directory = working_directory;
      collect->daemon = "bacula-fd";
      collect->jcr = new_jcr(sizeof(JCR), filed_free_jcr);
      Dmsg1(100, "Starting statistics thread for \"%s\"\n", collect->name());
      start_collector_thread(collect);
      collector_threads_started = true;
   };
   if (((rblist *)res_head[R_COLLECTOR-r_first]->res_list)->size() > 0){
      initdata.interval = interval;
      initdata.jcr = new_jcr(sizeof(JCR), filed_free_jcr);
      initdata.routine = &update_permanent_stats;
      initdata.data = NULL;
      /* start update thread */
      start_updcollector_thread(initdata);
   }
};

/*
 * Terminates all Statistics's threads executed.
 */
void terminate_collector_threads()
{
   COLLECTOR *collect;

   if (collector_threads_started){
      foreach_res(collect, R_COLLECTOR){
         stop_collector_thread(collect);
      };
      stop_updcollector_thread();
      collector_threads_started = false;
   }
};

/*
 * Display a text representation of the metric for UA.
 *
 * in:
 *    user - a BSOCK class for sending rendered text to UA
 *    m - metric to display
 *    format - a display format
 * out:
 *    metric rendered and sent to the UA
 */
void display_metric(BSOCK *user, bstatmetric *m, display_format_t format, int nr)
{
   POOL_MEM out(PM_MESSAGE);
   int len;

   rendermetric(out, m, format, nr);
   len = strlen(out.c_str()) + 1;
   user->msg = check_pool_memory_size(user->msg, len);
   memcpy(user->msg, out.c_str(), len);
   user->msglen = len;
   user->send();
};

/*
 * Collect command from Director
 */
int collect_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *cmd;
   POOLMEM *fmt;
   POOLMEM *metrics = NULL;
   char *ch;
   char *met;
   bool doall = true;
   display_format_t format;
   bstatmetric *item;
   alist *list;
   int nr;

   cmd = get_memory(dir->msglen+1);
   fmt = get_memory(dir->msglen+1);

   Dmsg1(100, "collect cmd: %s\n", dir->msg);
   if (sscanf(dir->msg, collect_all_cmd, fmt) != 1) {
      if (sscanf(dir->msg, collect_metrics_cmd, fmt, cmd) != 2) {
         pm_strcpy(&jcr->errmsg, dir->msg);
         Jmsg1(jcr, M_FATAL, 0, _("Bad statistics command: %s\n"), jcr->errmsg);
         dir->fsend(_("2900 Bad statistics command, missing argument.\n"));
         dir->signal(BNET_EOD);
         free_memory(cmd);
         free_memory(fmt);
         return 0;
      } else {
         doall = false;
         metrics = strstr(dir->msg, cmd);
         pm_strcpy(cmd, metrics);
         metrics = cmd;
      }
   }

   format = scandisplayformat(fmt);
   if (!doall) Dmsg1(100, "statistics metrics: %s\n", metrics);
   if (format == COLLECT_JSON){
      dir->fsend("[");
   }
   update_permanent_stats(NULL);
   nr = 0;
   if (doall){
      /* we should display all the metrics found */
      list = statcollector->get_all();
      foreach_alist(item, list){
         display_metric(dir, item, format, nr++);
      }
      free_metric_alist(list);
   } else {
      /* we should display selected metrics only */
      met = metrics;
      /* eliminate the EOL char in metrics */
      ch = strchr(met, '\n');
      if (ch){
         *ch = ' ';
      }
      while (*met != 0 && (ch = strchr(met, ' ')) != NULL){
         /* prepare and get a single metric */
         *ch = '\0';
         Dmsg1(100, "statistics get metric: %s\n", met);
         item = statcollector->get_metric(met);
         if (item){
            Dmsg0(200, "metric found\n");
            display_metric(dir, item, format, nr++);
            delete(item);
         }
         met = ch + 1;
      }
   }
   if (format == COLLECT_JSON){
      dir->fsend("\n]\n");
   }

   dir->signal(BNET_EOD);
   free_memory(cmd);
   free_memory(fmt);
   return 1;
}
