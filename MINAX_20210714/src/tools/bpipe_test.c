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
#include "../lib/unittests.h"

int main(int argc, char **argv)
{
   Unittests u("t");
   char buf[512] = {0};
   BPIPE *bp;
   FILE *fp;
   fp = fopen("toto.sh", "w");
   fprintf(fp, "#!/bin/sh\necho hello\nread a\necho $a\n");
   fclose(fp);
   chmod("toto.sh", 0755);
   
   bp = open_bpipe("./toto.sh", 0, "rw");
   ok(bp != NULL, "open_bpipe");

   fgets(buf, sizeof(buf), bp->rfd);
   ok(strcmp(buf, "hello\n") == 0, "fgets on bpipe");
   printf("%s", buf);

   fprintf(bp->wfd, "titi\n");
   fflush(bp->wfd);

   fgets(buf, sizeof(buf), bp->rfd);
   ok(strcmp(buf, "titi\n") == 0, "fgets on bpipe");
   printf("%s", buf);
   
   ok(close_bpipe(bp) == 0, "closing bpipe");
   return report();
}
