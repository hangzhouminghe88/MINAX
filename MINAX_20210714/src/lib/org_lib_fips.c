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

bool crypto_check_digest(JCR *jcr, crypto_digest_t type)
{
   return true;
}

int crypto_check_fips(bool enable)
{
   if (enable) {
      return -1;
   }
   return 0;
}

const char *crypto_get_fips_enabled()
{
   return "N/A";
}

bool crypto_get_fips()
{
   return false;
}
