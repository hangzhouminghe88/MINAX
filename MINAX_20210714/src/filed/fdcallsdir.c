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

static bool quit=false;         /* Global "quit" */
static bool started=false;
static bool sched_server_started=false;
static alist *servers=NULL;
static workq_t client_wq;

static bool schedules_invalidated = false;

/* Local variables */
struct call_item {
   RUNRES *run;
   DIRRES *dir;
   time_t runtime;
   dlink link;                        /* link for list */
};

static int dbglvl = DT_SCHEDULER;

/* List of jobs to be run. They were scheduled in this hour or the next */
static dlist *calls_to_do=NULL;               /* list of calls to be run */

class fdcallsdir_arg_t: public SMARTALLOC
{
public:
   DIRRES *dir;               /* Director resource (connect info) */
   workq_t *client_wq;
   pthread_t server_id;       /* pthread identifier for the current request */
   uint32_t  duration;        /* How many time we keep the connection active */
   bool      do_quit;
   fdcallsdir_arg_t(DIRRES *d, workq_t *wq): dir(d), client_wq(wq), duration(0), do_quit(false) {};
   ~fdcallsdir_arg_t() {
      do_quit=true;
      pthread_kill(server_id, SIGUSR2); /* Stop any read()/select() */
      pthread_join(server_id, NULL);
   };
};

/* Forward references */
static void *fdcallsdir_start_one_server(void *a);
static fdcallsdir_arg_t *wait_for_next_call();
static pthread_t sched_server_id;
static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

/* We allow to use a scheduler to do connections */

/* This is the thread handler to start the scheduler */
static void *fdcallsdir_start_sched_server(void *a)
{
   (void)a;
   fdcallsdir_arg_t *arg;
   call_item *next_call=NULL;
   calls_to_do = New(dlist(next_call, &next_call->link));
   while (!quit) {
      arg = wait_for_next_call();
      /* TODO: A bad schedule can disconnect/connect to the director */
      if (arg) {
         Dmsg0(dbglvl, "Got a call to do!\n");
         pthread_create(&arg->server_id, NULL, fdcallsdir_start_one_server, arg);
         P(server_mutex);
         servers->append(arg);
         V(server_mutex);
      }
   }
   delete calls_to_do;
   calls_to_do = NULL;
   return NULL;
}

/* This is the function called to start the scheduler thread */
static void fdcallsdir_start_sched_server()
{
   if (!sched_server_started) {
      Dmsg0(dbglvl, "Start the fdcallsdir scheduler thread\n");
      pthread_create(&sched_server_id, NULL, fdcallsdir_start_sched_server, NULL);
      sched_server_started = true;
   }
}

/* Stop the scheduler thread */
static void fdcallsdir_stop_sched_server()
{
   if (sched_server_started) {
      Dmsg0(dbglvl, "Stop the fdcallsdir scheduler thread\n");
      pthread_kill(sched_server_id, SIGUSR2); /* Stop any read()/select() */
      pthread_join(sched_server_id, NULL);
      sched_server_started = false;
   }
}

/* Stop all threads that are trying to connect the DIR and the scheduler thread if any */
void fdcallsdir_stop_server()
{
   int stat;
   quit=true;
   if (started) {
      Dmsg0(dbglvl, "Stop the fdcallsdir server\n");
      fdcallsdir_stop_sched_server();

      fdcallsdir_arg_t *arg;
      foreach_alist(arg, servers) {
         delete arg;
      }

      P(server_mutex);
      delete servers;
      servers = NULL;
      V(server_mutex);

      /* Stop work queue thread */
      if ((stat = workq_destroy(&client_wq)) != 0) {
         berrno be;
         be.set_errno(stat);
         Jmsg1(NULL, M_FATAL, 0, _("Could not destroy client queue: ERR=%s\n"),
               be.bstrerror());
      }
   }
   started=false;
}

