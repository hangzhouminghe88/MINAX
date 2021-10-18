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

#include "backupservice.h"
#include "cdp.h"

static void strip_trailing_slash(char *arg)
{
#ifndef HAVE_WIN32
   const char slash = '/';
#else
   const char slash = '\\';
#endif
   int len = strlen(arg);
   if (len == 0) {
      return;
   }   
   len--;
   if (arg[len] == slash) {       /* strip any trailing slash */
      arg[len] = 0;
   }   
}

static void waitForChanges(BackupService *bservice) {
    if (bservice->_watchers.size() > 0) {
        FolderWatcher *watcher = bservice->_watchers.begin()->second;
#ifndef HAVE_WIN32
        pthread_join(watcher->_inotifyThread, NULL);
#else
        pthread_join(watcher->_watcherThread, NULL);
#endif
    } else {
        Dmsg0(0, "No folders te be watched. Terminating Client.\n");
    }
}

static void writeFolders(Journal *journal, alist *newFolders) {
    char *folder;
    void *tmp1;
    void *tmp2;
    alist savedFolders(100, false);
    FolderRecord *rec;   

    // Read all the saved Folders to make sure we are
    // not saving any repeated Folder
    journal->beginTransaction("r");

    for(;;) {
        FolderRecord *rec = journal->readFolderRecord();

        if(rec == NULL) {
            break;
        }

        savedFolders.append((void *) rec);
    }

    journal->endTransaction();

    //We add the Folders specified by the user, making sure
    //that we don't add any repeated one
    bool found = false;
    foreach_alist(tmp1, newFolders) {
        folder = (char *) tmp1;

        foreach_alist(tmp2, &savedFolders) {
            rec = (FolderRecord *) tmp2;
            if (strcmp(rec->path, folder) == 0) {
                found = true;
            }
        }

        if (found) {
            found = false;
            continue;
        }

        FolderRecord rec;
        rec.path = bstrdup(folder);

       if(!journal->writeFolderRecord(rec)) {
           Dmsg1(0, "ERROR: Could not write folder %s", rec.path);
           exit(-1);
       }
    }

}

static void makeJournalFile(Journal *journal, const char *journal_path, const char *spool_dir) {

    if (!journal->setJournalPath(journal_path, spool_dir)) {
       Dmsg1(0, "Could not set journal file to: %s\n", journal_path);
       exit(1);
    }

    Dmsg1(0, "Set journal file to: %s\n", journal_path);
}

static void makeSpoolDir(const char *spool_dir) {
    int rc = mkdir(spool_dir, 0750);
    const char *error = NULL;

    switch (rc) {
    case EACCES:
       error = "Permission Denied";
       break;
    case ELOOP:
       error = "Symbolic Link loop";
       break;
    case EMLINK:
       error = "Link would exceed LINK_MAX";
       break;
    case ENAMETOOLONG:
       error = "The path length is too long";
       break;
    case ENOENT:
    case ENOTDIR:
       error = "A component on the path does not exist";
       break;
    case ENOSPC:
       error = "No space available";
       break;
    case EROFS:
       error = "Read-only file system";
       break;
    default:
       break;  
    }

    if (error != NULL) {
       Dmsg2(0, "(ERROR) - could not set spool directory to %s. %s\n", spool_dir, error);
       exit(-1);
    }
      
    Dmsg1(0, "Set spool directory to: %s\n", spool_dir);
}

static void printHelp()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\n%sVersion: %s (%s) %s %s %s\n\n"
"Usage: cdp-client\n"
"       -j <dir>      sets the location of the journal file to <dir>\n"
"       -s <dir>      sets spool directory to <dir>\n"
"       -f <dir>      watches the directory <dir> for changes.\n"
"       -d <nn>       set debug level to <nn>\n"
"       -?            print this message.\n"
"\n"), 2004, BDEMO, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);
}

/**
 *         Main Bacula CDP Client -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
    char *fpath;
    char ch;
    POOLMEM *spool_dir = NULL;
    POOLMEM *journal_path = NULL;
    alist folders(100, true);
    BackupService bservice;
    Journal journal;
    lmgr_init_thread();

    while ((ch = getopt(argc, argv, "f:s:j:d:h?")) != -1) {
       switch (ch) {
       case 's':
          free_and_null_pool_memory(spool_dir);
          spool_dir = get_pool_memory(PM_FNAME);
          strip_trailing_slash(optarg);
          CDP::winToUnixPath(optarg);
          pm_strcpy(spool_dir, optarg);
          break;
       case 'j':
          free_and_null_pool_memory(journal_path);
          journal_path = get_pool_memory(PM_FNAME);
          strip_trailing_slash(optarg);
          CDP::winToUnixPath(optarg);
          Mmsg(journal_path, "%s/%s", optarg, JOURNAL_CLI_FNAME);
          break;
       case 'f':
          strip_trailing_slash(optarg);
          fpath = bstrdup(optarg);
          folders.append((void *) fpath);
          break;
       case 'd':
          debug_level = atoi(optarg);
          if (debug_level <= 0) {
              debug_level = 1;
          }
          break;
       case 'h':
       case '?':
       default:
          printHelp();
          exit(1);
       }
    }

    argc -= optind;

    if (argc) {
       printHelp();
       exit(1);
    }
   
    if (spool_dir == NULL) {
        spool_dir = CDP::spoolDir();
    }

    if (journal_path == NULL) {
        journal_path = CDP::journalPath(JOURNAL_CLI_FNAME);
    }

    makeSpoolDir(spool_dir);
    makeJournalFile(&journal, journal_path, spool_dir);
    writeFolders(&journal, &folders);
    bservice.start(spool_dir, &journal);
    waitForChanges(&bservice);
    free_and_null_pool_memory(spool_dir);
    free_and_null_pool_memory(journal_path);
    lmgr_cleanup_main();
    return 0;
}
