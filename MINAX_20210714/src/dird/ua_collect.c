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
 *   Bacula Director -- User Agent Collector Commands
 *
 * Radoslaw Korzeniewski, MMXVIII
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 *
 * This is a Bacula statistics internal collector thread.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bacula.h"
#include "dird.h"

/* imported variables */
static bool collector_threads_started = false;

/*
 * Catalog database query handler function used for catalog metrics.
 */
int dirstat_handler(void *ctx, int num_fields, char **row)
{
   int64_t *cx = (int64_t *)ctx;
   *cx = str_to_int64(row[0]);
   return 0;
}

/*
 * Updates permanent metrics based on catalog query.
 */
void update_permanent_metric(BDB *db, int metric, const char *sql)
{
   int64_t statcount;

   db_sql_query(db, sql, dirstat_handler, &statcount);
   statcollector->set_value_int64(metric, statcount);
};

/*
 * This is a callback function executed by updatecollector_thread() every time some permanent and hard to count
 *    metrics should be collected. This include a memory allocator statistics and catalog based metrics.
 */
bool update_permanent_stats(void *data)
{
   CAT *catalog;
   JCR *jcr;
   int nrunning = 0;
   int nqueued = 0;

   /* update memory statistics */
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_bufs, sm_buffers);
#ifdef HAVE_WIN32
   uint64_t memused;
   memused = get_memory_info(NULL, 0);
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_heap, memused);
#else
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_heap, heap_used());
#endif
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_maxbufs, sm_max_buffers);
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_maxbytes, sm_max_bytes);
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_memory_smbytes, sm_bytes);
   /* Loop over databases */
   foreach_res(catalog, R_CATALOG) {
      BDB *db;
      /*
       * Make sure we can open catalog, otherwise print a warning
       * message because the server is probably not running.
       */
      db = db_init_database(NULL, catalog->db_driver, catalog->db_name,
              catalog->db_user,
              catalog->db_password, catalog->db_address,
              catalog->db_port, catalog->db_socket,
              catalog->db_ssl_mode, catalog->db_ssl_key,
              catalog->db_ssl_cert, catalog->db_ssl_ca,
              catalog->db_ssl_capath, catalog->db_ssl_cipher,
              catalog->mult_db_connections,
              catalog->disable_batch_insert);
      if (!db || !db_open_database(NULL, db)) {
         if (db) {
            db_close_database(NULL, db);
         }
         continue;
      }
      /* update permanent runtime metrics from catalog */
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_all, count_all_jobs);
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_success_all, count_success_jobs);
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_errors_all, count_error_jobs);
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_warning_all, count_warning_jobs);
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_files, sum_jobs_files);
      update_permanent_metric(db, dirstatmetrics.bacula_jobs_bytes, sum_jobs_bytes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_all, count_all_volumes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_bytes, sum_volumes_bytes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_available, count_avl_volumes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_errors_all, count_error_volumes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_full_all, count_full_volumes);
      update_permanent_metric(db, dirstatmetrics.bacula_volumes_used_all, count_used_volumes);

      if (db) db_close_database(NULL, db);
   };
   /* handle queued and running jobs */
   foreach_jcr(jcr) {
      /* skip console jobs */
      if (jcr->JobId == 0) {
         continue;
      }
      switch(jcr->JobStatus){
         case JS_Blocked:
         case JS_Differences:
         case JS_DataCommitting:
         case JS_WaitMount:
         case JS_Running:
         case JS_WaitSD:
         case JS_WaitFD:
         case JS_AttrDespooling:
         case JS_AttrInserting:
         case JS_DataDespooling:
         case JS_WaitMedia:
         case JS_CloudUpload:
         case JS_CloudDownload:
            nrunning++;
            break;
         case JS_Created:
         case JS_WaitMaxJobs:
         case JS_WaitPriority:
         case JS_WaitDevice:
         case JS_WaitStartTime:
            nqueued++;
            break;
         default:
            break;
      }
   };
   endeach_jcr(jcr);
   statcollector->set_value_int64(dirstatmetrics.bacula_jobs_queued_all, nqueued);
   statcollector->set_value_int64(dirstatmetrics.bacula_jobs_running_all, nrunning);
   return true;
};