void fdcallsdir_start_server(int max_clients, void *handle_client_request(void *bsock))
{
   DIRRES *dir;
   fdcallsdir_arg_t *arg;
   int stat;
   Dmsg0(dbglvl, "Start the fdcallsdir server\n");

   servers = New(alist(5, not_owned_by_alist));
   /* Start work queue thread */
   if ((stat = workq_init(&client_wq, max_clients, handle_client_request)) != 0) {
      berrno be;
      be.set_errno(stat);
      Emsg1(M_ABORT, 0, _("Could not init client queue: ERR=%s\n"), be.bstrerror());
   }

   foreach_res(dir, R_DIRECTOR) {
      if (dir->connect_to_dir) {
         if (dir->schedule) {
            Dmsg1(dbglvl, "Starting fdcallsdir sched thread for %s\n", dir->hdr.name);
            fdcallsdir_start_sched_server();

         } else {
            Dmsg1(dbglvl, "Starting fdcallsdir thread for %s\n", dir->hdr.name);
            arg = New(fdcallsdir_arg_t(dir, &client_wq));
            pthread_create(&arg->server_id, NULL, fdcallsdir_start_one_server, arg);
            P(server_mutex);
            servers->append(arg);
            V(server_mutex);
         }
      } else {
         Dmsg1(dbglvl, "Director %s without fdcallsdir\n", dir->hdr.name);
      }
   }
   started=true;
}

static void *fdcallsdir_start_one_server(void *a)
{
   fdcallsdir_arg_t *arg = (fdcallsdir_arg_t*) a;
   bool OK=true, fdcalled = false;
   BSOCK *cons_bsock;
   int v, maxfd;
   fd_set fdset;
   struct timeval tv;
   btimer_t *t;
   btime_t connect_time, now;   
   JCR *jcr = new_fd_jcr();
   if (!jcr) {
      return NULL;
   }
   Dmsg1(dbglvl, "Start to call Dir %s\n", arg->dir->hdr.name);
   while (!quit && !arg->do_quit) {
      cons_bsock = connect_director(jcr, me->hdr.name, &arg->dir->dirinfo, CONNECT_FDCALLSDIR_MODE);
      if (!cons_bsock) {
         bmicrosleep(5, 0);
         continue;
      }
      t = start_bsock_timer(cons_bsock, 10);
#if 0
      if (cons_bsock->recv() <= 0) {
         Dmsg1(dbglvl, "Got signal... %d\n", cons_bsock->msglen);
         cons_bsock->close();
         free_bsock(cons_bsock);
         stop_bsock_timer(t);
         bmicrosleep(5, 0);
         continue;
      }
#endif
      stop_bsock_timer(t);

      if (cons_bsock->recv() <= 0 || strcmp(cons_bsock->msg, "OK\n") != 0) {
         Dmsg1(dbglvl, "not OK from the director to the setip keep command %s\n",
               cons_bsock->msg);
         cons_bsock->close();
         free_bsock(cons_bsock);
         bmicrosleep(5, 0);
         continue;
      }

      /* Keep track of the connection time */
      connect_time = time(NULL);

      Dmsg0(dbglvl, "Got OK from the director to the setip keep command\n");
      maxfd = cons_bsock->m_fd + 1;

      /* Start to forward events from one to the other
       * It can be done with 2 threads, or with a select
       */
      do {
         FD_ZERO(&fdset);
         FD_SET((unsigned)cons_bsock->m_fd, &fdset);

         tv.tv_sec = 5;
         tv.tv_usec = 0;
         switch ((v = select(maxfd, &fdset, NULL, NULL, &tv))) {
         case 0:                      /* timeout */
            OK = !jcr->is_canceled() && !arg->do_quit;
            break;
         case -1:
            Dmsg1(dbglvl, "Bad call to select ERR=%d\n", errno);
            OK = false;
            break;
         default:
#ifdef HAVE_TLS
            if (cons_bsock->tls && !tls_bsock_probe(cons_bsock)) {
               /* maybe a session key negotiation waked up the socket */
               FD_CLR(cons_bsock->m_fd, &fdset);
            }
#endif
            OK = !jcr->is_canceled() && !arg->do_quit;
            break;
         }
         Dmsg2(dbglvl, "select = %d OK=%d\n", v, OK);
         if (OK) {
            if (FD_ISSET(cons_bsock->m_fd, &fdset)) {
               v = cons_bsock->recv();
               if (v == BNET_SIGNAL) {
                  if (cons_bsock->msglen == BNET_ISALIVE) {
                     cons_bsock->signal(BNET_ISALIVE);
                     OK = false;
                     fdcalled = true;
                     if (cons_bsock->tls) {
                        cons_bsock->free_tls();
                     }
                  }

               } else {
                  /* We should not have such kind of message */
                  OK = false;
               }
            }
         }
         if (cons_bsock->is_error()) {
            OK = false;
         }
         now = time(NULL);
         if (arg->duration) {
            if ((connect_time + arg->duration) < now) {
               Dmsg0(dbglvl, "Need to stop with this director\n");
               arg->do_quit = true; /* Stop this thread */
               OK = false;
            }
         }
         /* Reconnect after a given time */
         if ((connect_time + arg->dir->reconnection_time) < now) {
            Dmsg0(dbglvl, "Need to reconnect to the director\n");
            OK = false;
         }
      } while (OK && !jcr->is_canceled() && !quit && !arg->do_quit);

      if (fdcalled) {
         /* Queue client to be served */
         int stat;
         if ((stat = workq_add(arg->client_wq, (void *)cons_bsock, NULL, 0)) != 0) {
            berrno be;
            be.set_errno(stat);
            cons_bsock->destroy();
            Qmsg1(NULL, M_ABORT, 0, _("Could not add job to client queue: ERR=%s\n"),
                  be.bstrerror());
         }
         fdcalled = false;
      } else {
// TODO fix deadlock that happens on Android while trying to free this socket.
#ifndef __ANDROID_API__
         free_bsock(cons_bsock);
#endif
      }
   }
   free_jcr(jcr);
   Dmsg3(dbglvl, "Stop to call Dir %s (quit=%d do_quit=%d)\n",
         arg->dir->hdr.name, quit, arg->do_quit);
   return NULL;
}

