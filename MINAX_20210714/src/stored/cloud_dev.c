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
 * Generic routines for creating Cloud compatibile Volumes.
 *  NOTE!!! This cloud device is not compatible with
 *   any disk-changer script for changing Volumes.
 *   It does however work with Bacula Virtual autochangers.
 *
 * Written by Kern Sibbald, May MMXVI
 *
 */

#include "bacula.h"
#include "stored.h"
#include <dlfcn.h>
#include "cloud_dev.h"
#include "s3_driver.h"
#include "file_driver.h"
#if BEEF
#include "generic_driver.h"
#endif
#include "cloud_parts.h"
#include "math.h"

static const int64_t dbglvl = DT_CLOUD|50;

#define ASYNC_TRANSFER 1
#define NUM_DOWNLOAD_WORKERS  3
#define NUM_UPLOAD_WORKERS  3

/* Debug only: Enable to introduce random transfer delays*/
/* #define RANDOM_WAIT_ENABLE*/
#define RANDOM_WAIT_MIN 2 /* minimum delay time*/
#define RANDOM_WAIT_MAX 12 /* maxinum delay time*/

/* retry timeouts on transfer manager (arbitrary) start 1 minute, max 5 minutes*/
#define WAIT_TIMEOUT_INC_INSEC 60
#define MAX_WAIT_TIMEOUT_INC_INSEC 300

#define XFER_TMP_NAME "xfer"
#include <fcntl.h>

#if defined(HAVE_WIN32)
  #define lseek _lseeki64
#endif

/* standard dcr cancel callback function */
bool DCR_cancel_cb(void* arg)
{
   DCR *dcr = (DCR*)arg;
   if (dcr && dcr->jcr) {
      return dcr->jcr->is_canceled();
   }
   return false;
}

#ifdef __cplusplus
extern "C" {
#endif

DEVICE *BaculaSDdriver(JCR *jcr, DEVRES *device)
{
   DEVICE *dev;
   if (!device->cloud) {
      Jmsg0(jcr, M_FATAL, 0, _("A Cloud resource is required for the Cloud driver, but is missing.\n"));
      return NULL;
   }
   dev = New(cloud_dev(jcr, device));
   return dev;
}

#ifdef __cplusplus
}
#endif

/******************* Driver loading *******************/

/* Define possible extensions */
#if defined(HAVE_WIN32)
#define DRV_EXT ".dll"
#elif defined(HAVE_DARWIN_OS)
#define DRV_EXT ".dylib"
#else
#define DRV_EXT ".so"
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

/* Forward referenced functions */
extern "C" {
typedef cloud_driver *(*newCloudDriver_t)(void);
}

static cloud_driver *load_driver(JCR *jcr, uint cloud_driver_type);

/*
 * Driver item for driver table
*/
struct cloud_driver_item {
   const char *name;
   void *handle;
   newCloudDriver_t newDriver;
   bool builtin;
   bool loaded;
};

/*
 * Driver table. Must be in same order as the B_xxx_DEV type
 *   name   handle, builtin  loaded
 */
static cloud_driver_item driver_tab[] = {
/*   name   handle, newDriver builtin  loaded */
   {"s3",      NULL, NULL,    false, false},
   {"file",    NULL, NULL,    false, false},
   {"was",     NULL, NULL,    false, false},
   {"gs",      NULL, NULL,    false, false},
   {"oci",     NULL, NULL,    false, false},
   {"generic", NULL, NULL,    false, false},
   {"swift",   NULL, NULL,    false, false},
   {NULL,      NULL, NULL,    false, false}
};

static const char* cloud_driver_type_name[] = {
   "Unknown",
   "S3",
   "File",
   "Azure",
   "Google",
   "Oracle",
   "Generic",
   "Swift",
   NULL
};

//static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/******************* Driver loading *******************/

transfer_manager cloud_dev::download_mgr(transfer_manager(NUM_DOWNLOAD_WORKERS));
transfer_manager cloud_dev::upload_mgr(transfer_manager(NUM_UPLOAD_WORKERS));

/* Imported functions */
const char *mode_to_str(int mode);
int breaddir(DIR *dirp, POOLMEM *&dname);

/* Forward referenced functions */
bool makedir(JCR *jcr, char *path, mode_t mode);

/* Const and Static definitions */

/* Address manipulations:
 *
 * The current idea is internally use part and offset-in-part
 * However for sending back JobMedia, we need to use
 *   get_full_addr() which puts the file in the top 20 bits.
 */

static boffset_t get_offset(boffset_t ls_offset)
{
   boffset_t pos = ls_offset & off_mask;
   return pos;
}

static boffset_t make_addr(uint32_t my_part, boffset_t my_offset)
{
   return (boffset_t)(((uint64_t)my_part)<<off_bits) | my_offset;
}

/* From lst (a transfer alist), retrieve the first transfer matching VolumeName and part idx
 */
transfer *get_list_transfer(alist *lst, const char* VolumeName, uint32_t upart)
{
   transfer *t;
   foreach_alist(t, lst) {
      if (t && bstrcmp(VolumeName, t->m_volume_name) && (upart == t->m_part)) {
         return t;
      }
   }
   return NULL;
}

/*
 * This upload_engine is called by workq in a worker thread.
 * it receives the transfer packet to be processed
 * it returns the state the manager should transition to after once processed.
 * TRANS_STATE_DONE: processing was OK
 * TRANS_STATE_ERROR: error during processing (no retry)
 * TRANS_STATE_QUEUED: processing should be retry
 */
transfer_state upload_engine(transfer *tpkt)
{
#ifdef RANDOM_WAIT_ENABLE
   srand(time(NULL));
   /* wait between 2 and 12 seconds */
   int s_time = RANDOM_WAIT_MIN + rand() % (RANDOM_WAIT_MAX-RANDOM_WAIT_MIN);
   bmicrosleep(s_time, 0);
#endif
   if (tpkt && tpkt->m_driver) {
      /* call the driver method async */
      Dmsg4(dbglvl, "Upload start %s-%d JobId : %d driver :%p\n",
         tpkt->m_volume_name, tpkt->m_part, tpkt->m_dcr->jcr->JobId, tpkt->m_driver);

      cancel_callback cancel_cb;
      cancel_cb.fct = DCR_cancel_cb;
      cancel_cb.arg = tpkt->m_dcr;

      /* don't version part.1 */
      if (tpkt->m_part != 1) {
         /* from btime.c */
         time_t td = time(NULL);
         struct tm tm;
         (void)localtime_r(&td, &tm);
         POOL_MEM strtime;
         strftime(strtime.c_str(), strtime.size(), "part%Y%m%d%H%M%S", &tm);
         /* form the target part name */
         POOL_MEM target_part_name;
         Mmsg(target_part_name, "%s.%d", strtime.c_str(), tpkt->m_part);
         /* exists is passed as argument */
         int exists=0;
         POOLMEM *msg = get_pool_memory(PM_FNAME);
         msg[0] = 0;
         /* try to move the part to the target part */
         if (tpkt->m_driver->move_cloud_part(tpkt->m_volume_name, 
                                             tpkt->m_part,
                                             target_part_name.c_str(),
                                             &cancel_cb,
                                             msg,
                                             exists) )
         {
            if (exists == 0) {
               /* sucessful + !exists : the source path has not been found -> OK ignore */
            } else {
               /* sucessful + exists : the source part has been moved to target part name */
               Jmsg(tpkt->m_dcr->jcr, M_INFO, 0, _("%s/part.%d was present on the cloud and has been versioned to %s\n"),
                     tpkt->m_volume_name, tpkt->m_part, msg);
            }
         } else {
            Dmsg4(dbglvl, "Move error!! JobId=%d part=%d Vol=%s cache=%s\n",
               tpkt->m_dcr->jcr->JobId, tpkt->m_part, tpkt->m_volume_name, tpkt->m_cache_fname);
            POOL_MEM dmsg(PM_MESSAGE);
            tpkt->append_status(dmsg);
            Dmsg1(dbglvl, "%s\n",dmsg.c_str());
         }
         free_pool_memory(msg);
      } 

      /* upload the cache part to the cloud */
      if (!tpkt->m_driver->copy_cache_part_to_cloud(tpkt)) {
         /* Error message already sent by Qmsg() */
         Dmsg4(dbglvl, "Upload error!! JobId=%d part=%d Vol=%s cache=%s\n",
            tpkt->m_dcr->jcr->JobId, tpkt->m_part, tpkt->m_volume_name, tpkt->m_cache_fname);
         POOL_MEM dmsg(PM_MESSAGE);
         tpkt->append_status(dmsg);
         Dmsg1(dbglvl, "%s\n",dmsg.c_str());
         return TRANS_STATE_ERROR;
      }

      Dmsg2(dbglvl, "Upload end JobId : %d driver :%p\n",
         tpkt->m_dcr->jcr->JobId, tpkt->m_driver);

      if (tpkt->m_do_cache_truncate && tpkt->m_part!=1) {
         if (unlink(tpkt->m_cache_fname) != 0) {
               berrno be;
               Dmsg2(dbglvl, "Truncate cache option after upload. Unable to delete %s. ERR=%s\n", tpkt->m_cache_fname, be.bstrerror());
         } else {
            Dmsg1(dbglvl, "Truncate cache option after upload. Unlink file %s\n", tpkt->m_cache_fname);
         }
      }
   }
   return TRANS_STATE_DONE;
}

/* Forward declaration*/
transfer_state download_engine(transfer *);

/* This wait_engine is called by workq in a worker thread 
 * as an alternative to download_engine when download has been postponned.
 * it receives the transfer packet to be processed
 * it returns the state the manager should transition to once processed.
 * TRANS_STATE_DONE: processing was OK
 * TRANS_STATE_ERROR: error during processing (no retry)
 * TRANS_STATE_QUEUED: processing should be retry
 */
transfer_state wait_engine(transfer *tpkt)
{
   if (tpkt) {
      btime_t now = time(NULL);
      if (now < tpkt->m_wait_timeout) {
         sleep(10);
         return TRANS_STATE_QUEUED;
      }
      if (tpkt->m_driver && tpkt->m_driver->is_waiting_on_server(tpkt)) {
         Dmsg3(dbglvl, "JobId=%d %s/part.%d waiting...\n",
         tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part);
         lock_guard lg(tpkt->m_mutex);
         /* increase the timeout increment up to MAX_WAIT_TIMEOUT_INC_INSEC value */
         if (tpkt->m_wait_timeout_inc_insec < MAX_WAIT_TIMEOUT_INC_INSEC)
         {
            tpkt->m_wait_timeout_inc_insec += WAIT_TIMEOUT_INC_INSEC;
            if (tpkt->m_wait_timeout_inc_insec > MAX_WAIT_TIMEOUT_INC_INSEC) {
               tpkt->m_wait_timeout_inc_insec = MAX_WAIT_TIMEOUT_INC_INSEC;
            }
         }
         tpkt->m_wait_timeout = time(NULL)+tpkt->m_wait_timeout_inc_insec;
         return TRANS_STATE_QUEUED;
      } else {
         Dmsg3(dbglvl, "JobId=%d %s/part.%d is ready!\n",
         tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part);
         lock_guard lg(tpkt->m_mutex);
         tpkt->m_wait_timeout_inc_insec = 0;
         tpkt->m_funct = download_engine;
         return TRANS_STATE_QUEUED;
      }
   }
   return TRANS_STATE_QUEUED;
}

/*
 * This download_engine is called by workq in a worker thread.
 * it receives the transfer packet to be processed
 * it returns the state the manager should transition to after process
 * TRANS_STATE_DONE: processing was OK
 * TRANS_STATE_ERROR: error during processing (no retry)
 * TRANS_STATE_QUEUED: processing should be retry
 */
