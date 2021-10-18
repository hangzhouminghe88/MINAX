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
#include "lib/unittests.h"

/* Function that reproduce what the director is supposed to do
 *  - Accept the connection from "filedaemon"
 *  - Accept the command "setip"
 *  - Store the socket in the BsockMeeting structure in a global list
 *
 *  - Accept the connection from "bconsole"
 *  - Do a command that connects to the client
 *  - Get the socket from BsockMeeting list
 *  - do some discussion
 */

void *start_heap;
int port=2000;
int nb_job=10;
int done=nb_job;
int started=0;
int connected=0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int nb_send=10;
char *remote = (char *)"localhost";
bool quit=false;
ilist clients(1000, not_owned_by_alist);

BsockMeeting *get_client(const char *name)
{
   lock_guard m(mutex);
   BsockMeeting *b;
   int id=0;
   if (sscanf(name, "client-%d", &id) != 1) {
      return NULL;
   }
   b = (BsockMeeting *)clients.get(id);
   if (!b) {
      b = New(BsockMeeting());
      clients.put(id, b);
   }
   return b;
}

void set_client(const char *name, BsockMeeting *b)
{
   lock_guard m(mutex);
   int id=0;
   if (sscanf(name, "client-%d", &id) != 1) {
      return;
   }
   clients.put(id, b);
}

void client_close()
{
   BsockMeeting *b;
   int last = clients.last_index();
   for (int i = 0; i <= last ; i++) {
      b = (BsockMeeting *)clients.get(i);
      if (b) {
         delete b;
         clients.put(i, NULL);
      }
   }
   clients.destroy();
}

/* When a client connects */
static void *handle_client_request(void *fdp)
{
   BsockMeeting *proxy;
   BSOCK *fd = (BSOCK *)fdp;
   if (fd->recv() > 0) {
      char *name = fd->msg;
      Pmsg1(0, "Got connection from %s\n", name);
      proxy = get_client(name);
      fd->fsend("OK\n");
      proxy->set(fd);
   } else {
      free_bsock(fd);
      return NULL;
   }

   P(mutex);
   connected++;
   V(mutex);
   return NULL;
}

static void *th_server(void *)
{
   static workq_t dir_workq;             /* queue of work from Director */
   IPADDR *tmp = 0;
   dlist *lst;
   lst = New(dlist(tmp, &tmp->link));
   
   init_default_addresses(&lst, port);
   bnet_thread_server(lst, 100, &dir_workq, handle_client_request);
   delete lst;
   return NULL;
}


#ifdef HAVE_WIN32
#define WD "c:\\program files\\bacula\\working\\"
#else
#define WD "/tmp/"
#endif

/* Simulate a console and do some action */
void *th_console(void *arg)
{
   bool quit;
   int64_t total=0;
   char ed1[50], ed2[50];
   int32_t sig;
   btime_t timer, end_timer, elapsed;

   BSOCK *dir=NULL;
   char *name = (char *) arg;
   BsockMeeting *proxy = get_client(name);
   if (proxy == NULL) {
      Pmsg1(0, "Unable to find %s\n", name);
      goto bail_out;
   }
   Pmsg0(0, "Can go in sleep mode. Remove " WD "pause to continue\n");
   {
      struct stat sp;
      fclose(fopen(WD "pause", "w"));
      while (stat(WD "pause", &sp) == 0) {
         bmicrosleep(1, 0);
      }
   }
   dir = proxy->get(30);
   if (!dir) {
      Pmsg1(0, "Unable to get socket %s\n", name);
      goto bail_out;
   }

   Pmsg0(0, "send command\n");
   dir->fsend("command\n");
   timer = get_current_btime();

   for (quit=false; dir && !quit;) {
      /* Read command */
      sig = dir->recv();
      if (sig < 0) {
         Pmsg0(0, "Connection terminated\n");
         break;               /* connection terminated */
      } else if (!strncmp(dir->msg, "quit", 4)) {
         Pmsg0(0, "got quit...\n");
         dir->fsend("quit\n");
         break;
      } else {
         total += dir->msglen;
      }
   }
   end_timer = get_current_btime();
   elapsed = (end_timer - timer)/1000; /* 0.001s */

   if (elapsed > 0) {
      printf("got bytes=%sB in %.2fs speed=%sB/s\n", edit_uint64_with_suffix(total, ed1), 
             (float) elapsed/1000, edit_uint64_with_suffix(total/elapsed*1000, ed2));
   }

bail_out:
   free_bsock(dir);
   free(name);
   return NULL;
}

