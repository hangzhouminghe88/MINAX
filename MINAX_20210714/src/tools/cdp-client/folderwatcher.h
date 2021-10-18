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

#ifndef FOLDERWATCHER_H
#define FOLDERWATCHER_H

#ifndef HAVE_WIN32
#include <sys/inotify.h>
#include <map>
#include <set>
#include <dirent.h>
#endif

#include "bacula.h"
#include "cdp.h"

/**
 * @brief Monitors a Folder recursively for changes on files.
 *  Changes are signalled through the fileChanged() signal.
 */
class FileChangeHandler {
public:
     /**
     * @brief Emitted when a file is created or modified.
     * @param file - Absolute path of the file.
     */
    virtual void onChange(const char *fpath) = 0;

    virtual ~FileChangeHandler() {}
};

#ifndef HAVE_WIN32
class FolderWatcher
{

private:
    FileChangeHandler *_changeHandler;
    std::map<int, char *> _watchedDirs;
    std::set<int> _openedFiles;

    POOLMEM *watchDirRecursive(const char *dir);
    void handleEvent(struct inotify_event *e);

public:
    int _fd;
    bool _run_inotify_thread;
    pthread_t _inotifyThread;

    FolderWatcher(FileChangeHandler *handler);
    ~FolderWatcher();

    /**
     * @brief watches recursively the Folder.
     * @param folder - Absolue path of the folder.
     */
    POOLMEM *watch(const char *folder);

    /**
    * @brief those slots are called by our INotify Thread
    * when it detects a change on a directory or a file.
    */
    void handleINotifyEvents(int fd);
};
#else

class FolderWatcher
{

public:
    pthread_t _watcherThread;

    FileChangeHandler *_changeHandler;
    char * _watchedDirPath;
    HANDLE _dirHandle;
    bool _run_watcher_thread;

    FolderWatcher(FileChangeHandler *handler);
    ~FolderWatcher();

    /**
     * @brief watches recursively the Folder.
     * @param folder - Absolue path of the folder.
     */
    POOLMEM *watch(const char *folder);
};

#endif

#endif
