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
 * Support routines for Unit Tests.
 */

/*
 * This is an example how to use unittests in your code:

int main()
{
   int rc;

   prolog("app_test");

   ( ... your test application goes here ...)

   rc = report();
   epilog();
   return rc;
}

 * and a C++ approach for any C++ geeks:

int main()
{
   Unittests tests("app_test");

   ( ... your test application goes here ...)

   return report();
}

 */

#include <stdio.h>
#include "bacula.h"
#include "unittests.h"

static int err=0;
static int nb=0;
static bool lmgrinit = false;
static bool quiet = false;
static bool print_var = false;

Unittests::Unittests(const char *name, bool lmgr/*=false*/, bool motd/*=true*/)
{
   if (getenv("UNITTEST_PRINT_VAR")) {
      print_var = true;
   }
   prolog(name, lmgr, motd);
};

void configure_test(uint64_t options)
{
   if (options & TEST_QUIET) {
      quiet = true;
   }
   if (options & TEST_PRINT_LOCAL) {
      print_var = true;
   }
}

/*
 * Test success if value is not zero.
 */
bool _ok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg4(-1, "ERR %.80s %s:%i on %s\n", label, file, l, op);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is zero.
 */
bool _nok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (value) {
      err++;
      if (err < 1000) {
         Pmsg4(-1, "ERR %.80s %s:%i on !%s\n", label, file, l, op);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return !value;
}

/*
 * Test success if value is different
 */
bool _is(const char *file, int l, const char *op, const char *str, const char *str2, const char *label)
{
   nb++;
   bool value = (strcmp(str, str2) == 0);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s %s == %s\n", label, file, l, op, str, str2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _is(const char *file, int l, const char *op, int64_t v, int64_t v2, const char *label)
{
   nb++;
   bool value = (v == v2);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s %lld == %lld\n", label, file, l, op, v, v2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _isnt(const char *file, int l, const char *op, const char *str, const char *str2, const char *label)
{
   nb++;
   bool value = (strcmp(str, str2) != 0);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s %s == %s\n", label, file, l, op, str, str2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Test success if value is different
 */
bool _isnt(const char *file, int l, const char *op, int64_t v, int64_t v2, const char *label)
{
   nb++;
   bool value = (v != v2);
   if (!value) {
      err++;
      if (err < 1000) {
         Pmsg6(-1, "ERR %.80s %s:%i on %s == %lld\n", label, file, l, op, v, v2);
      } else if (err == 1000) {
         Pmsg0(-1, "ERR Too much errors\n");
      }
      if (print_var) {
         gdb_print_local(1);
      }
   } else if (!quiet) {
      Pmsg1(-1, "OK  %.80s\n", label);
   }
   return value;
}

/*
 * Short report of successful/all tests.
 */
int report()
{
   Pmsg0(-1, "==== Report ====\n");
   Pmsg2(-1, "Result %i/%i OK\n", nb - err, nb);
   return err > 0;
}

void terminate(int sig) {};

/*
 * Initializes the application env, including lockmanager.
 */
void prolog(const char *name, bool lmgr, bool motd)
{
   if (motd) {
      Pmsg1(-1, "==== Starting %s ... ====\n", name);
   }
   my_name_is(0, NULL, name);
   init_signals(terminate);

#ifdef HAVE_WIN32
   InitWinAPIWrapper();
   WSA_Init();
#endif

   init_stack_dump();

   if (lmgr){
      lmgr_init_thread();     /* initialize the lockmanager stack */
      lmgrinit = true;
   }
};

/*
 * Finish the application, shows report about memory leakage and terminates the lockmanager.
 */
void epilog()
{
   Pmsg0(-1, "\n");
   stop_watchdog();
   if (lmgrinit) {
      lmgr_cleanup_main();
   }
   close_memory_pool();
   sm_dump(false);
   Pmsg0(-1, "==== Finish ====\n");
};
