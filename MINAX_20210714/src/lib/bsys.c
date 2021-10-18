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
 * Miscellaneous Bacula memory and thread safe routines
 *   Generally, these are interfaces to system or standard
 *   library routines.
 *
 *  Bacula utility functions are in util.c
 *
 */

#include "bacula.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer = PTHREAD_COND_INITIALIZER;

/* bacula => Bacula
 * Works only for standard ASCII strings
 */
char *ucfirst(char *dst, const char *src, int len)
{
   int i=0;
   len--;                       /* Keep the last byte for \0 */
   for (i=0; src[i] && i < len ; i++) {
      dst[i] = (i == 0) ? toupper(src[i]) : tolower(src[i]);
   }
   dst[i] = 0;
   return dst;
}

/*
 * Quote a string
 */
POOLMEM *quote_string(POOLMEM *&snew, const char *old)
{
   char *n;
   int i;

   if (!old) {
      strcpy(snew, "null");
      return snew;
   }
   snew = check_pool_memory_size(snew, strlen(old)*2+2+1);
   n = snew;
   *n++ = '"';
   for (i=0; old[i]; i++) {
      switch (old[i]) {
      case '"':
         *n++ = '\\';
         *n++ = '"';
         break;
      case '\\':
         *n++ = '\\';
         *n++ = '\\';
         break;
      case '\r':
         *n++ = '\\';
         *n++ = 'r';
         break;
      case '\n':
         *n++ = '\\';
         *n++ = 'n';
         break;
      default:
         *n++ = old[i];
         break;
      }
   }
   *n++ = '"';
   *n = 0;
   return snew;
}

/*
 * Quote a where (list of addresses separated by spaces)
 */
POOLMEM *quote_where(POOLMEM *&snew, const char *old)
{
   char *n;
   int i;

   if (!old) {
      strcpy(snew, "null");
      return snew;
   }
   snew = check_pool_memory_size(snew, strlen(old)*3+2+1);
   n = snew;
   *n++ = '"';
   for (i=0; old[i]; i++) {
      switch (old[i]) {
      case ' ':
         *n++ = '"';
         *n++ = ',';
         *n++ = '"';
         break;
      case '"':
         *n++ = '\\';
         *n++ = '"';
         break;
      case '\\':
         *n++ = '\\';
         *n++ = '\\';
         break;
      default:
         *n++ = old[i];
         break;
      }
   }
   *n++ = '"';
   *n = 0;
   return snew;
}

/*
 * This routine is a somewhat safer unlink in that it
 *   allows you to run a regex on the filename before
 *   excepting it. It also requires the file to be in
 *   the working directory.
 */
int safer_unlink(const char *pathname, const char *regx)
{
   int rc;
   regex_t preg1;
   char prbuf[500];
   const int nmatch = 30;
   regmatch_t pmatch[nmatch];
   int rtn;

   /* Name must start with working directory */
   if (strncmp(pathname, working_directory, strlen(working_directory)) != 0) {
      Pmsg1(000, "Safe_unlink excluded: %s\n", pathname);
      return EROFS;
   }

   /* Compile regex expression */
   rc = regcomp(&preg1, regx, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg1, prbuf, sizeof(prbuf));
      Pmsg2(000,  _("safe_unlink could not compile regex pattern \"%s\" ERR=%s\n"),
           regx, prbuf);
      return ENOENT;
   }

   /* Unlink files that match regexes */
   if (regexec(&preg1, pathname, nmatch, pmatch,  0) == 0) {
      Dmsg1(100, "safe_unlink unlinking: %s\n", pathname);
      rtn = unlink(pathname);
   } else {
      Pmsg2(000, "safe_unlink regex failed: regex=%s file=%s\n", regx, pathname);
      rtn = EROFS;
   }
   regfree(&preg1);
   return rtn;
}

/*
 * This routine will sleep (sec, microsec).  Note, however, that if a
 *   signal occurs, it will return early.  It is up to the caller
 *   to recall this routine if he/she REALLY wants to sleep the
 *   requested time.
 */
int bmicrosleep(int32_t sec, int32_t usec)
{
   struct timespec timeout;
   struct timeval tv;
   struct timezone tz;
   int stat;

   timeout.tv_sec = sec;
   timeout.tv_nsec = usec * 1000;

#ifdef HAVE_NANOSLEEP
   stat = nanosleep(&timeout, NULL);
   if (!(stat < 0 && errno == ENOSYS)) {
      return stat;
   }
   /* If we reach here it is because nanosleep is not supported by the OS */
#endif

   /* Do it the old way */
   gettimeofday(&tv, &tz);
   timeout.tv_nsec += tv.tv_usec * 1000;
   timeout.tv_sec += tv.tv_sec;
   while (timeout.tv_nsec >= 1000000000) {
      timeout.tv_nsec -= 1000000000;
      timeout.tv_sec++;
   }

   Dmsg2(200, "pthread_cond_timedwait sec=%d usec=%d\n", sec, usec);
   /* Note, this unlocks mutex during the sleep */
   P(timer_mutex);
   stat = pthread_cond_timedwait(&timer, &timer_mutex, &timeout);
   if (stat != 0) {
      berrno be;
      Dmsg2(200, "pthread_cond_timedwait stat=%d ERR=%s\n", stat,
         be.bstrerror(stat));
   }
   V(timer_mutex);
   return stat;
}

/* allow using strncpy in this file */
#undef strncpy

/*
 * Guarantee that the string is properly terminated */
