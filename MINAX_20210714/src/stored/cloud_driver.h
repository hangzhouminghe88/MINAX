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
 * Routines for writing Cloud drivers
 *
 * Written by Kern Sibbald, May MMXVI
 *
 */

#include "bacula.h"
#include "stored.h"
#include "cloud_parts.h"
#include "cloud_transfer_mgr.h"
#include "lib/bwlimit.h"

#ifndef _CLOUD_DRIVER_H_
#define _CLOUD_DRIVER_H_

#define NUM_UPLOAD_RETRIES 10
class cloud_dev;

/* define the cancel callback type */
typedef bool (cancel_cb_type)(void*);
typedef struct  {
  cancel_cb_type* fct;
   void *arg;
} cancel_callback;

typedef struct {
   const char *pattern;
   int pattern_num;
} cleanup_ctx_type;
typedef bool (cleanup_cb_type)(const char*, cleanup_ctx_type*);


/* Abstract class cannot be instantiated */
class cloud_driver: public SMARTALLOC {
public:
   cloud_driver() : max_upload_retries(NUM_UPLOAD_RETRIES) {};
   virtual ~cloud_driver() {};

   virtual bool copy_cache_part_to_cloud(transfer *xfer) = 0;
   virtual bool move_cloud_part(const char *VolumeName, uint32_t apart , const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists) = 0;
   enum {
      CLOUD_DRIVER_COPY_PART_TO_CACHE_OK                      = 0,
      CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR                   = 1,
      CLOUD_DRIVER_COPY_PART_TO_CACHE_RETRY                   = 2
   };
   virtual int copy_cloud_part_to_cache(transfer *xfer) = 0;
   virtual bool restore_cloud_object(transfer *xfer, const char *cloud_fname) = 0;
   virtual bool is_waiting_on_server(transfer *xfer) = 0;
   virtual bool truncate_cloud_volume(const char *VolumeName, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err) = 0;
   virtual bool clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *ctx, cancel_callback *cancel_cb, POOLMEM *&err) = 0;
   virtual bool init(CLOUD *cloud, POOLMEM *&err) = 0;
   virtual bool term(POOLMEM *&err) = 0;
   virtual bool start_of_job(POOLMEM *&err) = 0;
   virtual bool end_of_job(POOLMEM *&err) = 0;
   virtual bool get_cloud_volume_parts_list(const char* VolumeName, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err) = 0;
   virtual bool get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err) = 0;
   static void add_vol_and_part(POOLMEM *&filename, const char *VolumeName, const char *name, uint32_t apart)
   {
      POOL_MEM partname;
      Mmsg(partname, "%s.%d", name, apart);
      add_vol_and_part(filename, VolumeName, partname.c_str());
   }
   static void add_vol_and_part(POOLMEM *&filename, const char *VolumeName, const char *name)
   {
      POOL_MEM partnumber;
      int len = strlen(filename);

      if (len > 0 && !IsPathSeparator((filename)[len-1])) {
         pm_strcat(filename, "/");
      }

      pm_strcat(filename, VolumeName);
      Mmsg(partnumber, "/%s", name);
      pm_strcat(filename, partnumber);
   }
   bwlimit upload_limit;
   bwlimit download_limit;
   uint32_t max_upload_retries;
};

class dummy_driver: public cloud_driver {
public:
   dummy_driver() {};
   bool copy_cache_part_to_cloud(transfer *xfer) {
      Mmsg(xfer->m_message, "Cloud driver not properly loaded");
      return false;
   };
   bool move_cloud_part(const char *VolumeName, uint32_t apart , const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *ctx, cancel_callback *cancel_cb, POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   int copy_cloud_part_to_cache(transfer *xfer) {
      Mmsg(xfer->m_message, "Cloud driver not properly loaded");
      return CLOUD_DRIVER_COPY_PART_TO_CACHE_ERROR;
   };
   bool restore_cloud_object(transfer *xfer, const char *cloud_fname)
   {
      Mmsg(xfer->m_message, "Cloud driver not properly loaded");
      return false;
   }
   bool is_waiting_on_server(transfer *xfer) {
      Mmsg(xfer->m_message, "Cloud driver not properly loaded");
      return false;
   };
   bool truncate_cloud_volume(const char *VolumeName, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool init(CLOUD *cloud, POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool term(POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool start_of_job(POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   }
   bool end_of_job(POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool get_cloud_volume_parts_list(const char* VolumeName, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
   bool get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err) {
      Mmsg(err, "Cloud driver not properly loaded");
      return false;
   };
};

#endif /* _CLOUD_DRIVER_H_ */
