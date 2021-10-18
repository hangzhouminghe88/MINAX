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

#include "folderwatcher.h"

#ifndef HAVE_WIN32
/**
  Linux Implementation
 */
void *thread_inotify(void *arg) {
   FolderWatcher *watcher = (FolderWatcher *) arg;
   fd_set rfds;
   int rc; 

   while(watcher->_run_inotify_thread) {
      FD_ZERO(&rfds);
      FD_SET(watcher->_fd, &rfds);
      /* Wait until an event happens or we get interrupted
         by a signal that we catch */
      rc = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);

      if (!watcher->_run_inotify_thread) {
         break;
      }   

      if (rc > 0) {
         watcher->handleINotifyEvents(watcher->_fd);
      }   
   }   

   return NULL;
}

FolderWatcher::FolderWatcher(FileChangeHandler *handler)
{
   _run_inotify_thread = false;
   _changeHandler = handler;
}

POOLMEM *FolderWatcher::watch(const char *folder)
{
   _fd = inotify_init();

   if (_fd == -1) {
      POOLMEM *err_msg = get_pool_memory(PM_EMSG);
      Mmsg(err_msg, "Error: could not initialize Watcher\n");
      return err_msg;
   }

   _run_inotify_thread = true;
   int rc = pthread_create(&_inotifyThread, NULL, thread_inotify, (void *) this);
   if (rc) {
      POOLMEM *err_msg = get_pool_memory(PM_EMSG);
      Mmsg(err_msg, "Error: could not start INotify Thread."
            "Please, contact Bacula Support.\n");
      return err_msg;
   }

   return this->watchDirRecursive(folder);
}

POOLMEM *FolderWatcher::watchDirRecursive(const char *dir)
{
   DIR *dirReader = NULL;
   struct dirent *dirFile = NULL;
   const char *separator = NULL;
   POOLMEM *subdirPath = NULL;
   char *err_msg = NULL;

   uint32_t mask = IN_CLOSE | IN_ATTRIB | IN_MOVE
      | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_OPEN
      | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR;

   int wd = inotify_add_watch(_fd, dir, mask);

   if (wd < 0) {
      err_msg = get_pool_memory(PM_EMSG);

      switch (errno) {
         case EACCES:
            Mmsg(err_msg, "Could not watch Directory. Access Denied for: %s", dir);
            break;

         case ENOENT:
            Mmsg(err_msg, "(ENOENT) Directory Not found: %s", dir);
            break;

         case ENOTDIR:
            Mmsg(err_msg, "(ENOTDIR) Directory Not found: %s", dir);
            break;

         case ENOMEM:
            Mmsg(err_msg, "Insuficient kernel memory available.");
            break;

         case ENOSPC:
            Mmsg(err_msg, "Exceeded limit of watched directories. "
                  "Please increase the number of available "
                  "file descriptors in your system.");
            break;

         default:
            Mmsg(err_msg, "Unknown Error. Please contact Bacula Support.");
            break;
      }

      goto bail_out;
   }

   Dmsg1(0, "Started Watching: %s\n", dir);
   _watchedDirs[wd] = bstrdup(dir);
   dirReader = opendir(dir);

   if (strcmp(dir, "/") == 0) {
      separator = "";
   } else {
      separator = "/";
   }

   while ((dirFile = readdir(dirReader)) != NULL) {
      if (dirFile->d_type != DT_DIR
            || strcmp(dirFile->d_name, ".") == 0
            || strcmp(dirFile->d_name, ".." ) == 0) {
         continue;
      }

      subdirPath = get_pool_memory(PM_FNAME);
      Mmsg(subdirPath, "%s%s%s", dir, separator, dirFile->d_name);
      err_msg = this->watchDirRecursive(subdirPath);
      free_and_null_pool_memory(subdirPath);

      if (err_msg != NULL) {
         goto bail_out;
      }
   }

bail_out:

   if (dirReader != NULL) {
      closedir(dirReader);
   }

   return err_msg;
}