transfer_state download_engine(transfer *tpkt)
{
#ifdef RANDOM_WAIT_ENABLE
   srand(time(NULL));
   /* wait between 2 and 12 seconds */
   int s_time = RANDOM_WAIT_MIN + rand() % (RANDOM_WAIT_MAX-RANDOM_WAIT_MIN);
   bmicrosleep(s_time, 0);
#endif
   if (tpkt && tpkt->m_driver) {
      /* call the driver method async */
      Dmsg4(dbglvl, "JobId=%d %s/part.%d download started to %s.\n",
      tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part, tpkt->m_cache_fname);
      Dmsg4(dbglvl, "%s/part.%d download started. job : %d driver :%p\n",
         tpkt->m_volume_name, tpkt->m_part, tpkt->m_dcr->jcr->JobId, tpkt->m_driver);
      int ret = tpkt->m_driver->copy_cloud_part_to_cache(tpkt);
      switch (ret) {
         case cloud_driver::CLOUD_DRIVER_COPY_PART_TO_CACHE_OK:
         {
            POOLMEM *cache_fname = get_pool_memory(PM_FNAME);
            pm_strcpy(cache_fname, tpkt->m_cache_fname);
            char *p = strstr(cache_fname, XFER_TMP_NAME);
            char partnumber[50];
            bsnprintf(partnumber, sizeof(partnumber), "part.%d", tpkt->m_part);
            strcpy(p,partnumber);
            if (rename(tpkt->m_cache_fname, cache_fname) != 0) {
               Dmsg5(dbglvl, "JobId=%d %s/part.%d download. part copy from %s to %s error!!\n",
               tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part, tpkt->m_cache_fname, cache_fname);
               free_pool_memory(cache_fname);
               return TRANS_STATE_ERROR;
            }
            free_pool_memory(cache_fname);
            return TRANS_STATE_DONE;
         }
         case cloud_driver::CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR:
         {
            Dmsg4(dbglvl, "JobId=%d %s/part.%d download to cache=%s error!!\n",
               tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part, tpkt->m_cache_fname);
            POOL_MEM dmsg(PM_MESSAGE);
            tpkt->append_status(dmsg);
            Dmsg1(dbglvl, "%s\n",dmsg.c_str());

            /* download failed -> remove the temp xfer file */
            if (unlink(tpkt->m_cache_fname) != 0) {
               berrno be;
               Dmsg2(dbglvl, "Unable to delete %s. ERR=%s\n", tpkt->m_cache_fname, be.bstrerror());
            } else {
               Dmsg1(dbglvl, "Unlink file %s\n", tpkt->m_cache_fname);
            }
            return TRANS_STATE_ERROR;
         }
         case cloud_driver::CLOUD_DRIVER_COPY_PART_TO_CACHE_RETRY:
         {
            lock_guard lg(tpkt->m_mutex);
            Dmsg4(dbglvl, "JobId=%d %s/part.%d download to cache=%s retry... \n",
               tpkt->m_dcr->jcr->JobId, tpkt->m_volume_name, tpkt->m_part, tpkt->m_cache_fname);
            tpkt->m_wait_timeout_inc_insec = WAIT_TIMEOUT_INC_INSEC;
            tpkt->m_wait_timeout = time(NULL)+ tpkt->m_wait_timeout_inc_insec;
            tpkt->m_funct = wait_engine;
            return TRANS_STATE_QUEUED;
         }
         default:
         break;
      }
   }
   return TRANS_STATE_DONE;
}

/*
 * Upload the given part to the cloud
 */
bool cloud_dev::upload_part_to_cloud(DCR *dcr, const char *VolumeName, uint32_t upart, bool do_truncate)
{
   /* for a "regular" backup job, don't proceed with the upload, but pretend everything went OK. */
   /* Otherwise, pass thru and proceed with the upload, even if UPLOAD_NO is set */
   bool internal_job = dcr->jcr->is_internal_job() || dcr->jcr->is_JobType(JT_ADMIN);
   if ((upload_opt == UPLOAD_NO) && !internal_job) {
      return true;
   }
   bool ret=false;
   if (upart == 0 || get_list_transfer(dcr->uploads, VolumeName, upart)) {
      return ret;
   }

   uint64_t file_size=0;
   POOLMEM *cache_fname = get_pool_memory(PM_FNAME);
   make_cache_filename(cache_fname, VolumeName, upart);

   /* part is valid and no upload for the same part is scheduled */
   if (!upload_mgr.find(VolumeName,upart)) {
      Enter(dbglvl);

      /* statistics require the size to transfer */
      struct stat statbuf;
      if (lstat(cache_fname, &statbuf) < 0) {
         berrno be;
         Mmsg2(errmsg, "Failed to find cache part file %s. ERR=%s\n",
            cache_fname, be.bstrerror());
         Dmsg1(dbglvl, "%s", errmsg);
         free_pool_memory(cache_fname);
         return false;
      }
      file_size = statbuf.st_size;

      /* Nothing to do with this empty part */
      if (file_size == 0) {
         free_pool_memory(cache_fname);
         return true; /* consider the transfer OK */
      }

      ret=true;
   }

   Dmsg1(dbglvl, "upload_part_to_cloud: %s\n", cache_fname);
   /* get_xfer either returns a new transfer or a similar one if it already exists.
    * in this case, the transfer is shared and ref_count is incremented. The caller should only care to release()
    * the transfer eventually. The transfer_mgr is in charge of deleting the transfer when no one shares it anymore*/
   transfer *item = upload_mgr.get_xfer(file_size,
                                       upload_engine,
                                       cache_fname,/* cache_fname is duplicated in the transfer constructor*/
                                       VolumeName, /* VolumeName is duplicated in the transfer constructor*/
                                       upart,
                                       driver,
                                       dcr,
                                       cloud_prox);
   dcr->uploads->append(item);
   /* transfer are queued manually, so the caller has control on when the transfer is scheduled
    * this should come handy for upload_opt */
   item->set_do_cache_truncate(do_truncate);
   if ( (upload_opt == UPLOAD_EACHPART) ||
        ((upload_opt == UPLOAD_NO) && internal_job) ) {
      /* in each part upload option, queue right away */
      item->queue();
   }
   free_pool_memory(cache_fname);

   if (ret) {
      /* Update the Media information */
      if (upart >= VolCatInfo.VolCatParts) {
         VolCatInfo.VolCatParts = upart;
         VolCatInfo.VolLastPartBytes = file_size;
      }
      /* We do not call dir_update_volume_info() because the part is not yet
       * uploaded, but we may call it to update VolCatParts or VolLastPartBytes.
       */
   }

   return ret;
}

/* Small helper to get  */
static int64_t part_get_size(ilist *cachep, int index)
{
   int64_t ret=0;
   if (index <= cachep->last_index()) {
      cloud_part *p = (cloud_part*) cachep->get(index);
      if (p) {
         ret = p->size;
      }
   }
   return ret;
}

/*
 * Download the part_idx part to the cloud. The result is store in the DCR context
 * The caller should use free_transfer()
 */
transfer *cloud_dev::download_part_to_cache(DCR *dcr, const char *VolumeName, uint32_t dpart)
{
   if (dpart == 0) {
      return NULL;
   }

   /* if item's already in the dcr list, it's already in the download_mgr, we don't need any duplication*/
   transfer *item = get_list_transfer(dcr->downloads, VolumeName, dpart);
   if (!item) {
      POOLMEM *cache_fname = get_pool_memory(PM_FNAME);
      pm_strcpy(cache_fname, dev_name);
      /* create a uniq xfer file name with XFER_TMP_NAME and the pid */
      POOL_MEM xferbuf;
      Mmsg(xferbuf, "%s_%d_%d", XFER_TMP_NAME, (int)getpid(), (int)dcr->jcr->JobId);
      cloud_driver::add_vol_and_part(cache_fname, VolumeName, xferbuf.c_str(), dpart);

      /* use the cloud proxy to retrieve the transfer size */
      uint64_t cloud_size = cloud_prox->get_size(VolumeName, dpart);

      /* check if the part is already in the cache and if it's bigger or equal to the cloud conterpart*/
      ilist cachep;
      if (!get_cache_volume_parts_list(dcr, getVolCatName(), &cachep)) {
         free_pool_memory(cache_fname);
         return NULL;
      }

      if (cachep.get(dpart)) {
         uint64_t cache_size = part_get_size(&cachep, dpart);

         Dmsg3(dbglvl, "download_part_to_cache: %s. cache_size=%d cloud_size=%d\n", cache_fname, cache_size, cloud_size);

         if (cache_size >= cloud_size) {
            /* We could/should use mtime */
            /* cache is "better" than cloud, no need to download */
            Dmsg2(dbglvl, "part %ld is up-to-date in the cache %lld\n", (int32_t)dpart, cache_size);
            free_pool_memory(cache_fname);
            return NULL;
         }
      }

      /* Unlikely, but still possible : the xfer cache file already exists */
      struct stat statbuf;
      if (lstat(cache_fname, &statbuf) == 0) {
         Dmsg1(dbglvl, "download_part_to_cache: %s already exists: remove it.", cache_fname);
         if (unlink(cache_fname) < 0) {
            berrno be;
            Dmsg2(dbglvl, "download_part_to_cache: failed to remove file %s. ERR: %s\n",cache_fname, be.bstrerror());
         } else {
            Dmsg1(dbglvl, "=== unlinked: %s\n", cache_fname);
         }
      }

      /* get_xfer either returns a new transfer or a similar one if it already exists.
       * in this case, the transfer is shared and ref_count is incremented. The caller should only care to release()
       * the transfer eventually. The transfer_mgr is in charge of deleting the transfer when no one shares it anymore*/
      item = download_mgr.get_xfer(cloud_size,
                                 download_engine,
                                 cache_fname,/* cache_fname is duplicated in the transfer constructor*/
                                 VolumeName, /* VolumeName is duplicated in the transfer constructor*/
                                 dpart,
                                 driver,
                                 dcr,
                                 NULL); // no proxy on download to cache
      dcr->downloads->append(item);
      /* transfer are queued manually, so the caller has control on when the transfer is scheduled */
      item->queue();

      free_pool_memory(cache_fname);
   }
   return item;
}

/*
 * Note, we might want to make a new download_first_part_to_read()
 *  where it waits for the first part, then this routine
 *  can simply start the other downloads that will be needed, and
 *  we can wait for them in each new open().
 */
bool cloud_dev::download_parts_to_read(DCR *dcr, alist* parts)
{
   intptr_t part;
   transfer *part_1=NULL, *item;
   ilist cachep;
   int64_t size;

   /* Find and download any missing parts for read */
   if (!driver) {
      return false;
   }

   if (!get_cache_volume_parts_list(dcr, getVolCatName(), &cachep)) {
      return false;
   }

   foreach_alist(part, parts) {
      /* TODO: get_cache_sizes is called before; should be an argument */
      size = part_get_size(&cachep, part);
      if (size == 0) {
         item = download_part_to_cache(dcr, getVolCatName(), (int32_t)part);
         if (part == 1) {
            part_1 = item;   /* Keep it, we continue only if the part1 is downloaded */
         }
      } else {
         Dmsg2(dbglvl, "part %ld is already in the cache %lld\n", (int32_t)part, size);
      }
   }

   /* wait for the part.1 */
   if (part_1) {
      wait_end_of_transfer(dcr, part_1);
   }
   return true;
}

uint32_t cloud_dev::get_part(boffset_t ls_offset)
{
   return (uint32_t)(ls_offset>>off_bits);
}

DEVICE *cloud_dev::get_dev(DCR */*dcr*/)
{
   return this;
}

uint32_t cloud_dev::get_hi_addr()
{
   return (uint32_t)(file_addr >> 32);
}

uint32_t cloud_dev::get_low_addr()
{
   return (uint32_t)(file_addr);
}

uint64_t cloud_dev::get_full_addr()
{
   uint64_t pos;
   pos = make_addr(part, get_offset(file_addr));
   return pos;
}

uint64_t cloud_dev::get_full_addr(boffset_t addr)
{
   uint64_t pos;
   pos = make_addr(part, get_offset(addr));
   return pos;
}

#if 0
static transfer* find_transfer(DCR *dcr, const char *VolumeName, uint32_t part)
{
   transfer *item;
   foreach_alist(item, dcr->transfers) {
      if (part == item->m_part && strcmp(item->m_volume_name, VolumeName) == 0) {
         return item;
      }
   }
   return NULL;
}
#endif

/*
 * Make a list of cache sizes and count num_cache_parts
 *
 * TODO: It seems that we are interested only by one part
 * when we call this function. So, it can be enhanced or removed
 */