char *bstrncpy(char *dest, const char *src, int maxlen)
{
   strncpy(dest, src, maxlen-1);
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Guarantee that the string is properly terminated */
char *bstrncpy(char *dest, POOL_MEM &src, int maxlen)
{
   strncpy(dest, src.c_str(), maxlen-1);
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char *bstrncat(char *dest, const char *src, int maxlen)
{
   int len = strlen(dest);
   if (len < maxlen-1) {
      strncpy(dest+len, src, maxlen-len-1);
   }
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Note: Here the maxlen is the maximum length permitted
 *  stored in dest, while on Unix systems, it is the maximum characters
 *  that may be copied from src.
 */
char *bstrncat(char *dest, POOL_MEM &src, int maxlen)
{
   int len = strlen(dest);
   if (len < maxlen-1) {
      strncpy(dest+len, src.c_str(), maxlen-len-1);
   }
   dest[maxlen-1] = 0;
   return dest;
}

/*
 * Allows one or both pointers to be NULL
 */
bool bstrcmp(const char *s1, const char *s2)
{
   if (s1 == s2) return true;
   if (s1 == NULL || s2 == NULL) return false;
   return strcmp(s1, s2) == 0;
}

/*
 * Allows one or both pointers to be NULL
 */
bool bstrcasecmp(const char *s1, const char *s2)
{
   if (s1 == s2) return true;
   if (s1 == NULL || s2 == NULL) return false;
   return strcasecmp(s1, s2) == 0;
}


/*
 * Get character length of UTF-8 string
 *
 * Valid UTF-8 codes
 * U-00000000 - U-0000007F: 0xxxxxxx
 * U-00000080 - U-000007FF: 110xxxxx 10xxxxxx
 * U-00000800 - U-0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
 * U-00010000 - U-001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-00200000 - U-03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
int cstrlen(const char *str)
{
   uint8_t *p = (uint8_t *)str;
   int len = 0;
   if (str == NULL) {
      return 0;
   }
   while (*p) {
      if ((*p & 0xC0) != 0xC0) {
         p++;
         len++;
         continue;
      }
      if ((*p & 0xD0) == 0xC0) {
         p += 2;
         len++;
         continue;
      }
      if ((*p & 0xF0) == 0xD0) {
         p += 3;
         len++;
         continue;
      }
      if ((*p & 0xF8) == 0xF0) {
         p += 4;
         len++;
         continue;
      }
      if ((*p & 0xFC) == 0xF8) {
         p += 5;
         len++;
         continue;
      }
      if ((*p & 0xFE) == 0xFC) {
         p += 6;
         len++;
         continue;
      }
      p++;                      /* Shouln't get here but must advance */
   }
   return len;
}

/* We need to disable the malloc() macro if SMARTALLOC is not used,
 * else, it points to b_malloc() and causes problems.
 */
#ifndef SMARTALLOC
 #ifdef malloc
  #undef malloc
 #endif
#endif

#ifndef bmalloc
void *bmalloc(size_t size)
{
  void *buf;

#ifdef SMARTALLOC
  buf = sm_malloc(file, line, size);
#else
  buf = malloc(size);
#endif
  if (buf == NULL) {
     berrno be;
     Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
  }
  return buf;
}
#endif

void *b_malloc(const char *file, int line, size_t size)
{
  void *buf;

#ifdef SMARTALLOC
  buf = sm_malloc(file, line, size);
#else
  buf = malloc(size);
#endif
  if (buf == NULL) {
     berrno be;
     e_msg(file, line, M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
  }
  return buf;
}


void bfree(void *buf)
{
#ifdef SMARTALLOC
  sm_free(__FILE__, __LINE__, buf);
#else
  free(buf);
#endif
}

void *brealloc (void *buf, size_t size)
{
#ifdef SMARTALOC
   buf = sm_realloc(__FILE__, __LINE__, buf, size);
#else
   buf = realloc(buf, size);
#endif
   if (buf == NULL) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
   }
   return buf;
}


void *bcalloc(size_t size1, size_t size2)
{
  void *buf;

   buf = calloc(size1, size2);
   if (buf == NULL) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Out of memory: ERR=%s\n"), be.bstrerror());
   }
   return buf;
}

/* Code now in src/lib/bsnprintf.c */
#ifndef USE_BSNPRINTF

#define BIG_BUF 5000
/*
 * Implement snprintf
 */
int bsnprintf(char *str, int32_t size, const char *fmt,  ...)
{
   va_list   arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   len = bvsnprintf(str, size, fmt, arg_ptr);
   va_end(arg_ptr);
   return len;
}

/*
 * Implement vsnprintf()
 */
int bvsnprintf(char *str, int32_t size, const char  *format, va_list ap)
{
#ifdef HAVE_VSNPRINTF
   int len;
   len = vsnprintf(str, size, format, ap);
   str[size-1] = 0;
   return len;

#else

   int len, buflen;
   char *buf;
   buflen = size > BIG_BUF ? size : BIG_BUF;
   buf = get_memory(buflen);
   len = vsprintf(buf, format, ap);
   if (len >= buflen) {
      Emsg0(M_ABORT, 0, _("Buffer overflow.\n"));
   }
   memcpy(str, buf, len);
   str[len] = 0;                /* len excludes the null */
   free_memory(buf);
   return len;
#endif
}
#endif /* USE_BSNPRINTF */

#ifndef HAVE_LOCALTIME_R

struct tm *localtime_r(const time_t *timep, struct tm *tm)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct tm *ltm,

    P(mutex);
    ltm = localtime(timep);
    if (ltm) {
       memcpy(tm, ltm, sizeof(struct tm));
    }
    V(mutex);
    return ltm ? tm : NULL;
}
#endif /* HAVE_LOCALTIME_R */

#ifndef HAVE_WIN32
#include <dirent.h>
/*
 * This is bacula own readdir function, that should be used instead of any
 * other function
 * This function is thread safe.
 * Not all supported systems have a thread safe readdir() function
 * This is why we are using a mutex.
 *
 * The name of the "next" file or directory is returned into d_name
 * that can be resized to fit the size of the entry
 *
 * return 0 for OK
 * return -1 for EOF
 * return >0 is for error, the value returned is errno
*/
int breaddir(DIR *dirp, POOLMEM *&d_name)
{
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

   P(mutex);
   errno = 0;
   struct dirent *d=readdir(dirp);
   int ret = errno;
   if (d != NULL) {
      pm_strcpy(d_name, d->d_name);
      ret=0;
   } else {
      ret = errno==0?-1:errno; // -1 for EOF or errno for error
   }
   V(mutex);
   return ret;
}
#endif

int b_strerror(int errnum, char *buf, size_t bufsiz)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int stat = 0;
    const char *msg;

    P(mutex);

    msg = strerror(errnum);
    if (!msg) {
       msg = _("Bad errno");
       stat = -1;
    }
    bstrncpy(buf, msg, bufsiz);
    V(mutex);
    return stat;
}

#ifdef DEBUG_MEMSET
/* These routines are not normally turned on */
#undef memset
void b_memset(const char *file, int line, void *mem, int val, size_t num)
{
   /* Testing for 2000 byte zero at beginning of Volume block */
   if (num > 1900 && num < 3000) {
      Pmsg3(000, _("Memset for %d bytes at %s:%d\n"), (int)num, file, line);
   }
   memset(mem, val, num);
}
#endif

#if !defined(HAVE_WIN32)
static int del_pid_file_ok = FALSE;
#endif
static int pid_fd = -1;

#ifdef HAVE_FCNTL_LOCK
/* a convenient function [un]lock file using fnctl()
 * code must be in F_UNLCK, F_RDLCK, F_WRLCK
 * return -1 for error and errno is set
 */
int fcntl_lock(int fd, int code)
{
   struct flock l;
   l.l_type = code;
   l.l_whence = l.l_start = l.l_len = 0;
   l.l_len = 1;
   return fcntl(fd, F_SETLK, &l);
}
#endif

/* Create a disk pid "lock" file
 *  returns
 *    0: Error with the error message in errmsg
 *    1: Succcess
 *    2: Successs, but a previous file was found
 */