/*
 * Initialize a daemon wide Bacula statcollector.
 *    Creates a global daemon collector and registers all global metrics.
 *    This is a statcollector initialization function specific for Director.
 */
void initialize_statcollector()
{
   /* create a statistics collector */
   statcollector = New(bstatcollect);
   /* register config metrics */
   dirstatmetrics.bacula_dir_config_clients =
         statcollector->registration_int64("bacula.dir.config.clients", METRIC_UNIT_CLIENT,
            ((rblist *)res_head[R_CLIENT-r_first]->res_list)->size(),
            "The number of defined clients in Director.");
   dirstatmetrics.bacula_dir_config_jobs =
         statcollector->registration_int64("bacula.dir.config.jobs", METRIC_UNIT_JOB,
            ((rblist *)res_head[R_JOB-r_first]->res_list)->size(),
            "The number of defined jobs in Director.");
   dirstatmetrics.bacula_dir_config_filesets =
         statcollector->registration_int64("bacula.dir.config.filesets", METRIC_UNIT_FILESET,
            ((rblist *)res_head[R_FILESET-r_first]->res_list)->size(),
            "The number of defined filesets in Director.");
   dirstatmetrics.bacula_dir_config_pools =
         statcollector->registration_int64("bacula.dir.config.pools", METRIC_UNIT_POOL,
            ((rblist *)res_head[R_POOL-r_first]->res_list)->size(),
            "The number of defined pools in Director.");
   dirstatmetrics.bacula_dir_config_schedules =
         statcollector->registration_int64("bacula.dir.config.schedules", METRIC_UNIT_SCHEDULE,
            ((rblist *)res_head[R_SCHEDULE-r_first]->res_list)->size(),
            "The number of defined schedules in Director.");
   dirstatmetrics.bacula_dir_config_storages =
         statcollector->registration_int64("bacula.dir.config.storages", METRIC_UNIT_STORAGE,
            ((rblist *)res_head[R_STORAGE-r_first]->res_list)->size(),
            "The number of defined storages in Director.");
   /* register runtime metrics */
   dirstatmetrics.bacula_jobs_queued_all =
         statcollector->registration_int64("bacula.jobs.queued.all", METRIC_UNIT_JOB,
            0, "The number of currently queued jobs.");
   dirstatmetrics.bacula_jobs_running_all =
         statcollector->registration_int64("bacula.jobs.running.all", METRIC_UNIT_JOB,
            0, "The number of currently running jobs.");
   /* register permanent runtime metrics - from catalog */
   dirstatmetrics.bacula_jobs_all =
         statcollector->registration("bacula.jobs.all", METRIC_INT, METRIC_UNIT_JOB,
            "Number of all jobs.");
   dirstatmetrics.bacula_jobs_success_all =
         statcollector->registration("bacula.jobs.success.all", METRIC_INT, METRIC_UNIT_JOB,
            "Number of successful jobs.");
   dirstatmetrics.bacula_jobs_errors_all =
         statcollector->registration("bacula.jobs.error.all", METRIC_INT, METRIC_UNIT_JOB,
            "Number of error jobs.");
   dirstatmetrics.bacula_jobs_warning_all =
         statcollector->registration("bacula.jobs.warning.all", METRIC_INT, METRIC_UNIT_JOB,
            "Number of warning jobs.");
   dirstatmetrics.bacula_jobs_files =
         statcollector->registration("bacula.jobs.files", METRIC_INT, METRIC_UNIT_FILE,
            "Number of all jobs files.");
   dirstatmetrics.bacula_jobs_bytes =
         statcollector->registration("bacula.jobs.bytes", METRIC_INT, METRIC_UNIT_BYTE,
            "Sum size of all jobs.");
   /* register volumes information */
   dirstatmetrics.bacula_volumes_all =
         statcollector->registration("bacula.volumes.all", METRIC_INT, METRIC_UNIT_VOLUME,
            "Number of all defined volumes.");
   dirstatmetrics.bacula_volumes_available =
         statcollector->registration("bacula.volumes.available", METRIC_INT, METRIC_UNIT_VOLUME,
            "Number of all available volumes.");
   dirstatmetrics.bacula_volumes_bytes =
         statcollector->registration("bacula.volumes.bytes", METRIC_INT, METRIC_UNIT_BYTE,
            "Sum size of all volumes written.");
   dirstatmetrics.bacula_volumes_errors_all =
         statcollector->registration("bacula.volumes.errors.all", METRIC_INT, METRIC_UNIT_VOLUME,
            "Number of Error volumes.");
   dirstatmetrics.bacula_volumes_full_all =
         statcollector->registration("bacula.volumes.full.all", METRIC_INT, METRIC_UNIT_VOLUME,
            "Number of Full volumes.");
   dirstatmetrics.bacula_volumes_used_all =
         statcollector->registration("bacula.volumes.used.all", METRIC_INT, METRIC_UNIT_VOLUME,
            "Number of Used volumes.");
   /* register memory utilization */
   dirstatmetrics.bacula_dir_memory_bufs =
         statcollector->registration("bacula.dir.memory.bufs", METRIC_INT, METRIC_UNIT_NUMBER,
            "The number of buffers allocated.");
   dirstatmetrics.bacula_dir_memory_heap =
         statcollector->registration("bacula.dir.memory.heap", METRIC_INT, METRIC_UNIT_BYTE,
            "The size of the heap.");
   dirstatmetrics.bacula_dir_memory_maxbufs =
         statcollector->registration("bacula.dir.memory.maxbufs", METRIC_INT, METRIC_UNIT_NUMBER,
            "The maximum buffers allocated.");
   dirstatmetrics.bacula_dir_memory_maxbytes =
         statcollector->registration("bacula.dir.memory.maxbytes", METRIC_INT, METRIC_UNIT_BYTE,
            "The allocated memory size.");
   dirstatmetrics.bacula_dir_memory_smbytes =
         statcollector->registration("bacula.dir.memory.smbytes", METRIC_INT, METRIC_UNIT_BYTE,
            "The allocated memory size.");
   // statcollector->dump();
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
      collect->daemon = "bacula-dir";
      collect->jcr = new_jcr(sizeof(JCR), dird_free_jcr);
      Dmsg1(100, "Starting statistics thread for \"%s\"\n", collect->name());
      start_collector_thread(collect);
      collector_threads_started = true;
   };
   if (((rblist *)res_head[R_COLLECTOR-r_first]->res_list)->size() > 0){
      initdata.interval = interval;
      initdata.jcr = new_jcr(sizeof(JCR), dird_free_jcr);
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
 * Updates metrics values based on the configuration resources.
 */
void update_config_stats()
{
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_clients,
      ((rblist *)res_head[R_CLIENT-r_first]->res_list)->size());
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_jobs,
      ((rblist *)res_head[R_JOB-r_first]->res_list)->size());
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_filesets,
      ((rblist *)res_head[R_FILESET-r_first]->res_list)->size());
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_pools,
      ((rblist *)res_head[R_POOL-r_first]->res_list)->size());
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_schedules,
      ((rblist *)res_head[R_SCHEDULE-r_first]->res_list)->size());
   statcollector->set_value_int64(dirstatmetrics.bacula_dir_config_storages,
      ((rblist *)res_head[R_STORAGE-r_first]->res_list)->size());
};

