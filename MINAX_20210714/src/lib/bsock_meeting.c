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
 * Written by Eric Bollengier MMXVIII
 */

/* Code to handle Bacula Client behind a NAT without a VPN 
 * We keep a connection idle with a short TCP keepalive to detect
 * any problem very quickly.
 *
 * A thread can be responsible to listen all sockets and reply to
 * a ISALIVE request. A ISALIVE request can be send after a wake up
 * for example.
 */

#include "bacula.h"
#include "bsock_meeting.h"

static int dbglvl = DT_NETWORK|50;

BsockMeeting::BsockMeeting(): socket(NULL), keepidle(0), keepintvl(0) {
   pthread_mutex_init(&mutex, NULL);
   pthread_cond_init(&cond, NULL);
};

BsockMeeting::~BsockMeeting() {
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&cond);
   free_bsock(socket);
};

void BsockMeeting::set(BSOCK *s) {
   int turnon=1;
   P(mutex);
   free_bsock(socket);          /* We are the owner of the socket object, we can use free */
   socket = s;

   /*
    * Keep socket from timing out from inactivity
    */
   if (setsockopt(socket->m_fd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&turnon, sizeof(turnon)) < 0) {
      berrno be;
      Dmsg1(dbglvl, _("Cannot set SO_KEEPALIVE on socket: %s\n"), be.bstrerror());
   }
#if defined(TCP_KEEPIDLE)
   if (getsockopt(socket->m_fd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&keepidle, sizeof(keepidle)) < 0) {
      berrno be;
      Dmsg1(dbglvl, _("Cannot get TCP_KEEPIDLE on socket: %s\n"), be.bstrerror());
   }
   opt = 10000;
   if (setsockopt(socket->m_fd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&opt, sizeof(opt)) < 0) {
      berrno be;
      Dmsg1(dbglvl, _("Cannot set TCP_KEEPIDLE on socket: %s\n"), be.bstrerror());
   }
#endif
#if defined(TCP_KEEPINTVL)
   if (getsockopt(socket->m_fd, SOL_TCP, TCP_KEEPINTVL, (sockopt_val_t)&keepintvl, sizeof(keepintvl)) < 0) {
      berrno be;
      Dmsg1(dbglvl, _("Cannot get TCP_KEEPINTVL on socket: %s\n"), be.bstrerror());
   }
   opt = 5000;
   if (setsockopt(socket->m_fd, SOL_TCP, TCP_KEEPINTVL, (sockopt_val_t)&opt, sizeof(opt)) < 0) {
      berrno be;
      Dmsg1(dbglvl, _("Cannot set TCP_KEEPINTVL on socket: %s\n"), be.bstrerror());
   }
#endif
   pthread_cond_signal(&cond);
   V(mutex);
};

/* Get the current Socket, wait timeout to get it */
BSOCK *BsockMeeting::get(int timeout) {
   BSOCK *ret=NULL;
   struct timespec to;
   btimer_t *t;
   int32_t sig=0;

   P(mutex);
   to.tv_sec = time(NULL) + timeout;
   to.tv_nsec = 0;
   while (socket == NULL) {
      Dmsg0(dbglvl, "socket is null...\n");
      int err = pthread_cond_timedwait(&cond, &mutex, &to);
      if (err == ETIMEDOUT) {
         Dmsg0(dbglvl, "Timeout\n");
         /* timeout, do something */
         break;
      } else {
         Dmsg2(dbglvl, "timedwait=%d socket=%p\n", err, socket);
      }
   }
   /* TODO: We need to test if the socket is still valid, and we need a read() to raise
    * an error. We don't want to give a broken socket to the job
    */
   if (socket) {
      Dmsg0(dbglvl, "Found a socket in the proxy\n");
      t = start_bsock_timer(socket, 10);
      socket->signal(BNET_ISALIVE);
      sig = socket->recv();
      stop_bsock_timer(t);

      if (sig != -1 || socket->msglen != BNET_ISALIVE) {
         Dmsg2(dbglvl, "Socket seems broken sig=%d msglen=%d\n", sig, socket->msglen);
         free_bsock(socket);
         V(mutex);           /* Let's get a new one */
         return get(timeout);
      }
      Dmsg0(dbglvl, "Socket seems OK\n");
   }
   if (socket) {
#if defined(TCP_KEEPIDLE)
      if (setsockopt(socket->m_fd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&keepidle, sizeof(keepidle)) < 0) {
         berrno be;
         Dmsg1(dbglvl, _("Cannot set TCP_KEEPIDLE on socket: %s\n"), be.bstrerror());
      }
#endif
#if defined(TCP_KEEPINTVL)
      if (setsockopt(socket->m_fd, SOL_TCP, TCP_KEEPINTVL, (sockopt_val_t)&keepintvl, sizeof(keepintvl)) < 0) {
         berrno be;
         Dmsg1(dbglvl, _("Cannot set TCP_KEEPINTVL on socket: %s\n"), be.bstrerror());
      }
#endif
   }
   ret = socket;
   socket = NULL;
   V(mutex);          
   return ret;
};

bool BsockMeeting::is_set(POOLMEM *&address)
{
   lock_guard g(mutex);
   if (address) {
      *address = 0;
   }
   if (!socket) {
      return false;
   }
   if (address) {
      pm_strcpy(address, socket->host());
   }
   return true;
}

void BsockMeeting::wait_request(BSOCK *dir) {
   int32_t sig = dir->recv();
   if (sig == -1 && dir->msglen == BNET_ISALIVE) {
      /* nop */
      dir->signal(BNET_ISALIVE);
   } else {
      Dmsg1(dbglvl, "got incorrect message sig=%d\n", sig);
      dir->close();
   }
};