#if !defined(HAVE_FCNTL_LOCK) || defined(HAVE_WIN32)
int create_lock_file(char *fname, const char *progname, const char *filetype, POOLMEM **errmsg, int *fd)
{
   int ret = 1;
#if !defined(HAVE_WIN32)
   int pidfd, len;
   int oldpid;
   char  pidbuf[20];
   struct stat statp;

   if (stat(fname, &statp) == 0) {
      /* File exists, see what we have */
      *pidbuf = 0;
      if ((pidfd = open(fname, O_RDONLY|O_BINARY, 0)) < 0 ||
           read(pidfd, &pidbuf, sizeof(pidbuf)) < 0 ||
           sscanf(pidbuf, "%d", &oldpid) != 1) {
         berrno be;
         Mmsg(errmsg, _("Cannot open %s file. %s ERR=%s\n"), filetype, fname,
              be.bstrerror());
         close(pidfd); /* if it was successfully opened */
         return 0;
      }
      /* Some OSes (IRIX) don't bother to clean out the old pid files after a crash, and
       * since they use a deterministic algorithm for assigning PIDs, we can have
       * pid conflicts with the old PID file after a reboot.
       * The intent the following code is to check if the oldpid read from the pid
       * file is the same as the currently executing process's pid,
       * and if oldpid == getpid(), skip the attempt to
       * kill(oldpid,0), since the attempt is guaranteed to succeed,
       * but the success won't actually mean that there is an
       * another Bacula process already running.
       * For more details see bug #797.
       */
       if ((oldpid != (int)getpid()) && (kill(oldpid, 0) != -1 || errno != ESRCH)) {
          Mmsg(errmsg, _("%s is already running. pid=%d\nCheck file %s\n"),
               progname, oldpid, fname);
          return 0;
      }
      /* He is not alive, so take over file ownership */
      unlink(fname);                  /* remove stale pid file */
      ret = 2;
   }
   /* Create new pid file */
   if ((pidfd = open(fname, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0640)) >= 0) {
      len = sprintf(pidbuf, "%d\n", (int)getpid());
      write(pidfd, pidbuf, len);
      close(pidfd);
      /* ret is already 1 */
   } else {
      berrno be;
      Mmsg(errmsg, _("Could not open %s file. %s ERR=%s\n"), filetype, fname, be.bstrerror());
      return 0;
   }
#endif
   return ret;
}
#else /* defined(HAVE_FCNTL_LOCK) */
int create_lock_file(char *fname, const char *progname, const char *filetype, POOLMEM **errmsg, int *fd)
{
   int len;
   int oldpid;
   char pidbuf[20];

   /* Open the pidfile for writing */
   if ((*fd = open(fname, O_CREAT|O_RDWR, 0640)) >= 0) {
      if (fcntl_lock(*fd, F_WRLCK) == -1) {
         berrno be;
         /* already locked by someone else, try to read the pid */
         if (read(*fd, &pidbuf, sizeof(pidbuf)) > 0 &&
             sscanf(pidbuf, "%d", &oldpid) == 1) {
            Mmsg(errmsg, _("%s is already running. pid=%d, check file %s\n"),
                 progname, oldpid, fname);
         } else {
            Mmsg(errmsg, _("Cannot lock %s file. %s ERR=%s\n"), filetype, fname, be.bstrerror());
         }
         close(*fd);
         *fd=-1;
         return 0;
      }
      /* write the pid */
      len = sprintf(pidbuf, "%d\n", (int)getpid());
      write(*fd, pidbuf, len);
      /* KEEP THE FILE OPEN TO KEEP THE LOCK !!! */
      return 1;
   } else {
      berrno be;
      Mmsg(errmsg, _("Cannot not open %s file. %s ERR=%s\n"), filetype, fname, be.bstrerror());
      return 0;
   }
}
#endif

/*
 * Create a standard "Unix" pid file.
 */
void create_pid_file(char *dir, const char *progname, int port)
{
   POOLMEM *errmsg = get_pool_memory(PM_MESSAGE);
   POOLMEM *fname = get_pool_memory(PM_FNAME);

   Mmsg(fname, "%s/%s.%d.pid", dir, progname, port);
   if (create_lock_file(fname, progname, "pid", &errmsg, &pid_fd) == 0) {
      Emsg1(M_ERROR_TERM, 0, "%s", errmsg);
      /* never return */
   }
#if !defined(HAVE_WIN32)
   del_pid_file_ok = TRUE;         /* we created it so we can delete it */
#endif

   free_pool_memory(fname);
   free_pool_memory(errmsg);
}

/*
 * Delete the pid file if we created it
 */
int delete_pid_file(char *dir, const char *progname, int port)
{
#if !defined(HAVE_WIN32)
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   if (pid_fd!=-1) {
      close(pid_fd);
   }
   if (!del_pid_file_ok) {
      free_pool_memory(fname);
      return 0;
   }
   del_pid_file_ok = FALSE;
   Mmsg(&fname, "%s/%s.%d.pid", dir, progname, port);
   unlink(fname);
   free_pool_memory(fname);
#endif
   return 1;
}

struct s_state_hdr {
   char id[14];
   int32_t version;
   uint64_t last_jobs_addr;
   uint64_t reserved[20];
};

static struct s_state_hdr state_hdr = {
   "Bacula State\n",
   4,
   0
};

/*
 * Open and read the state file for the daemon
 */
void read_state_file(char *dir, const char *progname, int port)
{
   int sfd;
   ssize_t stat;
   bool ok = false;
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   struct s_state_hdr hdr;
   int hdr_size = sizeof(hdr);

   Mmsg(&fname, "%s/%s.%d.state", dir, progname, port);
   /* If file exists, see what we have */
// Dmsg1(10, "O_BINARY=%d\n", O_BINARY);
   if ((sfd = open(fname, O_RDONLY|O_BINARY)) < 0) {
      berrno be;
      Dmsg3(010, "Could not open state file. sfd=%d size=%d: ERR=%s\n",
            sfd, (int)sizeof(hdr), be.bstrerror());
      goto bail_out;
   }
   if ((stat=read(sfd, &hdr, hdr_size)) != hdr_size) {
      berrno be;
      Dmsg4(010, "Could not read state file. sfd=%d stat=%d size=%d: ERR=%s\n",
                    sfd, (int)stat, hdr_size, be.bstrerror());
      goto bail_out;
   }
   if (hdr.version != state_hdr.version) {
      Dmsg2(010, "Bad hdr version. Wanted %d got %d\n",
         state_hdr.version, hdr.version);
      goto bail_out;
   }
   hdr.id[13] = 0;
   if (strcmp(hdr.id, state_hdr.id) != 0) {
      Dmsg0(000, "State file header id invalid.\n");
      goto bail_out;
   }
// Dmsg1(010, "Read header of %d bytes.\n", sizeof(hdr));
   if (!read_last_jobs_list(sfd, hdr.last_jobs_addr)) {
      goto bail_out;
   }
   ok = true;
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }
   if (!ok) {
      unlink(fname);
    }
   free_pool_memory(fname);
}

/*
 * Write the state file
 */
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

