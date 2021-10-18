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
 * Bacula Cloud Transfer Manager:
 * transfer manager wraps around the work queue.
 * Reports transfer status and error
 * Reports statistics about current past and future work
 * Written by Norbert Bizet, May MMXVI
 *
 */
#include "cloud_transfer_mgr.h"
#include "stored.h"

#define ONE_SEC 1000000LL /* number of microseconds in a second */

static const int64_t dbglvl = DT_CLOUD|50;

/* constructor
   * size : the size in bytes of the transfer
   * funct : function to process
   * arg : argument passed to the function
   * cache_fname  : cache file name is duplicated in the transfer constructor
   * volume_name  :  volume name is duplicated in the transfer constructor
   * part         : part index
   * driver       : pointer to the cloud_driver
   * dcr          : pointer to DCR
*/
transfer::transfer(uint64_t    size,
                  transfer_engine* funct,
                  const char   *cache_fname,
                  const char   *volume_name,
                  uint32_t     part,
                  cloud_driver *driver,
                  DCR          *dcr,
                  cloud_proxy  *proxy) :
   m_stat_size(size),
   m_stat_processed_size(0),
   m_stat_start(0),
   m_stat_duration(0),
   m_stat_eta(0),
   m_stat_average_rate(0),
   m_message(NULL),
   m_state(TRANS_STATE_CREATED),
   m_mgr(NULL),
   m_funct(funct),
   m_wait_timeout_inc_insec(0),
   m_wait_timeout(time(NULL)),
   m_debug_retry(true),
   m_cache_fname(bstrdup(cache_fname)), /* cache fname is duplicated*/
   m_volume_name(bstrdup(volume_name)), /* volume name is duplicated*/
   m_part(part),
   m_driver(driver),
   m_dcr(dcr),
   m_proxy(proxy),
   m_workq_elem(NULL),
   m_use_count(0),
   m_retry(0),
   m_cancel(false),
   m_do_cache_truncate(false)
{
   pthread_mutex_init(&m_stat_mutex, 0);
   pthread_mutex_init(&m_mutex, 0);
   pthread_cond_init(&m_done, NULL);

   m_message = get_pool_memory(PM_MESSAGE);
   *m_message = 0;
}

/* destructor */
transfer::~transfer()
{
   free_pool_memory(m_message);
   pthread_cond_destroy(&m_done);
   pthread_mutex_destroy(&m_mutex);
   pthread_mutex_destroy(&m_stat_mutex);

   free(m_volume_name);
   free(m_cache_fname);
   if (m_use_count > 0) {
      ASSERT(FALSE);
      Dmsg1(dbglvl, "!!!m_use_count = %d\n", m_use_count);
   }
}

/* queue this transfer for processing in the manager workq
 * ret :true if queuing is successful */
bool transfer::queue()
{
   if (!transition(TRANS_STATE_QUEUED)) {
      return false;
   }
   return true;
}

/* reset processed size */
void transfer::reset_processed_size()
{
   lock_guard lg(m_stat_mutex);
   m_stat_processed_size = 0;
   ASSERTD(m_stat_processed_size==0, "invalid locking of processed size");
}

/* set absolute value process size */
void transfer::set_processed_size(uint64_t size)
{
   lock_guard lg(m_stat_mutex);
   m_stat_processed_size = size;
   m_stat_duration = get_current_btime()-m_stat_start;
   if (m_stat_duration > 0) {
      m_stat_average_rate = (m_stat_processed_size*ONE_SEC)/m_stat_duration;
   }
   ASSERTD(m_stat_processed_size <= m_stat_size, "increment_processed_size increment too big");

}
/* add increment to the current processed size */
void transfer::increment_processed_size(uint64_t increment)
{
   set_processed_size(m_stat_processed_size+increment);
}

/* opaque function that processes m_funct with m_arg as parameter
 * depending on m_funct return value, changes state to TRANS_STATE_DONE
 *  or TRANS_STATE_ERROR
 */