/* Time interval in secs to sleep if nothing to be run */
static int const next_check_secs = 60;

static void add_call(DIRRES *dir, RUNRES *run, time_t now, time_t runtime)
{
   call_item *ci;
   bool inserted = false;
   Dmsg1(dbglvl, "Will call %s\n", dir->hdr.name);
   /*
    * Don't run any job that ran less than a minute ago, but
    *  do call any dir scheduled less than a minute ago.
    */
   if (((runtime - run->last_run) < 61) || ((runtime+59) < now)) {
      Dmsg3(dbglvl, "Too early for %s (runtime - last_run) = %d || ((runtime+59) < now %d)\n",
            dir->hdr.name,
            (int)(runtime - run->last_run),
            (int) (runtime+59) - (int)(now));
      return;
   }

   /* accept to run this job */
   call_item *ce = (call_item *)malloc(sizeof(call_item));
   ce->run = run;
   ce->dir = dir;
   ce->runtime = runtime;

   /* Add this job to the wait queue in runtime, priority sorted order */
   foreach_dlist(ci, calls_to_do) {
      if (ci->runtime > ce->runtime ||
          (ci->runtime == ce->runtime)) {
         calls_to_do->insert_before(ce, ci);
         inserted = true;
         break;
      }
   }
   /* If place not found in queue, append it */
   if (!inserted) {
      calls_to_do->append(ce);
   }
}

/*
 * Find all calls to be done this hour and the next hour.
 */
