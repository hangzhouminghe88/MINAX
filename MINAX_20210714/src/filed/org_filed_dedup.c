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

#include "filed.h"
#include "protos.h"
#include "backup.h"

bool do_dedup_client_side(bctx_t &bctx)
{
   return true;
}

GetMsg *get_msg_buffer(JCR *jcr, BSOCK *sd, const char *rec_header)
{
   return New(GetMsg(jcr, sd, rec_header, DEDUP_MAX_MSG_SIZE));
}

bool is_dedup_enabled(JCR *jcr, FF_PKT *ff_pkt)
{
   return false;
}

bool dedup_init_storage_bsock(JCR *jcr, BSOCK *sd)
{
   return true;
}

void dedup_release_storage_bsock(JCR *jcr, BSOCK *sd)
{
}

void dedup_init_jcr(JCR *jcr)
{
}
