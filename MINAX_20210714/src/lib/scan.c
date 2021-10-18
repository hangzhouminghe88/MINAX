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
 *   scan.c -- scanning routines for Bacula
 *
 *    Kern Sibbald, MM  separated from util.c MMIII
 *
 */


#include "bacula.h"
#include "jcr.h"
#include "findlib/find.h"

/* Strip leading space from command line arguments */
void strip_leading_space(char *str)
{
   char *p = str;
   while (B_ISSPACE(*p)) {
      p++;
   }
   if (str != p) {
      do {
         *str++ = *p;
      } while (*p++ != 0);
   }
}

/* Strip any trailing junk from the command */
char *strip_trailing_junk(char *cmd)
{
   char *p;

   /* strip trailing junk from command */
   p = cmd - 1 + strlen(cmd);
   while ((p >= cmd) && (B_ISSPACE(*p) || *p == '\n' || *p == '\r')) {
      *p-- = 0;
   } 
   return cmd;
}

/* Strip any trailing newline characters from the string */
char *strip_trailing_newline(char *cmd)
{
   char *p;
   p = cmd - 1 + strlen(cmd);
   while ((p >= cmd) && (*p == '\n' || *p == '\r')) *p-- = 0;
   return cmd;
}

/* Strip any trailing slashes from a directory path */
char *strip_trailing_slashes(char *dir)
{
   char *p;

   /* strip trailing slashes */
   p = dir -1 + strlen(dir);
   while (p >= dir && IsPathSeparator(*p)) *p-- = 0;
   return dir;
}

/*
 * Skip spaces
 *  Returns: 0 on failure (EOF)
 *           1 on success
 *           new address in passed parameter
 */