/*
 * Display a text representation of the metric for UA.
 *
 * in:
 *    ua - an UA context class for sending rendered text to UA
 *    m - metric to display
 *    format - a display format
 * out:
 *    metric rendered and sent to the UA
 */
void display_metric(UAContext *ua, bstatmetric *m, display_format_t format, int nr)
{
   POOL_MEM out(PM_MESSAGE);

   rendermetric(out, m, format, nr);
   ua->send_msg(out.c_str());
};

/*
 * Sends a collect command to client.
 *
 * in:
 *    ua - an UA context class for sending rendered text to UA
 *    client - a CLIENT class which we are query
 *    doall - when true the "collect all" command should be executed else render specific metric
 *    format - a display format
 *    margc, margk - a command argc/argk parameters
 * out:
 *    collect command handled by a client
 */
void do_collect_client(UAContext *ua, CLIENT *client, bool doall, display_format_t format, int margc, char**margk)
{
   int i;
   BSOCK *fd;
   POOL_MEM buf;

   if (!acl_access_client_ok(ua, client->name(), JT_BACKUP_RESTORE)) {
      ua->error_msg(_("No authorization for Client \"%s\"\n"), client->name());
      return;
   }
   /* Connect to File daemon */
   ua->jcr->client = client;
   /* Release any old dummy key */
   if (ua->jcr->sd_auth_key) {
      free(ua->jcr->sd_auth_key);
   }
   /* Create a new dummy SD auth key */
   ua->jcr->sd_auth_key = bstrdup("dummy");

   /* Try to connect for 15 seconds */
   if (!ua->api) ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
                              client->name(), get_client_address(ua->jcr, client, buf.addr()), client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      ua->send_msg(_("Failed to connect to Client %s.\n====\n"),
         client->name());
      free_bsock(ua->jcr->file_bsock);
      return;
   }
   Dmsg0(20, _("Connected to file daemon\n"));
   fd = ua->jcr->file_bsock;
   /* command */
   if (doall){
      fd->fsend(collect_all_send_cmd, displayformat2str(format));
   } else {
      Mmsg(buf, collect_metrics_send_cmd, displayformat2str(format));
      for (i = 0; i < margc; i++) {
         pm_strcat(buf, " ");
         pm_strcat(buf, margk[i]);
      }
      pm_strcat(buf, "\n");
      fd->fsend(buf.c_str());
   }
   while (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   free_bsock(ua->jcr->file_bsock);
   return;
};