void write_state_file(char *dir, const char *progname, int port)
{
   int sfd;
   bool ok = false;
   POOLMEM *fname = get_pool_memory(PM_FNAME);

   P(state_mutex);                    /* Only one job at a time can call here */
   Mmsg(&fname, "%s/%s.%d.state", dir, progname, port);
   /* Create new state file */
   unlink(fname);
   if ((sfd = open(fname, O_CREAT|O_WRONLY|O_BINARY, 0640)) < 0) {
      berrno be;
      Dmsg2(000, "Could not create state file. %s ERR=%s\n", fname, be.bstrerror());
      Emsg2(M_ERROR, 0, _("Could not create state file. %s ERR=%s\n"), fname, be.bstrerror());
      goto bail_out;
   }
   if (write(sfd, &state_hdr, sizeof(state_hdr)) != sizeof(state_hdr)) {
      berrno be;
      Dmsg1(000, "Write hdr error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }
// Dmsg1(010, "Wrote header of %d bytes\n", sizeof(state_hdr));
   state_hdr.last_jobs_addr = sizeof(state_hdr);
   state_hdr.reserved[0] = write_last_jobs_list(sfd, state_hdr.last_jobs_addr);
// Dmsg1(010, "write last job end = %d\n", (int)state_hdr.reserved[0]);
   if (lseek(sfd, 0, SEEK_SET) < 0) {
      berrno be;
      Dmsg1(000, "lseek error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }
   if (write(sfd, &state_hdr, sizeof(state_hdr)) != sizeof(state_hdr)) {
      berrno be;
      Pmsg1(000, _("Write final hdr error: ERR=%s\n"), be.bstrerror());
      goto bail_out;
   }
   ok = true;
// Dmsg1(010, "rewrote header = %d\n", sizeof(state_hdr));
bail_out:
   if (sfd >= 0) {
      close(sfd);
   }
   if (!ok) {
      unlink(fname);
   }
   V(state_mutex);
   free_pool_memory(fname);
}


/* BSDI does not have this.  This is a *poor* simulation */
#ifndef HAVE_STRTOLL
long long int
strtoll(const char *ptr, char **endptr, int base)
{
   return (long long int)strtod(ptr, endptr);
}
#endif

/*
 * Bacula's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD).
 */
#undef fgetc
char *bfgets(char *s, int size, FILE *fd)
{
   char *p = s;
   int ch;
   *p = 0;
   for (int i=0; i < size-1; i++) {
      do {
         errno = 0;
         ch = fgetc(fd);
      } while (ch == EOF && ferror(fd) && (errno == EINTR || errno == EAGAIN));
      if (ch == EOF) {
         if (i == 0) {
            return NULL;
         } else {
            return s;
         }
      }
      *p++ = ch;
      *p = 0;
      if (ch == '\r') { /* Support for Mac/Windows file format */
         ch = fgetc(fd);
         if (ch != '\n') { /* Mac (\r only) */
            (void)ungetc(ch, fd); /* Push next character back to fd */
         }
         p[-1] = '\n';
         break;
      }
      if (ch == '\n') {
         break;
      }
   }
   return s;
}

/*
 * Bacula's implementation of fgets(). The difference is that it handles
 *   being interrupted by a signal (e.g. a SIGCHLD) and it has a
 *   different calling sequence which implements input lines of
 *   up to a million characters.
 */
char *bfgets(POOLMEM *&s, FILE *fd)
{
   int ch;
   int soft_max;
   int i = 0;

   s[0] = 0;
   soft_max = sizeof_pool_memory(s) - 10;
   for ( ;; ) {
      do {
         errno = 0;
         ch = fgetc(fd);
      } while (ch == EOF && ferror(fd) && (errno == EINTR || errno == EAGAIN));
      if (ch == EOF) {
         if (i == 0) {
            return NULL;
         } else {
            return s;
         }
      }
      if (i > soft_max) {
         /* Insanity check */
         if (soft_max > 1000000) {
            return s;
         }
         s = check_pool_memory_size(s, soft_max+10000);
         soft_max = sizeof_pool_memory(s) - 10;
      }
      s[i++] = ch;
      s[i] = 0;
      if (ch == '\r') { /* Support for Mac/Windows file format */
         ch = fgetc(fd);
         if (ch != '\n') { /* Mac (\r only) */
            (void)ungetc(ch, fd); /* Push next character back to fd */
         }
         s[i-1] = '\n';
         break;
      }
      if (ch == '\n') {
         break;
      }
   }
   return s;
}

/*
 * Make a "unique" filename.  It is important that if
 *   called again with the same "what" that the result
 *   will be identical. This allows us to use the file
 *   without saving its name, and re-generate the name
 *   so that it can be deleted.
 */
void make_unique_filename(POOLMEM **name, int Id, char *what)
{
   Mmsg(name, "%s/%s.%s.%d.tmp", working_directory, my_name, what, Id);
}

char *escape_filename(const char *file_path)
{
   if (file_path == NULL || strpbrk(file_path, "\"\\") == NULL) {
      return NULL;
   }

   char *escaped_path = (char *)bmalloc(2 * (strlen(file_path) + 1));
   char *cur_char = escaped_path;

   while (*file_path) {
      if (*file_path == '\\' || *file_path == '"') {
         *cur_char++ = '\\';
      }

      *cur_char++ = *file_path++;
   }

   *cur_char = '\0';

   return escaped_path;
}

#if HAVE_BACKTRACE && HAVE_GCC && HAVE_LINUX_OS
/* if some names are not resolved you can try using : addr2line, like this
 * $ addr2line -e bin/bacula-sd -a 0x43cd11
 * OR
 * use the the -rdynamic option in the linker, like this
 * $ LDFLAGS="-rdynamic" make setup
 */
#include <cxxabi.h>
#include <execinfo.h>
void stack_trace()
{
   const size_t max_depth = 100;
   size_t stack_depth;
   void *stack_addrs[max_depth];
   char **stack_strings;
   char syscom[512];
   BPIPE *bpipe;
   bool ok;

   stack_depth = backtrace(stack_addrs, max_depth);
   stack_strings = backtrace_symbols(stack_addrs, stack_depth);

   for (size_t i = 3; i < stack_depth; i++) {
      size_t sz = 200; /* just a guess, template names will go much wider */
      char *begin = 0, *end = 0, *final = 0;
      /* find the parentheses and address offset surrounding the mangled name */
      for (char *j = stack_strings[i]; *j; ++j) {
         if (*j == '(') {
            begin = j;
         } else if (*j == '+') {
            end = j;
         } else if (*j == ')') {
            final = j;
         }
      }
      ok = false;
      if (begin && end && end>(begin+1)) {
         /* /home/bac/workspace2/bee/regress/bin/bacula-dir(+0x3c400) */
         char *function = (char *)actuallymalloc(sz);
         *begin++ = '\0';
         *end = '\0';
         /* found our mangled name, now in [begin, end] */

         int status;
         char *ret = abi::__cxa_demangle(begin, function, &sz, &status);
         if (ret) {
            /* return value may be a realloc() of the input */
            function = ret;
         } else {
            /* demangling failed, just pretend it's a C function with no args */
            bstrncpy(function, begin, sz);
            bstrncat(function, "()", sz);
         }
         Pmsg2(000, "    %s:%s\n", stack_strings[i], function);
         actuallyfree(function);
         ok = true;
      } else if (begin) {
         /* .../regress/bin/bacula-dir(+0x3c400) */
         /* demangle cannot work on an empty function name, use addr2line() */
         // this should work, but it don't
         // sprintf(syscom, "addr2line %p -e %.*s", stack_addrs[i], (int)(begin-stack_strings[i]), stack_strings[i]);
         // use the "+0x3c400" above for the address
         if (end && final) {
            snprintf(syscom, sizeof(syscom), "addr2line %.*s -e %.*s", (int)(final-end)-1, end+1, (int)(begin-stack_strings[i]), stack_strings[i]);
            bpipe = open_bpipe(syscom, 0, "r");
            if (bpipe) {
               char buf[1000];
               *buf = '\0';
               while (fgets(buf, sizeof(buf), bpipe->rfd)) {
                  Pmsg1(000, "    %s\n", buf);
               }
               if (close_bpipe(bpipe) == 0) {
                  ok = true;
               }
            }
         }
      }
      if (!ok) {
         /* didn't find the mangled name, just print the whole line */
         Pmsg1(000, "    %s\n", stack_strings[i]);
      }
   }
   actuallyfree(stack_strings); /* malloc()ed by backtrace_symbols */
}
#include <sys/types.h>
#include <sys/syscall.h>

int gdb_get_threadid(char *name_buf, int len)
{
   int thread_num = -1;
   char syscom[1024];
   BPIPE *bpipe;

   int systag = syscall(__NR_gettid);

   name_buf[readlink("/proc/self/exe", name_buf, len-1)]=0;
   snprintf(syscom, sizeof(syscom), "gdb --batch -n -ex \"thread find %d\" %s %d", systag, name_buf, getpid());
   bpipe = open_bpipe(syscom, 0, "r");
   if (bpipe) {
      char buf[1000];
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
         // thread find 241041
         // Thread 7 has target id 'Thread 0x7f8c62ffd700 (LWP 241041)'
         int tn;
         if (scan_string(buf, "Thread %d", &tn)==1) {
            thread_num = tn;
         }
      }
      if (close_bpipe(bpipe) !=0) {
         return -1;
      }
   } else {
      return -1;
   }
   return thread_num;
}

void gdb_stack_trace()
{
   char name_buf[512];
   char syscom[1024];
   BPIPE *bpipe;

   int thread_num = gdb_get_threadid(name_buf, sizeof(name_buf));
   if (thread_num < 0) {
      return;
   }
   snprintf(syscom, sizeof(syscom), "gdb --batch -n -ex \"thread apply %d bt\" %s %d", thread_num, name_buf, getpid());
   bpipe = open_bpipe(syscom, 0, "r");
   if (bpipe) {
      bool ok = false;
      char buf[1000];
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
         if (!ok) {
            // Skip the "header" up to the caller of gdb_stack_trace()
            ok = strstr(buf, "in gdb_stack_trace")!=NULL;
         } else {
            Pmsg1(000, "    %s", buf);
         }
      }
      if (close_bpipe(bpipe) !=0) {
         return;
      }
   }
}

void gdb_print_local(int level)
{
   char name_buf[512];
   char syscom[1024];
   char fname[64];
   int fd;
   FILE *fp;
   BPIPE *bpipe = NULL;
   int thread_num = gdb_get_threadid(name_buf, sizeof(name_buf));
   if (thread_num < 0) {
      return;
   }
   bstrncpy(fname, "/tmp/traces.XXXXXX", sizeof(fname));
   fd = mkstemp(fname);
   if (fd < 0) {
      return;
   }
   fp = fdopen(fd, "w");
   if (!fp) {
      goto bail_out;
   }
   fprintf(fp, "thread %d\nf %d\nprint \":here:\"\ninfo locals\ndetach\nquit\n", thread_num, level + 5);
   fclose(fp);
   snprintf(syscom, sizeof(syscom), "gdb -quiet --batch -x %s %s %d", fname, name_buf, getpid());
   bpipe = open_bpipe(syscom, 0, "r");
   if (bpipe) {
      bool ok = false;
      char buf[1000];
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
         if (!ok) {
            // Skip the "header" up to the caller of gdb_stack_trace()
            ok = strstr(buf, ":here:")!=NULL;
         } else {
            Pmsg1(000, "    %s", buf);
         }
      }
   }
bail_out:
   unlink(fname);
   if (bpipe) {
      close_bpipe(bpipe);
   }

}

#else /* HAVE_BACKTRACE && HAVE_GCC */
void stack_trace() {}
void gdb_stack_trace() {}
void gdb_print_local() {}
#endif /* HAVE_BACKTRACE && HAVE_GCC */

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#else
#define statvfs statfs
#endif
/* statvfs.h defines ST_APPEND, which is also used by Bacula */
#undef ST_APPEND


int fs_get_free_space(const char *path, int64_t *freeval, int64_t *totalval)
{
#if defined(HAVE_SYS_STATVFS_H) || !defined(HAVE_WIN32)
   struct statvfs st;

   if (statvfs(path, &st) == 0) {
      *freeval  = (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
      *totalval = (uint64_t)st.f_blocks * (uint64_t)st.f_frsize;
      return 0;
   }
#endif

   *totalval = *freeval = 0;
   return -1;
}

#if defined(HAVE_DARWIN_OS)
#include <malloc/malloc.h>
#endif

/*
 * Determine the amount of heap used
 * macOS - sbrk(0) is deprecated, use malloc info
 * Windows - not implemented
 * others - use sbrk(0)
 */

/* the initial heap value */
static int64_t start_heap = 0;

void mark_heap()
{
#if defined(HAVE_WIN32)
   start_heap = 0;
#elif defined(HAVE_DARWIN_OS)
   struct mstats ms = mstats();
   start_heap = (int64_t) ms.bytes_used;
#else
   start_heap = (int64_t) sbrk(0);
#endif
}

int64_t heap_used()
{
#if defined(HAVE_WIN32)
   return get_memory_info(NULL, 0);
#elif defined(HAVE_DARWIN_OS)
   struct mstats ms = mstats();
   return (int64_t) ms.bytes_used - start_heap;
#else
   return (int64_t) sbrk(0) - start_heap;
#endif
}

/*
 * This function is used after a fork, the memory manager is not be initialized
 * properly, so we must stay simple.
 */
void setup_env(char *envp[])
{
   if (envp) {
#if defined(HAVE_SETENV)
      char *p;
      for (int i=0; envp[i] ; i++) {
         p = strchr(envp[i], '='); /* HOME=/tmp */
         if (p) {
            *p=0;                       /* HOME\0tmp\0 */
            setenv(envp[i], p+1, true);
            *p='=';
         }
      }
#elif defined(HAVE_PUTENV)
      for (int i=0; envp[i] ; i++) {
         putenv(envp[i]);
      }
#else
#error "putenv() and setenv() are not available on this system"
#endif
   }
}

/* Small function to copy a file somewhere else,
 * for debug purpose.
 */
#ifndef HAVE_WIN32
int copyfile(const char *src, const char *dst)
{
   int     fd_src=-1, fd_dst=-1;
   ssize_t len, lenw;
   char    buf[4096];
   berrno  be;
   fd_src = open(src, O_RDONLY);
   if (fd_src < 0) {
      Dmsg2(0, "Unable to open %s ERR=%s\n", src, be.bstrerror(errno));
      goto bail_out;
   }
   fd_dst = open(dst, O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd_dst < 0) {
      Dmsg2(0, "Unable to open %s ERR=%s\n", dst, be.bstrerror(errno));
      goto bail_out;
   }

   while ((len = read(fd_src, buf, sizeof(buf))) > 0)
    {
        char *out_ptr = buf;
        do {
            lenw = write(fd_dst, out_ptr, len);
            if (lenw >= 0) {
                len -= lenw;
                out_ptr += lenw;
            } else if (errno != EINTR) {
               Dmsg3(0, "Unable to write %d bytes in %s. ERR=%s\n", len, dst, be.bstrerror(errno));
               goto bail_out;
            }
        } while (len > 0);
    }

    if (len == 0) {
       close(fd_src);
       if (close(fd_dst) < 0) {
          Dmsg2(0, "Unable to close %s properly. ERR=%s\n", dst, be.bstrerror(errno));
          return -1;
       }
       /* Success! */
       return 0;
    }
bail_out:
    close(fd_src);
    close(fd_dst);
    return -1;
}
#else

int copyfile(const char *src, const char *dst)
{
    if (CopyFile((LPCSTR) src, (LPCSTR) dst, false)) {
       return 0;
    }

    return -1;
}

#endif


/* The poll() code is currently disabled */
#ifdef HAVE_POLL

#include <poll.h>
#define NB_EVENT 1

int fd_wait_data(int fd, fd_wait_mode mode, int sec, int msec)
{
   int ret;
   struct pollfd fds[NB_EVENT]; /* The structure for one event */

   fds[0].fd = fd;
   fds[0].events = (mode == WAIT_READ) ? POLLIN : POLLOUT;

   ret = poll(fds, NB_EVENT, sec * 1000 + msec);

   /* Check if poll actually succeed */
   switch(ret) {
   case 0:                      /* timeout; no event detected */
      return 0;

   case -1:                     /* report error and abort */
      return -1;

   default:
      if (fds[0].revents & POLLIN || fds[0].revents & POLLOUT) {
         return 1;

      } else {
         return -1;             /* unexpected... */
      }
   }
   return -1;                   /* unexpected... */
}
#else

/* The select() code with a bigger fd_set was tested on Linux, FreeBSD and SunOS */
#if defined(HAVE_LINUX_OS) || defined(HAVE_FREEBSD_OS) || defined(HAVE_SUN_OS) || defined(HAVE_WIN32)
 #define SELECT_MAX_FD 7990
#else
 #define SELECT_MAX_FD 1023     /* For others, we keep it low */
#endif

int fd_wait_data(int fd, fd_wait_mode mode, int sec, int msec)
{
   union {
      fd_set fdset;
      char bfd_buf[1000];
   };

   fd_set *pfdset=NULL, *tmp=NULL;
   struct timeval tv;
   int ret;

   /* If the amount of static memory is not big enough to handle the file
    * descriptor, we allocate a new buffer ourself
    */
   if (fd > SELECT_MAX_FD) {
      int len = (fd+1+1024) * sizeof(char);
      tmp = (fd_set *) malloc(len);
      pfdset = tmp;
      memset(tmp, 0, len);      /* FD_ZERO() */

   } else {
      pfdset = &fdset;
      memset(&bfd_buf, 0, sizeof(bfd_buf)); /* FD_ZERO(&fdset) */
   }

   FD_SET((unsigned)fd, pfdset);

   tv.tv_sec = sec;
   tv.tv_usec = msec * 1000;

   if (mode == WAIT_READ) {
      ret = select(fd + 1, pfdset, NULL, NULL, &tv);

   } else { /* WAIT_WRITE */
      ret = select(fd + 1, NULL, pfdset, NULL, &tv);
   }
   if (tmp) {
      free(tmp);
   }
   switch (ret) {
   case 0:                      /* timeout */
      return 0;
   case -1:
      return -1;                /* error return */
   default:
      break;
   }
   return 1;
}
#endif

/* Use SOCK_CLOEXEC option when calling accept(). If not available,
 * do it ourself (but with a race condition...)
 */
int baccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
   int fd;
#ifdef HAVE_ACCEPT4
   fd = accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
#else
   fd = accept(sockfd, addr, addrlen);

# ifdef HAVE_DECL_FD_CLOEXEC
   if (fd >= 0) {
      int tmp_errno = errno;
      if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0) {
         berrno be;
         Dmsg2(0, "Unable to set the CLOEXEC flag on fd=%d ERR=%s\n", fd, be.bstrerror());
      }
      errno = tmp_errno;
   }

# endif  /* HAVE_DECL_FD_CLOEXEC */
#endif   /* HAVE_ACCEPT4 */
   return fd;
}

