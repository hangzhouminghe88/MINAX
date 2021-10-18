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
 * This is a Bacula statistics internal collector thread.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bacula.h"
#include "parse_conf.h"
#include "jcr.h"
#include <time.h>

#define SPOOLFNAME   "%s/%s.collector.%s.spool"

/* this is a single and global update collector class */
static UPDATECOLLECTOR updcollector;

/*
 * COLLECTOR resource class lock.
 */
void COLLECTOR::lock()
{
   pthread_mutex_lock(&mutex);
};

/*
 * COLLECTOR resource class unlock.
 */
void COLLECTOR::unlock()
{
   pthread_mutex_unlock(&mutex);
};

/*
 * Sets a spooled flags for collector.
 */
void COLLECTOR::setspooled(int s)
{
   lock();
   spooled = s;
   unlock();
};

/*
 * Gets a spooled flags from collector.
 */
int COLLECTOR::getspooled()
{
   int s;

   lock();
   s = spooled;
   unlock();
   return s;
};

/*
 * Updates a collector timestamp.
 */
void COLLECTOR::updatetimestamp()
{
   lock();
   timestamp = time(NULL);
   unlock();
};

/*
 * Render a CSV representation of the metric and writes them to opened file.
 *
 * in:
 *    fd - opened file descriptor used for write
 *    collector - the Statistics resource class
 *    item - metric to render and write
 *    timestamp - the current Unix time used for metric rendering
 * out:
 *    len - the length of rendered and data written
 */
int write_metricascsv(int fd, COLLECTOR *collector, bstatmetric *item, time_t timestamp)
{
   int len;
   int status;
   POOL_MEM met(PM_NAME);
   POOL_MEM buf(PM_MESSAGE);
   POOL_MEM out(PM_MESSAGE);

   render_metric_prefix(collector, met, item);
   item->render_metric_value(buf);
   Mmsg(out, "%lld, %s, %s\n", timestamp, met.c_str(), buf.c_str());
   len = strlen(out.c_str());
   status = write(fd, out.c_str(), len);
   return status == len;
};

/*
 * Render a Graphite representation of the metric and store in the buffer.
 *
 * in:
 *    collector - the Statistics resource class
 *    out - the POLL_MEM buffer to render a metric to
 *    item - metric to render and write
 *    timestamp - the current Unix time used for metric rendering
 */
void render_metric_graphite(COLLECTOR *collector, POOL_MEM &out, bstatmetric *item, time_t timestamp)
{
   POOL_MEM tmp1(PM_NAME);
   POOL_MEM tmp2(PM_NAME);

   render_metric_prefix(collector, tmp1, item);
   item->render_metric_value(tmp2);
   Mmsg(out, "%s %s %lld\n", tmp1.c_str(), tmp2.c_str(), timestamp);
};

/*
 * Saves a metrics grouped at an array list to file pointed by a File parameter from Statistics
 *  resource as CSV data.
 *
 * in:
 *    collector - the Statistics resource class
 *    list - an array list of metrics to save to file
 *    timestamp - the current Unix time used for metric rendering
 * out:
 *    True when the Statistics should continue to work (i.e. a save was successful)
 *    False when the Statistics should finish the work because some fatal error occurred which
 *       prevents the Statistics from work
 */
bool save_metrics2csv(COLLECTOR *collector, alist *list)
{
   int fd;
   bstatmetric *item;

   fd = open(collector->file, O_WRONLY|O_CREAT|O_APPEND, 0640);
   if (fd > 0){
      /* open success */
      foreach_alist(item, list){
         if (!write_metricascsv(fd, collector, item, collector->timestamp)){
            berrno be;
            Emsg2(M_ERROR, 0, "Error saving data file: %s Err=%s\n", collector->file, be.bstrerror());
            collector->lock();
            Mmsg(collector->errmsg, "Error saving data file: %s Err=%s", collector->file, be.bstrerror());
            collector->unlock();
            close(fd);
            return false;
         }
      }
      close(fd);
      return true;
   }
   /* error */
   berrno be;
   Emsg2(M_ERROR, 0, "Error opening collector data file: %s Err=%s\n", collector->file, be.bstrerror());
   collector->lock();
   Mmsg(collector->errmsg, "Error opening collector data file: %s Err=%s", collector->file, be.bstrerror());
   collector->unlock();
   return false;
};