void transfer::proceed()
{
   if (transition(TRANS_STATE_PROCESSED)) {
         transfer_state state = m_funct(this);
         if (!transition(state)) {
            Mmsg1(m_message, _("wrong transition to %s after proceed\n"), state);
         }
   } else {
      Mmsg(m_message, _("wrong transition to TRANS_STATE_PROCESS in proceed review logic\n"));
   }
}

int transfer::wait()
{
   lock_guard lg(m_mutex);

   int stat = 0;
   while (m_state != TRANS_STATE_DONE &&
          m_state != TRANS_STATE_ERROR) {

      if ((stat = pthread_cond_wait(&m_done, &m_mutex)) != 0) {
         return stat;
      }
   }
   return stat;
}

int transfer::timedwait(const timeval& tv)
{
   lock_guard lg(m_mutex);
   struct timespec timeout;
   struct timeval ttv;
   struct timezone tz;
   int stat = 0;
   timeout.tv_sec = tv.tv_sec;
   timeout.tv_nsec = tv.tv_usec * 1000;
   gettimeofday(&ttv, &tz);
   timeout.tv_nsec += ttv.tv_usec * 1000;
   timeout.tv_sec += ttv.tv_sec;

   while (m_state != TRANS_STATE_DONE &&
          m_state != TRANS_STATE_ERROR) {

      if ((stat = pthread_cond_timedwait(&m_done, &m_mutex, &timeout)) != 0) {
         return stat;
      }
   }
   return stat;
}

/* place the cancel flag and wait until processing is done */
bool transfer::cancel()
{
   {
      lock_guard lg(m_mutex);
      m_cancel = true;
   }
   return wait();
}

/* checking the cancel status : doesnt request locking */
bool transfer::is_canceled() const
{
   return m_cancel;
}

uint32_t transfer::append_status(POOL_MEM& msg)
{
   POOLMEM *tmp_msg = get_pool_memory(PM_MESSAGE);
   char ec[50], ed1[50];
   uint32_t ret=0;
   static const char *state[]  = {"created",  "queued",  "process", "done", "error"};

   lock_guard lg(m_stat_mutex);
   if (m_state > TRANS_STATE_PROCESSED) {
      if (m_hash64[0]||m_hash64[1]||m_hash64[2]||m_hash64[3]||m_hash64[4]||m_hash64[5]||m_hash64[6]||m_hash64[7]) {
         ret =  Mmsg(tmp_msg,_("%s/part.%-5d state=%-7s size=%sB duration=%ds hash=%02x%02x%02x%02x%02x%02x%02x%02x%s%s%s%s\n"),
                     m_volume_name, m_part,
                     state[m_state],
                     edit_uint64_with_suffix(m_stat_size, ec),
                     m_stat_duration/ONE_SEC,
                     m_hash64[0],m_hash64[1],m_hash64[2],m_hash64[3],m_hash64[4],m_hash64[5],m_hash64[6],m_hash64[7],
                     (strlen(m_message) != 0)?" msg=":"",
                     (strlen(m_message) != 0)?m_message:"",
                     (m_retry > 1)?" retry=":"",
                     (m_retry > 1)?edit_uint64(m_retry, ed1):"");
      } else {
         ret =  Mmsg(tmp_msg,_("%s/part.%-5d state=%-7s size=%sB duration=%ds%s%s%s%s\n"),
                     m_volume_name, m_part,
                     state[m_state],
                     edit_uint64_with_suffix(m_stat_size, ec),
                     m_stat_duration/ONE_SEC,
                     (strlen(m_message) != 0)?" msg=":"",
                     (strlen(m_message) != 0)?m_message:"",
                     (m_retry > 1)?" retry=":"",
                     (m_retry > 1)?edit_uint64(m_retry, ed1):"");
      }
      pm_strcat(msg, tmp_msg);
   } else {
      ret = Mmsg(tmp_msg,_("%s/part.%-5d state=%-7s size=%sB eta=%ds%s%s%s%s\n"),
                  m_volume_name, m_part,
                  state[m_state],
                  edit_uint64_with_suffix(m_stat_size, ec),
                  m_stat_eta/ONE_SEC,
                  (strlen(m_message) != 0)?" msg=":"",
                  (strlen(m_message) != 0)?m_message:"",
                  (m_retry > 1)?" retry=":"",
                  (m_retry > 1)?edit_uint64(m_retry, ed1):"");
      pm_strcat(msg, tmp_msg);
   }
   free_pool_memory(tmp_msg);
   return ret;
}

