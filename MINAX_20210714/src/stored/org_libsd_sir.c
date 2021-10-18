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

bool sir_init(DCR *dcr)
{
   return false;
}

/* Return: SIR_OK       nothing to do
 *         SIR_BREAK    break
 *         SIR_CONTINUE continue
 */
int sir_init_loop(DCR *dcr,
                  DEVICE **dev, DEV_BLOCK **block,
                  bool record_cb(DCR *dcr, DEV_RECORD *rec),
                  bool mount_cb(DCR *dcr))
{
   return SIR_OK;
}

bool DCR::need_to_reposition()
{
   return false;
}

void DCR::set_interactive_reposition(BSR *)
{
}

BSR *DCR::clear_interactive_reposition()
{
   return NULL;
}

void DCR::set_session_interactive()
{
}
