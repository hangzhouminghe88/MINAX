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

#ifndef ORG_FILED_DEDUP_H
#define ORG_FILED_DEDUP_H

#include "bacula.h"

class DedupFiledInterface: public SMARTALLOC, public BSOCKCallback
{
public:
   DedupFiledInterface(JCR *jcr, int, int) {};
   virtual ~DedupFiledInterface() {};

   void activate_flowcontrol_dedup() {};
   bool wait_flowcontrol_dedup(int free_rec_count, int timeoutms);
   bool disable_flowcontrol_dedup(BSOCK *sd) { return true; };
   void TransferJobBytes(uint64_t *JobBytes) {};
   int handle_command(BSOCK *sd) { return 0; };

   bool wait_quarantine() { return true;};
   void enter_heartbeat(BSOCK *sd) {};
   void leave_heartbeat(BSOCK *sd) {};
   int wait_for_heartbeat_running(int usec) { return false; };

   virtual bool bsock_send_cb() { return true;}; // inherited from BSOCKCallback
};

 #endif /* ORG_FILED_DEDUP_H */