/* Return the memory available on this system, 0 if not implemented */
/* See https://github.com/ganglia/monitor-core/tree/master/libmetrics to have
 * an implementation for all kind of systems (from windows, solaris, aix...)
 * mem_free_func()
 */
uint64_t bget_os_memory()
{
   POOLMEM *buf=NULL;
   uint64_t ret = 0;

#ifdef HAVE_LINUX_OS
   bool ok=true;
   const char *keyword="MemTotal:";
   static int len=strlen(keyword);
   FILE *fp = bfopen("/proc/meminfo", "r");

   if (!fp) {
      berrno be;
      Dmsg1(10, "Unable to open /proc/meminfo. ERR=%s\n", be.bstrerror());
      goto bail_out;
   }

   buf = get_pool_memory(PM_FNAME);
   while (ok && bfgets(buf, fp)) {
      if (strcmp(buf, keyword) > 0) {
         if (!size_to_uint64(buf+len, strlen(buf+len), &ret)) {
            ret = 0;
         }
         ok=false;
      }
   }
   fclose(fp);

bail_out:
#endif  /* TODO: Implement more systems as needed */

   free_and_null_pool_memory(buf);
   return ret;
}

/* Determine the amount of mlock memory we can allocate on the current
 * system. If the value is < 0, this is the memory to keep for the os.
 */
