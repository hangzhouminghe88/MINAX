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
/* Simple tool to test chk_dbglvl() macro */

#include "bacula.h"
#include "lib/unittests.h"

void *start_heap;

int main (int argc, char *argv[])
{
   Unittests t("tags_test", true, true);
   start_heap = sbrk(0);
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   my_name_is(argc, argv, "dmsg");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   set_working_directory("/tmp/");
   set_thread_concurrency(150);

   debug_level = 0;
   debug_level_tags = 0;

   nok(chk_dbglvl(10), "debug_level < 10");
   ok(chk_dbglvl(0), "lvl 0");
   nok(chk_dbglvl(DT_NETWORK), "no tag defined");
   nok(chk_dbglvl(DT_NETWORK|10), "no tag, debug_level < 10");

   debug_level = 10;
   debug_level_tags = 0;

   ok(chk_dbglvl(10), "debug_level = 10");
   ok(chk_dbglvl(0), "lvl 0");
   nok(chk_dbglvl(DT_NETWORK), "no tag defined");
   nok(chk_dbglvl(DT_NETWORK|10), "no tag, debug_level = 10");

   debug_level = 20;
   debug_level_tags = 0;

   ok(chk_dbglvl(10), "debug_level > 10");
   ok(chk_dbglvl(0), "lvl 0");
   nok(chk_dbglvl(DT_NETWORK), "no tag defined");
   nok(chk_dbglvl(DT_NETWORK|10), "no tag, debug_level = 20");
   
   debug_level = 20;
   debug_level_tags = DT_NETWORK;

   ok(chk_dbglvl(10), "debug_level > 10");
   ok(chk_dbglvl(0), "lvl 0");
   ok(chk_dbglvl(DT_NETWORK), "network tag defined");
   ok(chk_dbglvl(DT_NETWORK|10), "tag defined, debug_level > lvl");
   nok(chk_dbglvl(DT_NETWORK|30), "tag defined, debug_level < lvl");

   debug_level = 0;
   debug_level_tags = 0;
   ok(debug_parse_tags("network,volume", &debug_level_tags), "parse tags");

   nok(chk_dbglvl(10), "debug_level > 10");
   ok(chk_dbglvl(0), "lvl 0");
   ok(chk_dbglvl(DT_NETWORK), "network tag defined");
   nok(chk_dbglvl(DT_NETWORK|10), "tag defined, debug_level > lvl");

   ok(debug_parse_tags("!network,!volume", &debug_level_tags), "parse tags");

   nok(chk_dbglvl(10), "debug_level > 10");
   ok(chk_dbglvl(0), "lvl 0");
   nok(chk_dbglvl(DT_NETWORK), "network tag not defined");
   nok(chk_dbglvl(DT_NETWORK|10), "no tag, debug_level > lvl");

   ok(debug_parse_tags("all,!network", &debug_level_tags), "parse tags");
   nok(chk_dbglvl(DT_NETWORK), "network not defined");
   ok(chk_dbglvl(DT_VOLUME), "volume tag defined");
   ok(chk_dbglvl(DT_SQL), "sql tag defined");

   nok(debug_parse_tags("blabla", &debug_level_tags), "bad tags");
   ok(debug_parse_tags("", &debug_level_tags), "empty tags");
   term_msg();
   return report();
}