bool skip_spaces(char **msg)
{
   char *p = *msg;
   if (!p) {
      return false;
   }
   while (*p && B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/*
 * Skip nonspaces
 *  Returns: 0 on failure (EOF)
 *           1 on success
 *           new address in passed parameter
 */
bool skip_nonspaces(char **msg)
{
   char *p = *msg;

   if (!p) {
      return false;
   }
   while (*p && !B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/* folded search for string - case insensitive */
int
fstrsch(const char *a, const char *b)   /* folded case search */
{
   const char *s1,*s2;
   char c1, c2;

   s1=a;
   s2=b;
   while (*s1) {                      /* do it the fast way */
      if ((*s1++ | 0x20) != (*s2++ | 0x20))
         return 0;                    /* failed */
   }
   while (*a) {                       /* do it over the correct slow way */
      if (B_ISUPPER(c1 = *a)) {
         c1 = tolower((int)c1);
      }
      if (B_ISUPPER(c2 = *b)) {
         c2 = tolower((int)c2);
      }
      if (c1 != c2) {
         return 0;
      }
      a++;
      b++;
   }
   return 1;
}


/*
 * Return next argument from command line.  Note, this
 *   routine is destructive because it stored 0 at the end
 *   of each argument.
 * Called with pointer to pointer to command line. This
 *   pointer is updated to point to the remainder of the
 *   command line.
 *
 * Returns pointer to next argument -- don't store the result
 *   in the pointer you passed as an argument ...
 *   The next argument is terminated by a space unless within
 *   quotes. Double quote characters (unless preceded by a \) are
 *   stripped.
 *
 */
char *next_arg(char **s)
{
   char *p, *q, *n;
   bool in_quote = false;

   /* skip past spaces to next arg */
   for (p=*s; *p && B_ISSPACE(*p); ) {
      p++;
   }
   Dmsg1(900, "Next arg=%s\n", p);
   for (n = q = p; *p ; ) {
      if (*p == '\\') {                 /* slash? */
         p++;                           /* yes, skip it */
         if (*p) {
            *q++ = *p++;
         } else {
            *q++ = *p;
         }
         continue;
      }
      if (*p == '"') {                  /* start or end of quote */
         p++;
         in_quote = !in_quote;          /* change state */
         continue;
      }
      if (!in_quote && B_ISSPACE(*p)) { /* end of field */
         p++;
         break;
      }
      *q++ = *p++;
   }
   *q = 0;
   *s = p;
   Dmsg2(900, "End arg=%s next=%s\n", n, p);
   return n;
}

/*
 * This routine parses the input command line.
 * It makes a copy in args, then builds an
 *  argc, argk, argv list where:
 *
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = argument value (part after =)
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2
 *  argv[1] = abc
 *  argk[2] = arg3
 *  argv[2] =
 */
int parse_args(POOLMEM *cmd, POOLMEM **args, int *argc,
               char **argk, char **argv, int max_args)
{
   char *p;

   parse_args_only(cmd, args, argc, argk, argv, max_args);

   /* Separate keyword and value */
   for (int i=0; i < *argc; i++) {
      p = strchr(argk[i], '=');
      if (p) {
         *p++ = 0;                    /* terminate keyword and point to value */
      }
      argv[i] = p;                    /* save ptr to value or NULL */
   }
#ifdef xxx_debug
   for (int i=0; i < *argc; i++) {
      Pmsg3(000, "Arg %d: kw=%s val=%s\n", i, argk[i], argv[i]?argv[i]:"NULL");
   }
#endif
   return 1;
}


/*
 * This routine parses the input command line.
 *   It makes a copy in args, then builds an
 *   argc, argk, but no argv (values).
 *   This routine is useful for scanning command lines where the data
 *   is a filename and no keywords are expected.  If we scan a filename
 *   for keywords, any = in the filename will be interpreted as the
 *   end of a keyword, and this is not good.
 *
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = NULL
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2=abc
 *  argv[1] = NULL
 *  argk[2] = arg3
 *  argv[2] =
 */
int parse_args_only(POOLMEM *cmd, POOLMEM **args, int *argc,
                    char **argk, char **argv, int max_args)
{
   char *p, *n;

   pm_strcpy(args, cmd);
   strip_trailing_junk(*args);
   p = *args;
   *argc = 0;
   /* Pick up all arguments */
   while (*argc < max_args) {
      n = next_arg(&p);
      if (*n) {
         argk[*argc] = n;
         argv[(*argc)++] = NULL;
      } else {
         break;
      }
   }
   return 1;
}


/*
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the arguments provided.
 */
void split_path_and_filename(const char *fname, POOLMEM **path, int *pnl,
        POOLMEM **file, int *fnl)
{
   const char *f;
   int slen;
   int len = slen = strlen(fname);

   /*
    * Find path without the filename.
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   f = fname + len - 1;
   /* "strip" any trailing slashes */
   while (slen > 1 && IsPathSeparator(*f)) {
      slen--;
      f--;
   }
   /* Walk back to last slash -- begin of filename */
   while (slen > 0 && !IsPathSeparator(*f)) {
      slen--;
      f--;
   }
   if (IsPathSeparator(*f)) {         /* did we find a slash? */
      f++;                            /* yes, point to filename */
   } else {                           /* no, whole thing must be path name */
      f = fname;
   }
   Dmsg2(200, "after strip len=%d f=%s\n", len, f);
   *fnl = fname - f + len;
   if (*fnl > 0) {
      *file = check_pool_memory_size(*file, *fnl+1);
      memcpy(*file, f, *fnl);    /* copy filename */
   }
   (*file)[*fnl] = 0;

   *pnl = f - fname;
   if (*pnl > 0) {
      *path = check_pool_memory_size(*path, *pnl+1);
      memcpy(*path, fname, *pnl);
   }
   (*path)[*pnl] = 0;

   Dmsg2(200, "pnl=%d fnl=%d\n", *pnl, *fnl);
   Dmsg3(200, "split fname=%s path=%s file=%s\n", fname, *path, *file);
}

/*
 * Extremely simple sscanf. Handles only %(u,d,ld,qd,qu,lu,lld,llu,c,nns)
 *
 * Note, BIG is the default maximum length when no length
 *   has been specified for %s. If it is not big enough, then
 *   simply add a length such as %10000s.
 *
 * Don't use this bsscanf() anymore, use scan_string() instead
 */
const int BIG = 1000;
int bsscanf(const char *buf, const char *fmt, ...)
{
   va_list ap;
   int count = 0;
   void *vp;
   char *cp;
   int l = 0;
   int max_len = BIG;
   uint64_t value;
   bool error = false;
   bool negative;

   va_start(ap, fmt);
   while (*fmt && !error) {
//    Dmsg1(000, "fmt=%c\n", *fmt);
      if (*fmt == '%') {
         fmt++;
//       Dmsg1(000, "Got %% nxt=%c\n", *fmt);
switch_top:
         switch (*fmt++) {
         case 'u':
            value = 0;
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((uint32_t *)vp) = (uint32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((uint64_t *)vp) = (uint64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'd':
            value = 0;
            if (*buf == '-') {
               negative = true;
               buf++;
            } else {
               negative = false;
            }
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            if (negative) {
               value = -value;
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((int32_t *)vp) = (int32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((int64_t *)vp) = (int64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'l':
//          Dmsg0(000, "got l\n");
            l = 1;
            if (*fmt == 'l') {
               l++;
               fmt++;
            }
            if (*fmt == 'd' || *fmt == 'u') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u\n", *fmt);
            error = true;
            break;
         case 'q':
            l = 2;
            if (*fmt == 'd' || *fmt == 'u') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u\n", *fmt);
            error = true;
            break;
         case 's':
//          Dmsg1(000, "Store string max_len=%d\n", max_len);
            cp = (char *)va_arg(ap, char *);
            while (*buf && !B_ISSPACE(*buf) && max_len-- > 0) {
               *cp++ = *buf++;
            }
            *cp = 0;
            count++;
            max_len = BIG;
            break;
         case 'c':
            cp = (char *)va_arg(ap, char *);
            *cp = *buf++;
            count++;
            break;
         case '%':
            if (*buf++ != '%') {
               error = true;
            }
            break;
         default:
            fmt--;
            max_len = 0;
            while (B_ISDIGIT(*fmt)) {
               max_len = B_TIMES10(max_len) + *fmt++ - '0';
            }
//          Dmsg1(000, "Default max_len=%d\n", max_len);
            if (*fmt == 's') {
               goto switch_top;
            }
//          Dmsg1(000, "Default c=%c\n", *fmt);
            error = true;
            break;                    /* error: unknown format */
         }
         continue;

      /* White space eats zero or more whitespace */
      } else if (B_ISSPACE(*fmt)) {
         fmt++;
         while (B_ISSPACE(*buf)) {
            buf++;
         }
      /* Plain text must match */
      } else if (*buf++ != *fmt++) {
//       Dmsg2(000, "Mismatch buf=%c fmt=%c\n", *--buf, *--fmt);
         error = true;
         break;
      }
   }
   va_end(ap);
// Dmsg2(000, "Error=%d count=%d\n", error, count);
   if (error) {
      count = -1;
   }
   return count;
}

/*
 * scan_string() is more strict than the standard sscanf() function.
 * The format sting must match exactly ! If so the function
 * return then number or argument (%) parsed, else it return EOF
 * One space in the format string match one or more spaces in the input string
 * see the unittest below to see how it works.
 *
 */
int scan_string(const char *buf, const char *fmt, ...)
{
   va_list ap;
   int count = 0;
   void *vp;
   char *cp;
   int l = 0;
   int max_len = BIG;
   uint64_t value;
   bool error = false;
   bool negative;

   if ((buf==NULL || fmt==NULL)) {
      return EOF; // original sscanf segfault
   }

   if (*buf=='\0' && *fmt!='\0') {
      return EOF; // mimic sscanf
   }

   va_start(ap, fmt);
   while (*fmt && !error) {
//    Dmsg1(000, "fmt=%c\n", *fmt);
      if (*fmt == '%') {
         fmt++;
//       Dmsg1(000, "Got %% nxt=%c\n", *fmt);
switch_top:
         switch (*fmt++) {
         case 'x':
            value = 0;
            if (buf[0] != '0' || buf[1] != 'x') {
               error = true;
               break;
            }
            buf++;              /* skip 0 */
            buf++;              /* skip x */
            while (B_ISXDIGIT(*buf)) {
               if (B_ISDIGIT(*buf)) {
                  value = (value) * 16 + *buf++ - '0';
               } else {
                  value = (value) * 16 + toupper(*buf++) - 'A' + 10;
               }
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((uint32_t *)vp) = (uint32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((uint64_t *)vp) = (uint64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'u':
            value = 0;
            if (!B_ISDIGIT(*buf)) {
               error = true;
               break;
            }
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((uint32_t *)vp) = (uint32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((uint64_t *)vp) = (uint64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'd':
            value = 0;
            if (*buf == '-') {
               negative = true;
               buf++;
            } else {
               negative = false;
            }
            if (!B_ISDIGIT(*buf)) {
               error = true;
               break;
            }
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            if (negative) {
               value = -value;
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((int32_t *)vp) = (int32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((int64_t *)vp) = (int64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'l':
//          Dmsg0(000, "got l\n");
            l = 1;
            if (*fmt == 'l') {
               l++;
               fmt++;
            }
            if (*fmt == 'd' || *fmt == 'u' || *fmt == 'x') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u,x\n", *fmt);
            error = true;
            break;
         case 'q':
            l = 2;
            if (*fmt == 'd' || *fmt == 'u') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u\n", *fmt);
            error = true;
            break;
         case 's':
//          Dmsg1(000, "Store string max_len=%d\n", max_len);
            cp = (char *)va_arg(ap, char *);
            while (*buf && !B_ISSPACE(*buf) && max_len-- > 0) {
               *cp++ = *buf++;
            }
            *cp = 0;
            count++;
            max_len = BIG;
            break;
         case 'c':
            cp = (char *)va_arg(ap, char *);
            if (*buf == '\0') {
               error = true;
               break;
            }
            *cp = *buf++;
            count++;
            break;
         case '%':
            if (*buf++ != '%') {
               error = true;
            }
            break;
         default:
            fmt--;
            max_len = 0;
            while (B_ISDIGIT(*fmt)) {
               max_len = B_TIMES10(max_len) + *fmt++ - '0';
            }
//          Dmsg1(000, "Default max_len=%d\n", max_len);
            if (*fmt == 's') {
               goto switch_top;
            }
//          Dmsg1(000, "Default c=%c\n", *fmt);
            error = true;
            break;                    /* error: unknown format */
         }
         continue;

      /* White space eats ONE or more whitespace */
      } else if (B_ISSPACE(*fmt)) {
         /* Ignore extra white space in the format string */
         while (B_ISSPACE(*fmt)) fmt++;
         if (!B_ISSPACE(*buf)) {
            error = true;
         }
         while (B_ISSPACE(*buf)) {
            buf++;
         }
      /* Plain text must match */
      } else if (*buf++ != *fmt++) {
//       Dmsg2(000, "Mismatch buf=%c fmt=%c\n", *--buf, *--fmt);
         error = true;
         break;
      }
   }
   va_end(ap);
// Dmsg2(000, "Error=%d count=%d\n", error, count);
   if (error) {
      count = -1;
   }
   return count;
}

/*
 * Return next name from a comma separated list.  Note, this
 *   routine is destructive because it stored 0 at the end
 *   of each argument.
 * Called with pointer to pointer to command line. This
 *   pointer is updated to point to the remainder of the
 *   command line.
 *
 * Returns pointer to next name -- don't store the result
 *   in the pointer you passed as an argument ...
 *   The next argument is terminated by a , unless within
 *   quotes. Double quote characters (unless preceded by a \) are
 *   stripped.
 *
 */
char *next_name(char **s)
{
   char *p, *q, *n;
   bool in_quote = false;

   if (s == NULL || *s == NULL || **s == '\0') {
      return NULL;
   }
   p = *s;
   Dmsg1(900, "Next name=%s\n", p);
   for (n = q = p; *p ; ) {
      if (*p == '\\') {                 /* slash? */
         p++;                           /* yes, skip it */
         if (*p) {
            *q++ = *p++;
         } else {
            *q++ = *p;
         }
         continue;
      }
      if (*p == '"') {                  /* start or end of quote */
         p++;
         in_quote = !in_quote;          /* change state */
         continue;
      }
      if (!in_quote && *p == ',') { /* end of field */
         p++;
         break;
      }
      *q++ = *p++;
   }
   *q = 0;
   *s = p;
   Dmsg2(900, "End arg=%s next=%s\n", n, p);
   return n;
}

#ifdef TEST_PROGRAM
#include "unittests.h"

#undef sscanf

int main(int argc, char *argv[])
{
   Unittests test1("bsscanf_test");
   {
      int cnt;
      char buf[512];
      int32_t a = -99;

      cnt=bsscanf("CatReq JobId=5 CreateFileMedia\n", "CatReq JobId=%ld CreateFileMedia\n", &a);
      ok(cnt == 1 && a==5, "[CatReq JobId=5 CreateFileMedia] => [CatReq JobId=%ld CreateFileMedia]");

      cnt=bsscanf("CatReq JobId=5 CreateJobMedia\n", "CatReq JobId=%ld CreateFileMedia\n", &a);
      ok(cnt == -1, "[CatReq JobId=5 CreateJobMedia] => [CatReq JobId=%ld CreateFileMedia]");

      uint32_t FirstIndex, LastIndex, StartFile, EndFile, StartBlock, EndBlock;
      const char *catreq = "CatReq Job=NightlySave.2004-06-11_19.11.32 CreateJobMedia FirstIndex=1 LastIndex=114 StartFile=0 EndFile=0 StartBlock=208 EndBlock=2903248";
      const char *Create_job_media = "CatReq Job=%127s CreateJobMedia "
     "FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
     "StartBlock=%u EndBlock=%u\n";
      memset(buf, '\0', sizeof(buf));
      FirstIndex=LastIndex=StartFile=EndFile=StartBlock=EndBlock=231170;
      cnt = bsscanf(catreq, Create_job_media, &buf,
         &FirstIndex, &LastIndex, &StartFile, &EndFile,
         &StartBlock, &EndBlock);
      ok(cnt==7 && strcmp(buf, "NightlySave.2004-06-11_19.11.32")==0 &&
            FirstIndex==1 && LastIndex==114 && StartFile==0 &&
            EndFile==0 && StartBlock==208 && EndBlock==2903248, "CatReq Job");

   }

   Unittests test2("scan_string_test");
   {
      int a,b,c,d,e,f;
      int u;
      int32_t cnt;
      int64_t g;
      char buf[512];

      strcpy(buf, "xxx");
      cnt=scan_string("string=", "string=%256s", buf);
      ok(cnt==1 && *buf == 0, "[string=] => [string=%256s]");
      //Dmsg2(0, "cnt=%d buf=%s\n", cnt, buf);

      a = b = c = d = e =f = g = -99;
      cnt=scan_string("1 2 223 6 0\n", "%d %d %d %d %d %d %lld\n",
                  &a,&b,&c,&d,&e,&f,&g);
      ok(cnt == -1, "[1 2 223 6 0] => [%d %d %d %d %d %d %lld]");

      a = b = c = d = e =f = g = -99;
      cnt=scan_string("1 2 -223 -0 0\n", "%d %d %d %d %lld\n", &a,&b,&c,&d,&g);
      ok(cnt == 5 && a==1 && b==2 && c==-223 && d==0 && g==0,
            "[1 2 -223 -0 0] => [%u %u %u %u %u %u %lld]");

      u = 99;
      cnt=scan_string("-223\n", "%u\n", &u);
      ok(cnt == -1 && u == 99, "[-223] => [%u]");

//      printf("%d %d %d %d %d %lld\n", cnt, a, b, c, d, g);

      Pmsg0(0, "Test scan_string bug ignore final 0\n");
      {
         int a, b, cnt;
         a=b=-99;
         cnt=scan_string("123", "%d %d", &a, &b); /* Format error */
         ok(cnt==-1, "Check count [%d %d] => [123]");
         ok(a==123, "Check first value [%d %d] => [123]");
         ok(b==-99, "Check second value [%d %d] => [123] ");
      }

      memset(buf, '\0', sizeof(buf)); a=b=-99;
      cnt=scan_string("Hello  world   123 456   end", "Hello %100s %d %d end", buf, &a, &b);
      ok(cnt==3 && strcmp(buf, "world")==0 && a==123 && b==456, "multi space");

      Pmsg0(0, "Test scan_string basic tests\n");
      {
         char buf[100];
         int a, b, c, cnt;
         uint32_t val32;
         uint64_t val64;
         int32_t sval32;
         int64_t sval64;

         memset(buf, '\0', sizeof(buf)); a=b=-99;
         cnt=scan_string("Hello world 123 456 end", "Hello %100s %d %d end", buf, &a, &b);
         ok(cnt==3 && strcmp(buf, "world")==0 && a==123 && b==456, "test1");

         memset(buf, '\0', sizeof(buf)); val64=val32=99;
         cnt=scan_string("world 123 72057594037927953", "%120s %ld %lld", buf, &val32, &val64);
         ok(cnt==3 && strcmp(buf, "world")==0 && val32==123 && val64==72057594037927953, "test2");


         memset(buf, '\0', sizeof(buf)); a=b=-99;
         cnt=scan_string("Hello world 123", "Hello %100s %d %d end", buf, &a, &b);
         //ok(cnt==2 && strcmp(buf, "world")==0 && a==123 && b==-99, "one missing"); // scanf compatible
         ok(cnt==-1 && strcmp(buf, "world")==0 && a==123 && b==-99, "one missing"); // scanf compatible

         a=b=-99;
         cnt=scan_string("Hello % -123", "Hello %% %d", &a);
         ok(cnt==1 && a==-123, "match %%");

         a=b=-99;
         cnt=scan_string("", "Hello %d %d end", &a, &b);
         ok(cnt==-1 && a==-99 && b==-99, "empty string 1");

         a=b=-99;
         cnt=scan_string("", "%d", &a);
         ok(cnt==-1 && a==-99, "empty string 2");

         a=b=-99;
         cnt=scan_string("", "", &a);
         ok(cnt==0 && a==-99, "empty string and format string");

         a=b=-99;
         cnt=scan_string("Hello", "", &a);
         ok(cnt==0 && a==-99, "empty string and format string");

         // Original sscanf segfault if any of the format or the input string are NULL
         // scan_string return EOF instead

   #if 1
         /* Float scan is not supported yet by scan_string */
         a=b=-99;
         cnt=scan_string("3.14 17", "%d %d", &a, &b);
         ok(cnt==-1 && a==3 && b==-99, "parse float instead of int");
   #endif

         a=b=c=-99;
         cnt=scan_string("Hello 123 c=2", "Hello %d %d c=%d", &a, &b, &c);
         ok(cnt==-1 && a==123 && b==-99, "bug continue parsing after a 'no match'");
         //      printf("%d %d %d %d\n", cnt, a, b, c);

         a=b=c=-99;
         cnt=scan_string("Hello -123 456 end", "Hello %d %d", &a, &b);
         ok(cnt==2 && a==-123 && b==456, "negative number");

   #if 1
         a=b=c=-99;
         cnt=scan_string("Hello -123 - 456", "Hello %d %d", &a, &b, &c);
         ok(cnt==-1 && a==-123 && b==-99 && c==-99, "incomplete negative number");
   #endif

         a=b=c=-99;
         cnt=scan_string("Hello -123 - 456", "Hello %d %d", &a, &b);
         ok(cnt==-1 && a==-123, "incomplete negative number");

         // 2^63==9223372036854775808
         val32=val64=sval32=sval64=99;
         cnt=scan_string("9223372036854775809", "%lld", &val64);
         ok(cnt==1 && val64==9223372036854775809UL, "long >63bits");

         val32=val64=sval32=sval64=99;
         cnt=scan_string("-9223372036854775807", "%lld", &sval64);
         ok(cnt==1 && sval64==-9223372036854775807L, "unsigned long <63bits");

         a=b=c=-99; memset(buf, 'A', sizeof(buf)); buf[sizeof(buf)-1]='\0';
         cnt=scan_string("Hello world", "Hello %2s", buf);
         ok(cnt==1 && strcmp(buf, "wo")==0 && buf[3]=='A', "string limitation");// This match sscanf

   #if 0
         /* The format is not correct */
         a=b=c=-99; memset(buf, 'A', sizeof(buf)); buf[sizeof(buf)-1]='\0';
         cnt=scan_string("Hello world 123", "Hello %2s %d", buf, &a);
         ok(cnt==1 && a==-99 && strcmp(buf, "wo")==0 && buf[3]=='A', "string limitation & stop matching"); // This match sscanf
   #endif
         a=b=c=-99; memset(buf, 'A', sizeof(buf)); buf[sizeof(buf)-1]='\0';
         cnt=scan_string("Hello world 123", "Hello %2s %d", buf, &a);
         ok(cnt==-1 && a==-99 && strcmp(buf, "wo")==0 && buf[3]=='A', "string limitation & stop matching");

         a=b=c=-99; memset(buf, 'A', sizeof(buf)); buf[sizeof(buf)-1]='\0';
         cnt=scan_string("Hello world", "Hello %2srld", buf);
         ok(cnt==1 && strcmp(buf, "wo")==0 && buf[3]=='A', "string limitation weird 1");// This match sscanf

         a=b=c=-99; memset(buf, 'A', sizeof(buf)); buf[sizeof(buf)-1]='\0';
         cnt=scan_string("Hello world 123", "Hello %2srld %d", buf, &a);
         ok(cnt==2 && a==123 && strcmp(buf, "wo")==0 && buf[3]=='A', "string limitation weird 2");// This match sscanf

         {
            Pmsg0(0, "Test FD-SD hello\n");

            char job_name[500];
            int fd_version, sd_version, tlspsk;
            const char *s="Hello Bacula SD: Start Job backupXXX 14 tlspsk=100";
            cnt=-99, fd_version=-99, sd_version=-99, tlspsk=-99;
            cnt=scan_string(s, "Hello Bacula SD: Start Job %127s %d %d tlspsk=%d", job_name, &fd_version, &sd_version, &tlspsk);
   #if 0
            /* The format is incorrect, it should return -1 */
            ok(cnt==2 && strcmp(job_name, "backupXXX")==0 && fd_version==14 && sd_version==-99 && tlspsk==-99, "SD-FD hello fmt1");
   #endif
            ok(cnt==-1 && strcmp(job_name, "backupXXX")==0 && fd_version==14, "SD-FD hello fmt1");
            // printf("%d %d %d %d %s\n", cnt, fd_version, sd_version, tlspsk, job_name);

            cnt=-99, fd_version=-99, sd_version=-99, tlspsk=-99;
            cnt=scan_string(s, "Hello Bacula SD: Start Job %127s %d %d", job_name, &fd_version, &sd_version);
   #if 0
            /* The format is incorrect, it should return -1 */
            ok(cnt==2 && strcmp(job_name, "backupXXX")==0 && fd_version==14 && sd_version==-99 && tlspsk==-99, "SD-FD hello fmt2");
   #endif
            ok(cnt==-1 && strcmp(job_name, "backupXXX")==0 && fd_version==14, "SD-FD hello fmt2");

            cnt=-99, fd_version=-99, sd_version=-99, tlspsk=-99;
            cnt=scan_string(s, "Hello Bacula SD: Start Job %127s %d tlspsk=%d", job_name, &fd_version, &tlspsk);
            ok(cnt==3 && strcmp(job_name, "backupXXX")==0 && fd_version==14 && sd_version==-99 && tlspsk==100, "SD-FD hello fmt3");

            cnt=-99, fd_version=-99, sd_version=-99, tlspsk=-99;
            cnt=scan_string(s, "Hello Bacula SD: Start Job %127s %d", job_name, &fd_version);
            ok(cnt==2 && strcmp(job_name, "backupXXX")==0 && fd_version==14 && sd_version==-99 && tlspsk==-99, "SD-FD hello fmt4");

            cnt=-99, fd_version=-99, sd_version=-99, tlspsk=-99;
            cnt=scan_string(s, "Hello FD: Bacula Storage calling Start Job %127s %d", job_name, &sd_version);
            ok(cnt==-1 && fd_version==-99 && sd_version==-99 && tlspsk==-99, "SD-FD hello fmt5");
         }

         Pmsg0(0, "Test bscanf vs sscanf behavior, all are expected to return an error\n");
         memset(buf, '\0', sizeof(buf)); a=b=-99;
         cnt=scan_string("DO YOU HAVE FIRE", "NO MATCH"); // => cnt==0
         ok(cnt==-1, "when literal dont match");

         // BUT !!!
         cnt=scan_string("Literal", "Literal");
         ok(cnt==0, "literal (strcmp like)");

         /* scan_string parameters order: string_to_test, pattern */
         Pmsg0(0, "Test scan_string test starting with literal\n");
         cnt=scan_string("YES", "NO");
         ok(cnt==-1, "wrong literal YES NO");

         cnt=scan_string("Hello World", "Hello Format");
         ok(cnt==-1, "different end");

         cnt=scan_string("Hello", "HelloFormat");
         ok(cnt==-1, "end missing in format (format incomplete)");

         cnt=scan_string("Hello", "Hello Format");
         ok(cnt==-1, "end missing in format with space  (format incomplete)");

         cnt=scan_string("Hello World", "Hello");
         ok(cnt==0, "format shorter than string");

         cnt=scan_string("Hello World", "Hello %d", &a);
   #if 0
         /* the format is not valid */
         ok(cnt==0, "format shorter with try to match");
   #endif
         ok(cnt==-1, "format shorter with try to match");

         a = -99;
         cnt=scan_string("3000 OK Hello 30005", "3000 OK Hello %d", &a);
         ok(cnt==1 && a == 30005, "[3000 OK Hello 30005] => [3000 OK Hello %d]");
         Dmsg2(0, "cnt=%d a=%d\n", cnt, a);

         a = -99; strcpy(buf, "xxx");
         cnt=scan_string("string=", "string=%256s", buf);
         ok(cnt==1 && *buf == 0, "[string=] => [string=%256s]");
         Dmsg2(0, "cnt=%d buf=%s\n", cnt, buf);

      }
   }


   return report();
}

#endif