bool cloud_dev::get_cache_sizes(DCR *dcr, const char *VolumeName)
{
   DIR* dp = NULL;
   struct dirent *entry = NULL;
   struct stat statbuf;
   int name_max;
   POOLMEM *vol_dir = get_pool_memory(PM_NAME);
   POOLMEM *fname = get_pool_memory(PM_NAME);
   uint32_t cpart;
   bool ok = false;
   POOL_MEM dname(PM_FNAME);
   int status = 0;

   /*
    * **FIXME**** do this only once for each Volume.  Currently,
    *  it is done for each part that is opened.
    *  NB : this should be substituted with get_cache_volume_parts_list
    */
   Enter(dbglvl);
   max_cache_size = MAX(part + 1, 100); /* We need to list at least part elements, but it can be more */
   if (cache_sizes) {
      free(cache_sizes);
   }
   cache_sizes = (uint64_t *)malloc(max_cache_size * sizeof(uint64_t));
   memset(cache_sizes, 0, max_cache_size * sizeof(uint64_t));
   num_cache_parts = 0;
   max_cache_part = 0;

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   make_cache_volume_name(vol_dir, VolumeName);
   if (!(dp = opendir(vol_dir))) {
      berrno be;
      Mmsg2(errmsg, "Cannot opendir to get cache sizes. Volume=%s does not exist. ERR=%s\n",
        vol_dir, be.bstrerror());
      Dmsg1(dbglvl, "%s", errmsg);
      goto get_out;
   }

   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);

   for ( ;; ) {
      if (dcr->jcr->is_canceled()) {
         goto get_out;
      }
      errno = 0;
      status = breaddir(dp, dname.addr());
      if (status == -1) {
         break;
      } else if (status > 0) {
         Mmsg1(errmsg, "breaddir failed: ERR=%s", status);
         Dmsg1(dbglvl, "%s\n", errmsg);
         goto get_out;
      }

      /* Always ignore . and .. */
      if (strcmp(".", dname.c_str()) == 0 || strcmp("..", dname.c_str()) == 0) {
         continue;
      }

      /* Look only for part files */
      if (strncmp("part.", dname.c_str(), 5) != 0) {
         continue;
      }

      /* Get size of part */
      Mmsg(fname, "%s/%s", vol_dir, dname.c_str());
      if (lstat(fname, &statbuf) == -1) {
         /* The part is no longer here, might be a truncate in an other thread, just
          * do like if the file wasn't here
          */
	 continue;
      }

      cpart = (int)str_to_int64((char *)&(dname.c_str()[5]));
      Dmsg3(dbglvl+100, "part=%d file=%s fname=%s\n", cpart, dname.c_str(), dname.c_str());
      if (cpart > max_cache_part) {
         max_cache_part = cpart;
      }
      if (cpart >= max_cache_size) {
         int new_max_cache_size = cpart + 100;
         cache_sizes = (uint64_t *)realloc(cache_sizes, new_max_cache_size * sizeof(uint64_t));
         for (int i=max_cache_size; i<(int)new_max_cache_size; i++) {
            cache_sizes[i] = 0;
         }
         max_cache_size = new_max_cache_size;
      }
      num_cache_parts++;
      cache_sizes[cpart] = (uint64_t)statbuf.st_size;
      Dmsg2(dbglvl+100, "found part=%d size=%llu\n", cpart, cache_sizes[cpart]);
   }

   if (chk_dbglvl(dbglvl)) {
      Pmsg1(0, "Cache objects Vol=%s:\n", VolumeName);
      for (int i=1; i <= (int)max_cache_part; i++) {
         Pmsg2(0, "  part num=%d size=%llu\n", i, cache_sizes[i]);
      }
      Pmsg2(0, "End cache obj list: nparts=%d max_cache_part=%d\n",
         num_cache_parts, max_cache_part);
   }
   ok = true;

get_out:
   if (dp) {
      closedir(dp);
   }
   if (entry) {
      free(entry);
   }
   free_pool_memory(vol_dir);
   free_pool_memory(fname);
   return ok;
}

void cloud_dev::make_cache_filename(POOLMEM *&filename,
        const char *VolumeName, uint32_t upart)
{
   Enter(dbglvl);

   pm_strcpy(filename, dev_name);
   cloud_driver::add_vol_and_part(filename, VolumeName, "part", upart);
}

void cloud_dev::make_cache_volume_name(POOLMEM *&volname,
        const char *VolumeName)
{
   Enter(dbglvl);
   POOL_MEM archive_name(PM_FNAME);

   pm_strcpy(archive_name, dev_name);
   if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
      pm_strcat(archive_name, "/");
   }
   pm_strcat(archive_name, VolumeName);
   pm_strcpy(volname, archive_name.c_str());
}

/*
 * DEVICE virtual functions that we redefine.
 */
cloud_dev::~cloud_dev()
{
   Enter(dbglvl);

   cloud_prox->release();

   if (cache_sizes) {
      free(cache_sizes);
      cache_sizes = NULL;
   }
   if (driver) {
      driver->term(errmsg);
      delete driver;
      driver = NULL;
   }
   for(int i=0 ; driver_tab[i].name != NULL; i++) {
      if (driver_tab[i].handle) {
         dlclose(driver_tab[i].handle);
      }
   }
   if (m_fd != -1) {
      if (d_close(m_fd) < 0) {
         berrno be;
         Dmsg1(dbglvl, "Unable to close device. ERR=%s\n", be.bstrerror());
      }
      m_fd = -1;
   }
}

static cloud_driver *load_driver(JCR *jcr, uint driver_type)
{
   POOL_MEM fname(PM_FNAME);
   cloud_driver *dev;
   cloud_driver_item drv;
   const char *slash;
   void *pHandle;
   newCloudDriver_t newDriver;
   
   if (!me->plugin_directory || strlen(me->plugin_directory) == 0) {
      Jmsg1(jcr, M_FATAL, 0,  _("Plugin directory not defined. Cannot load cloud driver %d.\n"),
         driver_type);
      return NULL;
   }

   if (IsPathSeparator(me->plugin_directory[strlen(me->plugin_directory) - 1])) {
      slash = "";
   } else {
      slash = "/";
   }

   drv = driver_tab[driver_type - 1];

   Mmsg(fname, "%s%sbacula-sd-cloud-%s-driver%s%s", me->plugin_directory, slash,
        drv.name, "-" VERSION, DRV_EXT);
   if (!drv.loaded) {
      Dmsg1(10, "Open SD driver at %s\n", fname.c_str());
      pHandle = dlopen(fname.c_str(), RTLD_NOW);
      if (pHandle) {
         Dmsg2(100, "Driver=%s handle=%p\n", drv.name, pHandle);
         /* Get global entry point */
         Dmsg1(10, "Lookup \"BaculaCloudDriver\" in driver=%s\n", drv.name);
         newDriver = (newCloudDriver_t)dlsym(pHandle, "BaculaCloudDriver");
         Dmsg2(10, "Driver=%s entry point=%p\n", drv.name, newDriver);
         if (!newDriver) {
            const char *error = dlerror();
            Jmsg(NULL, M_ERROR, 0, _("Lookup of symbol \"BaculaCloudDriver\" in driver %d for device %s failed: ERR=%s\n"),
               driver_type, fname.c_str(), NPRT(error));
            Dmsg2(10, "Lookup of symbol \"BaculaCloudDriver\" driver=%s failed: ERR=%s\n",
               fname.c_str(), NPRT(error));
            dlclose(pHandle);
            return NULL;
         }
         drv.handle = pHandle;
         drv.loaded = true;
         drv.newDriver = newDriver;
      } else {
         /* dlopen failed */
         const char *error = dlerror();
         Jmsg3(jcr, M_FATAL, 0, _("dlopen of SD driver=%s at %s failed: ERR=%s\n"),
              drv.name, fname.c_str(), NPRT(error));
         Dmsg2(0, "dlopen plugin %s failed: ERR=%s\n", fname.c_str(),
               NPRT(error));
         return NULL;
      }
   } else {
      Dmsg1(10, "SD driver=%s is already loaded.\n", drv.name);
   }

   /* Call driver initialization */
   dev = drv.newDriver();
   return dev;
}

cloud_dev::cloud_dev(JCR *jcr, DEVRES *device)
{
   Enter(dbglvl);
   m_fd = -1;
   *full_type = 0;

   /* Initialize Cloud driver */
   if (!driver) {
      switch (device->cloud->driver_type) {
      case C_S3_DRIVER:
         driver = load_driver(jcr, C_S3_DRIVER);
         break;
      case C_WAS_DRIVER:
      {
         if (!device->cloud->driver_command) {
            POOL_MEM tmp(PM_FNAME);
            Mmsg(tmp, "%s/was_cloud_driver", me->plugin_directory);
            device->cloud->driver_command = bstrdup(tmp.c_str());
         }
         struct stat mstatp;
         if (lstat(device->cloud->driver_command, &mstatp) == 0) {
            driver = load_driver(jcr, C_WAS_DRIVER);
         }
         break;
      }
      case C_GOOGLE_DRIVER:
      {
         if (!device->cloud->driver_command) {
            POOL_MEM tmp(PM_FNAME);
            Mmsg(tmp, "%s/google_cloud_driver", me->plugin_directory);
            device->cloud->driver_command = bstrdup(tmp.c_str());
         }
         struct stat mstatp;
         if (lstat(device->cloud->driver_command, &mstatp) == 0) {
            driver = load_driver(jcr, C_GOOGLE_DRIVER);
         }
         break;
      }
      case C_ORACLE_DRIVER:
      {
         if (!device->cloud->driver_command) {
            POOL_MEM tmp(PM_FNAME);
            Mmsg(tmp, "%s/oracle_cloud_driver", me->plugin_directory);
            device->cloud->driver_command = bstrdup(tmp.c_str());
         }
         struct stat mstatp;
         if (lstat(device->cloud->driver_command, &mstatp) == 0) {
            driver = load_driver(jcr, C_ORACLE_DRIVER);
         }
         break;
      }
      case C_GEN_DRIVER:
      {
         if (!device->cloud->driver_command) {
            POOL_MEM tmp(PM_FNAME);
            Mmsg(tmp, "%s/generic_cloud_driver", me->plugin_directory);
            device->cloud->driver_command = bstrdup(tmp.c_str());
         }
         struct stat mstatp;
         if (lstat(device->cloud->driver_command, &mstatp) == 0) {
            driver = load_driver(jcr, C_GEN_DRIVER);
         }
         break;
      }
      case C_SWIFT_DRIVER:
      {
        if (!device->cloud->driver_command) {
            POOL_MEM tmp(PM_FNAME);
            Mmsg(tmp, "%s/swift_cloud_driver", me->plugin_directory);
            device->cloud->driver_command = bstrdup(tmp.c_str());
         }
         struct stat mstatp;
         if (lstat(device->cloud->driver_command, &mstatp) == 0) {
            driver = load_driver(jcr, C_SWIFT_DRIVER);
         }
         break;
      }
      case C_FILE_DRIVER:
         driver = New(file_driver);
         break;
      default:
         break;
      }
      current_driver_type = 0;
      if (!driver) {
         /* We are outside from a job */
         Qmsg2(jcr, M_FATAL, 0, _("Could not open Cloud driver type=%d for Device=%s.\n"),
            device->cloud->driver_type, device->hdr.name);
         /* We create a dummy driver that will fail all jobs automatically. We do not abort
          * the storage daemon because we can have other device type like tape, dedup or file
          * that are perfectly working. TODO: See if we abort the SD startup here.
          */
         driver = New(dummy_driver);
      } else {
         current_driver_type = device->cloud->driver_type;
      }
      /* Make local copy in device */
      if (device->cloud->upload_limit) {
         driver->upload_limit.set_bwlimit(device->cloud->upload_limit);
      }

      if (device->cloud->download_limit) {
         driver->download_limit.set_bwlimit(device->cloud->download_limit);
      }

      trunc_opt = device->cloud->trunc_opt;
      upload_opt = device->cloud->upload_opt;
      Dmsg2(dbglvl, "Trunc_opt=%d upload_opt=%d\n", trunc_opt, upload_opt);
      if (device->cloud->max_concurrent_uploads) {
         upload_mgr.m_wq.max_workers = device->cloud->max_concurrent_uploads;
      }
      if (device->cloud->max_concurrent_downloads) {
         download_mgr.m_wq.max_workers = device->cloud->max_concurrent_downloads;
      }

      POOL_MEM err;             /* device->errmsg is not yet ready */
      /* Initialize the driver */
      if (!driver->init(device->cloud, err.addr())) {
         Qmsg1(jcr, M_FATAL, 0, "Cloud driver initialization error %s\n", err.c_str());
         Tmsg1(0, "Cloud driver initialization error %s\n", err.c_str());
      }
      bsnprintf(full_type, sizeof(full_type), "Cloud (%s Plugin)", print_driver_type());
   }

   /* the cloud proxy owns its cloud_parts, so we can 'set and forget' them */
   cloud_prox = cloud_proxy::get_instance();
}

