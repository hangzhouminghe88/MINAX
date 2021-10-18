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
#include "dird.h"

bool use_max_pool_bytes(JCR *jcr)
{
   return false;
}

bool check_max_pool_bytes(POOL_DBR *pr)
{
   return false;
}

bool has_quota_reached(JCR *jcr, POOL_DBR *pr)
{
   return false;
}

bool has_quota_reached(JCR *jcr, MEDIA_DBR *mr)
{
   return false;
}

bool has_quota_reached(JCR *jcr)
{
   return false;
}

bool catreq_get_pool_info(JCR *jcr, BSOCK *bs)
{
   return false;
}
