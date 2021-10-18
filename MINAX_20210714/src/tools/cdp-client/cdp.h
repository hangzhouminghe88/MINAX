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

#ifndef CDP_CLI
#define CDP_CLI

#include "bacula.h"

#ifdef HAVE_WIN32
#define HOME_VAR "APPDATA"
#define SPOOL_DIR "cdp-sdir"
#else
#define HOME_VAR "HOME"
#define SPOOL_DIR ".cdp-sdir"
#endif

class CDP {

public:

    static void winToUnixPath(char *path) {
#ifdef HAVE_WIN32
        for (int i = 0; path[i] != '\0'; i++) {
            if (path[i] == '\\') {
                path[i] = '/';
            }
        }
#endif
    }
   
    static POOLMEM *spoolDir() {
        char *home = getenv(HOME_VAR);

        if (HOME_VAR == NULL) {
            return NULL;
        }

        CDP::winToUnixPath(home);
        POOLMEM *spath = get_pool_memory(PM_FNAME);
        Mmsg(spath, "%s/%s", home, SPOOL_DIR);
        return spath;
    }

    static POOLMEM *journalPath(const char *journalName) {
        char *home = getenv(HOME_VAR);

        if(home == NULL) {
            return NULL;
        }

        CDP::winToUnixPath(home);
        POOLMEM *jpath = get_pool_memory(PM_FNAME);
        Mmsg(jpath, "%s/%s", home, journalName);
        return jpath;
    }

};

#endif