uint64_t bget_max_mlock(int64_t value)
{
   uint64_t sys, val;

   sys = bget_os_memory();
   if (sys == 0) {
      Dmsg0(50, "Unable to determine the memory for mlock_max\n");
      if (value < 0) {          /* We cannot compute the number  */
         return 0;
      }
      return value;             /* We can't say our word... */
   }

   if (value == 0) {
      Dmsg0(50, "Limit not set, use the maximum for mlock_max\n");
      value = sys;              /* Limit automatically to the maximum */
   }

   /* When the value is negative, this is the amount in bytes to keep for the
    * system.
    */
   if (value < 0) {
      value = sys + value;
      if (value < 0) {
         Dmsg0(50, "Limit incorrect set, use the maximum for mlock_max\n");
         /* Request to keep 2GB, we have only 1GB, something is incorrect, so
          * we take the maximum
          */
         value = sys;
      }
   }

   /*
    * Min     Max  | Allowed
    *--------------+------------
    * 0    -> 2GB  | 0   -> 1GB
    * 2GB  -> 10GB | 1GB -> 9GB
    * 10GB -> 60GB | 9GB -> 54GB
    * 60GB -> ...  | 54GB ...
    *
    */
   val = value;
   if (sys < 2*1024*1024*1024LL) {
      /* If we have less than 2GB of ram, we can allow up to 50% */
      val = MIN(sys * 0.5, val);

   } else if (sys < 10*1024*1024*1024LL) {
      /* If we have a lot of memory, keep at least 1G for the system */
      val = MIN(sys - 1*1024*1024*1024LL, val);

   } else if (sys < 60*1024*1024*1024LL) {
      /* Below 60GB of ram, keep 10% for the system */
      val = MIN(sys * 0.9, val);

   } else {
      /* For very large systems, keep 6G of ram */
      val = MIN(sys - 6*1024*1024*1024LL, val);
   }
   Dmsg2(50, "Requested %lld can %lld\n", value, val);
   return val;
}