int cloud_dev::device_specific_init(JCR *jcr, DEVRES *device)
{
   // Don't forget to do capabilities |= CAP_LSEEK in device_specific_init()
   return file_dev::device_specific_init(jcr, device);
}

/*
 * DEVICE virtuals that we redefine.
 */

static const char *seek_where(int whence)
{
   switch (whence) {
   case SEEK_SET:
      return "SEEK_SET";
   case SEEK_CUR:
      return "SEEK_CUR";
   case SEEK_END:
      return "SEEK_END";
   default:
      return "UNKNOWN";
   }
}


/*
 * Note, we can enter with a full address containing a part number
 *  and an offset or with an offset.  If the part number is zero
 *  at entry, we use the current part.
 *
 * This routine always returns a full address (part, offset).
 *
 */
boffset_t cloud_dev::lseek(DCR *dcr, boffset_t ls_offset, int whence)
{
   boffset_t pos;
   uint32_t new_part;
   boffset_t new_offset;
   char ed1[50];

   if (!dcr) {                  /* can be NULL when called from rewind(NULL) */
      return -1;
   }

   /* Convert input ls_offset into part and off */
   if (ls_offset < 0) {
      return -1;
   }
   new_part = get_part(ls_offset);
   new_offset  = get_offset(ls_offset);
   if (new_part == 0) {
      new_part = part;
      if (new_part == 0) {
         new_part = 1;
      }
   }
   Dmsg6(dbglvl+10, "lseek(%d, %s, %s) part=%d nparts=%d off=%lld\n",
      m_fd, print_addr(ed1, sizeof(ed1), ls_offset), seek_where(whence), part, num_cache_parts, new_offset);
   if (whence != SEEK_CUR && new_part != part) {
      Dmsg2(dbglvl, "new_part=%d part=%d call close_part()\n", new_part, part);
      close_part(dcr);
      part = new_part;
      Dmsg0(dbglvl, "now open_device()\n");
      if (!open_device(dcr, openmode)) {
         return -1;
      }
      ASSERT2(part==new_part, "Big problem part!=new_partn");
   }

   switch (whence) {
   case SEEK_SET:
      /* We are staying in the current part, just seek */
      pos = ::lseek(m_fd, new_offset, SEEK_SET);
      if (pos < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
         Dmsg1(000, "Seek error. ERR=%s\n", errmsg);
         return pos;
      }
      Dmsg4(dbglvl+10, "lseek_set part=%d pos=%s fd=%d offset=%lld\n",
            part, print_addr(ed1, sizeof(ed1), pos), m_fd, new_offset);
      return get_full_addr(pos);

   case SEEK_CUR:
      pos = ::lseek(m_fd, 0, SEEK_CUR);
      if (pos < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
         Dmsg1(000, "Seek error. ERR=%s\n", errmsg);
         return pos;
      }
      Dmsg4(dbglvl+10, "lseek %s fd=%d offset=%lld whence=%s\n",
            print_addr(ed1, sizeof(ed1)), m_fd, new_offset, seek_where(whence));
      return get_full_addr(pos);

   case SEEK_END:
      /*
       * Bacula does not use offsets for SEEK_END
       *  Also, Bacula uses seek_end only when it wants to
       *  append to the volume.
       */
      pos = ::lseek(m_fd, new_offset, SEEK_END);
      if (pos < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
         Dmsg1(000, "Seek error. ERR=%s\n", errmsg);
         return pos;
      }
      Dmsg4(dbglvl+10, "lseek_end part=%d pos=%lld fd=%d offset=%lld\n",
            part, pos, m_fd, new_offset);
      return get_full_addr(pos);

   default:
      Dmsg0(dbglvl, "Seek call error.\n");
      errno = EINVAL;
      return -1;
   }
}

/* use this to track file usage */
bool cloud_dev::update_pos(DCR *dcr)
{
   Enter(dbglvl);
   return file_dev::update_pos(dcr);
}

bool cloud_dev::rewind(DCR *dcr)
{
   Enter(dbglvl);
   Dmsg3(dbglvl, "rewind res=%d fd=%d %s\n", num_reserved(), m_fd, print_name());
   state &= ~(ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   if (m_fd < 0) {
      Mmsg1(errmsg, _("Rewind failed: device %s is not open.\n"), print_name());
      return false;
   }
   if (part != 1) {
      close_part(dcr);
      part = 1;
      if (!open_device(dcr, openmode)) {
         return false;
      }
   }
   if (lseek(dcr, (boffset_t)0, SEEK_SET) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("lseek to 0 error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
      return false;
   }
   file_addr = 0;
   return true;
}

bool cloud_dev::reposition(DCR *dcr, uint64_t raddr)
{
   Enter(dbglvl);
   char ed1[50];
   Dmsg2(dbglvl, "part=%d num_cache_parts=%d\n", part, num_cache_parts);
   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to reposition. Device not open\n"));
      Qmsg0(dcr->jcr, M_FATAL, 0, errmsg);
      return false;
   }

   if (lseek(dcr, (boffset_t)raddr, SEEK_SET) == (boffset_t)-1) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
      return false;
   }
   file_addr = raddr;
   Dmsg1(dbglvl, "=== reposition lseeked to %s\n", print_addr(ed1, sizeof(ed1)));
   return true;
}

#define INTPTR(a) (void*)(intptr_t)(a)

/* Small cloud scanner for the BSR list, we check if all parts are in the cache area. */
class BSRPartScanner {
private:
   DCR    *dcr;
   cloud_dev *dev;
   uint32_t lastPart;                     /* Last part used, mark the part for download only one time */
   alist   *parts;

public:
   BSRPartScanner(DCR *adcr, cloud_dev *adev) {
      dcr = adcr;
      dev = adev;
      lastPart = 0;
      parts = New(alist(100, not_owned_by_alist));
   };

   ~BSRPartScanner() {
      delete parts;
   };

   /* accessor for parts list*/
   alist *get_parts_list() {
      return parts;
   };

   /* We check if the current Parts in the voladdr list are needed
    * The BSR structure should progress forward in the volume.
    */
   void get_parts(BSR_VOLUME *volume, BSR_VOLADDR *voladdr)
   {
      while (voladdr) {
         for (uint32_t part = dev->get_part(voladdr->saddr); part <=dev->get_part(voladdr->eaddr); ++part)
         {
            if (lastPart != part) {
               lastPart = part;
               parts->append(INTPTR(part));
            }
         }
         voladdr = voladdr->next;
      }
   };

   /* Get Volume/Parts that must be downloaded For each MediaType, we must find
    * the right device and check if it's a Cloud device. If we have a cloud device,
    * then we need to check all VolAddress to get the Part list that is associated.
    *
    * It's likely that we will always query the same MediaType and the same
    * Volume.
    */
   void get_all_parts(BSR *bsr, const char *cur_volume)
   {
      bool done=false;
      BSR_VOLUME  *volume;
      parts->destroy();
      /* Always download the part.1 */
      parts->append(INTPTR(1));

      while (bsr) {
         volume = bsr->volume;

         if (strcmp(volume->VolumeName, cur_volume) == 0) {
            get_parts(volume, bsr->voladdr);
            done = true;

         } else if (done) { /* TODO: We can stop when it's no longer the right volume */
            break;
         }

         bsr = bsr->next;
      }

      intptr_t part;
      if (chk_dbglvl(dbglvl)) {
         Dmsg1(0, "Display list of parts to download for volume %s:\n", cur_volume);
         foreach_alist(part, parts) {
            Dmsg2(0, "   Must download part %s/part.%lld\n", cur_volume, (int64_t)part);
         }
      }
   };
};

/* Wait for the download of a particular part */
bool cloud_dev::wait_one_transfer(DCR *dcr, char *VolName, uint32_t part)
{
   dcr->jcr->setJobStatus(JS_CloudDownload);

   transfer *item = download_part_to_cache(dcr, VolName, part);
   if (item) {
      bool ok = wait_end_of_transfer(dcr, item);
      ok &= (item->m_state == TRANS_STATE_DONE);
      dcr->jcr->setJobStatus(JS_Running);

      if (!ok) {
         Qmsg2(dcr->jcr, M_FATAL, 0,
               _("Unable to download Volume=\"%s\"%s.\n"), VolName,
               (part==1)?" label":"");
      }
      return ok;
   } else {
      /* no item to download -> up-to-date */
      return true;
   }
   return false;
}

bool cloud_dev::open_device(DCR *dcr, int omode)
{
   POOL_MEM archive_name(PM_FNAME);
   POOL_MEM part_name(PM_FNAME);
   struct stat sp;

   Enter(dbglvl);
   /* Call base class to define generic variables */
   if (DEVICE::open_device(dcr, omode)) {
      Dmsg2(dbglvl, "fd=%d device %s already open\n", m_fd, print_name());
      Leave(dbglvl);
      return true;
   }
   omode = openmode;

   /* At this point, the device is closed, so we open it */

   /* reset the cloud parts proxy for the current volume */
   /* this can modify errmsg, so it's useful to test errmsg before overwritting it */
   probe_cloud_proxy(dcr, getVolCatName());

   /* Now Initialize the Cache part */
   pm_strcpy(archive_name, dev_name);
   if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
      pm_strcat(archive_name, "/");
   }
   pm_strcat(archive_name, getVolCatName());

   /* If create make directory with Volume name */
   if (part <= 0 && omode == CREATE_READ_WRITE) {
      Dmsg1(dbglvl, "=== makedir=%s\n", archive_name.c_str());
      if (!makedir(dcr->jcr, archive_name.c_str(), 0740)) {
         berrno be;
         if (errmsg[0] == 0) {
            Mmsg2(errmsg, _("Could not make dir %s. %s"), archive_name.c_str(), be.bstrerror());
         }
         Dmsg2(dbglvl, _("Could not make dir %s. %s"), archive_name.c_str(), be.bstrerror());
         Leave(dbglvl);
         return false;
      }
   }
   if (part <= 0) {
      part = 1;                       /* always start from 1 */
   }
   Dmsg2(dbglvl, "part=%d num_cache_parts=%d\n", part, num_cache_parts);

   /*
    * If we are doing a restore, get the necessary parts
    */
   if (dcr->is_reading()) {
      BSRPartScanner scanner(dcr, this);
      scanner.get_all_parts(dcr->jcr->bsr, getVolCatName());
      download_parts_to_read(dcr, scanner.get_parts_list());
   }
   get_cache_sizes(dcr, getVolCatName()); /* refresh with what may have downloaded */

   /* We need to make sure the current part is loaded */
   uint64_t cld_size = cloud_prox->get_size(getVolCatName(), 1);
   if (cache_sizes[1] == 0 && cld_size != 0) {
      if (!wait_one_transfer(dcr, getVolCatName(), 1)) {
         if (errmsg[0] == 0) {
            Mmsg1(errmsg, _("wait for part.1 transfert failed for volume %s"), getVolCatName());
         }
         Dmsg1(dbglvl, _("wait for part.1 transfert failed for volume %s"), getVolCatName());
         return false;
      }
   }

   /* TODO: Merge this part of the code with the previous section */
   cld_size = cloud_prox->get_size(getVolCatName(), part);
   if (dcr->is_reading()
       &&  part > 1
       && (part > max_cache_part || cache_sizes[part] == 0)) /* Out of the existing parts or size == 0 */
   {
      if (!wait_one_transfer(dcr, getVolCatName(), part)) {
         if (errmsg[0] == 0) {
            Mmsg2(errmsg, _("wait for part.%d transfert failed for volume %s"), part, getVolCatName());
         }
         Dmsg2(dbglvl, _("wait for part.%d transfert failed for volume %s"), part, getVolCatName());
         return false;
      }
   }

   Mmsg(part_name, "/part.%d", part);
   pm_strcat(archive_name, part_name.c_str());

   set_mode(omode);
   /* If creating file, give 0640 permissions */
   Dmsg3(DT_CLOUD|10, "open mode=%s open(%s, 0x%x, 0640)\n", mode_to_str(omode),
         archive_name.c_str(), mode);

   /* Use system open() */
   /* NB: Shall we really reset the error message here? */
   errmsg[0] = 0;
   if ((m_fd = ::open(archive_name.c_str(), mode|O_CLOEXEC, 0640)) < 0) {
      berrno be;
      dev_errno = errno;
      if (part == 1 && omode != CREATE_READ_WRITE) {
         part = 0;  /* Open failed, reset part number */
         Mmsg3(errmsg, _("Could not open(%s,%s,0640): ERR=%s\n"),
               archive_name.c_str(), mode_to_str(omode), be.bstrerror());
         Dmsg1(dbglvl, "open failed: %s", errmsg);
      }
   }
   if (m_fd >= 0 && !get_cache_sizes(dcr, getVolCatName())) {
      /* get_cache_sizes should populate errMsg */
      ASSERTD(errmsg[0] != 0, "get_cache_sizes reported error state without errmsg");
      return false;
   }
   /* TODO: Make sure max_cache_part and max_cloud_part are up to date */
   uint32_t max_cloud_part = cloud_prox->last_index(getVolCatName());
   if (can_read() && m_fd < 0 && part > MAX(max_cache_part, max_cloud_part)) {
      Dmsg4(dbglvl, "set_eot: part=%d num_cache_parts=%d max_cache_part=%d max_cloud_part=%d\n",
            part, num_cache_parts, max_cache_part, max_cloud_part);
      set_eot();
   }
   if (m_fd >= 0) {
      if (omode == CREATE_READ_WRITE || omode == OPEN_READ_WRITE) {
         set_append();
      }
      dev_errno = 0;
      file = 0;
      file_addr = 0;
      if (part > num_cache_parts) {
         num_cache_parts = part;
         if (part > max_cache_part) {
            max_cache_part = part;
         }
      }

      /* Refresh the device id and the current file size */
      if (fstat(m_fd, &sp) == 0) {
         devno = sp.st_dev;
         part_size = sp.st_size;

      } else {
          /* Should never happen */
         berrno be;
         Mmsg1(errmsg, _("Could not use fstat on file descriptor. ERR=%s\n") ,be.bstrerror());
         d_close(m_fd);
         m_fd = -1;
      }
   } else if (dcr->jcr) {
      pm_strcpy(dcr->jcr->errmsg, errmsg);
   }
   state |= preserve;                 /* reset any important state info */

   Dmsg3(dbglvl, "fd=%d part=%d num_cache_parts=%d\n", m_fd, part, num_cache_parts);
   Leave(dbglvl);

   /* No errmsg and m_fd < 0 -> let's report unknown error anyway */
   if (m_fd < 0) {
      if (errmsg[0] == 0) {
         Mmsg(errmsg, _("unknown error"));
      }
   }
   return m_fd >= 0;
}