static void find_calls()
{
   time_t now, next_hour, runtime;
   RUNRES *run;
   DIRRES *dir;
   SCHEDRES *sched;
   struct tm tm;
   int hour, mday, wday, month, wom, woy, ldom;
   /* Items corresponding to above at the next hour */
   int nh_hour, nh_mday, nh_wday, nh_month, nh_wom, nh_woy, nh_ldom;

   Dmsg0(dbglvl, "enter find_runs()\n");

   /* compute values for time now */
   now = time(NULL);
   (void)localtime_r(&now, &tm);
   hour = tm.tm_hour;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wom = mday / 7;
   woy = tm_woy(now);                     /* get week of year */
   ldom = tm_ldom(month, tm.tm_year + 1900);

   Dmsg7(dbglvl, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d\n",
         now, hour, month, mday, wday, wom, woy);

   /*
    * Compute values for next hour from now.
    * We do this to be sure we don't miss a job while
    * sleeping.
    */
   next_hour = now + 3600;
   (void)localtime_r(&next_hour, &tm);
   nh_hour = tm.tm_hour;
   nh_mday = tm.tm_mday - 1;
   nh_wday = tm.tm_wday;
   nh_month = tm.tm_mon;
   nh_wom = nh_mday / 7;
   nh_woy = tm_woy(next_hour);              /* get week of year */
   nh_ldom = tm_ldom(nh_month, tm.tm_year + 1900);

   Dmsg7(dbglvl, "nh = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d\n",
         next_hour, nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy);

   /* Loop through all jobs */
   LockRes();
   foreach_res(dir, R_DIRECTOR) {
      sched = dir->schedule;
      if (!sched || !sched->is_enabled() || !dir->connect_to_dir) {
         continue;                    /* no, skip this call */
      }
      Dmsg1(dbglvl, "Got call: %s\n", dir->hdr.name);
      for (run=sched->run; run; run=run->next) {
         bool run_now, run_nh;
         /*
          * Find runs scheduled between now and the next hour.
          */
         run_now = bit_is_set(hour, run->hour) &&
            ((bit_is_set(mday, run->mday) &&
              bit_is_set(wday, run->wday) &&
              bit_is_set(month, run->month) &&
              bit_is_set(wom, run->wom) &&
              bit_is_set(woy, run->woy)) ||
             (bit_is_set(month, run->month) &&
              bit_is_set(31, run->mday) && mday == ldom));

         run_nh = bit_is_set(nh_hour, run->hour) &&
            ((bit_is_set(nh_mday, run->mday) &&
              bit_is_set(nh_wday, run->wday) &&
              bit_is_set(nh_month, run->month) &&
              bit_is_set(nh_wom, run->wom) &&
              bit_is_set(nh_woy, run->woy)) ||
             (bit_is_set(nh_month, run->month) &&
              bit_is_set(31, run->mday) && nh_mday == nh_ldom));

         Dmsg3(dbglvl, "run@%p: run_now=%d run_nh=%d\n", run, run_now, run_nh);

         if (run_now || run_nh) {
           /* find time (time_t) job is to be run */
           (void)localtime_r(&now, &tm);      /* reset tm structure */
           tm.tm_min = run->minute;     /* set run minute */
           tm.tm_sec = 0;               /* zero secs */
           runtime = mktime(&tm);
           if (run_now) {
             add_call(dir, run, now, runtime);
           }
           /* If job is to be run in the next hour schedule it */
           if (run_nh) {
             add_call(dir, run, now, runtime + 3600);
           }
         }
      }
   }
   UnlockRes();
   Dmsg0(dbglvl, "Leave find_runs()\n");
}

static fdcallsdir_arg_t *wait_for_next_call()
{
   fdcallsdir_arg_t *arg;
   RUNRES *run;
   time_t now, prev;
   call_item *next_call = NULL;

   /* Wait until we have something in the
    * next hour or so.
    */
again:
   while (calls_to_do->empty()) {
      find_calls();
      if (!calls_to_do->empty()) {
         Dmsg0(dbglvl, "calls_to_do is not empty\n");
         break;
      }
      bmicrosleep(next_check_secs, 0); /* recheck once per minute */
   }

   /*
    * Pull the first call to do (already sorted by runtime
    *  then wait around until it is time to run it.
    */
   next_call = (call_item *)calls_to_do->first();
   calls_to_do->remove(next_call);

   Dmsg1(dbglvl, "Found a director to connect %s\n", next_call->dir->hdr.name);
   
   if (!next_call) {                /* we really should have something now */
      Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
   }
   
   /* Now wait for the time to run the job */
   for (;;) {
      time_t twait;
      /** discard scheduled queue and rebuild with new schedule objects. **/
      if (schedules_invalidated) {
          free(next_call);
          while (!calls_to_do->empty()) {
             next_call = (call_item *)calls_to_do->first();
             calls_to_do->remove(next_call);
             free(next_call);
          }
          schedules_invalidated = false;
          goto again;
      }
      prev = now = time(NULL);
      twait = next_call->runtime - now;
      if (twait <= 0) {               /* time to run it */
         break;
      }
      /* Recheck at least once per minute */
      bmicrosleep((next_check_secs < twait)?next_check_secs:twait, 0);
      /* Attempt to handle clock shift (but not daylight savings time changes)
       * we allow a skew of 10 seconds before invalidating everything.
       */
      now = time(NULL);
      if (now < prev-10 || now > (prev+next_check_secs+10)) {
         schedules_invalidated = true;
      }
   }
   run = next_call->run;
   run->last_run = now;               /* mark as run now */
   Dmsg1(dbglvl, "set run->last_run=%d\n", (int) now);
   arg = New(fdcallsdir_arg_t(next_call->dir, &client_wq));
   arg->dir = next_call->dir;
   arg->duration = run->MaxConnectTime_set ? run->MaxConnectTime : 0;
   free(next_call);

   Dmsg0(dbglvl, "Leave wait_for_next_call()\n");
   return arg;
}