void transfer::append_api_status(OutputWriter &ow)
{
   static const char *state[]  = {"created", "queued", "process", "done", "error"};

   lock_guard lg(m_stat_mutex);
   if (m_state > TRANS_STATE_PROCESSED) {
         ow.get_output(OT_START_OBJ,
                  OT_STRING,"volume_name",            NPRTB(m_volume_name),
                  OT_INT32, "part",                   m_part,
                  OT_INT32, "jobid",                  m_dcr ? (m_dcr->jcr ? m_dcr->jcr->JobId : 0) : 0,
                  OT_STRING,"state",                  (m_state == TRANS_STATE_QUEUED) ? 
                                                         (m_wait_timeout_inc_insec == 0) ? "queued":"waiting" :state[m_state],
                  OT_INT64, "size",                   m_stat_size,
                  OT_DURATION, "duration",            m_stat_duration/ONE_SEC,
                  OT_STRING,"message",                NPRTB(m_message),
                  OT_INT32, "retry",                  m_retry,
                  OT_END);
   } else {
         ow.get_output(OT_START_OBJ,
                  OT_STRING,"volume_name",            NPRTB(m_volume_name),
                  OT_INT32, "part",                   m_part,
                  OT_INT32, "jobid",                  m_dcr ? (m_dcr->jcr ? m_dcr->jcr->JobId : 0) : 0,
                  OT_STRING,"state",                  (m_state == TRANS_STATE_QUEUED) ? 
                                                         (m_wait_timeout_inc_insec == 0) ? "queued":"waiting" :state[m_state],
                  OT_INT64, "size",                   m_stat_size,
                  OT_INT64, "processed_size",         m_stat_processed_size,
                  OT_DURATION, "eta",                 m_stat_eta/ONE_SEC,
                  OT_STRING,"message",                NPRTB(m_message),
                  OT_INT32, "retry",                  m_retry,
                  OT_END);
   }
}

/* the manager references itself through this function */
void transfer::set_manager(transfer_manager *mgr)
{
   lock_guard lg(m_mutex);
   m_mgr = mgr;
}

