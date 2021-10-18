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
 * Routines for writing to the Cloud using S3 protocol.
 *
 * Written by Kern Sibbald, May MMXVI
 *
 */

#ifndef _S3_DRV_H
#define _S3_DRV_H

#include "bacula.h"
#include "stored.h"

#ifdef HAVE_LIBS3
#include <libs3.h>
#include "cloud_driver.h"   /* get base class definitions */

class s3_driver: public cloud_driver {
private:
   S3BucketContext s3ctx;       /* Main S3 bucket context */
   S3RestoreTier transfer_priority;
   uint32_t transfer_retention_days;
public:
   cloud_dev *dev;              /* device that is calling us */

   s3_driver() {
   };
   ~s3_driver() {
   };

   void make_cloud_filename(POOLMEM *&filename, const char *VolumeName, uint32_t part);
   bool init(CLOUD *cloud, POOLMEM *&err);
   bool start_of_job(POOLMEM *&err);
   bool term(POOLMEM *&err);
   bool end_of_job(POOLMEM *&err);
   bool truncate_cloud_volume(const char *VolumeName, ilist *trunc_parts, cancel_callback *cancel_cb, POOLMEM *&err);
   bool clean_cloud_volume(const char *VolumeName, cleanup_cb_type *cb, cleanup_ctx_type *context, cancel_callback *cancel_cb, POOLMEM *&err);
   bool copy_cache_part_to_cloud(transfer *xfer);
   bool move_cloud_part(const char *VolumeName, uint32_t apart , const char *to, cancel_callback *cancel_cb, POOLMEM *&err, int& exists);
   int copy_cloud_part_to_cache(transfer *xfer);
   bool restore_cloud_object(transfer *xfer, const char *cloud_fname);
   bool is_waiting_on_server(transfer *xfer);
   bool get_cloud_volume_parts_list(const char* VolumeName, ilist *parts, cancel_callback *cancel_cb, POOLMEM *&err);
   bool get_cloud_volumes_list(alist *volumes, cancel_callback *cancel_cb, POOLMEM *&err);
   S3Status put_object(transfer *xfer, const char *cache_fname, const char *cloud_fname);
   bool retry_put_object(S3Status status, int retry);
   int get_cloud_object(transfer *xfer, const char *cloud_fname, const char *cache_fname);

private:
   bool get_one_cloud_volume_part(const char* part_path_name, ilist *parts, POOLMEM *&err);
};

#endif  /* HAVE_LIBS3 */
#endif /* _S3_DRV_H */