void FolderWatcher::handleEvent(struct inotify_event *event)
{
   Dmsg1(50, "INotify Event: 0x%08x\n", event->mask);
   POOLMEM *fpath = get_pool_memory(PM_FNAME);
   Mmsg(fpath, "%s/%s",
         _watchedDirs[event->wd],
         event->name
       );
   Dmsg1(50, "TAGH: %s\n", fpath);
   bool openFileEvent = ((event->mask & IN_OPEN) == IN_OPEN);
   bool closeWriteFileEvent = ((event->mask & IN_CLOSE_WRITE) == IN_CLOSE_WRITE);
   bool closeNoWriteFileEvent = ((event->mask & IN_CLOSE_NOWRITE) == IN_CLOSE_NOWRITE);
   bool modifyEvent = ((event->mask & IN_MODIFY) == IN_MODIFY);
   bool attribsChangeEvent = ((event->mask & IN_ATTRIB) == IN_ATTRIB);
   bool movedToEvent = ((event->mask & IN_MOVED_TO) == IN_MOVED_TO);
   bool createEvent = ((event->mask & IN_CREATE) == IN_CREATE);
   bool isDir = ((event->mask & IN_ISDIR) == IN_ISDIR);

   if (openFileEvent && !isDir) {
      _openedFiles.insert(event->wd);
   } else if (closeWriteFileEvent && !isDir) {
      _openedFiles.erase(event->wd);
      _changeHandler->onChange(fpath);
   } else if (closeNoWriteFileEvent) {
      _openedFiles.erase(event->wd);
   } else if (createEvent && isDir) {
      this->watchDirRecursive(fpath);
   } else if (movedToEvent) {
      _changeHandler->onChange(fpath);
   } else if ((modifyEvent || attribsChangeEvent)
         && (_openedFiles.find(event->wd) == _openedFiles.end())
         && !isDir) {
      _changeHandler->onChange(fpath);
   }

   free_and_null_pool_memory(fpath);
}

void FolderWatcher::handleINotifyEvents(int fd)
{
   int readlen;
   struct inotify_event *event;
   int event_struct_size = sizeof(struct inotify_event);
   int i = 0;
   int error;
   int bsize = 2048;
   char *buffer = (char *) malloc(bsize);

try_again:
   readlen = read(fd, buffer, bsize);
   error = errno;
   if (readlen < 0 && error == EINVAL) {
      bsize = bsize * 2;
      realloc(buffer, bsize);
      goto try_again;
   } else if (readlen < 0) {
      return;
   }

   while (i + event_struct_size < readlen) {
      event = (struct inotify_event *) &buffer[i];
      if (event == NULL) {
         i += event_struct_size;
         continue;
      }

      if (event->len > 0 && event->wd > -1) {
         this->handleEvent(event);
      }

      i += event_struct_size + event->len;
   }

   free(buffer);
}

FolderWatcher::~FolderWatcher()
{
   _run_inotify_thread = false;
   pthread_kill(_inotifyThread, SIGUSR2);
   pthread_join(_inotifyThread, NULL);

   std::map<int, char *>::iterator it;
   for(it = _watchedDirs.begin(); it != _watchedDirs.end(); it++) {
      inotify_rm_watch(_fd, it->first);
      Dmsg1(0, "Stopped Watching: %s\n", it->second);
      free(it->second);
   }
}

#else

BOOL IsDirectory(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);
  return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

/**
  Windows Implementation
 */