/* change the state */
bool transfer::transition(transfer_state state)
{
   /* lock state mutex*/
   lock_guard lg(m_mutex);

   /* transition from current state (m_state) to target state (state)*/
   bool ret = false; /*impossible transition*/
   switch(m_state)
   {
      case TRANS_STATE_CREATED:
         /* CREATED -> QUEUED */
         if (state == TRANS_STATE_QUEUED) {
            /* valid transition*/
            ret = true;
            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               /*increment the number of queued transfer*/
               if (m_wait_timeout_inc_insec == 0) {
                  /* queued state */
                  m_mgr->m_stat_nb_transfer_queued++;
                  /*add the current size into manager queued size*/
                  m_mgr->m_stat_size_queued += m_stat_size;
               } else {
                  /* pseudo-waiting state */
                  m_mgr->m_stat_nb_transfer_waiting++;
                  /*add the current size into manager pseudo-waiting size*/
                  m_mgr->m_stat_size_waiting += m_stat_size;
               }
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);

               P(m_mgr->m_mutex);
               m_mgr->add_work(this);
               V(m_mgr->m_mutex);
            }
         }
         break;

      case TRANS_STATE_QUEUED:
         /* QUEUED -> CREATED : back to initial state*/
         if (state == TRANS_STATE_CREATED) {
            /* valid transition*/
            ret = true;
            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               /*decrement the number of queued transfer*/
               if (m_wait_timeout_inc_insec == 0) {
                  /* queued state */
                  m_mgr->m_stat_nb_transfer_queued--;
                  /*remove the current size into manager queued size*/
                  m_mgr->m_stat_size_queued -= m_stat_size;
               } else {
                  /* pseudo-waiting state */
                  m_mgr->m_stat_nb_transfer_waiting--;
                  /*remove the current size into manager pseudo-waiting size*/
                  m_mgr->m_stat_size_waiting -= m_stat_size;
               }
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);

               P(m_mgr->m_mutex);
               m_mgr->remove_work(m_workq_elem);
               V(m_mgr->m_mutex);
            }
         }
         /* QUEUED -> PROCESSED : a worker aquired the transfer*/
         if (state == TRANS_STATE_PROCESSED) {
            /*valid transition*/
            ret = true;
            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               /*decrement the number of queued transfer*/
               if (m_wait_timeout_inc_insec == 0) {
                  /* queued state */
                  m_mgr->m_stat_nb_transfer_queued--;
                  /*remove the current size into manager queued size*/
                  m_mgr->m_stat_size_queued -= m_stat_size;
               } else {
                  /* pseudo-waiting state */
                  m_mgr->m_stat_nb_transfer_waiting--;
                  /*remove the current size into manager pseudo-waiting size*/
                  m_mgr->m_stat_size_waiting -= m_stat_size;
               }
               /*increment the number of processed transfer*/
               m_mgr->m_stat_nb_transfer_processed++;
               /*... and add it to the manager processed size*/
               m_mgr->m_stat_size_processed += m_stat_size;
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);

               /*transfer starts now*/
               P(m_stat_mutex);
               m_stat_start = get_current_btime();
               V(m_stat_mutex);
            }
         }
         break;

      case TRANS_STATE_PROCESSED:
         /* PROCESSED -> DONE : Success! */
         if (state == TRANS_STATE_DONE) {
            /*valid transition*/
            ret = true;
            /*transfer stops now : compute transfer duration*/
            P(m_stat_mutex);
            m_stat_duration = get_current_btime()-m_stat_start;
            if (m_stat_duration > 0) {
               m_stat_processed_size = m_stat_size;
               ASSERTD(m_stat_size == m_stat_processed_size, "xfer done before processed size is equal to size.");
               m_stat_average_rate = (m_stat_size*ONE_SEC)/m_stat_duration;
            }
            V(m_stat_mutex);

            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               /* ... from processed to done*/
               m_mgr->m_stat_nb_transfer_processed--;
               m_mgr->m_stat_nb_transfer_done++;
               m_mgr->m_stat_size_processed -= m_stat_size;
               m_mgr->m_stat_size_done += m_stat_size;
               /*add local duration to manager duration */
               m_mgr->m_stat_duration_done += m_stat_duration;
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);
            }

            if (m_proxy) {
               m_proxy->set(m_volume_name, m_part, m_res_mtime, m_res_size, m_hash64);
            }

            /* in both case, success or failure, life keeps going on */
            pthread_cond_broadcast(&m_done);
         }
         /* PROCESSED -> ERROR : Failure! */
         if (state == TRANS_STATE_ERROR) {
            /*valid transition*/
            ret = true;
            /*transfer stops now, even if in error*/
            P(m_stat_mutex);
            m_stat_duration = get_current_btime()-m_stat_start;
            V(m_stat_mutex);

            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               /* ... from processed to error*/
               m_mgr->m_stat_nb_transfer_processed--;
               m_mgr->m_stat_nb_transfer_error++;
               m_mgr->m_stat_size_processed -= m_stat_size;
               m_mgr->m_stat_size_error += m_stat_size;
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);
            }
            /* in both case, success or failure, life keeps going on */
            pthread_cond_broadcast(&m_done);
         }
         /* PROCESSED -> QUEUED */
         if (state == TRANS_STATE_QUEUED) {
            /*valid transition*/
            ret = true;
            if (m_mgr) {
               /*lock manager statistics */
               P(m_mgr->m_stat_mutex);
               m_mgr->m_stat_nb_transfer_processed--;
               /*increment the number of queued transfer*/
               if (m_wait_timeout_inc_insec == 0) {
                  /* queued state */
                  m_mgr->m_stat_nb_transfer_queued++;
                  /*add the current size into manager queued size*/
                  m_mgr->m_stat_size_queued += m_stat_size;
               } else {
                  /* pseudo-waiting state */
                  m_mgr->m_stat_nb_transfer_waiting++;
                  /*add the current size into manager queued size*/
                  m_mgr->m_stat_size_waiting += m_stat_size;
               }
               /*remove the current size into manager processed size*/
               m_mgr->m_stat_size_processed -= m_stat_size;
               /*unlock manager statistics */
               V(m_mgr->m_stat_mutex);

               P(m_mgr->m_mutex);
               m_mgr->add_work(this);
               V(m_mgr->m_mutex);
            }
            /* leave */
            pthread_cond_broadcast(&m_done);
         }
         break;
      case TRANS_STATE_DONE:
      case TRANS_STATE_ERROR:
      default:
         ret = false;
         break;
   }

   /* update state when transition is valid*/
   if (ret) {
      m_state = state;
   }

   return ret;
}