/*
 * Saves a metrics grouped at an array list to Graphite pointed by a Statistics parameters.
 *  The backend connects to a Graphite server and push all metrics data from metrics array list.
 *  When connection to Graphite server is unsuccessful then a backend will spool metrics data
 *  automatically into a local file located at WorkingDirectory of the daemon. A spooling to file
 *  continue until connection will be restored again. The spooling file is preserved between any
 *  collector thread or daemon restarts.
 *
 * in:
 *    collector - the Statistics resource class
 *    list - an array list of metrics to save to file
 *    timestamp - the current Unix time used for metric rendering
 *    spooled - a variable which saves a backend spooling status between calling
 * out:
 *    True when the Statistics should continue to work (i.e. a save was successful)
 *    False when the Statistics should finish the work because some fatal error occurred which
 *       prevents the Statistics from work
 */
bool save_metrics2graphite(COLLECTOR *collector, alist *list)
{
   BSOCKCORE *bs;
   int fd;
   int len;
   int spooled;
   bstatmetric *item;
   POOL_MEM buf(PM_MESSAGE);
   POOL_MEM sname(PM_FNAME);

   bs = New(BSOCKCORE);
   if (bs->connect(collector->jcr, 1, 3, 0, collector->hdr.name, collector->host, NULL, collector->port, 0)) {
      /* connect success, despool data if already spooled */
      collector->errmsg[0] = 0;
      spooled = collector->getspooled();
      if (spooled != BCOLLECT_SPOOL_NO && collector->spool_directory){
         collector->setspooled(BCOLLECT_SPOOL_DESPOOL);
         Mmsg(sname, SPOOLFNAME, collector->spool_directory, collector->daemon, collector->hdr.name);
         fd = open(sname.c_str(), O_RDONLY);
         if (fd > 0){
            /* we have some spooled data */
            Dmsg2(500, "%s despooling metrics from: %s\n", collector->hdr.name, sname.c_str());
            while ((len = read(fd, bs->msg, sizeof_pool_memory(bs->msg))) > 0){
               bs->msglen = len;
               bs->send();
            }
            close(fd);
            unlink(sname.c_str());
         }
      }
      bs->msg[0] = 0;
      /* render and send metrics to Graphite */
      foreach_alist(item, list){
         render_metric_graphite(collector, buf, item, collector->timestamp);
         pm_strcat(bs->msg, buf);
      }
      bs->msglen = strlen(bs->msg);
      bs->send();
      bs->close();
      collector->setspooled(BCOLLECT_SPOOL_NO);
   } else {
      /* error connecting */
      berrno be;
      collector->lock();
      /* TODO: I'm using the errno set by bsockcore, but it return "Interrupted system call"
       * instead of a proper error number. We have to consider a better solution. */
      Mmsg(collector->errmsg, "Could not connect to %s:%d Err=%s", collector->host, collector->port, be.bstrerror());
      collector->unlock();
      /* spool data if requested */
      if (collector->spool_directory){
         /* spooling only when spool_directory defined */
         Mmsg(sname, SPOOLFNAME, collector->spool_directory, collector->daemon, collector->hdr.name);
         fd = open(sname.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0640);
         if (fd > 0){
            /* open success */
            Dmsg2(500, "%s spooling metrics to: %s\n", collector->hdr.name, sname.c_str());
            foreach_alist(item, list){
               render_metric_graphite(collector, buf, item, collector->timestamp);
               len = strlen(buf.c_str());
               if (write(fd, buf.c_str(), len) != len){
                  berrno be;
                  Emsg2(M_ERROR, 0, "Error saving spool file: %s Err=%s\n", collector->file, be.bstrerror());
                  Emsg1(M_ERROR, 0, "Statistics spooling for %s disabled.\n", collector->hdr.name);
                  collector->lock();
                  collector->spool_directory = NULL;
                  Mmsg(collector->errmsg, "Error saving spool file: %s Err=%s", collector->file, be.bstrerror());
                  collector->unlock();
                  close(fd);
                  delete(bs);
                  return true;
               }
            }
            close(fd);
            collector->setspooled(BCOLLECT_SPOOL_YES);
         } else {
            /* error */
            berrno be;
            Emsg2(M_ERROR, 0, "Error opening collector spool file: %s Err=%s\n", sname.c_str(), be.bstrerror());
            Emsg1(M_ERROR, 0, "Statistics spooling for %s disabled.\n", collector->hdr.name);
            collector->lock();
            collector->spool_directory = NULL;
            Mmsg(collector->errmsg, "Error opening collector spool file: %s Err=%s", sname.c_str(), be.bstrerror());
            collector->unlock();
         }
      }
   }

   delete (bs);
   return true;
};