#undef fopen
FILE *bfopen(const char *path, const char *mode)
{
   FILE *fp;
   char options[50];

   bstrncpy(options, mode, sizeof(options));

#if defined(HAVE_STREAM_CLOEXEC)
   bstrncat(options, STREAM_CLOEXEC, sizeof(options));
#endif

   fp = fopen(path, options);

#if !defined(HAVE_STREAM_CLOEXEC) && defined(HAVE_DECL_FD_CLOEXEC)
   if (fp) {
      int fd = fileno(fp);
      if (fd >= 0) {
         int tmp_errno = errno;
         if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0) {
            berrno be;
            Dmsg2(0, "Unable to set the CLOEXEC flag on fd=%d ERR=%s\n", fd, be.bstrerror());
         }
         errno = tmp_errno;
      }
   }
#endif
   return fp;
}

/* Used to test the program */
static int init_size=1024;
static int dbglevel=500;

/* alist(100, owned_by_alist) */
/* return 0: ok, -1: error, 1: not found
 * Will return a list of users for a group. We look for /etc/groups
 * and in /etc/passwd
 */
int get_group_members(const char *name, alist *users)
{
   int ret = -1;
   /* Need to create implementation for other OSes */
#ifdef HAVE_LINUX_OS
#ifndef __ANDROID__
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   struct group grp, *pgrp;
   struct passwd pw, *ppw;
   char *buf = NULL;
   int size = init_size;
   gid_t gid = 0;
   bool loop;

again:
   buf = (char *) realloc(buf, size);
   errno = 0;
   ret = getgrnam_r(name, &grp, buf, size, &pgrp);
   if (ret == ERANGE) {
      if (size > 1000000) {
         ret = -1;
         goto bail_out;
      }
      Dmsg2(DT_MEMORY|dbglevel, "realloc from %d to %d\n", size, 2*size);
      size = 2*size;
      goto again;

   } else if (ret == EINTR) {
      goto again;

   } else if (ret != 0) {
      berrno be;
      Dmsg1(dbglevel, "Got error for getgrnam_r %s\n", be.bstrerror(ret));
      ret = -1;
      goto bail_out;

   } else if (pgrp == NULL) {   /* Not found */
      Dmsg1(dbglevel, "group %s not found\n", name);
      ret = 1;
      goto bail_out;

   } else {
      Dmsg1(dbglevel, "Got group definition for %s\n", name);
   }

   gid = grp.gr_gid;
   for (char **p = grp.gr_mem; p && *p ; p++) {
      Dmsg1(dbglevel, "Group Member is: %s\n", *p);
      users->append(bstrdup(*p));
   }

   P(mutex);
   setpwent();
   do {
      loop=false;
      errno = 0;
      ret = getpwent_r(&pw, buf, size, &ppw);
      if (ret == ERANGE) {
         if (size > 1000000) {
            ret = -1;
            endpwent();
            V(mutex);
            goto bail_out;
         }
         Dmsg2(DT_MEMORY|dbglevel, "realloc from %d to %d\n", size, 2*size);
         size = 2*size;
         buf = (char *)realloc(buf, size);
         loop=true;

      } else if (ret == ENOENT) {
         Dmsg0(dbglevel, "End of loop\n");
         ppw = NULL;
         ret = 0;

      } else if (ret != 0) {
         berrno be;
         Dmsg2(dbglevel, "Got error for getpwent_r %d ERR=%s\n", ret, be.bstrerror());
         ret = -1;
         ppw = NULL;

      } else {
         Dmsg1(dbglevel, "Got user %s\n", ppw->pw_name);
         if (ppw->pw_gid == gid) {
            Dmsg1(dbglevel, "Add %s\n", ppw->pw_name);
            users->append(bstrdup(ppw->pw_name));
         }
      }
   } while (ppw || loop);
   endpwent();
   V(mutex);

bail_out:
   if (buf) {
      free(buf);
   }
#endif // __ANDROID__
#endif // HAVE_LINUX_OS
   return ret;
}

/* Get the home directory for a user
 * TODO: Need a Windows implementation
 */
int get_user_home_directory(const char *user, POOLMEM *&home)
{
   int ret=-1;

#ifdef HAVE_LINUX_OS
   struct passwd pw, *ppw;
   int size = init_size;
   char *buf = (char *)malloc(size);

again:
   errno = 0;
   ret =  getpwnam_r(user, &pw, buf, size, &ppw);
   if (ret == ERANGE) {
      if (size > 1000000) {
         ret = -1;
         goto bail_out;
      }
      Dmsg2(DT_MEMORY|dbglevel, "realloc from %d to %d\n", size, 2*size);
      size = 2*size;
      buf = (char *)realloc(buf, size);
      goto again;
   } else if (ret == EINTR) {
      goto again;
   } else if (ret != 0) {
      berrno be;
      Dmsg1(dbglevel, "Got error for getpwnam_r %s\n", be.bstrerror(ret));
      ret = -1;
   } else if (ppw == NULL) {
      Dmsg0(dbglevel, "User not found\n");
      ret = -1;
   }  else {
      Dmsg0(dbglevel, "Got user\n");
      pm_strcpy(home, ppw->pw_dir);
   }
bail_out:
   if (buf) {
      free(buf);
   }
#endif // HAVE_LINUX_OS
   return ret;
}

/* Get the list of the home directories for a given unix group */
int get_home_directories(const char *grpname, alist *dirs)
{
   char *name;
   POOL_MEM dir;
   alist users(100, owned_by_alist);
   if (get_group_members(grpname, &users) == 0) {
      Dmsg1(dbglevel, "get_group_members() = %d\n", users.size());
      foreach_alist(name, &users) {
         Dmsg1(dbglevel, "Get home directory for %s\n", name);
         if (get_user_home_directory(name, dir.addr()) == 0) {
            dirs->append(bstrdup(dir.c_str()));
         }
      }
   }
   return (dirs->size() > 0) ? 0 : -1;
}

#ifdef TEST_PROGRAM

#include "unittests.h"

/* The main idea of the test is pretty simple, we have a writer and a reader, and
 * they wait a little bit to read or send data over the fifo.
 * So, for the first packets, the writer will wait, then the reader will wait
 * read/write requests should always be fast. Only the time of the fd_wait_data()
 * should be long.
 */