void transfer::set_do_cache_truncate(bool do_cache_truncate)
{
   m_do_cache_truncate=do_cache_truncate;
}

void transfer::inc_retry()
{
   lock_guard lg(m_mutex);
   m_retry++;
}

int transfer::inc_use_count()
{
   lock_guard lg(m_mutex);
   return ++m_use_count;
}

int transfer::dec_use_count()
{
   lock_guard lg(m_mutex);
   return --m_use_count;
}

void *transfer_launcher(void *arg)
{
   transfer *t = (transfer *)arg;
   if (t) {
      t->proceed();
   }
   return NULL;
}

/* -----------------------------------------------------------
   transfer manager declarations
   -----------------------------------------------------------
 */

/* constructor */
/* nb_workers: maximum number of workers allowed for this manager */
transfer_manager::transfer_manager(uint32_t nb_worker)
{
   transfer *item=NULL;
   m_transfer_list.init(item, &item->link);
   pthread_mutex_init(&m_stat_mutex, 0);
   pthread_mutex_init(&m_mutex, 0);
   workq_init(&m_wq, nb_worker, transfer_launcher);
}

/* destructor */
transfer_manager::~transfer_manager()
{
   workq_wait_idle(&m_wq);
   pthread_mutex_destroy(&m_mutex);
   pthread_mutex_destroy(&m_stat_mutex);
}

/* create a new or inc-reference a similar transfer. (factory)
 * ret: transfer* is ref_counted and must be kept, used
 * and eventually released by caller with release() */
transfer *transfer_manager::get_xfer(uint64_t     size,
            transfer_engine *funct,
            POOLMEM      *cache_fname,
            const char   *volume_name,
            uint32_t     part,
            cloud_driver *driver,
            DCR          *dcr,
            cloud_proxy  *proxy)
{
   lock_guard lg (m_mutex);

   /* do we have a similar transfer on tap? */
   transfer *item;
   foreach_dlist(item, (&m_transfer_list)) {
      /* this is where "similar transfer" is defined:
       * same volume_name, same part idx */
      if (strcmp(item->m_volume_name, volume_name) == 0 && item->m_part == part) {
         item->inc_use_count();
         return item;
      }
   }
   /* no existing transfer: create a new one */
   item = New(transfer(size,
                       funct,
                       cache_fname,/* cache_fname is duplicated in the transfer constructor*/
                       volume_name, /* volume_name is duplicated in the transfer constructor*/
                       part,
                       driver,
                       dcr,
                       proxy));

   ASSERT(item->m_state == TRANS_STATE_CREATED);
   item->set_manager(this);
   /* inc use_count once for m_transfer_list insertion */
   item->inc_use_count();
   m_transfer_list.append(item);
   /* inc use_count once for caller ref counting */
   item->inc_use_count();
   return item;
}