void *thread_watcher(void *arg) {
   FolderWatcher *watcher = (FolderWatcher *) arg;
   HANDLE _resultEvent = CreateEvent(NULL, true, false, NULL);
   HANDLE _stopEvent = CreateEvent(NULL, true, false, NULL); 
   OVERLAPPED overlapped;
   overlapped.hEvent = _resultEvent;
   int bsize = 10 * 4096;
   char *buffer = (char *) malloc(bsize);
   POOLMEM *fpath;
   char *fsubpath;
   FILE * fp;

   while(watcher->_run_watcher_thread) {
      ResetEvent(_resultEvent);
      FILE_NOTIFY_INFORMATION *pBuffer =
         (FILE_NOTIFY_INFORMATION *) buffer; 
      DWORD dwBytesReturned = 0;
      SecureZeroMemory(pBuffer, bsize);

      if (!ReadDirectoryChangesW(watcher->_dirHandle, (LPVOID) pBuffer,
               bsize, true,
               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE, 
               &dwBytesReturned,
               &overlapped,
               NULL)) {
         DWORD errorCode = GetLastError();

         if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
            Dmsg0(0, "WinNotify buffer overflow\n");
         } else {
            Dmsg0(0, "Generic ReadDirectoryChangesW error\n");
         }
         continue;
      }

      HANDLE handles[] = { _resultEvent, _stopEvent };
      DWORD result = WaitForMultipleObjects(
            2, handles,
            false, // awake once one of them arrives
            INFINITE);

      if (result == 1) {
         Dmsg0(0, "Received stop event, aborting folder watcher thread\n");
         continue;
      }

      if (result != 0) {
         Dmsg1(0, "WaitForMultipleObjects failed. Error: %ld\n", GetLastError());
         continue;
      }

      bool ok = GetOverlappedResult(watcher->_dirHandle, &overlapped, &dwBytesReturned, false);

      if (!ok) {
         DWORD errorCode = GetLastError();
         if (errorCode == ERROR_NOTIFY_ENUM_DIR) {
            Dmsg0(0, "WinNotify buffer overflow\n");
         } else {
            Dmsg0(0, "Generic GetOverlappedResult error\n");
         }
         continue;
      }

      FILE_NOTIFY_INFORMATION *curEntry = pBuffer;  

      for (;;) {
         fpath = get_pool_memory(PM_FNAME);
         fsubpath = get_pool_memory(PM_FNAME);
         const size_t fnameLen = curEntry->FileNameLength * 2;
         check_pool_memory_size(fsubpath, fnameLen);
         wcstombs(fsubpath, curEntry->FileName, fnameLen);
         Mmsg(fpath, "%s/%s",
               watcher->_watchedDirPath,
               fsubpath);
         CDP::winToUnixPath(fpath);
         Dmsg1(0, "WinNotify Event: 0x%08x\n", curEntry->Action);
         Dmsg1(0, "Event for file: %s\n", fpath);

#ifdef UNICODE
         const size_t fpathSize = strlen(fpath) + 1;
         wchar_t *wc_fpath = new wchar_t[fpathSize];
         mbstowcs(wc_fpath, fpath, fpathSize);
         LPCTSTR lpct_fpath = wc_fpath;
#else
         LPCTSTR lpct_fpath = fpath;
#endif

         // Only notify changes if fpath is a file
         // and if fpath is NOT a new empty file
         if ((curEntry->Action == FILE_ACTION_MODIFIED
             || curEntry->Action == FILE_ACTION_RENAMED_NEW_NAME)
             && !IsDirectory(lpct_fpath)) {

            /* Windows may still be holding the file lock,
               so we only notify the change when we make sure
               we can access the file */
            for (int i = 0; i < 120; i++) { // Run for 1 minute 
               fp = fopen(fpath, "r");
   
               if (fp) {
                  Dmsg1(0, "Change detected in file: %s\n", fpath);
                  watcher->_changeHandler->onChange(fpath);
                  fclose(fp);
                  break;
               }

               Sleep(500);
            }

         }

         free_and_null_pool_memory(fsubpath);
         free_and_null_pool_memory(fpath);

         if (curEntry->NextEntryOffset == 0) {
            break;
         }

         curEntry = (FILE_NOTIFY_INFORMATION *) ((char *) curEntry + curEntry->NextEntryOffset);
      }
   }

   return NULL;
}

FolderWatcher::FolderWatcher(FileChangeHandler *handler)
{
   _run_watcher_thread = false;
   _changeHandler = handler;
}

POOLMEM *FolderWatcher::watch(const char *folder)
{
   _watchedDirPath = bstrdup(folder);
   const size_t size = cstrlen(folder) + 1;
   wchar_t *wfolder = new wchar_t[size];
   mbstowcs(wfolder, folder, size);

   _dirHandle = CreateFileW(
         wfolder,
         FILE_LIST_DIRECTORY,
         FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
         NULL,
         OPEN_EXISTING,
         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
         NULL);

   if (_dirHandle == INVALID_HANDLE_VALUE) {
      _dirHandle = NULL;
      POOLMEM *err_msg = get_pool_memory(PM_EMSG);
      Mmsg(err_msg, "Error: could not create handle for folder %s. "
            "Please, contact Bacula Support.\n", folder);
      return err_msg;
   }

   _run_watcher_thread = true;
   int rc = pthread_create(&_watcherThread, NULL, thread_watcher, (void *) this);

   if (rc) {
      POOLMEM *err_msg = get_pool_memory(PM_EMSG);
      Mmsg(err_msg, "Error: could not start Watcher Thread. "
            "Please, contact Bacula Support.\n");
      return err_msg;
   }

   delete wfolder;
   Dmsg1(0, "Started watching: %s\n", folder);
   return NULL;
}

FolderWatcher::~FolderWatcher()
{
   _run_watcher_thread = false;
   pthread_kill(_watcherThread, SIGUSR2);
   pthread_join(_watcherThread, NULL);
   CancelIo(_dirHandle);
   CloseHandle(_dirHandle);
   _dirHandle = NULL;
   Dmsg1(0, "Stopped watching: %s\n", _watchedDirPath);
   free(_watchedDirPath);
}
#endif
