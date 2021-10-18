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
#include "stored.h"

bool is_dedup_server_side(DEVICE *dev, int32_t stream, uint64_t stream_len)
{
   return false;
}

bool is_dedup_ref(DEV_RECORD *rec, bool lazy)
{
   return false;
}


void dedup_get_limits(int64_t *nofile, int64_t *memlock)
{
}

/* dump the status of all dedupengines */
void list_dedupengines(char *cmd, STATUS_PKT *sp)
{
}

bool dedup_parse_filter(char *fltr)
{
   return false;
}
