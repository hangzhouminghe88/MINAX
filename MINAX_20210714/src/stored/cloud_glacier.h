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

#ifndef _CLOUD_GLACIER_H_
#define _CLOUD_GLACIER_H_

typedef struct cloud_glacier_cgc
{
   const char *host_name;
   const char *bucket_name;
}  cloud_glacier_ctx;

/* Abstract class cannot be instantiated */
class cloud_glacier: public SMARTALLOC {
public:
   cloud_glacier() {};
   virtual ~cloud_glacier() {};
   
   virtual bool init(cloud_glacier_ctx *ctx, POOLMEM *&err) = 0;
   virtual bool restore_cloud_object(transfer *xfer, const char *cloud_fname) = 0;
   virtual bool is_waiting_on_server(transfer *xfer, const char *cloud_fname) = 0;
};

class dummy_glacier: public cloud_glacier {
public:
   dummy_glacier() {};
   bool init (cloud_glacier_ctx *ctx, POOLMEM *&err)
   {
      Mmsg(err, "Cloud glacier not properly loaded");
      return false;
   }
   bool restore_cloud_object(transfer *xfer, const char *cloud_fname)
   {
      Mmsg(xfer->m_message, "Cloud glacier not properly loaded");
      return false;
   }
   bool is_waiting_on_server(transfer *xfer, const char *cloud_fname) {
      Mmsg(xfer->m_message, "Cloud glacier not properly loaded");
      return false;
   };
};

#endif /* _CLOUD_GLACIER_H_ */