#include "findlib/namedpipe.h"
#define PIPENAME "/tmp/wait.pipe.%d"

#define NBPACKETS 10
#define BUFSIZE   128*512       /* The pipe size looks to be 65K */

typedef struct {
   int       nb;
   pthread_t writer;
   pthread_t reader;
} job;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
int nb_ready=0;

void *th1(void *a)
{
   NamedPipe p;
   int fd, r;
   btime_t s, e;
   ssize_t nb;
   char buf[BUFSIZE];
   job *j = (job *)a;

   namedpipe_init(&p);
   bsnprintf(buf, sizeof(buf), PIPENAME, j->nb);
   if (namedpipe_create(&p, buf, 0600) < 0) {
      berrno be;
      Dmsg2(0, "R: Unable to create the fifo %s. ERR=%s\n", buf, be.bstrerror());
      namedpipe_free(&p);
      exit(2);
   }
   fd = namedpipe_open(&p, buf, O_RDONLY);
   if (fd < 0) {
      berrno be;
      Dmsg2(0, "R: Unable to open the fifo %s. ERR=%s\n", buf, be.bstrerror());
      return NULL;
   }
   P(cond_mutex);
   nb_ready++;
   pthread_cond_wait(&cond, &cond_mutex);
   V(cond_mutex);
   for (int i = 0; i < NBPACKETS; i++) {
      if (i < (NBPACKETS/2)) {
         bmicrosleep(5, 0);
      }
      s = get_current_btime();
      r = fd_wait_data(fd, WAIT_READ, 10, 0);
      if (r > 0) {
         e = get_current_btime();
         Dmsg2(0, "Wait to read pkt %d %lldms\n",i, (int64_t) (e - s));

         if (i <= NBPACKETS/2) {
            ASSERT2((e-s) < 10000, "In the 1st phase, we are blocking the process");
         } else {
            ASSERT2((e-s) > 10000, "In the 2nd phase, the writer is slowing down things");
         }

         s = get_current_btime();
         nb = read(fd, buf, sizeof(buf));
         e = get_current_btime();
         Dmsg3(0, "Read pkt %d %d bytes in %lldms\n",i, (int)nb, (int64_t) (e - s));
         ASSERT2((e-s) < 10000, "The read operation should be FAST");
      }
   }
   namedpipe_free(&p);
   return NULL;
}

void *th2(void *a)
{
   NamedPipe p;
   btime_t s, e;
   job *j = (job *)a;
   char buf[BUFSIZE];
   int fd;
   ssize_t nb;

   bsnprintf(buf, sizeof(buf), PIPENAME, j->nb);
   namedpipe_init(&p);
   if (namedpipe_create(&p, buf, 0600) < 0) {
      berrno be;
      Dmsg2(0, "W: Unable to create the fifo %s. ERR=%s\n", buf, be.bstrerror());
      namedpipe_free(&p);
      exit(2);
   }

   fd = namedpipe_open(&p, buf, O_WRONLY);
   if (fd < 0) {
      berrno be;
      Dmsg2(0, "W: Unable to open the fifo %s. ERR=%s\n", buf, be.bstrerror());
      namedpipe_free(&p);
      exit(2);
   }

   P(cond_mutex);
   nb_ready++;
   pthread_cond_wait(&cond, &cond_mutex);
   V(cond_mutex);

   unlink(buf);

   for (int i=0; i < NBPACKETS; i++) {
      if (i > (NBPACKETS/2)) {
         bmicrosleep(5, 0);
      }
      s = get_current_btime();
      if (fd_wait_data(fd, WAIT_WRITE, 10, 0) > 0) {
         e = get_current_btime();
         Dmsg2(0, "Wait to write pkt %d %lldms\n",i, (int64_t) (e - s));

         if (i == 0 || i > NBPACKETS/2) { /* The first packet doesn't count */
            ASSERT2((e-s) < 100000, "In the 2nd phase, it's fast to send, we are the blocker");
         } else {
            ASSERT2((e-s) > 100000, "In the 1st phase, we wait for the reader");
         }

         s = get_current_btime();
         nb = write(fd, buf, sizeof(buf));
         e = get_current_btime();
         Dmsg3(0, "Wrote pkt %d %d bytes in %lldms\n", i, (int)nb, (int64_t) (e - s));
         ASSERT2((e-s) < 100000, "The write operation should never block");
      }
   }
   namedpipe_free(&p);
   return NULL;
}

int main(int argc, char **argv)
{
   Unittests u("bsys", true);
   job pthread_list[10000];
   int j = (argc >= 2) ? atoi(argv[1]) : 1;
   int maxfd = (argc == 3) ? atoi(argv[2]) : 0;
   uint64_t mem = bget_os_memory();
   Dmsg1(0, "mem:  %lld\n", mem);
   Dmsg1(0, "max1: %lld\n", bget_max_mlock(mem));
   Dmsg1(0, "max2: %lld\n", bget_max_mlock(-mem));
   Dmsg1(0, "max3: %lld\n", bget_max_mlock(-100000));
   Dmsg1(0, "max4: %lld\n", bget_max_mlock(mem+10000));
   Dmsg1(0, "max5: %lld\n", bget_max_mlock(mem-10000));

   j = MIN(10000, j);
   lmgr_init_thread();
   set_debug_flags((char *)"h");

   for (int i=3; i < maxfd; i++) {
      open("/dev/null", O_RDONLY);
   }

   for (int i=0; i < j; i++) {
      pthread_list[i].nb=i;
      pthread_create(&pthread_list[i].writer, NULL, th2, &pthread_list[i]);
      pthread_create(&pthread_list[i].reader, NULL, th1, &pthread_list[i]);
   }

   while (nb_ready < j*2) {
      bmicrosleep(1, 0);
   }

   Dmsg0(0, "All threads are started\n");
   P(cond_mutex);
   pthread_cond_broadcast(&cond);
   V(cond_mutex);

   for (int i=0; i < j; i++) {
      pthread_join(pthread_list[i].writer, NULL);
      pthread_join(pthread_list[i].reader, NULL);
   }

   for (int i=3; i < maxfd; i++) {
      close(i);
   }

   /* Start with a small buffer to test if we increase it */
   init_size=20;
   dbglevel=0;
   debug_level=DT_MEMORY;
   alist a(100, owned_by_alist);
   POOL_MEM home;
   char *name;
   int ret;

   ret = get_group_members("bin", &a);
   ok(ret == 0, "get_group_members()");
   ok(a.size() > 0, "get_group_members() size");

   foreach_alist(name, &a) {
      Pmsg1(0, "%s\n", name);
   }
   a.destroy();

   ret = get_home_directories("bin", &a);
   ok(ret == 0, "get_home_directories()");
   ok(a.size() > 0, "get_home_directories(users)");
   foreach_alist(name, &a) {
      Pmsg1(0, "%s\n", name);
   }

   ok(get_user_home_directory("root", home.addr()) == 0, "get_user_home_directory()");

   return report();
}
#endif
