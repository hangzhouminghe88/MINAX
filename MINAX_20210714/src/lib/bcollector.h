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

#ifndef __BCOLLECTOR_H_
#define __BCOLLECTOR_H_

/* Supported backend types */
enum {
    COLLECTOR_BACKEND_Undef = 0,
    COLLECTOR_BACKEND_CSV,
    COLLECTOR_BACKEND_Graphite,
};

/* spooling status for supported backends */
enum {
    BCOLLECT_SPOOL_UNK,
    BCOLLECT_SPOOL_YES,
    BCOLLECT_SPOOL_NO,
    BCOLLECT_SPOOL_DESPOOL,
};

/* forward reference only */
class COLLECTOR;
class OutputWriter;

class UPDATECOLLECTOR: public SMARTALLOC {
public:
   utime_t interval;                   /* interval in seconds between metrics collection */
   utime_t lastupdate;                 /* when last update */
   pthread_t thid;                     /* thread id for collector thread */
   pthread_mutex_t mutex;              /* when accessing collector resource data you should lock it first */
   bool valid;                         /* when set to false the collector thread should involuntary exit */
   bool running;                       /* show if the background update thread is running */
   void *data;                         /* data parameter for thread routine */
   bool (*routine)(void *data);        /* routine for update collector thread */
   JCR *jcr;
public:
   /* Methods */
   char *name() const;
   void lock();
   void unlock();
   UPDATECOLLECTOR();
   ~UPDATECOLLECTOR();
};

typedef struct {
   JCR *jcr;
   utime_t interval;
   void *data;
   bool (*routine)(void *data);
} UPDATE_COLLECTOR_INIT_t;

void start_collector_thread(COLLECTOR *collector);
void stop_collector_thread(COLLECTOR *collector);
void start_updcollector_thread(UPDATE_COLLECTOR_INIT_t &initdata);
void stop_updcollector_thread();
void dump_collector_resource(COLLECTOR &res_collector, void sendit(void *sock, const char *fmt, ...), void *sock);
void free_collector_resource(COLLECTOR &res_collector);
int render_updcollector_status(POOL_MEM &buf);
void api_render_updcollector_status(OutputWriter &ow);

#endif /* __BCOLLECTOR_H_ */