/* Simulate a filedaemon */
void *th_filedaemon(void *arg)
{
   BSOCK *sock = NULL;
   char name[512];
   BsockMeeting proxy;
   bstrncpy(name, (char *)arg, sizeof(name));
   free(arg);
   P(mutex);
   started++;
   V(mutex);

   /* The FD will use a loop to connect */
connect_again:
   free_bsock(sock);
   sock = new_bsock();
   if (!sock->connect(NULL, 5, 10, 2000, (char *)"*name*", remote, (char *)"*service*", port, 0)) {
      bmicrosleep(1, 0);
      goto connect_again;
   }
   Pmsg0(0, ">Connected!\n");

   /* Do "authentication" */
   sock->fsend("%s", name);
   if (sock->recv() <= 0 || strcmp(sock->msg, "OK\n") != 0) {
      free_bsock(sock);
      return NULL;
   }

   /* Read command and wait to be used */
   proxy.wait_request(sock);

   if (sock->is_closed()) {
      goto connect_again;
   }

   /* get a command */
   if (sock->recv() <= 0) {
      Pmsg0(0, "got incorrect message. Expecting command\n");
      goto connect_again;
   }
   
   /* Do something useful or not */
   sock->msg = check_pool_memory_size(sock->msg, 4100);

   Pmsg1(0, ">Ready to send %u buffers of 4KB\n", nb_send);
   for (int i = 0; i < nb_send ; i++) {
      memset(sock->msg, i, 4096);
      sock->msglen = 4096;
      sock->msg[sock->msglen] = 0;
      sock->send();
   }
   Pmsg1(0, ">Send quit command\n", nb_send);
   sock->fsend("quit\n");
   sock->recv();
   sock->close();
   P(mutex);
   done--;
   V(mutex);
//   goto connect_again;

   free_bsock(sock);
   Pmsg4(0, ">done=%u started=%u connected=%u name=%s\n", done, started, connected, name);
   return NULL;
}

int main (int argc, char *argv[])
{
   char ch;
   int olddone=0;
   bool server=false;
   pthread_t server_id, client_id[1000], console_id[1000];
   Unittests t("BsockMeeting", true, true);
   InitWinAPIWrapper();
   WSA_Init();
   start_heap = sbrk(0);
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   set_working_directory((char *)WD);
   set_thread_concurrency(150);
   set_trace(0);

   while ((ch = getopt(argc, argv, "?n:j:r:p:sd:")) != -1) {
      switch (ch) {
      case 'j':
         done = nb_job = MIN(atoi(optarg), 1000);
         break;

      case 'n':
         nb_send = atoi(optarg);
         break;

      case 'r':
         remote = optarg;
         break;

      case 'p':
         port = atoi(optarg);
         break;

      case 's':
         server = true;
         break;

      case 'd':
         debug_level = atoi(optarg);
         break;

      case '?':
      default:
         Pmsg0(0, "Usage: bsock_meeting_test [-r remote] [-s] [-p port] [-n nb_send] [-j nb_job]\n");
         return 0;
      }
   }
   argc -= optind;
   argv += optind;

   if (strcmp(remote, "localhost") == 0) {
      Pmsg1(0, "Start server on port %d\n", port);
      pthread_create(&server_id, NULL, th_server, NULL);

      Pmsg0(0, ">starting fake console\n");
      for (int i = nb_job - 1; i >= 0 ; i--) {
         char *tmp = (char *) malloc (32);
         snprintf(tmp, 32, "client-%d", i);
         pthread_create(&console_id[i], NULL, th_console, tmp);
      }
   }

   if (!server) {
      Pmsg0(0, ">starting fake clients\n");
      for (int i = 0; i < nb_job ; i++) {
         char *tmp = (char *) malloc (32);
         snprintf(tmp, 32, "client-%d", i);
         pthread_create(&client_id[i], NULL, th_filedaemon, tmp);
      }
      
      while (done>=1) {
         if (done != olddone) {
            Pmsg3(0, ">done=%u started=%u connected=%u\n", done, started, connected);
            olddone = done;
         }
         sleep(1);
      }
      quit=true;
      sleep(1);
   }

   if (strcmp(remote, "localhost") == 0) {
      Pmsg0(0, "Stop bnet server...\n");
      bnet_stop_thread_server(server_id);
      pthread_join(server_id, NULL);
      Pmsg0(0, "done.\n");
      Pmsg0(0, "Join console threads...\n");
      for (int i = 0; i < nb_job ; i++) {
         pthread_join(console_id[i], NULL);
      }
      Pmsg0(0, "done.\n");
   }

   if (!server) {
#ifdef pthread_kill
#undef pthread_kill
#endif
      for (int i = 0; i < nb_job ; i++) {
         pthread_kill(client_id[i], SIGUSR2);
      }
      Pmsg0(0, "Join client threads...\n");
      for (int i = 0; i < nb_job ; i++) {
         pthread_join(client_id[i], NULL);
      }
      Pmsg0(0, "done.\n");
   }
   client_close();
   term_last_jobs_list();
   dequeue_daemon_messages(NULL);
   dequeue_messages(NULL);
   term_msg();
   return 0;
}