/*
 * The main Statistics backend thread function.
 */
extern "C"
void *collector_thread(void *arg)
{
   COLLECTOR *collector;
   alist *data = NULL;
   bstatmetric *item;
   char *filter;
   char *fltm;
   alist *filtered = NULL;
   bool oper, toappend, prevmatch;
   int match;
   bool status = true;

   collector = (COLLECTOR*)arg;
   collector->lock();
   collector->valid = true;
   collector->running = true;
   collector->spooled = BCOLLECT_SPOOL_UNK;    /* when thread start we do not know if spooled */
   switch (collector->type){
      case COLLECTOR_BACKEND_CSV:
         /* no spooling for CSV collector */
         collector->spool_directory = NULL;
         break;
   }
   collector->errmsg = get_pool_memory(PM_MESSAGE);
   collector->errmsg[0] = 0;
   collector->unlock();

   while (status){
      collector->lock();
      if (!collector->valid){
         /* exit thread */
         collector->unlock();
         Dmsg1(100, "Statistics \"%s\" exited on request.\n", collector->name());
         goto cleanup;
      }
      collector->unlock();
      /* grab data from bstatcollect */
      data = collector->statcollector->get_all();
      collector->updatetimestamp();
      if (data){
         /* we have some data to proceed */
         if (collector->metrics){
            /* have some metrics to filter */
            filtered = New(alist(100, not_owned_by_alist));
            /* iterate trough all metrics to filter it out */
            foreach_alist(item, data){
               Dmsg1(1500, "processing: %s\n", item->name);
               toappend = true;
               prevmatch = false;
               foreach_alist(filter, collector->metrics){
                  fltm = filter;
                  oper = false;           // add filtered metric
                  if (filter[0] == '!'){
                     fltm = filter + 1;
                     oper = true;         // remove filtered metric
                  }
                  match = fnmatch(fltm, item->name, 0);
                  /* now we have to decide if metric should be filtered or not */
                  toappend = (!oper && match == 0) || (match !=0 && prevmatch);
                  prevmatch = match == 0;
               }
               if (toappend){
                  /* found */
                  Dmsg0(1500, "metric append\n");
                  filtered->append(item);
               }
            }
         } else {
            filtered = data;
         }
         Dmsg1(1000, "collected metrics: %d\n", filtered->size());
         /* save data to destination */
         switch (collector->type){
            case COLLECTOR_BACKEND_CSV:
               status = save_metrics2csv(collector, filtered);
               break;
            case COLLECTOR_BACKEND_Graphite:
               status = save_metrics2graphite(collector, filtered);
               break;
         }
         /* free data */
         if (filtered != data){
            delete(filtered);
         }
         free_metric_alist(data);
         data = NULL;
         filtered = NULL;
      }
      if (status){
         /* wait $Interval seconds*/
         Dmsg1(2000, "collector sleep (%d secs)\n", collector->interval);
         bmicrosleep(collector->interval, 0);
      }
   }

   Dmsg1(100, "Statistics \"%s\" exited.\n", collector->name());
cleanup:
   collector->lock();
   collector->running = false;
   free_jcr(collector->jcr);
   collector->unlock();
   return NULL;
};

