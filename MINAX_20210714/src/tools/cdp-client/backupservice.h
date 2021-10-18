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

#ifndef BACKUPSERVICE_H
#define BACKUPSERVICE_H

#ifdef HAVE_WIN32
#define _STAT_DEFINED
#endif

#include <libgen.h>
#include <string.h>
#include <string>
#include "folderwatcher.h"
#include "cdp.h"
#include "../../plugins/fd/journal.h"
#include "../../plugins/fd/file-record.h"
#include <map>

/**
 * @brief This class controls which files should be sent to the Spool
 * Directory (and also have their metadata saved on the Journal).
 */

class BackupService: public FileChangeHandler
{
private:
    std::map<std::string, int64_t> _last_mtime;

public:
    char *_spoolPath;
    Journal *_journal;
    std::map<std::string, FolderWatcher *> _watchers;

    BackupService(): _spoolPath(NULL) {}
    virtual ~BackupService();

    void start(const char *spoolPath, Journal *journal);

    POOLMEM *watch(const char *folder);
    void stopWatching(const char *folder);

    /** Called by one of the @param _watchers */
    void onChange(const char* fpath);
};

#endif