/* does the xfer belong to us? */
bool transfer_manager::owns(transfer *xfer)
{
   lock_guard lg(m_mutex);
   transfer *item;
   foreach_dlist(item, (&m_transfer_list)) {
      /* same address */
      if (item == xfer) {
         return true;
      }
   }
   return false;
}

/* un-ref transfer and free if ref count goes to zero
 * caller must NOT use xfer anymore after this has been called */
void transfer_manager::release(transfer *xfer)
{
   if (xfer) {
      ASSERTD(owns(xfer), "Wrong Manager");
      /* wait should have been done already by caller,
       * but we cannot afford deleting the transfer while it's not completed */
      wait(xfer);
      /* decrement the caller reference */
      if (xfer->dec_use_count() == 1) {
         /* the only ref left is the one from m_transfer_list
          * time for deletion */
         lock_guard lg(m_mutex);
         m_transfer_list.remove(xfer);
         xfer->dec_use_count();
         delete xfer;
      }
   }
}

/* accessors to xfer->queue */
bool transfer_manager::queue(transfer *xfer)
{
   if (xfer) {
      ASSERTD(owns(xfer), "Wrong Manager");
      return xfer->queue();
   }
   return false;
}

/* accessors to xfer->wait */
int transfer_manager::wait(transfer *xfer)
{
   if (xfer) {
      ASSERTD(owns(xfer), "Wrong Manager");
      return xfer->wait();
   }
   return 0;
}

/* accessors to xfer->timedwait */
int transfer_manager::timedwait(transfer *xfer, const timeval& tv)
{
   if (xfer) {
      ASSERTD(owns(xfer), "Wrong Manager");
      return xfer->timedwait(tv);
   }
   return 0;
}

/* accessors to xfer->cancel */
bool transfer_manager::cancel(transfer *xfer)
{
   if (xfer) {
      ASSERTD(owns(xfer), "Wrong Manager");
      return xfer->cancel();
   }
   return false;
}

/* append a transfer object to this manager */
int transfer_manager::add_work(transfer* t)
{
   return workq_add(&m_wq, t, t ? &t->m_workq_elem : NULL, 0);
}

/* remove associated workq_ele_t from this manager workq */
int transfer_manager::remove_work(workq_ele_t *elem)
{
   return workq_remove(&m_wq, elem);
}
/* search the transfer list for similar transfer */
bool transfer_manager::find(const char *VolName, uint32_t index)
{
   /* Look in the transfer list if we have a download/upload for the current volume */
   lock_guard lg(m_mutex);
   transfer *item;
   foreach_dlist(item, (&m_transfer_list)) {
      if (strcmp(item->m_volume_name, VolName) == 0 && item->m_part == index) {
         return true;
      }
   }
   return false;
}

/* Call to this function just before displaying global statistics */
void transfer_manager::update_statistics()
{
   /* lock the manager stats */
   lock_guard lg_stat(m_stat_mutex);

   /* lock the queue so order and chaining cannot be modified */
   lock_guard lg(m_mutex);

   /* recompute global average rate */
   transfer *t;
   uint64_t accumulated_done_average_rate = 0;
   uint32_t nb_done_accumulated = 0;
   foreach_dlist(t, &m_transfer_list) {
      lock_guard lg_xferstatmutex(t->m_stat_mutex);
      if (t->m_stat_average_rate>0) {
         accumulated_done_average_rate += t->m_stat_average_rate;
         t->m_stat_average_rate = 0;
         ++nb_done_accumulated;
      }
   }
   if (nb_done_accumulated) {
      m_stat_average_rate = accumulated_done_average_rate / nb_done_accumulated;
   }

   /* ETA naive calculation for each element in the queue */
   if (m_stat_average_rate != 0) {
      uint64_t accumulator=0;
      foreach_dlist(t, &m_transfer_list) {
         if (t->m_state == TRANS_STATE_QUEUED) {
            lock_guard lg_xferstatmutex(t->m_stat_mutex);
            accumulator+= t->m_stat_size-t->m_stat_processed_size;
            t->m_stat_eta = (accumulator / m_stat_average_rate) * ONE_SEC;
         }
         if (t->m_state == TRANS_STATE_PROCESSED) {
            lock_guard lg_xferstatmutex(t->m_stat_mutex);
            t->m_stat_eta = ((t->m_stat_size-t->m_stat_processed_size) / m_stat_average_rate) * ONE_SEC;
         }
      }
      /* the manager ETA is the ETA of the last transfer in its workq */
      m_stat_eta = (accumulator / m_stat_average_rate) * ONE_SEC;
   }
}