/*
 * Sends a collect command to storage.
 *
 * in:
 *    ua - an UA context class for sending rendered text to UA
 *    client - a CLIENT class which we are query
 *    doall - when true the "collect all" command should be executed else render specific metric
 *    format - a display format
 *    margc, margk - a command argc/argk parameters
 * out:
 *    collect command handled by a storage
 */
void do_collect_storage(UAContext *ua, STORE *store, bool doall, display_format_t format, int margc, char**margk)
{
   int i;
   BSOCK *sd;
   USTORE lstore;
   POOL_MEM buf;

   if (!acl_access_ok(ua, Storage_ACL, store->name())) {
      ua->error_msg(_("No authorization for Storage \"%s\"\n"), store->name());
      return;
   }
   /*
    * The Storage daemon is problematic because it shows information
    *  related to multiple Job, so if there is a Client or Job
    *  ACL restriction, we forbid all access to the Storage.
    */
   if (have_restricted_acl(ua, Client_ACL) ||
       have_restricted_acl(ua, Job_ACL)) {
      ua->error_msg(_("Restricted Client or Job does not permit access to  Storage daemons\n"));
      return;
   }
   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(ua->jcr, &lstore);
   /* Try connecting for up to 15 seconds */
   if (!ua->api) ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
      store->name(), store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 1, 15, 0)) {
      ua->send_msg(_("\nFailed to connect to Storage daemon %s.\n====\n"),
         store->name());
      free_bsock(ua->jcr->store_bsock);
      return;
   }
   Dmsg0(20, "Connected to storage daemon\n");
   sd = ua->jcr->store_bsock;
   /* command */
   if (doall){
      sd->fsend(collect_all_send_cmd, displayformat2str(format));
   } else {
      Mmsg(buf, collect_metrics_send_cmd, displayformat2str(format));
      for (i = 0; i < margc; i++) {
         pm_strcat(buf, " ");
         pm_strcat(buf, margk[i]);
      }
      pm_strcat(buf, "\n");
      sd->fsend(buf.c_str());
   }
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);
   free_bsock(ua->jcr->store_bsock);
   return;
};

/*
 * A collect command handled by director.
 *    If the command points to "dir" then it will be executed on Director. The "client[=name]" parameter
 *    points to the client resource and storage[=name] will point to the storage resource.
 *
 * in:
 *    ua - an UA context class for sending rendered text to UA
 *    cmd - a command string to execute
 * out:
 *    collect command handled by dir/client/storage depends on command parameter
 */
