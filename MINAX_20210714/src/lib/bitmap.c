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
/* Written by Eric Bollengier June 2020 */

#include "bacula.h"
#include "bitmap.h"

#ifdef TEST_PROGRAM
#include "unittests.h"

int main(int argc, char **argv)
{
   Unittests("bitmap_test", true, true);
   bitmap a(10);
   a.dump();
   for (int i = 0; i < 10 ; i++) {
      is(a.is_set(i), -1, "Test all bits");
   }
   is(a.get_max_bit(), 9, "Test max_bit");
   is(a.is_set(11), -1, "Test incorrect value");
   is(a.is_set(200), -1, "Test incorrect value");
   ok(a.bit_set(5), "Test set value");
   ok(a.bit_set(8), "Test set value");
   nok(a.bit_set(12), "Test set value");
   is(a.is_set(12), -1, "Test is_set value");
   is(a.bit_set(8), 1, "Test is_set value");
   a.dump();
   
   a.resize(30);
   is(a.get_max_bit(), 29, "Test max_bit");
   is(a.is_set(8), 1, "Test is_set value");
   is(a.is_set(5), 1, "Test is_set value");
   is(a.bit_set(8), 1, "Test is_set value");
   for (int i = 0; i < 30 ; i++) {
      a.bit_clear(i);
      is(a.is_set(i), 0, "Test all bits");
   }
   for (int i = 0; i < 30 ; i++) {
      a.bit_reset(i);
      is(a.is_set(i), -1, "Test all bits");
   }
   a.dump();

   return report();
}

#endif