/* short status of the transfers */
uint32_t transfer_manager::append_status(POOL_MEM& msg, bool verbose)
{
   update_statistics();
   char ec0[30],ec1[30],ec2[30],ec3[30],ec4[30],ec5[30];
   POOLMEM *tmp_msg = get_pool_memory(PM_MESSAGE);
   lock_guard lg_stat(m_stat_mutex);
   uint32_t ret = Mmsg(tmp_msg, _("(%sB/s) (ETA %d s) "
            "Queued=%d %sB, Waiting=%d %sB, Processed=%d %sB, Done=%d %sB, Failed=%d %sB\n"),
            edit_uint64_with_suffix(m_stat_average_rate, ec0), m_stat_eta/ONE_SEC,
            m_stat_nb_transfer_queued,  edit_uint64_with_suffix(m_stat_size_queued, ec1),
            m_stat_nb_transfer_waiting,  edit_uint64_with_suffix(m_stat_size_waiting, ec2),
            m_stat_nb_transfer_processed,  edit_uint64_with_suffix(m_stat_size_processed, ec3),
            m_stat_nb_transfer_done,  edit_uint64_with_suffix(m_stat_size_done, ec4),
            m_stat_nb_transfer_error, edit_uint64_with_suffix(m_stat_size_error, ec5));
   pm_strcat(msg, tmp_msg);

   if (verbose) {
      lock_guard lg(m_mutex);
      if (!m_transfer_list.empty()) {
         ret += Mmsg(tmp_msg, _("------------------------------------------------------------ details ------------------------------------------------------------\n"));
         pm_strcat(msg, tmp_msg);
      }
      transfer *tpkt;
      foreach_dlist(tpkt, &m_transfer_list) {
         ret += tpkt->append_status(msg);
      }
   }
   free_pool_memory(tmp_msg);
   return ret;
}

void transfer_manager::append_api_status(OutputWriter &ow, bool verbose)
{
   update_statistics();

   lock_guard lg_stat(m_stat_mutex);
   ow.get_output(OT_START_OBJ,
                  OT_INT64, "average_rate",           m_stat_average_rate,
                  OT_DURATION, "eta",                 m_stat_eta/ONE_SEC,
                  OT_INT64, "nb_transfer_queued",     m_stat_nb_transfer_queued,
                  OT_INT64, "size_queued",            m_stat_size_queued,
                  OT_INT64, "nb_transfer_waiting",     m_stat_nb_transfer_waiting,
                  OT_INT64, "size_waiting",           m_stat_size_waiting,
                  OT_INT64, "nb_transfer_processed",  m_stat_nb_transfer_processed,
                  OT_INT64, "size_processed",         m_stat_size_processed,
                  OT_INT64, "nb_transfer_done",       m_stat_nb_transfer_done,
                  OT_INT64, "size_done",              m_stat_size_done,
                  OT_INT64, "nb_transfer_error",      m_stat_nb_transfer_error,
                  OT_INT64, "size_error",             m_stat_size_error,
                  OT_INT,   "transfers_list_size",    m_transfer_list.size(),
                  OT_END);
   if (verbose) {
      lock_guard lg(m_mutex);
      ow.start_list("transfers");
      transfer *tpkt;
      foreach_dlist(tpkt, &m_transfer_list) {
         tpkt->append_api_status(ow);
      }
      ow.end_list();
   }
}