int collect_cmd(UAContext *ua, const char *cmd)
{
   int i, nr, margc;
   alist *list;
   bstatmetric *item;
   bool doall = true;
   display_format_t format = COLLECT_SIMPLE;
   char **margk;
   STORE *store = NULL;
   CLIENT *client = NULL;

   Dmsg1(20, "cmd:%s:\n", cmd);

   margk = (char**)malloc(ua->argc * sizeof(char*));
   margc = 0;
   /* scan all parameters to set flags */
   for (i = 1; i < ua->argc; i++){
      Dmsg1(20, "statistics process param: %s\n", ua->argk[i]);
      if (strcasecmp(ua->argk[i], "all") == 0){
         doall = true;
         Dmsg0(20, "statistics: param doall\n");
         continue;
      }
      if (strcasecmp(ua->argk[i], "dir") == 0){
         /* this is a default */
         Dmsg0(20, "statistics: dir\n");
         continue;
      }
      if (strcasecmp(ua->argk[i], "simple") == 0){
         /* this is a default */
         Dmsg0(20, "statistics: Simple format\n");
         continue;
      }
      if (strcasecmp(ua->argk[i], "full") == 0){
         format = COLLECT_FULL;
         Dmsg0(20, "statistics format Full\n");
         continue;
      }

      if (strcasecmp(ua->argk[i], "json") == 0){
         format = COLLECT_JSON;
         Dmsg0(20, "statistics format JSON\n");
         continue;
      }
      if (strcasecmp(ua->argk[i], "client") == 0 && store == NULL) {
         client = get_client_resource(ua, JT_BACKUP_RESTORE);
         if (!client) {
            goto bailout;
         }
         continue;
      }
      if (strcasecmp(ua->argk[i], "storage") == 0 && client == NULL) {
         store = get_storage_resource(ua, false /*no default*/, true/*unique*/);
         if (!store) {
            goto bailout;
         }
         continue;
      }
      /* everything else is a metric to display */
      Dmsg1(20, "statistics add metric: %s\n", ua->argk[i]);
      margk[margc++] = ua->argk[i];
      doall = false;
   }

   if (doall){
      Dmsg0(20, "statistics default doall!\n");
   }

      /* If no args, ask for status type */
   if (ua->argc == 1) {
       char prmt[MAX_NAME_LENGTH];

      start_prompt(ua, _("Statistics available for:\n"));
      add_prompt(ua, NT_("Director"));
      add_prompt(ua, NT_("Storage"));
      add_prompt(ua, NT_("Client"));
      Dmsg0(20, "do_prompt: select daemon\n");
      if ((i=do_prompt(ua, "",  _("Select daemon type for statistics"), prmt, sizeof(prmt))) < 0) {
         goto bailout;
      }
      Dmsg1(20, "item=%d\n", i);
      switch (i) {
      case 0:                         /* Director, the default behavior */
         break;
      case 1:
         store = select_storage_resource(ua, true/*unique*/);
         if (!store) {
            goto bailout;
         }
         break;
      case 2:
         client = select_client_resource(ua, JT_BACKUP_RESTORE);
         if (!client) {
            goto bailout;
         }
         break;
      default:
         return 1;
      }
   }

   if (client){
      /* do collect client */
      do_collect_client(ua, client, doall, format, margc, margk);
   } else
   if (store){
      /* do collect storage */
      do_collect_storage(ua, store, doall, format, margc, margk);
   } else {
      /* it is simpler to handle JSON array here */
      if (format == COLLECT_JSON){
         ua->send_msg("[");
      }
      /* collect director */
      update_permanent_stats(NULL);
      nr = 0;
      if (doall){
         /* we should display all the metrics found */
         list = statcollector->get_all();
         foreach_alist(item, list){
            display_metric(ua, item, format, nr++);
         }
         free_metric_alist(list);
      } else {
         /* we should display selected metrics only */
         for (i = 0; i < margc; i++) {
            /* get a single metric */
            item = statcollector->get_metric(margk[i]);
            if (item){
               display_metric(ua, item, format, nr++);
               delete(item);
            }
         }
      }
      /* it is simpler to handle JSON array here */
      if (format == COLLECT_JSON){
         ua->send_msg("\n]\n");
      }
   }
bailout:
   if (margk){
      free(margk);
   }
   return 1;
}