/*
 * The main Statistics update thread function.
 */
extern "C"
void *updatecollector_thread(void *arg)
{
   bool status = true;

   updcollector.lock();
   if (updcollector.routine == NULL || updcollector.jcr == NULL || updcollector.interval == 0){
      /* cannot execute thread when uninitialized */
      updcollector.unlock();
      Dmsg0(100, "Update Statistics uninitialized!\n");
      return NULL;
   }
   updcollector.running = true;
   updcollector.valid = true;
   updcollector.unlock();
   /* main thread loop */
   while (status){
      updcollector.lock();
      if (!updcollector.valid){
         /* exit thread */
         updcollector.unlock();
         Dmsg0(100, "Update Statistics exited on request.\n");
         goto updcleanup;
      }
      updcollector.lastupdate = time(NULL);
      updcollector.unlock();
      status = updcollector.routine(updcollector.data);
      if (status){
         /* wait $Interval seconds*/
         Dmsg1(2000, "updcollector sleep (%d secs)\n", updcollector.interval);
         bmicrosleep(updcollector.interval, 0);
      }
   }
   Dmsg0(100, "Update Statistics exited.\n");
updcleanup:
   updcollector.lock();
   updcollector.running = false;
   updcollector.interval = 0;
   free_jcr(updcollector.jcr);
   updcollector.unlock();
   return NULL;
};

/*
 * Starts a single collector backend thread.
 *  If collector thread start will be unsuccessful then it will abort Bacula.
 *
 * in:
 *    collector - a collector resource used for thread start and initialization
 */