bool cloud_dev::close(DCR *dcr)
{
   Enter(dbglvl);
   bool ok = true;

   Dmsg6(dbglvl, "close_dev vol=%s part=%d fd=%d dev=%p adata=%d dev=%s\n",
      VolHdr.VolumeName, part, m_fd, this, adata, print_name());

   if (!is_open()) {
      //Dmsg2(1000, "Device %s already closed vol=%s\n", print_name(),VolHdr.VolumeName);
      Leave(dbglvl);
      return true;                    /* already closed */
   }

   if (d_close(m_fd) != 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("Error closing device %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
      ok = false;
   }

   unmount(1);                       /* do unmount if required */

   /* Ensure the last written part is uploaded */
   if ((part > 0) && dcr->is_writing()) {
      if (!upload_part_to_cloud(dcr, VolHdr.VolumeName, part, (trunc_opt == TRUNC_AFTER_UPLOAD))) {
         if (errmsg[0]) {
            Qmsg(dcr->jcr, M_ERROR, 0, "%s", errmsg);
         }
      }
   }

   /*
    * Clean up device packet so it can be re-opened
    *
    */
   state &= ~(ST_LABEL|ST_READ|ST_APPEND|ST_EOT|ST_WEOT|ST_EOF|
              ST_NOSPACE|ST_MOUNTED|ST_MEDIA|ST_SHORT);
   label_type = B_BACULA_LABEL;
   clear_opened();
   file = block_num = 0;
   part = 0;
   EndAddr = get_full_addr();
   file_addr = 0;
   EndFile = EndBlock = 0;
   openmode = 0;
   clear_volhdr();
   memset(&VolCatInfo, 0, sizeof(VolCatInfo));
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
   Leave(dbglvl);
   return ok;
}

/* When constructed, jcr_killable_lock captures the jcr current killable state and set it to false.
 * The original state is re-applied at destruction
 */
class jcr_not_killable
{
   JCR *m_jcr;
   bool m_killable;
public:
   jcr_not_killable(JCR* jcr) :
      m_jcr(jcr),
      m_killable(jcr->is_killable())
   {
      if (m_killable) {
         m_jcr->set_killable(false);
      }
   }
   ~jcr_not_killable()
   {
      /* reset killable state */
      m_jcr->set_killable(m_killable);
   }
};

/* update the cloud_proxy at VolName key. Only if necessary or if force-d */
bool cloud_dev::probe_cloud_proxy(DCR *dcr,const char *VolName, bool force)
{
   /* check if the current volume is present in the proxy by probing the label (part.1)*/
   if (!cloud_prox->volume_lookup(VolName)|| force) {
      /* Make sure the Job thread will not be killed in this function */
      jcr_not_killable jkl(dcr->jcr);
      ilist cloud_parts(100, false); /* !! dont own the parts here */
      /* first, retrieve the volume content within cloud_parts list*/
      cancel_callback cancel_cb;
      cancel_cb.fct = DCR_cancel_cb;
      cancel_cb.arg = dcr;
      if (!driver->get_cloud_volume_parts_list(VolName, &cloud_parts, &cancel_cb, errmsg)) {
         Dmsg2(dbglvl, "Cannot get cloud sizes for Volume=%s Err=%s\n", VolName, errmsg);
         return false;
      }

      /* then, add the content of cloud_parts in the proxy table */
      if (!cloud_prox->reset(VolName, &cloud_parts)) {
         Dmsg1(dbglvl, "could not reset cloud proxy for Volume=%s\n", VolName);
         return false;
      }
   }
   return true;
}

/*
 * Truncate cache parts that are also in the cloud
 *  NOTE! We do not delete part.1 so that after this
 *   truncate cache command (really a sort of purge),
 *   the user can still do a restore.
 */
int cloud_dev::truncate_cache(DCR *dcr, const char *VolName, int64_t *size, POOLMEM *&msg)
{
   int i, nbpart=0;
   Enter(dbglvl);
   ilist cache_parts;
   /* init the dev error message */
   errmsg[0] = 0;
   msg[0] = 0;
   POOLMEM *vol_dir = get_pool_memory(PM_NAME);
   POOLMEM *fname = get_pool_memory(PM_NAME);

   /* cache truncation is expected to be infrequent and not performance intensive.
    * However deleting parts from the cache that would not be present on the cloud results
    * in data lost, so it's high risk, altough it should not occurs in normal use, to have 
    * a de-sync cloud_proxy. For extra safety we do FORCE the cloud_proxy probing 
    * to cover out-of-band manual deletions b.e. */
   if (!probe_cloud_proxy(dcr, VolName, true)) {
      if (errmsg[0] == 0) {
         Mmsg1(errmsg, "Truncate cache cannot get cache volume parts list for Volume=%s\n", VolName);
      } 
      Mmsg1(msg, "Truncate cache cannot get cache volume parts list for Volume=%s\n", VolName);
      Dmsg1(dbglvl, "%s\n", errmsg);
      nbpart = -1;
      goto bail_out;
   }

   if (!get_cache_volume_parts_list(dcr, VolName, &cache_parts)) {
      if (errmsg[0] == 0) {
         Mmsg1(errmsg, "Truncate cache cannot get cache volume parts list for Volume=%s\n", VolName);
      }
      Mmsg1(msg, "Truncate cache cannot get cache volume parts list for Volume=%s\n", VolName);
      Dmsg1(dbglvl, "%s\n", errmsg);
      nbpart = -1;
      goto bail_out;
   }

   make_cache_volume_name(vol_dir, VolName);

   /*
    * Remove every cache part that is also in the cloud
    */
   for (i=2; i <= (int)cache_parts.last_index(); i++) {
      int64_t cache_size = part_get_size(&cache_parts, i);
      int64_t cloud_size = cloud_prox->get_size(VolName, i);

      /* remove cache parts that are empty or cache parts with matching cloud_part size*/
      if (cache_size != 0 && cache_size != cloud_size) {
         Dmsg3(dbglvl, "Skip truncate for part=%d size mismatch scloud=%lld scache=%lld\n", i, cloud_size, cache_size);
         Mmsg(msg, "Some part(s) couldn't be truncated from the cache because they are inconsistent with the cloud.");
         continue;
      }

      /* Look in the transfer list if we have a download/upload for the current volume */
      if (download_mgr.find(VolName, i)) {
         Dmsg1(dbglvl, "Skip truncate for part=%d because it's transfering\n", i);
         Mmsg(msg, "Some part(s) couldn't be truncated from the cache because they are still transferring.");
         continue;
      }

      Mmsg(fname, "%s/part.%d", vol_dir, i);
      if (unlink(fname) < 0) {
         berrno be;
         Mmsg2(errmsg, "Truncate cache failed to remove file %s. ERR: %s\n", fname, be.bstrerror());
         Dmsg1(dbglvl, "%s\n", errmsg);
      } else {
         *size = *size + cache_size;
         nbpart++;
         Dmsg1(dbglvl, "=== unlinked: part=%s\n", fname);
      }
   }
bail_out:
   free_pool_memory(vol_dir);
   free_pool_memory(fname);
   Leave(dbglvl);
   return nbpart;
}

/* callback called by the driver to test file for deletion */
/* return true if file should be deleted, false otherwise */
bool test_cleanup_file(const char* file, cleanup_ctx_type* ctx)
{
   if (ctx) {
      int64_t tag=0;
      int32_t index=0;
      bool ret = (ctx->pattern_num == scan_string(file, ctx->pattern, &tag, &index));
      return ret;
   }
   return false;
}
/*
 * Truncate both cache and cloud
 */
bool cloud_dev::truncate(DCR *dcr)
{
   DIR* dp = NULL;
   struct dirent *entry = NULL;
   int name_max;
   POOLMEM *vol_dir = get_pool_memory(PM_NAME);
   POOLMEM *fname = get_pool_memory(PM_NAME);
   bool ok = false;
   POOL_MEM dname(PM_FNAME);
   int status = 0;
   ilist * iuploads=New(ilist(100,true)); /* owns the parts */
   ilist *truncate_list=NULL;
   FILE *fp;
   errmsg[0] = 0;
   Enter(dbglvl);

   /* Make sure the Job thread will not be killed in this function */
   jcr_not_killable jkl(dcr->jcr);

   if (cache_sizes) {
      free(cache_sizes);
      cache_sizes = NULL;
   }
   num_cache_parts = 0;
   max_cache_part = 0;
   part = 0;
   if (m_fd) {
      ::close(m_fd);            /* No need to check the return code, we truncate */
      m_fd = -1;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   make_cache_volume_name(vol_dir, getVolCatName());
   Dmsg1(dbglvl, "===== truncate: %s\n", vol_dir);
   if (!(dp = opendir(vol_dir))) {
      berrno be;
      Mmsg2(errmsg, "Cannot opendir to get cache sizes. Volume %s does not exist. ERR=%s\n",
        vol_dir, be.bstrerror());
      Dmsg1(dbglvl, "%s\n", errmsg);
      goto get_out;
   }

   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   for ( ;; ) {
      errno = 0;
      status = breaddir(dp, dname.addr());
      if (status == -1) {
         break;
      } else if (status > 0) {
         Mmsg1(errmsg, "breaddir failed: status=%d", status);
         Dmsg1(dbglvl, "%s\n", errmsg);
         goto get_out;
      }

      /* Always ignore . and .. */
      if (strcmp(".", dname.c_str()) == 0 || strcmp("..", dname.c_str()) == 0) {
         continue;
      }

      /* Look only for part files */
      if (strncmp("part.", dname.c_str(), 5) != 0) {
         continue;
      }
      Mmsg(fname, "%s/%s", vol_dir, dname.c_str());
      if (unlink(fname) < 0) {
         berrno be;
         Mmsg2(errmsg, "Failed to remove file %s ERR: %s\n", fname, be.bstrerror());
         Dmsg1(dbglvl, "%s\n", errmsg);
         goto get_out;
      } else {
         Dmsg1(dbglvl, "=== unlinked: part=%s\n", fname);
      }
   }

   /* All parts have been unlinked. Recreate an empty part.1
    * FIX MT3450:Fatal error: Failed to re-open device after truncate on Cloud device */
   Dmsg1(dbglvl, "Recreate empty part.1 for volume: %s\n", vol_dir);
   Mmsg(fname, "%s/part.1", vol_dir);
   fp = bfopen(fname, "a");
   if (!fp || fclose(fp) != 0) {
      berrno be;
      Mmsg2(errmsg, "Failed to create empty file %s ERR: %s\n", fname,
            be.bstrerror());
   }

   if (!dir_get_volume_info(dcr, getVolCatName(), GET_VOL_INFO_FOR_READ)) {
      /* It may happen for label operation */
      Dmsg2(100, "dir_get_vol_info failed for vol=%s: %s\n", getVolCatName(), dcr->jcr->errmsg);
      goto get_out;
   }

   /* Update the Catalog information */
   dcr->VolCatInfo.VolCatParts = 0;
   dcr->VolCatInfo.VolLastPartBytes = 0;
   dcr->VolCatInfo.VolCatCloudParts = 0;

   openmode = CREATE_READ_WRITE;
   if (!open_next_part(dcr)) {
      goto get_out;
   }

   /* check if the current volume is present in the proxy */
   if (!probe_cloud_proxy(dcr, getVolCatName())) {
      goto get_out;
   }

   /* wrap the uploads in a parts ilist */
   transfer *tpkt;
   foreach_alist(tpkt, dcr->uploads) {
      /* convert xfer into part when VolName match*/
      if (strcmp(tpkt->m_volume_name,getVolCatName())!=0) {
         continue;
      }
      cloud_part *part = (cloud_part*) malloc(sizeof(cloud_part));
      part->index = tpkt->m_part;
      part->mtime = tpkt->m_res_mtime;
      part->size  = tpkt->m_res_size;
      if (tpkt->m_hash64) {
         memcpy(part->hash64, tpkt->m_hash64, 64);
      } else {
         bmemzero(part->hash64, 64);
      }
      iuploads->put(part->index, part);
   }
   /* returns the list of items to truncate : cloud parts-uploads*/
   cancel_callback cancel_cb;
   cancel_cb.fct = DCR_cancel_cb;
   cancel_cb.arg = dcr;
   truncate_list = cloud_prox->exclude(getVolCatName(), iuploads);
   if (truncate_list && !driver->truncate_cloud_volume(getVolCatName(), truncate_list, &cancel_cb, errmsg)) {
      Dmsg1(dbglvl, "%s", errmsg);
      Qmsg(dcr->jcr, M_ERROR, 0, "%s", errmsg);
      goto get_out;
   } else {
      Dmsg1(dbglvl, "%s", errmsg);
   }

   /* cleanup temporary copy files. */
   cleanup_ctx_type ctx;
   Mmsg(fname, "%s/part%%lld.%%d", getVolCatName());
   ctx.pattern = fname;
   ctx.pattern_num = 2;

   if (!driver->clean_cloud_volume(getVolCatName(), &test_cleanup_file, &ctx, &cancel_cb, errmsg)) {
      Dmsg1(dbglvl, "%s", errmsg);
      Qmsg(dcr->jcr, M_ERROR, 0, "%s", errmsg);
      goto get_out;
   } else {
      Dmsg1(dbglvl, "%s", errmsg);
   }

   /* force proxy refresh (volume should be empty so it should be fast) */
   /* another approach would be to reuse truncate_list to remove items */
   if (!probe_cloud_proxy(dcr, getVolCatName(), true)) {
      goto get_out;
   }
   /* check content of the list : only index should be available */
   for(uint32_t index=1; index<=cloud_prox->last_index(getVolCatName()); index++ ) {
      if (cloud_prox->get(getVolCatName(), index)) {
         Dmsg2(0, "truncate_cloud_volume proxy for volume %s got part.%d should be empty\n", getVolCatName(), index);
         Qmsg(dcr->jcr, M_WARNING, 0, "truncate_cloud_volume: %s/part.%d is still present\n", getVolCatName(), index);
      }
   }
   ok = true;

get_out:
   if (dp) {
      closedir(dp);
   }
   if (entry) {
      free(entry);
   }
   free_pool_memory(vol_dir);
   free_pool_memory(fname);

   delete iuploads;
   delete truncate_list;

   Leave(dbglvl);
   return ok;
}


int cloud_dev::read_dev_volume_label(DCR *dcr)
{
   int stat;
   Enter(dbglvl);
   Dmsg2(dbglvl, "part=%d num_cache_parts=%d\n", part, num_cache_parts);
   if (!is_open()) {
      part = 0;
   }
   stat = file_dev::read_dev_volume_label(dcr);
   Dmsg2(dbglvl, "part=%d num_cache_parts=%d\n", part, num_cache_parts);
   return stat;
}

const char *cloud_dev::print_type()
{
   return "Cloud";
}

const char *cloud_dev::print_driver_type()
{
   return cloud_driver_type_name[current_driver_type];
}

const char *cloud_dev::print_full_type()
{
   return full_type;
}

/*
 * makedir() is a lightly modified copy of the same function
 *   in findlib/mkpath.c
 *
 */
bool makedir(JCR *jcr, char *path, mode_t mode)
{
   struct stat statp;

   if (mkdir(path, mode) != 0) {
      berrno be;
      if (lstat(path, &statp) != 0) {
         Qmsg2(jcr, M_ERROR, 0, _("Cannot create directory %s: ERR=%s\n"),
              path, be.bstrerror());
         return false;
      } else if (!S_ISDIR(statp.st_mode)) {
         Qmsg1(jcr, M_ERROR, 0, _("%s exists but is not a directory.\n"), path);
         return false;
      }
      return true;                 /* directory exists */
   }
   return true;
}

/*
 * This call closes the device, but it is used for part handling
 *  where we close one part and then open the next part. The
 *  difference between close_part() and close() is that close_part()
 *  saves the state information of the device (e.g. the Volume label,
 *  the Volume Catalog record, ...  This permits opening and closing
 *  the Volume parts multiple times without losing track of what the
 *  main Volume parameters are.
 */
bool cloud_dev::close_part(DCR *dcr)
{
   bool ok = true;

   Enter(dbglvl);
   Dmsg5(dbglvl, "close_part vol=%s fd=%d dev=%p adata=%d dev=%s\n",
      VolHdr.VolumeName, m_fd, this, adata, print_name());

   if (!is_open()) {
      //Dmsg2(1000, "Device %s already closed vol=%s\n", print_name(),VolHdr.VolumeName);
      Leave(dbglvl);
      return true;                    /* already closed */
   }

   if (d_close(m_fd) != 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("Error closing device %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
      ok = false;
   }

   m_fd = -1;
   part = 0;
   file_addr = 0;
   Leave(dbglvl);
   return ok;
}

bool cloud_dev::open_next_part(DCR *dcr)
{
   Enter(dbglvl);
   int save_part;
   char ed1[50];

   Dmsg4(dbglvl, "open next: part=%d part_size=%d, can_append()=%s, openmode=%d\n", part, part_size, can_append() ? "true":"false", openmode);
   /* When appending, do not open a new part if the current is empty */
   if (can_append() && (part_size == 0)) {
      Dmsg2(dbglvl, "open next: part=%d num_cache_parts=%d exit OK no new part needed.\n", part, num_cache_parts);
      Leave(dbglvl);
      return true;
   }

   /* TODO: Get the the last max_part */
   uint32_t max_cloud_part = cloud_prox->last_index(getVolCatName());
   Dmsg2(dbglvl, "open next: part=%d max_cloud_part=%d\n", part, max_cloud_part);
   if (!can_append() && part >= MAX(max_cache_part, max_cloud_part)) {
      Dmsg3(dbglvl, "EOT: part=%d num_cache_parts=%d max_cloud_part=%d\n", part, num_cache_parts, max_cloud_part);
      Mmsg2(errmsg, "part=%d no more parts to read. addr=%s\n", part,
         print_addr(ed1, sizeof(ed1), EndAddr));
      Dmsg1(dbglvl, "%s", errmsg);
      part = 0;
      Leave(dbglvl);
      return false;
   }

   save_part = part;
   if (!close_part(dcr)) {               /* close current part */
      POOL_MEM tmp;
      Leave(dbglvl);
      Mmsg(tmp, " close_part failed: part=%d num_cache_parts=%d\n", part, num_cache_parts);
      pm_strcat(errmsg, tmp);
      Dmsg1(dbglvl, "%s", errmsg);
      return false;
   }
   if (openmode == CREATE_READ_WRITE) {
      VolCatInfo.VolCatParts = num_cache_parts;
      if (!dir_update_volume_info(dcr, false, false, true)) {
         Dmsg0(dbglvl, "Error from update_vol_info.\n");
         dev_errno = EIO;
         return false;
      }
      part_size = 0;
   }

   /* Restore part number */
   part = save_part;

   if (dcr->is_reading()) {
      wait_one_transfer(dcr, getVolCatName(), part);
   }

   /* Write part to cloud */
   Dmsg2(dbglvl, "=== part=%d num_cache_parts=%d\n", part, num_cache_parts);
   if (dcr->is_writing()) {
      if (!upload_part_to_cloud(dcr, getVolCatName(), part, (trunc_opt == TRUNC_AFTER_UPLOAD))) {
         if (errmsg[0]) {
            Qmsg(dcr->jcr, M_ERROR, 0, "%s", errmsg);
         }
      }
   }

   /* Try to open next part */
   part++;
   Dmsg2(dbglvl, "=== inc part: part=%d num_cache_parts=%d\n", part, num_cache_parts);
   if (can_append()) {
      Dmsg0(dbglvl, "Set openmode to CREATE_READ_WRITE\n");
      openmode = CREATE_READ_WRITE;
   }
   if (open_device(dcr, openmode)) {
      if (openmode == CREATE_READ_WRITE) {
         set_append();
         clear_eof();
         clear_eot();
         file_addr = 0;
         file_addr = get_full_addr();
         if (lseek(dcr, file_addr, SEEK_SET) < 0) {
            berrno be;
            dev_errno = errno;
            Mmsg2(errmsg, _("lseek to 0 error on %s. ERR=%s.\n"),
                  print_name(), be.bstrerror());
            Leave(dbglvl);
            return false;
         }
      }
   } else {                               /* open failed */
      /* TODO: Make sure max_cache_part and max_cloud_part are up to date */
      if (part > MAX(max_cache_part, max_cloud_part)) {
         Dmsg4(dbglvl, "set_eot: part=%d num_cache_parts=%d max_cache_part=%d max_cloud_part=%d\n",
               part, num_cache_parts, max_cache_part, max_cloud_part);
         set_eof();
         set_eot();
      }
      Leave(dbglvl);
      Mmsg2(errmsg, "EOT: part=%d num_cache_parts=%d\n", part, num_cache_parts);
      Dmsg1(dbglvl, "%s", errmsg);
      return false;
   }

   set_labeled();  /* all parts are labeled */

   Dmsg3(dbglvl, "opened next: append=%d part=%d num_cache_parts=%d\n", can_append(), part, num_cache_parts);
   Leave(dbglvl);
   return true;
}


/* Print the object address */
char *cloud_dev::print_addr(char *buf, int32_t buf_len)
{
   uint64_t full_addr = get_full_addr();
   buf[0] = 0;
   bsnprintf(buf, buf_len, "%lu:%llu", get_part(full_addr), get_offset(full_addr));
   return buf;
}

char *cloud_dev::print_addr(char *buf, int32_t buf_len, boffset_t addr)
{
   buf[0] = 0;
   bsnprintf(buf, buf_len, "%lu:%llu", get_part(addr), get_offset(addr));
   return buf;
}

/*
 * Check if the current position on the volume corresponds to
 *  what is in the catalog.
 *
 */
bool cloud_dev::is_eod_valid(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   ilist cache_parts;
   bool do_update = false;
   bool ok = true;
   POOL_MEM err, tmp;

   /* We need up to date information for Cloud and Cache */
   uint32_t max_cloud_part = cloud_prox->last_index(dcr->VolumeName);
   uint64_t last_cloud_size = cloud_prox->get_size(dcr->VolumeName, max_cloud_part);

   get_cache_volume_parts_list(dcr, dcr->VolumeName, &cache_parts);
   uint32_t max_cache_part = cache_parts.last_index();
   uint64_t last_cache_size = part_get_size(&cache_parts, max_cache_part);

   /* When we open a new part, the actual size is 0, so we are not very interested */
   if (last_cache_size == 0 && max_cache_part > 0) {
      max_cache_part--;
      last_cache_size = part_get_size(&cache_parts, max_cache_part);
   }

   uint32_t last_p = MAX(max_cloud_part, max_cache_part);
   uint64_t last_s = MAX(last_cache_size, last_cloud_size);

   Dmsg5(dbglvl, "vol=%s cache part=%ld size=%lld, cloud part=%ld size=%lld\n",
         dcr->VolumeName, max_cache_part, last_cache_size, max_cloud_part, last_cloud_size);

   /* If we have the same Part number in the cloud and in the cache. We check
    * the size of the two parts. The cache part may be truncated (size=0).
    */
   if (max_cloud_part == max_cache_part) {
      if (last_cache_size > 0 && last_cloud_size != last_cache_size) {
         ok = false;            /* Big consistency problem, which one do we take? Biggest one? */
         Mmsg(tmp, "For the last Part=%ld the Cache and Cloud sizes are not the same! Cache=%lld Cloud=%lld.\n",
              max_cloud_part, last_cloud_size, last_cache_size);
         pm_strcat(err, tmp.c_str());
      }
   }

   /* The catalog should have the right LastPart */
   if (VolCatInfo.VolCatParts != last_p) {
      Mmsg(tmp, "The number of parts do not match! Volume=%ld Catalog=%ld.\n",
           last_p, VolCatInfo.VolCatParts);
      VolCatInfo.VolCatParts = last_p;
      VolCatInfo.VolLastPartBytes = last_s;
      VolCatInfo.VolCatBytes = last_s;
      pm_strcat(err, tmp.c_str());
      do_update = true;

   /* The catalog should have the right LastPartBytes */
   } else if (VolCatInfo.VolLastPartBytes != last_s) {
      Mmsg(tmp, "Sizes of last part number=%ld do not match! Volume=%lld Catalog=%lld.\n",
           last_p, VolCatInfo.VolLastPartBytes, last_s);
      VolCatInfo.VolLastPartBytes = last_s;
      VolCatInfo.VolCatBytes = last_s;
      pm_strcat(err, tmp.c_str());
      do_update = true;
   }
   /* We also check that the last part uploaded in the cloud is correct */
   if (VolCatInfo.VolCatCloudParts != max_cloud_part) {
      Mmsg(tmp, "Number of Cloud Parts do not match! Volume=%ld Catalog=%ld.\n",
           max_cloud_part, VolCatInfo.VolCatCloudParts);
      /* FIXME: The VolCatCloudParts is not used in the code and the value is not correct
       * VolCatInfo.VolCatCloudParts = max_cloud_part;
       */
      pm_strcat(err, tmp.c_str());
      do_update = true;
   }
   if (ok) {
      if (do_update) {
         Jmsg2(jcr, M_INFO, 0, _("Correcting catalog for Volume \"%s\":\n%s\n"), dcr->VolumeName, err.c_str());
         if (!dir_update_volume_info(dcr, false, true)) {
            Jmsg(jcr, M_WARNING, 0, _("Error updating Catalog\n"));
            dcr->mark_volume_in_error();
            return false;
         }
      }
   } else {
      Mmsg2(jcr->errmsg, _("Bacula cannot write on disk Volume \"%s\" because: %s"),
            dcr->VolumeName, err.c_str());
      Jmsg(jcr, M_ERROR, 0, jcr->errmsg);
      Dmsg0(100, jcr->errmsg);
      dcr->mark_volume_in_error();
      return false;
   }
   return true;
}

/*
 * We are called here when Bacula wants to append to a Volume
 */
bool cloud_dev::eod(DCR *dcr)
{
   bool ok;
   uint32_t max_part = 1;
   Enter(dbglvl);

   uint32_t max_cloud_part = cloud_prox->last_index(getVolCatName());
   Dmsg5(dbglvl, "=== eod: part=%d num_cache_parts=%d max_cache_part=%d max_cloud_part=%d vol_parts=%d\n",
          part, num_cache_parts, max_cache_part,
          max_cloud_part, VolCatInfo.VolCatParts);

   /* First find maximum part */
   if (max_part < max_cache_part) {
      max_part = max_cache_part;
   }
   if (max_part < max_cloud_part) {
      max_part = max_cloud_part;
   }
   if (max_part < VolCatInfo.VolCatParts) {
      max_part = VolCatInfo.VolCatParts;
   }
   if (max_part < VolCatInfo.VolCatCloudParts) {
      max_part = VolCatInfo.VolCatCloudParts;
   }
   if (part < max_part) {
      if (!close_part(dcr)) {               /* close current part */
         Leave(dbglvl);
         Dmsg2(dbglvl, "close_part failed: part=%d num_cache_parts=%d\n", part, num_cache_parts);
         return false;
      }
      /* Try to open next part */
      part = max_part;
      /* Create new part */
      part_size = 0;
      part++;
      openmode = CREATE_READ_WRITE;
      Dmsg2(dbglvl, "=== eod: set part=%d num_cache_parts=%d\n", part, num_cache_parts);
      if (!open_device(dcr, openmode)) {
         Leave(dbglvl);
         Dmsg2(dbglvl, "Fail open_device: part=%d num_cache_parts=%d\n", part, num_cache_parts);
         return false;
      }
      if (part > 1) {
         /* When going to EOD with Cloud, we close the current part (that can
          * be part.1 with the label), and we open the last part file
          * available.  close_part()/open_device() is clearing the labeled
          * flag, and we absolutely need this flag in some cases, for example
          * after an incomplete job. Like in open_next_part(), we assume that
          * if we are beyond the part.1, the volume is labeled. 
          */
         set_labeled();
      }
   }
   ok = file_dev::eod(dcr);
   return ok;
}

bool cloud_dev::write_volume_label(DCR *dcr,
                  const char *VolName, const char *PoolName,
                  bool relabel, bool no_prelabel)
{
   bool ok = DEVICE::write_volume_label(dcr,
               VolName, PoolName, relabel, no_prelabel);
   if (!ok) {
      Dmsg0(dbglvl, "write_volume_label failed.\n");
      return false;
   }
   if (part != 1) {
      Dmsg1(000, "Big problem!!! part=%d, but should be 1\n", part);
      return false;
   }
   set_append();
   return true;
}

bool cloud_dev::rewrite_volume_label(DCR *dcr, bool recycle)
{
   Enter(100);
   bool ok = DEVICE::rewrite_volume_label(dcr, recycle);
   /*
    * Normally, at this point, the label has been written to disk
    *  but remains in the first part of the block, and will be
    *  "rewritten" when the full block is written.
    * However, in the case of a cloud device the label has
    *  already been written to a part, so we must now clear
    *  the block of the label data.
    */
   empty_block(dcr->block);
   if (!ok || !open_next_part(dcr)) {
      ok = false;
   }
   Leave(100);
   return ok;
}

bool cloud_dev::do_size_checks(DCR *dcr, DEV_BLOCK *block)
{
   if (!DEVICE::do_size_checks(dcr, block)) {
      return false;
   }

   /*
    *  Do Cloud specific size checks
    */
   /* Limit maximum part size to value specified by user */
   if (max_part_size > 0 && ((part_size + block->binbuf) >= max_part_size)) {
      if (part < num_cache_parts) {
         Qmsg3(dcr->jcr, M_FATAL, 0, _("Error while writing, current part number"
               " is less than the total number of parts (%d/%d, device=%s)\n"),
               part, num_cache_parts, print_name());
         dev_errno = EIO;
         return false;
      }

      if (!open_next_part(dcr)) {
         return false;
      }
   }

   // static, so it's not calculated everytime
   static uint64_t hard_max_part_size = ((uint64_t)1 << off_bits) -1;
   static uint32_t hard_max_part_number = ((uint32_t)1 << part_bits) -1;

   if (part_size >= hard_max_part_size) {
      Qmsg3(dcr->jcr, M_FATAL, 0, _("Error while writing, current part size"
            " is greater than the maximum part size (%d>%d, device=%s)\n"),
            part_size, hard_max_part_size, print_name());
      dev_errno = EIO;
      return false;
   }

   if (part >= hard_max_part_number) {
      Qmsg3(dcr->jcr, M_FATAL, 0, _("Error while writing, current part number"
            " is greater than the maximum part number (%d>%d, device=%s)\n"),
            part, hard_max_part_number, print_name());
      dev_errno = EIO;
      return false;
   }

   return true;
}

bool cloud_dev::start_of_job(DCR *dcr)
{
   bool ret=false;
   if (driver) {
      ret = driver->start_of_job(errmsg);
   } else {
      Mmsg(errmsg, "Cloud driver not properly loaded"); /* We should always have a dummy driver */
   }
   Jmsg(dcr->jcr, ret? M_INFO : M_FATAL, 0, "%s\n", errmsg);
   return ret;
}


/* Two jobs can try to update the catalog information for a given cloud
 * volume. It might be avoided by converting the vol_info_mutex to a recursive
 * lock
*/
static pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;

static void update_volume_record(DCR *dcr, transfer *ppkt)
{
   lock_guard lg(update_mutex); /* automatically released at exit */
   bool do_update=false;
   /*
    * At this point ppkt should have the last part for the
    *  previous volume, so update the Media record.
    */
   if (!dir_get_volume_info(dcr, ppkt->m_volume_name, GET_VOL_INFO_FOR_READ)) {
      /* It may happen for label operation */
      Dmsg2((ppkt->m_part == 1 ? 100 : 0) , "dir_get_vol_info failed for vol=%s: %s\n",
            ppkt->m_volume_name, dcr->jcr->errmsg);
      return;
   }

   /* Between the GET and the UPDATE, and other job can call the same
    * function and put more up to date information. So we are protected
    * by the update_mutex
    */
   /* Update the Media information */
   if ((ppkt->m_part > dcr->VolCatInfo.VolCatParts) ||
       (ppkt->m_part == dcr->VolCatInfo.VolCatParts && dcr->VolCatInfo.VolLastPartBytes != ppkt->m_stat_size))
   {
      do_update=true;
      dcr->VolCatInfo.VolCatParts = ppkt->m_part;
      dcr->VolCatInfo.VolLastPartBytes = ppkt->m_stat_size;
   }
   /* We update the CloudParts in the catalog only if the current transfer is correct */
   if (ppkt->m_state == TRANS_STATE_DONE && ppkt->m_part > dcr->VolCatInfo.VolCatCloudParts && ppkt->m_stat_size > 0) {
      do_update = true;
      dcr->VolCatInfo.VolCatCloudParts = ppkt->m_part;
   }
   if (do_update) {
      dir_update_volume_info(dcr, false, true, true/*use_dcr*/);
   }
}

bool cloud_dev::end_of_job(DCR *dcr, uint32_t truncate)
{
   Enter(dbglvl);
   transfer *tpkt;            /* current packet */
   transfer *ppkt=NULL;       /* previous packet */
   const char *prefix = "";

   /* before waiting on transfers, we might have to lauch the uploads */
   if (upload_opt == UPLOAD_AT_ENDOFJOB) {
      foreach_alist(tpkt, dcr->uploads) {
         tpkt->queue();
      }
   }

   /*
      * We wait for each of our uploads to complete
      * Note: we also want to update the cloud parts and cache parts for
      *   each part uploaded.  The deletes list contains transfer packet for
      *   each part that was upload in the order of the parts as they were
      *   created. Also, there may be multiple Volumes that were uploaded,
      *   so for each volume, we search until the end of the list or a
      *   different Volume is found in order to find the maximum part
      *   number that was uploaded.  Then we read the Media record for
      *   that Volume, update it, and write it back to the catalog.
      */
   POOL_MEM msg(PM_MESSAGE);
   if (!dcr->downloads->empty()) {
      if (!dcr->jcr->is_internal_job()) {
         Jmsg(dcr->jcr, M_INFO, 0, _("Cloud Download transfers:\n"));
      } else {
         prefix = "3000 Cloud Download: ";
      }
      Dmsg1(dbglvl, "%s", msg.c_str());
      foreach_alist(tpkt, dcr->downloads) {
         /* Do we really need to wait on downloads : if we didn't
          * wait for them until now, we basically didn't use them. And we
          * surelly won't anymore.  If the job is canceled we can cancel our
          * own downloads (do not touch downloads shared with other jobs).
          */
         wait_end_of_transfer(dcr, tpkt);
         POOL_MEM dmsg(PM_MESSAGE);
         tpkt->append_status(dmsg);
         Jmsg(dcr->jcr, M_INFO, 0, "%s%s", prefix, dmsg.c_str());
         download_mgr.release(tpkt);
      }
   }
   dcr->downloads->destroy();

   if (!dcr->uploads->empty()) {
      int oldstatus = dcr->jcr->JobStatus;
      dcr->jcr->sendJobStatus(JS_CloudUpload);
      if (!dcr->jcr->is_internal_job()) {
         Jmsg(dcr->jcr, M_INFO, 0, _("Cloud Upload transfers:\n"));
      } else {
         prefix = "3000 Cloud Upload: ";
      }
      foreach_alist(tpkt, dcr->uploads) {
         wait_end_of_transfer(dcr, tpkt);
         POOL_MEM umsg(PM_MESSAGE);
         tpkt->append_status(umsg);
         Jmsg(dcr->jcr, (tpkt->m_state == TRANS_STATE_ERROR) ? M_ERROR : M_INFO, 0, "%s%s", prefix, umsg.c_str());
         Dmsg1(dbglvl, "%s", umsg.c_str());
         bool do_truncate = (truncate==TRUNC_AT_ENDOFJOB) || (truncate==TRUNC_CONF_DEFAULT && trunc_opt==TRUNC_AT_ENDOFJOB);
         if (tpkt->m_state != TRANS_STATE_DONE) {
            Mmsg(dcr->jcr->StatusErrMsg, _("Upload to Cloud failed"));
         } else if (do_truncate && tpkt->m_part!=1) {
            /* else -> don't remove the cache file if the upload failed */
            if (unlink(tpkt->m_cache_fname) != 0) {
               berrno be;
               Dmsg2(dbglvl, "Truncate cache option at end of job. Unable to delete %s. ERR=%s\n", tpkt->m_cache_fname, be.bstrerror());
            } else {
               Dmsg1(dbglvl, "Truncate cache option at end of job. Unlink file %s\n", tpkt->m_cache_fname);
            }
         }

         if (ppkt == NULL) {
            ppkt = tpkt;
            continue;
         }
         if (strcmp(ppkt->m_volume_name, tpkt->m_volume_name) == 0) {
            ppkt = tpkt;
            continue;
         }
         /* vol name changed so update media for previous transfer */
         update_volume_record(dcr, ppkt);
         ppkt = tpkt;
      }
      dcr->jcr->sendJobStatus(oldstatus);
   }

   /* Update the last (previous) one */
   if (ppkt) {
      Dmsg3(dbglvl, "== Last part=%d size=%lld Volume=%s\n", ppkt->m_part,
         ppkt->m_stat_size, ppkt->m_volume_name);
      update_volume_record(dcr, ppkt);
      Dmsg3(dbglvl, "=== Very Last part=%d size=%lld Volume=%s\n", ppkt->m_part,
         ppkt->m_stat_size, ppkt->m_volume_name);
   }

   /* Now, clear our list and the global one if needed */
   foreach_alist(tpkt, dcr->uploads) {
      upload_mgr.release(tpkt);
   }
   dcr->uploads->destroy();

   if (driver) {
      driver->end_of_job(errmsg);
   }

   Leave(dbglvl);
   return true;
}

bool cloud_dev::wait_end_of_transfer(DCR *dcr, transfer *elem)
{
   if (!elem) {
      return false;
   }

   Enter(dbglvl);
   struct timeval tv;
   tv.tv_usec = 0;
   tv.tv_sec = 30;

   int stat = ETIMEDOUT;
   while (stat == ETIMEDOUT) {

      if (dcr->jcr->is_canceled()) {
         elem->cancel();
         break;
      }

      if (chk_dbglvl(dbglvl)) {
         POOL_MEM status(PM_FNAME);
         get_cloud_upload_transfer_status(status, false);
         Dmsg1(0, "%s",status.addr());
         get_cloud_download_transfer_status(status, false);
         Dmsg1(0, "%s",status.addr());
      }

      stat = elem->timedwait(tv);
   }

   Leave(dbglvl);
   return (stat == 0);
}
/* return the volumes list */
bool cloud_dev::get_cloud_volumes_list(DCR* dcr, alist *volumes, POOLMEM *&err)
{
   cancel_callback cancel_cb;
   cancel_cb.fct = DCR_cancel_cb;
   cancel_cb.arg = dcr;
   if (!driver) {
      return false;
   }
   return driver->get_cloud_volumes_list(volumes, &cancel_cb, err);
}

/* return the list of parts contained in VolumeName */
bool cloud_dev::get_cloud_volume_parts_list(DCR *dcr, const char *VolumeName, ilist *parts, POOLMEM *&err)
{
   cancel_callback cancel_cb;
   cancel_cb.fct = DCR_cancel_cb;
   cancel_cb.arg = dcr;
   if (!driver) {
      return false;
   }
   return driver->get_cloud_volume_parts_list(VolumeName, parts,  &cancel_cb, err);
}

/* TODO: Add .api2 mode for the status message */
/* format a status message of the cloud transfers. Verbose gives details on each transfer */
uint32_t cloud_dev::get_cloud_upload_transfer_status(POOL_MEM& msg, bool verbose)
{
   uint32_t ret = 0;
   ret = Mmsg(msg,_("   Uploads   "));
   ret += upload_mgr.append_status(msg, verbose);
   return ret;
}

void cloud_dev::get_api_cloud_upload_transfer_status(OutputWriter &ow, bool verbose)
{
   ow.start_group("uploads");
   upload_mgr.append_api_status(ow, verbose);
   ow.end_group();
}

/* format a status message of the cloud transfers. Verbose gives details on each transfer */
uint32_t cloud_dev::get_cloud_download_transfer_status(POOL_MEM& msg, bool verbose)
{
   uint32_t ret = 0;
   ret = Mmsg(msg,_("   Downloads "));
   ret += download_mgr.append_status(msg, verbose);
   return ret;
}

void cloud_dev::get_api_cloud_download_transfer_status(OutputWriter &ow, bool verbose)
{
   ow.start_group("downloads");
   download_mgr.append_api_status(ow, verbose);
   ow.end_group();
}

/* for a given volume VolumeName, return parts that is a list of the
 * cache parts within the volume */
bool cloud_dev::get_cache_volume_parts_list(DCR *dcr, const char* VolumeName, ilist *parts)
{
   JCR *jcr = dcr->jcr;
   Enter(dbglvl);

   if (!parts || strlen(VolumeName) == 0) {
      return false;
   }

   POOLMEM *part_path = get_pool_memory(PM_NAME);
   POOLMEM *vol_dir = get_pool_memory(PM_NAME);

   /*NB : *** QUESTION *** : it works with examples but is archive_name() the kosher fct to call to get the cache path? */
   pm_strcpy(vol_dir, archive_name());
   if (!IsPathSeparator(vol_dir[strlen(vol_dir)-1])) {
      pm_strcat(vol_dir, "/");
   }
   pm_strcat(vol_dir, VolumeName);

   DIR* dp = NULL;
   struct dirent *entry = NULL;
   struct stat statbuf;
   int name_max;
   bool ok = false;
   POOL_MEM dname(PM_FNAME);
   int status = 0;

   Enter(dbglvl);

   Dmsg1(dbglvl, "Searching for parts in: %s\n", VolumeName);

   if (!(dp = opendir(vol_dir))) {
      berrno be;
      Mmsg2(errmsg, "Cannot opendir to get parts list. Volume %s does not exist. ERR=%s\n",
      VolumeName, be.bstrerror());
      Dmsg1(dbglvl, "%s", errmsg);
      goto get_out;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);

   for ( ;; ) {
      if (jcr->is_canceled()) {
         goto get_out;
      }
      errno = 0;
      status = breaddir(dp, dname.addr());
      if (status == -1) {
         break;
      } else if (status < 0) {
         Mmsg1(errmsg, "breaddir failed: status=%d", status);
         Dmsg1(dbglvl, "%s\n", errmsg);
         goto get_out;
      }
      /* Always ignore . and .. */
      if (strcmp(".", dname.c_str()) == 0 || strcmp("..", dname.c_str()) == 0) {
         continue;
      }

      /* Look only for part files */
      if (strncmp("part.", dname.c_str(), 5) != 0) {
         continue;
      }
      char *ext = strrchr (dname.c_str(), '.');
      if (!ext || strlen(ext) < 2) {
         continue;
      }

      cloud_part *part = (cloud_part*) malloc(sizeof(cloud_part));
      if (!part) {
         berrno be;
         Dmsg1(dbglvl, "Failed to create part structure: %s\n",
            be.bstrerror());
         goto get_out;
      }

      /* save extension (part number) to cloud_part struct index*/
      part->index = atoi(&ext[1]);

      /* Bummer : caller is responsible for freeing label */
      pm_strcpy(part_path, vol_dir);
      if (!IsPathSeparator(part_path[strlen(vol_dir)-1])) {
         pm_strcat(part_path, "/");
      }
      pm_strcat(part_path, dname.c_str());

      /* Get size of part */
      if (lstat(part_path, &statbuf) == -1) {
         /* The part is no longer here, might be a truncate in an other thread, just
          * do like if the file wasn't here
          */
         continue;
      }
      
      part->size  = statbuf.st_size;
      part->mtime = statbuf.st_mtime;
      /* ***FIXME***: should get SHA512 from cloud */ 
      bmemzero(part->hash64, 64);
      parts->put(part->index, part);
   }

   ok = true;

get_out:
   if (dp) {
      closedir(dp);
   }
   if (entry) {
      free(entry);
   }
   free_pool_memory(vol_dir);
   free_pool_memory(part_path);
   return ok;
}

/*
 * Upload cache parts that are not in the cloud
 */
bool cloud_dev::upload_cache(DCR *dcr, const char *VolumeName, uint32_t truncate, POOLMEM *&err)
{
   int i;
   Enter(dbglvl);
   bool ret=true;
   ilist cloud_parts;
   ilist cache_parts;
   POOLMEM *vol_dir = get_pool_memory(PM_NAME);
   POOLMEM *fname = get_pool_memory(PM_NAME);
   cancel_callback cancel_cb;
   cancel_cb.fct = DCR_cancel_cb;
   cancel_cb.arg = dcr;
   if (!driver->get_cloud_volume_parts_list(VolumeName, &cloud_parts, &cancel_cb, err)) {
      Qmsg2(dcr->jcr, M_ERROR, 0, "Error while uploading parts for volume %s. %s\n", VolumeName, err);
      ret = false;
      goto bail_out;
   }

   if (!get_cache_volume_parts_list(dcr, VolumeName, &cache_parts)) {
      Qmsg1(dcr->jcr, M_ERROR, 0, "Error while listing cache parts for volume %s.\n", VolumeName);
      ret = false;
      goto bail_out;
   }

   make_cache_volume_name(vol_dir, VolumeName);

   /*
    * Upload every part where cache_size > cloud_size
    */
   for (i=1; i <= (int)cache_parts.last_index(); i++) {
      if (i <= (int)cloud_parts.last_index()) {   /* not on the cloud, but exists in the cache  */
         cloud_part *cachep = (cloud_part *)cache_parts[i];
         cloud_part *cloudp = (cloud_part *)cloud_parts[i];

         if (!cachep || cachep->size == 0) {         /* Not in the current cache */
            continue;
         }
         if (cloudp && cloudp->size >= cachep->size) {
            continue;              /* already uploaded */
         }
      }
      Mmsg(fname, "%s/part.%d", vol_dir, i);
      Dmsg1(dbglvl, "Do upload of %s\n", fname);
      bool do_truncate = (truncate==TRUNC_AFTER_UPLOAD) || (truncate==TRUNC_CONF_DEFAULT && (trunc_opt == TRUNC_AFTER_UPLOAD));
      if (!upload_part_to_cloud(dcr, VolumeName, i, do_truncate)) {
         if (errmsg[0]) {
            Qmsg(dcr->jcr, M_ERROR, 0, "%s", errmsg);
         }
         ret = false;
      } else {
         Qmsg(dcr->jcr, M_INFO, 0, "Uploaded cache %s\n", fname);
      }
   }
bail_out:
   free_pool_memory(vol_dir);
   free_pool_memory(fname);
   Leave(dbglvl);
   return ret;
}

