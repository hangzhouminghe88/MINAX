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

void BackupService::start(const char *spoolPath, Journal *j)
{
    POOLMEM *err_msg;
    _journal = j;
    _spoolPath = bstrdup(spoolPath);
    FolderRecord *rec;
    _journal->beginTransaction("r");

    for (;;) {
       rec = _journal->readFolderRecord();

       if(rec == NULL) {
           break;
       }

       FolderWatcher *w = new FolderWatcher(this);
       err_msg = w->watch(rec->path);

       if (err_msg != NULL) {
          Dmsg2(0, "Error while trying to watch %s. %s", rec->path, err_msg); 
       } else {
          _watchers[rec->path] = w;
       }

       delete rec;
    }

    _journal->endTransaction();
}

POOLMEM *BackupService::watch(const char *folder)
{
    POOLMEM *err_msg;
    char *fpath = bstrdup(folder);
    CDP::winToUnixPath(fpath);
    FolderWatcher *w = new FolderWatcher(this);
    err_msg = w->watch(fpath);

    if (err_msg == NULL) {
        FolderRecord rec;
        rec.path = fpath;
        _journal->writeFolderRecord(rec);
        _watchers[rec.path] = w;
    } else {
        Dmsg2(0, "Error while trying to watch %s. %s", fpath, err_msg); 
        delete w;
    }

    return err_msg;
}

void BackupService::stopWatching(const char *folder)
{
    _journal->removeFolderRecord(folder);
    delete _watchers[folder];
    _watchers[folder] = NULL;
}

void BackupService::onChange(const char *fpath)
{
    char *tmp_fpath = bstrdup(fpath);
    char *fname = basename(tmp_fpath);
    FileRecord rec;
    rec.name = bstrdup(fpath);
    POOLMEM *spoolFilename = get_pool_memory(PM_FNAME);

    if (strcmp(fpath, _journal->_jPath) == 0) {
        Dmsg0(0, "Change on Journal File ignored.\n");
        goto bail_out;
    }
        
    if (!rec.encode_attrs()) {
        goto bail_out;
    }

    if (rec.mtime - _last_mtime[fpath] < 5) {
        Dmsg3(50, "File %s: current mtime: %d. Previous mtime: %d. Ignoring\n",
              rec.name,
              rec.mtime,
              _last_mtime[fpath]);
        goto bail_out;
    }

    _last_mtime[fpath] = rec.mtime;
    char str_mtime[50];
    edit_int64(rec.mtime, str_mtime);
    Mmsg(spoolFilename, "%s/%s_%s", _spoolPath, str_mtime, fname);

    if (copyfile(fpath, spoolFilename) == 0) {
        Dmsg2(0, "Copied file %s into %s\n", fpath, spoolFilename);
        rec.sname = bstrdup(spoolFilename);
        _journal->writeFileRecord(rec);
    } else {
        Dmsg2(0, "Could not copy file %s into %s\n", fpath, spoolFilename);
    }

bail_out:
    free(tmp_fpath);
    free_and_null_pool_memory(spoolFilename);
}

BackupService::~BackupService()
{
    //Delete all Watchers
    std::map<std::string, FolderWatcher *>::iterator it;
    for(it = _watchers.begin(); it != _watchers.end(); it++) {
        delete it->second;
    }
}
