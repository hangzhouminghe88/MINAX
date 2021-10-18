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

#ifndef BSOCK_MEETING_H
#define BSOCK_MEETING_H

/*
 * Written by Eric Bollengier MMXVIII
 */

/*
 * The BsockMeeting class is reponsible to create a meeting 
 * point between a client and a server.
 *
 * It can be used for example for Client that are behind a
 * firewall and cannot be contacted by the Director.
 *
 * The Client can initiate the connection, it will be stored inside a
 * BsockMeeting object. The Director can query the BsockMeeting object at any
 * time and will wait to get the BSOCK object, or get the current one if any.
 *
 * The TCP keepalive settings are modified to maintain the connection alive and
 * detect problems. The TCP settings are restored automatically.
 */
class BsockMeeting: public SMARTALLOC
{
private:
   BSOCK *socket;               /* Socket stored, NULL if empty */
   int keepidle;                /* original keepidle setting */
   int keepintvl;               /* original keepintvl setting */

   /* Initialized in the construction body */
   pthread_mutex_t mutex;
   pthread_cond_t  cond;

public:
   BsockMeeting();
   ~BsockMeeting();

   void set(BSOCK *s);         /* Assign a BSOCK object */
   bool is_set(POOLMEM *&host);/* Query if we have a BSOCK, and from who */
   BSOCK *get(int timeout);   /* Get the current BSOCK Object, wait if needed */
   void wait_request(BSOCK *dir); /* Wait for a Director request (client side) */
};

#endif