void start_collector_thread(COLLECTOR *collector)
{
   int status;

   Dmsg1(100, "start_collector_thread: %p\n", collector);
   /* initialize collector res for thread */
   pthread_mutex_init(&collector->mutex, NULL);

   /* create thread */
   if ((status = pthread_create(&collector->thid, NULL, collector_thread, (void *)collector)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Cannot create Statistics thread: %s\n"), be.bstrerror(status));
   }
   return;
};

/*
 * Stops a collector backend thread.
 */
void stop_collector_thread(COLLECTOR *collector)
{
   char *elt;

   Dmsg2(500, "valid=%d metrics=%p\n", collector->valid, collector->metrics);
   if (collector->metrics) {
      foreach_alist(elt, collector->metrics) {
         Dmsg1(100, "str=%s\n", elt);
      }
   }
   collector->lock();
   collector->valid = false;
   pthread_kill(collector->thid, SIGUSR2);
   collector->unlock();
   /* wait for thread end */
   pthread_join(collector->thid, NULL);
};

/*
 * Starts a single update collector thread.
 *  If collector thread start will be unsuccessful then it will abort Bacula.
 *
 * in:
 *    initdata - the update collector initialization data
 */
void start_updcollector_thread(UPDATE_COLLECTOR_INIT_t &initdata)
{
   int status;

   Dmsg0(100, "start_updcollector_thread\n");
   updcollector.interval = initdata.interval;
   updcollector.routine = initdata.routine;
   updcollector.data = initdata.data;
   updcollector.jcr = initdata.jcr;
   /* create thread */
   if ((status = pthread_create(&updcollector.thid, NULL, &updatecollector_thread, NULL)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Cannot create Update Statistics thread: %s\n"), be.bstrerror(status));
   }
   return;
};

/*
 * Stops a collector update thread.
 */
void stop_updcollector_thread()
{
   updcollector.lock();
   updcollector.valid = false;
   pthread_kill(updcollector.thid, SIGUSR2);
   updcollector.unlock();
   /* wait for thread end */
   pthread_join(updcollector.thid, NULL);
};

/*
 * A support function used in resource's show/dump functions.
 */
void dump_collector_resource(COLLECTOR &res_collector, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   char *metric;

   sendit(sock, _("Statistics: name=%s\n"), res_collector.hdr.name);
   sendit(sock, _("            type=%d interval=%ld secs\n"), res_collector.type, res_collector.interval);
   sendit(sock, _("            prefix=%s\n"), res_collector.prefix ? res_collector.prefix : "");
   switch (res_collector.type){
      case COLLECTOR_BACKEND_CSV:
         sendit(sock, _("            file=%s\n"), res_collector.file ? res_collector.file : "<empty>");
         break;
      case COLLECTOR_BACKEND_Graphite:
         sendit(sock, _("            host=%s port=%d\n"),
               res_collector.host ? res_collector.host : "localhost",
               res_collector.port);
         break;
   }
   if (res_collector.metrics){
      foreach_alist(metric, res_collector.metrics){
         sendit(sock, _("            metric=%s\n"), metric);
      };
   }
};

/*
 * A support function frees a COLLECTOR class resources.
 */
void free_collector_resource(COLLECTOR &res_collector)
{
   if (res_collector.file){
      free(res_collector.file);
   }
   if (res_collector.host){
      free(res_collector.host);
   }
   if (res_collector.prefix){
      free(res_collector.prefix);
   }
   if (res_collector.errmsg){
      free_pool_memory(res_collector.errmsg);
   }
   if (res_collector.metrics){
      delete res_collector.metrics;
   }
   pthread_mutex_destroy(&res_collector.mutex);
};

/*
 * UPDATECOLLECTOR class lock.
 */
void UPDATECOLLECTOR::lock()
{
   pthread_mutex_lock(&mutex);
};

/*
 * UPDATECOLLECTOR class unlock.
 */
void UPDATECOLLECTOR::unlock()
{
   pthread_mutex_unlock(&mutex);
};

/*
 * UPDATECOLLECTOR constructor.
 */
UPDATECOLLECTOR::UPDATECOLLECTOR()
{
   running = false;
   valid = false;
   routine = NULL;
   data = NULL;
   jcr = NULL;
   interval = 0;
   lastupdate = 0;
   memset(&thid, 0, sizeof(thid));
   pthread_mutex_init(&mutex, NULL);
};

/*
 * UPDATECOLLECTOR destructor.
 */
UPDATECOLLECTOR::~UPDATECOLLECTOR()
{
   pthread_mutex_destroy(&mutex);
};

const char *str_updcollector_status()
{
   const char* status;

   if (updcollector.valid){
      status = updcollector.running?"running":"stopped";
   } else {
      status = updcollector.running?"waiting to exit":"stopped";
   }
   return status;
};

/*
 * A support function renders an Update Statistics thread status into a buffer.
 *
 * in:
 *    buf - a POLL_MEM buffer to render into
 * out:
 *    the length of rendered text
 */
int render_updcollector_status(POOL_MEM &buf)
{
   int len;
   char dt[MAX_TIME_LENGTH];
   const char *status;
   utime_t t;
   utime_t i;

   updcollector.lock();
   status = str_updcollector_status();
   t = updcollector.lastupdate;
   i = updcollector.interval;
   updcollector.unlock();
   bstrftime_nc(dt, sizeof(dt), t);
   len = Mmsg(buf, "Update Statistics: %s interval=%d secs lastupdate=%s\n\n",
         status, i, dt);
   return len;
};

/*
 * A support function renders an Update Statistics thread status into an OutputWriter for APIv2.
 *
 * in:
 *    ow - OutputWriter for apiv2
 * out:
 *    rendered status in OutputWritter buffer
 */
void api_render_updcollector_status(OutputWriter &ow)
{
   const char *status;
   time_t t;
   utime_t i;

   updcollector.lock();
   status = str_updcollector_status();
   t = updcollector.lastupdate;
   i = updcollector.interval;
   updcollector.unlock();
   ow.get_output(
         OT_START_OBJ,
         OT_STRING,  "status",         status,
         OT_INT,     "interval",       i,
         OT_UTIME,   "lasttimestamp",  t,
         OT_END_OBJ,
         OT_END
   );
   return;
};
