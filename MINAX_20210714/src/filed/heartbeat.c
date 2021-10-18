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
 *  Bacula File Daemon heartbeat routines
 *    Listens for heartbeats coming from the SD
 *    If configured, sends heartbeats to Dir
 *
 *    Kern Sibbald, May MMIII
 *
 */

#include "bacula.h"
#include "filed.h"

#define WAIT_INTERVAL 5

extern "C" void *sd_heartbeat_thread(void *arg);
extern "C" void *dir_heartbeat_thread(void *arg);
extern bool no_signals;

int handle_command(JCR *jcr, BSOCK *sd);

/*
 * Listen on the SD socket for heartbeat signals.
 * Send heartbeats to the Director every HB_TIME
 *   seconds.
 */
extern "C" void *sd_heartbeat_thread(void *arg)
{
   int32_t n;
   int32_t m;
   JCR *jcr = (JCR *)arg;
   BSOCK *sd, *dir;
   time_t last_heartbeat = time(NULL);
   time_t now;

   set_jcr_in_tsd(jcr);
   pthread_detach(pthread_self());

   /* Get our own local copy */
   sd = dup_bsock(jcr->store_bsock);
   sd->uninstall_send_hook_cb(); // this thread send only command that are not going into the Queue
   dir = dup_bsock(jcr->dir_bsock);

   jcr->hb_bsock = sd;    /* We keep the socket reference up to the free_jcr */
   jcr->hb_started = true;
   jcr->hb_dir_bsock = dir; /* We keep the socket reference up to the free_jcr */
   dir->suppress_error_messages(true);
   sd->suppress_error_messages(true);

   jcr->dedup->enter_heartbeat(sd);
   /* Hang reading the socket to the SD, and every time we get
    *   a heartbeat or we get a wait timeout (5 seconds), we
    *   check to see if we need to send a heartbeat to the
    *   Director.
    */
   while (!sd->is_stop()) {
      n = sd->wait_data_intr(WAIT_INTERVAL);
      if (n < 0 || sd->is_stop()) {
         break;
      }
      if (me->heartbeat_interval) {
         now = time(NULL);
         if (now-last_heartbeat >= me->heartbeat_interval) {
            dir->signal(BNET_HEARTBEAT);
            if (dir->is_stop()) {
               break;
            }
            last_heartbeat = now;
         }
      }
      if (n == 1) {               /* input waiting */

         /* TODO: ASX Do I need to do a recv() or a bget_msg() ? */
         m = sd->recv();              /* read it -- probably heartbeat from sd */
         if (m == BNET_COMMAND) {
            int ret = jcr->dedup->handle_command(sd);
            if (ret<0) {
               /* A FATAL ERROR */
               break;
            }
            if (ret==1) {
               Dmsg0(0, "Command unknown\n");
            }
            continue;
         }
         if (sd->is_stop()) {
            break;
         }
         if (sd->msglen <= 0) {
            Dmsg2(100, "Got m=%d BNET_SIG %d from SD\n", m, sd->msglen);
         } else {
            Dmsg3(100, "Got m=%d msglen=%d bytes from SD. MSG=%s\n", m, sd->msglen, sd->msg);
         }
      }
      Dmsg2(200, "wait_intr=%d stop=%d\n", n, sd->is_stop());
   }
   /*
    * Note, since sd and dir are local dupped sockets, this
    *  is one place where we can call destroy().
    */
   jcr->dedup->leave_heartbeat(sd);
   sd->close();
   dir->close();
   jcr->hb_started = false;
   return NULL;
}

/* Startup the heartbeat thread -- see above */
void start_heartbeat_monitor(JCR *jcr)
{
   /*
    * If no signals are set, do not start the heartbeat because
    * it gives a constant stream of TIMEOUT_SIGNAL signals that
    * make debugging impossible.
    */
   if (!no_signals && (me->heartbeat_interval > 0)) {
      jcr->hb_bsock = NULL;
      jcr->hb_started = false;
      jcr->hb_dir_bsock = NULL;
      pthread_create(&jcr->heartbeat_id, NULL, sd_heartbeat_thread, (void *)jcr);
   }
}

/* Terminate the heartbeat thread. Used for both SD and DIR */
void stop_heartbeat_monitor(JCR *jcr)
{
   int cnt = 0;
   if (no_signals) {
      return;
   }
   /* Wait max 10 secs for heartbeat thread to start */
   while (!jcr->hb_started && cnt++ < 200) {
      bmicrosleep(0, 50000);         /* wait for start */
   }

   if (jcr->hb_started) {
      jcr->hb_bsock->set_timed_out();       /* set timed_out to terminate read */
      jcr->hb_bsock->set_terminated();      /* set to terminate read */
   }
   if (jcr->hb_dir_bsock) {
      jcr->hb_dir_bsock->set_timed_out();     /* set timed_out to terminate read */
      jcr->hb_dir_bsock->set_terminated();    /* set to terminate read */
   }
   cnt = 0;
   /* Wait max 100 secs for SD heartbeat thread to stop */
   while (jcr->hb_started && cnt++ < 200) {
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);  /* make heartbeat thread go away */
      if (cnt == 1) {  // be verbose and quick the first time
         Dmsg0(100, "Send kill to SD heartbeat thread\n");
      }
      bmicrosleep(0, (cnt == 1) ? 50000 : 500000);
   }
}

/*
 * Thread for sending heartbeats to the Director when there
 *   is no SD monitoring needed -- e.g. restore and verify Vol
 *   both do their own read() on the SD socket.
 */
extern "C" void *dir_heartbeat_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;
   BSOCK *dir;
   time_t last_heartbeat = time(NULL);

   pthread_detach(pthread_self());

   /* Get our own local copy */
   dir = dup_bsock(jcr->dir_bsock);

   jcr->hb_bsock = dir;
   jcr->hb_started = true;
   dir->suppress_error_messages(true);

   while (!dir->is_stop()) {
      time_t now;

      now = time(NULL);
      if ((now - last_heartbeat) >= me->heartbeat_interval) {
         dir->signal(BNET_HEARTBEAT);
         if (dir->is_stop()) {
            break;
         }
         last_heartbeat = now;
      }
      bmicrosleep(30, 0);
   }
   dir->close();
   jcr->hb_bsock = NULL;
   jcr->hb_started = false;
   return NULL;
}

/*
 * Same as above but we don't listen to the SD
 */
void start_dir_heartbeat(JCR *jcr)
{
   if (!no_signals && (me->heartbeat_interval > 0)) {
      jcr->dir_bsock->set_locking();
      pthread_create(&jcr->heartbeat_id, NULL, dir_heartbeat_thread, (void *)jcr);
   }
}

void stop_dir_heartbeat(JCR *jcr)
{
   if (me->heartbeat_interval > 0) {
      stop_heartbeat_monitor(jcr);
   }
}
