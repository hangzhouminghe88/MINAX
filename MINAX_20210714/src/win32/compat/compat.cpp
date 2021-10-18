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
//                              -*- Mode: C++ -*-
// compat.cpp -- compatibilty layer to make bacula-fd run
//               natively under windows
//
// Copyright transferred from Raider Solutions, Inc to
//   Kern Sibbald and John Walker by express permission.
//
// Author          : Christopher S. Hull
// Created On      : Sat Jan 31 15:55:00 2004

#include "bacula.h"
#include "compat.h"
#include "jcr.h"
#include "findlib/find.h"

/* Note, if you want to see what Windows variables and structures
 * are defined, bacula.h includes <windows.h>, which is found in:
 *
 *   cross-tools/mingw32/mingw32/include
 * or
 *   cross-tools/mingw-w64/x86_64-pc-mingw32/include
 *
 * depending on whether we are building the 32 bit version or
 * the 64 bit version.
 */

static const int dbglvl = 500;

#define b_errno_win32 (1<<29)

#define MAX_PATHLENGTH  1024

/**
   UTF-8 to UCS2 path conversion is expensive,
   so we cache the conversion. During backup the
   conversion is called 3 times (lstat, attribs, open),
   by using the cache this is reduced to 1 time
 */
static POOLMEM *g_pWin32ConvUTF8Cache = NULL;
static POOLMEM *g_pWin32ConvUCS2Cache = NULL;
static DWORD g_dwWin32ConvUTF8strlen = 0;
static pthread_mutex_t Win32Convmutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */
static const char *errorString(void);

/* The following functions are available only in the FileDaemon with VSS 
 * These functions uses the VSSObject to resolve a Path to a Snapshot Path,
 * the VSSObject is available "per job", and some jobs such as Restore or Verify
 * may not have a VSSObject.
 */

static BOOL default_VSSPathConverter()
{
   return false;
}

static t_pVSSPathConvert   g_pVSSPathConvert = NULL;
static t_pVSSPathConvertW  g_pVSSPathConvertW = NULL;
static t_pVSSPathConverter g_pVSSPathConverter = default_VSSPathConverter; /* To know if we can use the VSSPath functions */


void SetVSSPathConvert(t_pVSSPathConverter pPathConverter, t_pVSSPathConvert pPathConvert, t_pVSSPathConvertW pPathConvertW)
{
   g_pVSSPathConvert = pPathConvert;
   g_pVSSPathConvertW = pPathConvertW;
   g_pVSSPathConverter = pPathConverter;
}

static void Win32ConvInitCache()
{
   if (g_pWin32ConvUTF8Cache) {
      return;
   }
   g_pWin32ConvUTF8Cache = get_pool_memory(PM_FNAME);
   g_pWin32ConvUCS2Cache = get_pool_memory(PM_FNAME);
}

void Win32ConvCleanupCache()
{
   P(Win32Convmutex);
   if (g_pWin32ConvUTF8Cache) {
      free_pool_memory(g_pWin32ConvUTF8Cache);
      g_pWin32ConvUTF8Cache = NULL;
   }

   if (g_pWin32ConvUCS2Cache) {
      free_pool_memory(g_pWin32ConvUCS2Cache);
      g_pWin32ConvUCS2Cache = NULL;
   }

   g_dwWin32ConvUTF8strlen = 0;
   V(Win32Convmutex);
}


/* to allow the usage of the original version in this file here */
#undef fputs


//#define USE_WIN32_COMPAT_IO 1
#define USE_WIN32_32KPATHCONVERSION 1

extern DWORD   g_platform_id;
extern DWORD   g_MinorVersion;

/* From Microsoft SDK (KES) is the diff between Jan 1 1601 and Jan 1 1970 */
#ifdef HAVE_MINGW
#define WIN32_FILETIME_ADJUST 0x19DB1DED53E8000ULL
#else
#define WIN32_FILETIME_ADJUST 0x19DB1DED53E8000I64
#endif

#define WIN32_FILETIME_SCALE  10000000             // 100ns/second

/**
 * Convert POSIX like UTF-8 path into Windows UTF-8 path (replace '/' with '\\')
 * and add the \\?\ prefix or a "VSS" prefix if VSS snapshot exists
 * don't convert to UTF-16-LE
 */
static void conv_unix_to_vss_win32_path(const char *name, char *win32_name, DWORD dwSize)
{
    const char *fname = name;
    char *tname = win32_name;

    Dmsg0(dbglvl, "Enter convert_unix_to_win32_path\n");

    if (IsPathSeparator(name[0]) &&
        IsPathSeparator(name[1]) &&
        name[2] == '.' &&
        IsPathSeparator(name[3])) {

        *win32_name++ = '\\';
        *win32_name++ = '\\';
        *win32_name++ = '.';
        *win32_name++ = '\\';

        name += 4;
    } else if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS && !g_pVSSPathConverter()) {
        /* allow path to be 32767 bytes */
        *win32_name++ = '\\';
        *win32_name++ = '\\';
        *win32_name++ = '?';
        *win32_name++ = '\\';
    }

    while (*name) {
        /** Check for Unix separator and convert to Win32 */
        if (name[0] == '/' && name[1] == '/') {  /* double slash? */
           name++;                               /* yes, skip first one */
        }
        if (*name == '/') {
            *win32_name++ = '\\';     /* convert char */
        /* If Win32 separator that is "quoted", remove quote */
        } else if (*name == '\\' && name[1] == '\\') {
            *win32_name++ = '\\';
            name++;                   /* skip first \ */
        } else {
            *win32_name++ = *name;    /* copy character */
        }
        name++;
    }
    /** Strip any trailing slash, if we stored something
     * but leave "c:\" with backslash (root directory case
     */
    if (*fname != 0 && win32_name[-1] == '\\' && strlen (fname) != 3) {
        win32_name[-1] = 0;
    } else {
        *win32_name = 0;
    }

    Dmsg1(dbglvl, "Leave cvt_u_to_win32_path path=%s\n", tname);
}

/** Conversion of a Unix filename to a Win32 filename */
void unix_name_to_win32(POOLMEM **win32_name, const char *name)
{
   /* One extra byte should suffice, but we double it */
   /* add MAX_PATH bytes for VSS shadow copy name */
   DWORD dwSize = 2*strlen(name)+MAX_PATH;
   *win32_name = check_pool_memory_size(*win32_name, dwSize);
   conv_unix_to_vss_win32_path(name, *win32_name, dwSize);
}


/**
 * This function normalize a wchar Windows path:
 *  - make the path absolute if relative
 *  - prefix the path with r"\\?\" but keep r"\\.\" prefix untouched
 *  - convert any '/' into '\\'
 * A device path starting with \\.\ is left untouched and pBIsRawPath is set
 * Prefixing the path with \\?\ allow to use 32K characters long paths.
 *
 * samples:
 * c:\path\file -> \\?\c:\path\file
 * path\file -> \\?\c:\path\file
 *
 *        created 02/27/2006 Thorsten Engel
 *
 * This is a "universal" function that does more than what Bacula need,
 * like handling relative path using the CWD.
 */
static void
norm_wchar_win32_path(POOLMEM **pszUCSPath, BOOL *pBIsRawPath /*= NULL*/)
{

   Dmsg1(dbglvl, "Enter norm_wchar_win32_path %ls\n", *pszUCSPath);
   if (pBIsRawPath) {
      *pBIsRawPath = FALSE;              /* Initialize, set later */
   }

   wchar_t *name = (wchar_t *)*pszUCSPath;
   wchar_t *pwszBuf = (wchar_t *)get_pool_memory(PM_FNAME);
   wchar_t *pwszCurDirBuf = (wchar_t *)get_pool_memory(PM_FNAME);
   DWORD dwCurDirPathSize = 0;

   /* get buffer with enough size (name+max 6. wchars+1 null terminator */
   DWORD dwBufCharsNeeded = (wcslen(name)+7);
   pwszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszBuf, dwBufCharsNeeded*sizeof(wchar_t));

   /* it is important to make absolute paths, so we add drive and
    *  current path if necessary
    */

   BOOL bAddDrive = TRUE;
   BOOL bAddCurrentPath = TRUE;
   BOOL bAddPrefix = TRUE;

   if (IsWPathSeparator(name[0]) && IsWPathSeparator(name[1])
       && (name[2] == L'?' || name[2] == L'.') && IsWPathSeparator(name[3])) {
      /* already starting with \\?\ or \\.\ */
      bAddDrive = FALSE;
      bAddCurrentPath = FALSE;
      bAddPrefix = FALSE;
      if (pBIsRawPath && name[2] == L'.') {
         /* the Win32 Device NameSpace like '\\.\PhysicalDrive0' */
         *pBIsRawPath = TRUE;
      }
   } else if (iswalpha(name[0]) && name[1] == L':' && IsWPathSeparator(name[2])) {
      /* path begins with a drive letter, it is absolute */
      bAddDrive = FALSE;
      bAddCurrentPath = FALSE;
   } else if (IsWPathSeparator(name[0])) {
      /* path is absolute */
      bAddCurrentPath = FALSE;
   }

   /* is path relative to itself?, if yes, skip ./ */
   if (name[0] == L'.' && IsWPathSeparator(name[1])) {
      name += 2;
   }

   /* get current path if needed */
   if (bAddDrive || bAddCurrentPath) {
      if (p_GetCurrentDirectoryW &&
            (dwCurDirPathSize = p_GetCurrentDirectoryW(0, NULL)) > 0) {
         /* get directory into own buffer as it may either return c:\... or \\?\C:\.... */
         pwszCurDirBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszCurDirBuf, (dwCurDirPathSize+1)*sizeof(wchar_t));
         p_GetCurrentDirectoryW(dwCurDirPathSize, pwszCurDirBuf);
      } else {
         /* we have no info for doing so */
         bAddDrive = FALSE;
         bAddCurrentPath = FALSE;
         bAddPrefix = FALSE;
      }
   }

   int nParseOffset = 0;

   /* add 4 bytes header */
   if (bAddPrefix) {
      nParseOffset = 4;
      wcscpy(pwszBuf, L"\\\\?\\");
   }

   /* add drive if needed */
   if (bAddDrive && !bAddCurrentPath) {
      wchar_t szDrive[3];

      if (IsWPathSeparator(pwszCurDirBuf[0]) &&
          IsWPathSeparator(pwszCurDirBuf[1]) &&
          pwszCurDirBuf[2] == L'?' &&
          IsWPathSeparator(pwszCurDirBuf[3])) {
         /* copy drive character */
         szDrive[0] = pwszCurDirBuf[4];
      } else {
         /* copy drive character */
         szDrive[0] = pwszCurDirBuf[0];
      }

      szDrive[1] = L':';
      szDrive[2] = L'\0';

      wcscat(pwszBuf, szDrive);
      nParseOffset +=2;
   }

   /* add path if needed */
   if (bAddCurrentPath) {
      /* the 1 add. character is for the eventually added backslash */
      dwBufCharsNeeded += dwCurDirPathSize+1;
      pwszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszBuf, dwBufCharsNeeded*sizeof(wchar_t));
      /* get directory into own buffer as it may either return c:\... or \\?\C:\.... */

      if (IsWPathSeparator(pwszCurDirBuf[0]) &&
          IsWPathSeparator(pwszCurDirBuf[1]) &&
          pwszCurDirBuf[2] == L'?' &&
          IsWPathSeparator(pwszCurDirBuf[3])) {
         /* copy complete string */
         wcscpy(pwszBuf, pwszCurDirBuf);
      } else {
         /* append path  */
         wcscat(pwszBuf, pwszCurDirBuf);
      }

      nParseOffset = wcslen((LPCWSTR) pwszBuf);

      /* check if path ends with backslash, if not, add one */
      if (!IsWPathSeparator(pwszBuf[nParseOffset-1])) {
         wcscat(pwszBuf, L"\\");
         nParseOffset++;
      }
   }

   wchar_t *win32_name = &pwszBuf[nParseOffset];
   wchar_t *name_start = name;

   while (*name) {
      /* Check for Unix separator and convert to Win32, eliminating
       * duplicate separators.
       */
      if (IsWPathSeparator(*name)) {
         *win32_name++ = L'\\';     /* convert char */

         /* Eliminate consecutive slashes, but not at the start so that
          * \\.\ still works.
          */
         if (name_start != name && IsWPathSeparator(name[1])) {
            name++;
         }
      } else {
         *win32_name++ = *name;    /* copy character */
      }
      name++;
   }

   /* null terminate string */
   *win32_name = L'\0';

   free_pool_memory(*pszUCSPath);
   free_pool_memory((POOLMEM *)pwszCurDirBuf);

   Dmsg1(dbglvl, "Leave norm_wchar_win32_path=%ls\n", pwszBuf);
   *pszUCSPath = (POOLMEM *)pwszBuf;
}

/*
 * Both functions wchar_path_2_wutf8() and wutf8_path_2_wchar() convert file path
 * between Windows UTF16-LE and Bacula UTF8.
 * Both function use WideCharToMultiByte() and WideCharToMultiByte() but
 * support invalid UTF16 input. Invalid UTF16 char are encoded into valid
 * UTF8 string using the WUTF8_ESCAPE char ('*') that is not allowed into
 * Windows path. The valid UTF8 path is decoded back into the same invalid
 * UTF16 path. WUTF8_ESCAPE are also escaped just in case.
 * The encoded path is valid UTF8 and it can be manipulate by Bacula it as any
 * other path.
 * This trick allows to restore these files on most POSIX and Windows systems.
 * The main improvement is that the FileDaemon was not able to backup these
 * files with invalid char because the code that navigate inside the directory
 * tree use UTF8 and the conversion back to UTF16 (to open the file) was
 * generating a path different of the original one that was not existing.
 *
 * Use these functions only for windows path. Don't use them for other strings
 * that could hold WUTF8_ESCAPE chars.
 *
 * return the length of the output string including the null terminator
 * or 0 for any error
 */
static int WUTF8_ESCAPE='*';
static wchar_t WUTF8_WCHAR_ESCAPE=L'*';

#ifndef WC_ERR_INVALID_CHARS
 #define WC_ERR_INVALID_CHARS 0x00000080 // Windows constant
#endif

int wchar_path_2_wutf8(POOLMEM **pszUTF, const wchar_t *pszUCS)
{
   if (!p_WideCharToMultiByte) {
      return 0;
   }
   // WC_ERR_INVALID_CHARS=0x00000080
   // ERROR_NO_UNICODE_TRANSLATION=1113
   int buflen = p_WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,pszUCS,-1,NULL,0,NULL,NULL);
   if (buflen > 0) {
      /* no invalid char: buflen is always > 0 because it counts the final '\0' */
      *pszUTF = check_pool_memory_size(*pszUTF, buflen);
      buflen = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,*pszUTF,buflen,NULL,NULL);
      if (strchr(*pszUTF, WUTF8_ESCAPE)==NULL) {
         // No WUTF8_ESCAPE char to escape
         return buflen;
      }
// IF WE GO BELOW THIS LINE, this is because a Windows file name holds an invalid
// char or an unexpected char WUTF8_ESCAPE
   } else {
      buflen = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,NULL,0,NULL,NULL);
      *pszUTF = check_pool_memory_size(*pszUTF, buflen);
      buflen = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,*pszUTF,buflen,NULL,NULL);
   }
   // pszUCS is an invalid UTF-16-LE string,
   // let WideCharToMultiByte() replace all invalid char with U+FFFD
   // then count the number of U+FFFD (aka \xef\xbf\xbd in utf8)
   for (char *p=*pszUTF; p!=NULL && *p!='\0'; p++) {
      p=strstr(p, "\xef\xbf\xbd");
      if (p == NULL) {
         break;
      }
      buflen += 2; // enlarge the buffer to replace "\xef\xbf\xbd" with "*XXXXX"
   }
   // count the number of WUTF8_ESCAPE to escape them
   for (char *p=*pszUTF; p!=NULL && *p!='\0'; p++) {
      p=strchr(p, WUTF8_ESCAPE);
      if (p == NULL) {
         break;
      }
      buflen += 1; // enlarge the buffer to escape WUTF8_ESCAPE
   }

   // enlarge the buffer to hold the escape sequence
   *pszUTF = check_pool_memory_size(*pszUTF, buflen);
   // Decode the UTF16 char by char using section
   // "2.2 Decoding UTF-16" at http://www.ietf.org/rfc/rfc2781.txt
   const wchar_t *s = pszUCS;
   char *d = *pszUTF;
   while (*s != 0) {
      if (*s == WUTF8_WCHAR_ESCAPE) {
         *d++ = WUTF8_ESCAPE;
         *d++ = WUTF8_ESCAPE;
         buflen -= 2;
         s++;
         continue;
      } else if (*s<0xd800 || *s>0xdfff) {
         // single Unicode scalar value
         int r = p_WideCharToMultiByte(CP_UTF8,0,s,1,d,buflen,NULL,NULL);
         if (r == 0) {
            Dmsg1(0, "Unexpected error for single unicode char wchar_off=%ld\n", s-pszUCS);
            return 0; // Unexpected error
         }
         buflen -= r;
         d += r;
         s++;
         continue;
      } else if (0xd800<=*s && *s<=0xdbff && 0xdc00<=*(s+1) && *(s+1)<=0xdfff) {
         // we have a surrogate of 2 valid 16bits chars
         int r = p_WideCharToMultiByte(CP_UTF8,0,s,2,d,buflen,NULL,NULL);
         if (r == 0) {
            Dmsg1(0, "Unexpected error for surrogate of 2 valid 16bits char wchar_off=%ld\n", s-pszUCS);
         }
         buflen -= r;
         d += r;
         s += 2;
         continue;
      }
      // Use WUTF8_ESCAPE char to escape the invalid char
      int r = bsnprintf(d, buflen, "%c%04x", (int)WUTF8_ESCAPE, (int)*s);
      if (r != 5) {
         Dmsg2(0, "Unexpected error buffer too small 1 %d %s\n", r, d);
         return 0; // Unexpected error, buffer too small !
      }
      buflen -= r;
      d += r;
      s++;
   }
   if (buflen <= 0) {
      Dmsg0(0, "Unexpected error buffer too small 2\n");
      return 0; // Unexpected error, buffer too small !
   }
   *d = '\0';
   d++;
   buflen--;
   return d-*pszUTF;
}

/* Do the inverse of function wchar_path_2_wutf8()
 * return the number of wchar_t written including the final '\0'
 * or O if any error
 */
int wutf8_path_2_wchar(POOLMEM **ppszUCS, const char *pszUTF)
{
   if (!p_MultiByteToWideChar) {
      return 0;
   }
   DWORD cchSize = (strlen(pszUTF)+1);
   *ppszUCS = check_pool_memory_size(*ppszUCS, cchSize*sizeof(wchar_t));
   int nRet = p_MultiByteToWideChar(CP_UTF8, 0, pszUTF, -1, (LPWSTR) *ppszUCS,cchSize);
   ASSERT (nRet > 0);
   if (strchr(pszUTF, WUTF8_WCHAR_ESCAPE) == NULL) {
      /* their is no WUTF8_ESCAPE char in the string, process the all string at once */
      /* strlen of UTF8+1 is enough */
      return nRet;
   }
// IF WE GO BELOW THIS LINE, this is because the original Windows file name
// was holding an invalid char or an unexpected char WUTF8_ESCAPE
   wchar_t *s=(wchar_t *)*ppszUCS; // source
   wchar_t *d=(wchar_t *)*ppszUCS; // destination is smaller than source
   int n = 0;
   while (*s != L'\0') {
      if (*s == WUTF8_WCHAR_ESCAPE && *(s+1) == WUTF8_WCHAR_ESCAPE) {
         // two WUTF8_WCHAR_ESCAPE in a row, copy one and drop the other
         *d++ = *s++;
         s++;
         n++;
      }  else if (*s == WUTF8_WCHAR_ESCAPE) {
         // replace the the encoded '*WXYZ' char into its code point '\0xWXYZ'
         wchar_t w = 0;
         int val;
         s++;
         for (int i = 0; i < 4; i++) {
            if (*s == L'\0') {
               Dmsg0(0, "unexpected WUTF8_WCHAR_ESCAPE at end of the string!\n");
               return 0;
            }
            if (L'0' <= *s && *s <= L'9') {
               val=*s-L'0';
            } else if (L'a' <= *s && *s <= L'f') {
               val=*s-L'a'+10;
            } else if (L'A' <= *s && *s <= L'F') {
               val=*s-L'A'+10;
            } else {
               Dmsg0(0, "a WUTF8_WCHAR_ESCAPE should be followed by 4 hexa digits!\n");
               return 0;
            }
            w = (w << 4) | val;
            s++;
         }
         *d++=w;
         n++;
      } else {
         *d++ = *s++;
         n++;
      }
   }
   *d = L'\0';
   n++;
   return n;
}

/*
 * Convert from WCHAR (UTF-16-LE) to UTF-8
 * Don't use this function for path, use wchar_path_2_wutf8() instead
 */
int
wchar_2_UTF8(POOLMEM **pszUTF, const wchar_t *pszUCS)
{
   /**
    * The return value is the number of bytes written to the buffer.
    * The number includes the byte for the null terminator.
    */

   if (p_WideCharToMultiByte) {
      int nRet = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,NULL,0,NULL,NULL);
      *pszUTF = check_pool_memory_size(*pszUTF, nRet);
      return p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,*pszUTF,nRet,NULL,NULL);

   }
   return 0;
}

/*
 * Convert from WCHAR (UTF-16-LE) to UTF-8
 * Don't use this function for path, use wchar_path_2_wutf8() instead
 */
int
wchar_2_UTF8(char *pszUTF, const wchar_t *pszUCS, int cchChar)
{
   /**
    * The return value is the number of bytes written to the buffer.
    * The number includes the byte for the null terminator.
    */

   if (p_WideCharToMultiByte) {
      int nRet = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,pszUTF,cchChar,NULL,NULL);
      ASSERT (nRet > 0);
      return nRet;
   }
   return 0;
}

/*
* Don't use this function for path, use wchar_path_2_wutf8() instead
*/
int
UTF8_2_wchar(POOLMEM **ppszUCS, const char *pszUTF)
{
   /* the return value is the number of wide characters written to the buffer. */
   /* convert null terminated string from utf-8 to ucs2, enlarge buffer if necessary */

   if (p_MultiByteToWideChar) {
      /* strlen of UTF8 +1 is enough */
      DWORD cchSize = (strlen(pszUTF)+1);
      *ppszUCS = check_pool_memory_size(*ppszUCS, cchSize*sizeof (wchar_t));

      int nRet = p_MultiByteToWideChar(CP_UTF8, 0, pszUTF, -1, (LPWSTR) *ppszUCS,cchSize);
      ASSERT (nRet > 0);
      return nRet;
   }
   return 0;
}

#if 0
// not used anywhere
void
wchar_win32_path(const char *name, wchar_t *win32_name)
{
    const char *fname = name;
    while (*name) {
        /* Check for Unix separator and convert to Win32 */
        if (*name == '/') {
            *win32_name++ = '\\';     /* convert char */
        /* If Win32 separated that is "quoted", remove quote */
        } else if (*name == '\\' && name[1] == '\\') {
            *win32_name++ = '\\';
            name++;                   /* skip first \ */
        } else {
            *win32_name++ = *name;    /* copy character */
        }
        name++;
    }
    /* Strip any trailing slash, if we stored something */
    if (*fname != 0 && win32_name[-1] == '\\') {
        win32_name[-1] = 0;
    } else {
        *win32_name = 0;
    }
}
#endif

/*
 * Convert a WUTF8 path into a normalized wchar windows path
 * Get the result from cache
 * or
 * call wutf8_path_2_wchar() and norm_wchar_win32_path()
 * and save the result in cache
 */
int
make_win32_path_UTF8_2_wchar(POOLMEM **pszUCS, const char *pszUTF, BOOL* pBIsRawPath /*= NULL*/)
{
   P(Win32Convmutex);
   /* if we find the utf8 string in cache, we use the cached wchar version. */
   if (!g_pWin32ConvUTF8Cache) {
      Win32ConvInitCache();
   } else if (bstrcmp(pszUTF, g_pWin32ConvUTF8Cache)) {
      /* Return cached value */
      int32_t nBufSize = sizeof_pool_memory(g_pWin32ConvUCS2Cache);
      *pszUCS = check_pool_memory_size(*pszUCS, nBufSize);
      wcscpy((LPWSTR) *pszUCS, (LPWSTR)g_pWin32ConvUCS2Cache);
      V(Win32Convmutex);
      return nBufSize / sizeof (WCHAR);
   }

   /* convert from utf-8 to wchar */
   int nRet = wutf8_path_2_wchar(pszUCS, pszUTF);

#ifdef USE_WIN32_32KPATHCONVERSION
   /* add \\?\ to support 32K long filepaths */
   norm_wchar_win32_path(pszUCS, pBIsRawPath);
#else
   if (pBIsRawPath)
      *pBIsRawPath = FALSE;
#endif

   /* populate cache */
   g_pWin32ConvUCS2Cache = check_pool_memory_size(g_pWin32ConvUCS2Cache, sizeof_pool_memory(*pszUCS));
   wcscpy((LPWSTR) g_pWin32ConvUCS2Cache, (LPWSTR) *pszUCS);

   g_dwWin32ConvUTF8strlen = strlen(pszUTF);
   g_pWin32ConvUTF8Cache = check_pool_memory_size(g_pWin32ConvUTF8Cache, g_dwWin32ConvUTF8strlen+2);
   bstrncpy(g_pWin32ConvUTF8Cache, pszUTF, g_dwWin32ConvUTF8strlen+1);
   V(Win32Convmutex);

   return nRet;
}

#if !defined(_MSC_VER) || (_MSC_VER < 1400) // VC8+
int umask(int)
{
   return 0;
}
#endif

#ifndef LOAD_WITH_ALTERED_SEARCH_PATH
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x00000008
#endif

void *dlopen(const char *file, int mode)
{
   void *handle;

   handle = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
   return handle;
}

void *dlsym(void *handle, const char *name)
{
   void *symaddr;
   symaddr = (void *)GetProcAddress((HMODULE)handle, name);
   return symaddr;
}

int dlclose(void *handle)
{
   if (handle && !FreeLibrary((HMODULE)handle)) {
      errno = b_errno_win32;
      return 1;        /* failed */
   }
   return 0;           /* OK */
}

char *dlerror(void)
{
   static char buf[200];
   const char *err = errorString();
   bstrncpy(buf, (char *)err, sizeof(buf));
   LocalFree((void *)err);
   return buf;
}

int fcntl(int fd, int cmd)
{
   return 0;
}

int chown(const char *k, uid_t, gid_t)
{
   return 0;
}

int lchown(const char *k, uid_t, gid_t)
{
   return 0;
}

long int
random(void)
{
    return rand();
}

void
srandom(unsigned int seed)
{
   srand(seed);
}
// /////////////////////////////////////////////////////////////////
// convert from Windows concept of time to Unix concept of time
// /////////////////////////////////////////////////////////////////
void
cvt_utime_to_ftime(const time_t  &time, FILETIME &wintime)
{
   uint64_t mstime = time;
   mstime *= WIN32_FILETIME_SCALE;
   mstime += WIN32_FILETIME_ADJUST;

#if defined(_MSC_VER)
   wintime.dwLowDateTime = (DWORD)(mstime & 0xffffffffI64);
#else
   wintime.dwLowDateTime = (DWORD)(mstime & 0xffffffffUL);
#endif
   wintime.dwHighDateTime = (DWORD) ((mstime>>32)& 0xffffffffUL);
}

time_t
cvt_ftime_to_utime(const FILETIME &time)
{
    uint64_t mstime = time.dwHighDateTime;
    mstime <<= 32;
    mstime |= time.dwLowDateTime;

    mstime -= WIN32_FILETIME_ADJUST;
    mstime /= WIN32_FILETIME_SCALE; // convert to seconds.

    return (time_t) (mstime & 0xffffffff);
}

static const char *errorString(void)
{
   LPVOID lpMsgBuf;

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default lang
                 (LPTSTR) &lpMsgBuf,
                 0,
                 NULL);

   /* Strip any \r or \n */
   char *rval = (char *) lpMsgBuf;
   char *cp = strchr(rval, '\r');
   if (cp != NULL) {
      *cp = 0;
   } else {
      cp = strchr(rval, '\n');
      if (cp != NULL)
         *cp = 0;
   }
   return rval;
}

/* retrieve information about reparse point
 * the HANDLE must have been open with flag FILE_FLAG_OPEN_REPARSE_POINT
 * return value can be any of
 * -1 for error
 *  0 this is not a M$ reparse point, BUT it can be reparse point !!!!
 * WIN32_REPARSE_POINT Can be any reparse point, but not one of the following
 * WIN32_MOUNT_POINT   A Volume mounted in a directory
 * WIN32_JUNCTION_POINT A special type of symlink to a directory
 * WIN32_SYMLINK_POINT A symlink to a file or a directory (look for FILE_ATTRIBUTE_DIRECTORY )
 */
static int win_get_reparse_point(HANDLE h, DWORD *reparse_tag=NULL, POOLMEM **reparse=NULL)
{
   int ret = WIN32_REPARSE_NONE;
   char buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
   REPARSE_DATA_BUFFER *rdata = (REPARSE_DATA_BUFFER *)buffer;

   // Query the reparse data
   DWORD dwRetLen;
   BOOL r = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, rdata, sizeof(buffer), &dwRetLen, NULL);
   if (r == FALSE)
   {
      Dmsg1(0, "DeviceIoControl error=%ld\n", GetLastError());
      return -1;
   }

   if (reparse_tag != NULL) {
      *reparse_tag = rdata->ReparseTag;
   }
   if (IsReparseTagMicrosoft(rdata->ReparseTag))
   {
      if (rdata->ReparseTag == IO_REPARSE_TAG_SYMLINK)
      {
         if (reparse != NULL) {
            wchar_path_2_wutf8(reparse,
                  (wchar_t *)&rdata->SymbolicLinkReparseBuffer.PathBuffer[rdata->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)]);
         }
         ret = WIN32_SYMLINK_POINT;
         goto bailout;
      } else if (rdata->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
         POOLMEM *path = NULL;
         POOLMEM **rpath = reparse;
         if (rpath == NULL) {
            path = get_pool_memory(PM_FNAME);
            rpath = &path;
         }
         wchar_path_2_wutf8(rpath,
               (wchar_t *)&rdata->MountPointReparseBuffer.PathBuffer[rdata->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)]);
         ret = (strncasecmp(*rpath, "\\??\\Volume{", 11) == 0)?WIN32_MOUNT_POINT:WIN32_JUNCTION_POINT;
         if (path != NULL) {
            free_pool_memory(path);
         }
         goto bailout;
      }
      ret = WIN32_REPARSE_POINT;
      goto bailout;
   }
   else
   {
      // Not a Microsoft-reparse point
      ret = WIN32_REPARSE_NONE;
   }
bailout:
   return ret;
}

static int win_get_reparse_point(const wchar_t *path, DWORD *reparse_tag=NULL, POOLMEM **reparse=NULL)
{
   HANDLE h = CreateFileW(path, FILE_READ_EA,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                NULL, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
   if (h == INVALID_HANDLE_VALUE) {
      return -1;
   }
   int type = win_get_reparse_point(h, reparse_tag, reparse);
   CloseHandle(h);
   return type;
}

static int win_get_reparse_point(const char *path, DWORD *reparse_tag=NULL, POOLMEM **reparse=NULL)
{
   POOL_MEM wpath(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&wpath.addr(), path);

   return win_get_reparse_point((wchar_t*)wpath.c_str(), reparse_tag, reparse);
}

/* Convert Windows file attributes into a "unix" st_mode */
static uint16_t attribute_to_mode(DWORD FileAttributes)
{
   uint16_t st_mode = 0777;               /* start with everything */
   if (FileAttributes & FILE_ATTRIBUTE_READONLY)
      st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
   if (FileAttributes & FILE_ATTRIBUTE_SYSTEM)
      st_mode &= ~S_IRWXO; /* remove everything for other */
   if (FileAttributes & FILE_ATTRIBUTE_HIDDEN)
      st_mode |= S_ISVTX; /* use sticky bit -> hidden */
   if (FileAttributes & FILE_ATTRIBUTE_ENCRYPTED)
      st_mode |= S_ISGID; /* use set group ID -> encrypted */
   if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      st_mode |= S_IFDIR;
   } else {
      st_mode |= S_IFREG;
   }
   return st_mode;
}

/* fill in sb with Windows attributes */
static void attributes_to_stat(struct stat *sb, DWORD FileAttributes,
      DWORD FileSizeHigh, DWORD FileSizeLow,
      const FILETIME &LastAccessTime, const FILETIME &LastWriteTime)
{
   sb->st_mode = attribute_to_mode(FileAttributes);
   sb->st_fattrs = FileAttributes;

   sb->st_size = FileSizeHigh;
   sb->st_size <<= 32;
   sb->st_size |= FileSizeLow;

   sb->st_blksize = 4096;

   sb->st_blocks = (uint32_t)(sb->st_size + 4095)/4096;

   sb->st_atime = cvt_ftime_to_utime(LastAccessTime);
   sb->st_mtime = cvt_ftime_to_utime(LastWriteTime);
   sb->st_ctime = MAX(sb->st_mtime, sb->st_ctime);
}
#if 0
/*
 * This is only called for directories, and is used to get the directory
 *  attributes and find out if we have a junction point or a mount point
 *  or other kind of "funny" directory.
 *  Notice: Python use CreateFileW() and GetFileInformationByHandle()
 *  to get this information instead and fallback to FindFirstFile()
 */
static int
statDir(const char *file, struct stat *sb, POOLMEM **readlnk=NULL)
{
   WIN32_FIND_DATAW info_w;       // window's file info
   HANDLE h = INVALID_HANDLE_VALUE;

   /*
    * Oh, cool, another exception: Microsoft doesn't let us do
    *  FindFile operations on a Drive, so simply fake root attributes.
    *  We could try CreateFileW() and GetFileInformationByHandle()
    */
   if (file[1] == ':' && file[2] == 0) {
      time_t now = time(NULL);
      Dmsg1(dbglvl, "faking ROOT attrs(%s).\n", file);
      sb->st_mode = S_IFDIR;
      sb->st_mode |= S_IREAD|S_IEXEC|S_IWRITE;
      sb->st_ctime = now;    /* File change time (inode change...) */
      sb->st_mtime = now;    /* File modify time */
      sb->st_atime = now;    /* File access time */
      sb->st_rdev = 0;
      return 0;
    }

   POOL_MEM pwszBuf(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), file);

   Dmsg1(dbglvl, "FindFirstFileW=%s\n", file);
   h = p_FindFirstFileW((LPCWSTR)pwszBuf.c_str(), &info_w);

   if (h == INVALID_HANDLE_VALUE) {
      const char *err = errorString();
      /*
       * Note, in creating leading paths, it is normal that
       * the file does not exist.
       */
      Dmsg2(2099, "FindFirstFile(%s):%s\n", file, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      return -1;
   }

   FindClose(h);

   attributes_to_stat(sb, info_w.dwFileAttributes,
         info_w.nFileSizeHigh, info_w.nFileSizeLow,
         info_w.ftLastAccessTime, info_w.ftLastWriteTime);

   Dmsg2(200, "Fattrs=0x%x st_mode=0x%x\n", sb->st_fattrs, sb->st_mode);
   /*
    * Store reparse/mount point info in st_rdev.  Note a
    *  Win32 reparse point (junction point) is like a link
    *  though it can have many properties (directory link,
    *  soft link, hard link, HSM, ...
    *  A mount point is a reparse point where another volume
    *  is mounted, so it is like a Unix mount point (change of
    *  filesystem).
    */
   if (info_w.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      sb->st_rdev = WIN32_MOUNT_POINT;
   } else {
      sb->st_rdev = 0;
   }
   /* This is a lot of work just to know that it is deduped */
   if (info_w.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT &&
       (info_w.dwReserved0 & IO_REPARSE_TAG_DEDUP)) {
      sb->st_fattrs |= FILE_ATTRIBUTE_DEDUP;  /* add our own bit */
   }
   if ((info_w.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
        (info_w.dwReserved0 & IO_REPARSE_TAG_MOUNT_POINT)) {
      sb->st_rdev = WIN32_MOUNT_POINT;           /* mount point */
      /*
       * Now to find out if the directory is a mount point or
       * a reparse point, we must do a song and a dance.
       * Explicitly open the file to read the reparse point, then
       * call DeviceIoControl to find out if it points to a Volume
       * or to a directory.
       */
      h = CreateFileW((LPCWSTR)pwszBuf.c_str(), GENERIC_READ,
             FILE_SHARE_READ, NULL, OPEN_EXISTING,
             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
             NULL);
      if (h != INVALID_HANDLE_VALUE) {
         char dummy[1000];
         REPARSE_DATA_BUFFER *rdb = (REPARSE_DATA_BUFFER *)dummy;
         rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
         DWORD bytes;
         bool ok;
         ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT,
                 NULL, 0,                           /* in buffer, bytes */
                 (LPVOID)rdb, (DWORD)sizeof(dummy), /* out buffer, btyes */
                 (LPDWORD)&bytes, (LPOVERLAPPED)0);
         if (ok) {
            POOLMEM *utf8 = get_pool_memory(PM_NAME);
            wchar_path_2_wutf8(&utf8, (wchar_t *)rdb->SymbolicLinkReparseBuffer.PathBuffer);
            Dmsg2(dbglvl, "Junction %s points to: %s\n", file, utf8);
            if (strncasecmp(utf8, "\\??\\volume{", 11) == 0) {
               Dmsg2(dbglvl, "FIRST MOUNT POINT %s points to: %s remove code for SECOND\n", file, utf8);
               sb->st_rdev = WIN32_MOUNT_POINT;
            } else if (strncasecmp(utf8, "\\\\?\\volume{", 11) == 0) {
               /* Alain: I think this one is the right one !!!! */
               Dmsg2(dbglvl, "SECOND MOUNT POINT %s points to: %s remove code for FIRST\n", file, utf8);
               sb->st_rdev = WIN32_MOUNT_POINT;
            } else {
               /* It points to a directory so we ignore it. */
               sb->st_rdev = WIN32_JUNCTION_POINT;
            }
            /* If requested, store the link for future use */
            if (readlnk) {
               pm_strcpy(readlnk, utf8);
            }
            free_pool_memory(utf8);
         }
         CloseHandle(h);
      } else {
         Dmsg1(dbglvl, "Invalid handle from CreateFile(%s)\n", file);
      }
   }
   Dmsg2(dbglvl, "st_rdev=%d file=%s\n", sb->st_rdev, file);
   sb->st_ctime = MAX(sb->st_mtime, sb->st_ctime);
   /* Note ctime is last change time -- not creation time */
   Dmsg2(200, "Fattrs=0x%x st_mode=0x%x\n", sb->st_fattrs, sb->st_mode);

   return 0;
}
#endif

/* On success, readlink() returns the number of bytes placed in  buf.   On
 * error, -1 is returned and errno is set to indicate the error.
 *
 * TODO: Still need to activate the readlink() call in find_one.c
 *       by returning a S_ISLNK(st_mode) compatible flag.
 */
int
readlink(const char *path, char *buf, int bufsiz)
{
   int ret=-1;
   POOLMEM *lnk = get_pool_memory(PM_FNAME);
   *lnk = 0;
   int type = win_get_reparse_point(path, NULL, &lnk);
   if (type == WIN32_SYMLINK_POINT) {
      ret = bstrncpy(buf, lnk, bufsiz) - buf - 1; // Don't count the last \0
   }
   free_pool_memory(lnk);
   return ret;
}

/* symlink() shall return 0; otherwise, it shall return -1 and set errno to
 * indicate the error.
 */
int
symlink(const char *path1, const char *path2)
{
   int ret = 0;
   struct stat st;
   DWORD isdir = 0;

   if (stat(path1, &st) == 0) {
      if (st.st_mode & S_IFDIR) {
         isdir=1;
      }
   } else {
      Dmsg1(200, "Cannot find the source directory %s\n", path1);
      return -1;
   }

   if (!p_CreateSymbolicLinkW) {
      Dmsg0(200, "No implementation of CreateSymbolicLink available\n");
      return -1;
   }

   POOL_MEM pwszBuf1(PM_FNAME);
   POOL_MEM pwszBuf2(PM_FNAME);

   make_win32_path_UTF8_2_wchar(&pwszBuf1.addr(), path1);
   make_win32_path_UTF8_2_wchar(&pwszBuf2.addr(), path2);

   Dmsg2(dbglvl, "Trying to symlink (%s -> %s)\n", path1, path2);

   if (!p_CreateSymbolicLinkW((LPCWSTR)pwszBuf2.c_str(), (LPCWSTR)pwszBuf1.c_str(), isdir)) {
      const char *err = errorString();
      Dmsg3(200, "Cannot create symlink (%s -> %s):%s\n", path1, path2, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      ret = -1;
   }

   return ret;
}

/* Do a stat() on a valid HANDLE (opened with CreateFile()) */
int hstat(HANDLE h, struct stat *sb)
{
   BY_HANDLE_FILE_INFORMATION info;

   if (!GetFileInformationByHandle(h, &info)) {
       const char *err = errorString();
       Dmsg1(dbglvl, "GetfileInformationByHandle: %s\n", err);
       LocalFree((void *)err);
       errno = b_errno_win32;
       return -1;
   }

   /* We should modify only variables that are modified in stat() 
    * everything else should be carefully tested.
    */

   /* When turned on, we see a lot of messages such as
    * C:/PerfLogs is a different filesystem. Will not descend from C:/ into it.
    */
   //sb->st_dev = info.dwVolumeSerialNumber;

   /* The st_ino is not used in stat() */
   sb->st_ino = info.nFileIndexHigh;
   sb->st_ino <<= 32;
   sb->st_ino |= info.nFileIndexLow;

   sb->st_nlink = 1;
#if 0                           // We don't have the link() call right now
   // TODO: something with CreateHardLinkFunc()
   sb->st_nlink = (short)info.nNumberOfLinks;
   if (sb->st_nlink > 1) {
      Dmsg1(dbglvl,  "st_nlink=%d\n", sb->st_nlink);
   }
#endif
   attributes_to_stat(sb, info.dwFileAttributes,
         info.nFileSizeHigh, info.nFileSizeLow,
         info.ftLastAccessTime, info.ftLastWriteTime);

   /* Get the ChangeTime information with an other API, when attributes are modified
    * the ChangeTime is modified while CreationTime and WriteTime are not
    */
   FILE_BASIC_INFO file_basic_info;
   if (p_GetFileInformationByHandleEx &&
       p_GetFileInformationByHandleEx(h, FileBasicInfo, &file_basic_info, sizeof(file_basic_info))) {
      FILETIME *pftChangeTime = (FILETIME *)&file_basic_info.ChangeTime;
      sb->st_ctime = cvt_ftime_to_utime(*pftChangeTime);
      sb->st_ctime = MAX(sb->st_mtime, sb->st_ctime);
   }

   /* Use st_rdev to store reparse attribute */
   if  (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      DWORD reparse_tag;
      sb->st_rdev = WIN32_REPARSE_POINT;
      int type = win_get_reparse_point(h, &reparse_tag);
      if (type == WIN32_MOUNT_POINT || type == WIN32_JUNCTION_POINT) {
         sb->st_rdev = type;
         if (!(sb->st_mode & S_IFDIR)) {
            Pmsg1(0, "A reparse point of type %d is expected to be a directory\n", type);
            ASSERTD(FALSE, "A mount point or a junction should be a directory\n");
         }
      } else if (type == WIN32_SYMLINK_POINT) {
         sb->st_rdev = type;
      }
   }
   Dmsg3(dbglvl, "st_rdev=%d sizino=%d ino=%lld\n", sb->st_rdev, sizeof(sb->st_ino),
      (long long)sb->st_ino);

   Dmsg2(200, "Fattrs=0x%x st_mode=0x%x\n", sb->st_fattrs, sb->st_mode);
   return 0;
}

/* add a trailing "\\" to a "naked" drive letter like "X:"
 * expect a wchar_t path */
void sanitize_drive_root(POOLMEM *&wpath)
{
   wchar_t *p=(wchar_t *)wpath;
   if (p[1] == L':' && p[2] == L'\0') {
      wpath = check_pool_memory_size(wpath, (wcslen((wchar_t*)wpath)+1)*sizeof(wchar_t));
      wcscat((wchar_t *)wpath, L"\\");
   }
}

/*
 * stat() don't like path like:
 * - c:
 * - \\?\GLOBALROOT\Device\HarddiskVolumeShadowCopy5
 * You must add a trailing '\'
 */
int stat(const char *file, struct stat *sb)
{
   WIN32_FILE_ATTRIBUTE_DATA data;

   errno = 0;
   /* We do the first try with a file HANDLER, because we want to use the
    * ChangeTime that is only available with GetFileInformationByHandleEx
    */
   POOL_MEM pwszBuf(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), file);
   sanitize_drive_root(pwszBuf.addr());

#if 0
   ret = stat2((wchar_t *)pwszBuf.c_str(), sb);

   if (!ret) {
      return ret;
   }

   if (!p_GetFileAttributesExW) {
      return -1;
   }
   /* We were not able to open a filehandler on the file to get attributes,
    * so we try with the name. It may happen for example with encrypted files.
    */
#endif
   memset(sb, 0, sizeof(*sb));
   BOOL b = p_GetFileAttributesExW((LPCWSTR)pwszBuf.c_str(), GetFileExInfoStandard, &data);

   if (!b) {
      const char *err = errorString();
      Dmsg3(dbglvl, "GetFileAttributesExW(%s):%s %ls\n", file, err, pwszBuf.c_str());
      LocalFree((void *)err);
      return -1;
   }

   attributes_to_stat(sb, data.dwFileAttributes,
         data.nFileSizeHigh, data.nFileSizeLow,
         data.ftLastAccessTime, data.ftLastWriteTime);

   /* Use st_rdev to store reparse attribute */
   sb->st_rdev = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? WIN32_REPARSE_POINT : 0;

   sb->st_nlink = 1;
   sb->st_ctime = sb->st_mtime;

   if  (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
#if 0
      WIN32_FIND_DATAW info_w;
      HANDLE h = p_FindFirstFileW((LPCWSTR)pwszBuf.c_str(), &info_w);

      sb->st_rdev = WIN32_REPARSE_POINT;
      if (h == INVALID_HANDLE_VALUE) {
         const char *err = errorString();
         /*
          * Note, in creating leading paths, it is normal that
          * the file does not exist.
          */
         Dmsg2(2099, "FindFirstFile(%s):%s\n", file, err);
         LocalFree((void *)err);
         errno = b_errno_win32;
         return -1;
      }
      FindClose(h);
#endif

      DWORD reparse_tag;
      int type = win_get_reparse_point((wchar_t*)pwszBuf.c_str(), &reparse_tag);
#ifndef DO_WE_NEED_THAT // ?????
      if (reparse_tag & IO_REPARSE_TAG_DEDUP) {
         sb->st_fattrs |= FILE_ATTRIBUTE_DEDUP;  /* add our own bit */
      }
#endif
      Dmsg1(dbglvl, "reparse_point type=%d\n", type);
      if (type == WIN32_MOUNT_POINT || type == WIN32_JUNCTION_POINT) {
         sb->st_rdev = type;
         if (!(sb->st_mode & S_IFDIR)) {
            Pmsg1(0, "A reparse point of type %d is expected to be a directory\n", type);
            ASSERTD(FALSE, "A mount point or a junction should be a directory\n");
         }
      } else if (type == WIN32_SYMLINK_POINT) {
         sb->st_rdev = type;
      }
   }
   Dmsg4(dbglvl, "sizino=%d ino=%lld file=%s rdev=%d\n", sizeof(sb->st_ino),
                       (long long)sb->st_ino, file, sb->st_rdev);
   Dmsg2(200, "Fattrs=0x%x st_mode=0x%x\n", sb->st_fattrs, sb->st_mode);
   return 0;
}

int
fstat(intptr_t fd, struct stat *sb)
{
   return hstat((HANDLE)_get_osfhandle(fd), sb);
}

/*
 * We write our own ftruncate because the one in the
 *  Microsoft library mrcrt.dll does not truncate
 *  files greater than 2GB.
 *  KES - May 2007
 */
int win32_ftruncate(int fd, int64_t length)
{
   /* Set point we want to truncate file */
   __int64 pos = _lseeki64(fd, (__int64)length, SEEK_SET);

   if (pos != (__int64)length) {
      errno = EACCES;         /* truncation failed, get out */
      return -1;
   }

   /* Truncate file */
   if (SetEndOfFile((HANDLE)_get_osfhandle(fd)) == 0) {
      errno = b_errno_win32;
      return -1;
   }
   errno = 0;
   return 0;
}

int win_flock_hold(HANDLE fhandle, int non_blocking, int exclusive) {
   DWORD file_lower, file_upper;
   file_lower = GetFileSize(fhandle, &file_upper);
   OVERLAPPED ov;
   memset(&ov, 0, sizeof ov);
   int flags = 0;

   if (non_blocking) {
      flags |= LOCKFILE_FAIL_IMMEDIATELY;
   }

   if (exclusive) {
      flags |= LOCKFILE_EXCLUSIVE_LOCK;
   }
   
   /* Lock the whole file */
   return LockFileEx(fhandle, flags, 0, file_lower, file_upper, &ov);
}

int win_flock_release(HANDLE fhandle) {
   DWORD size_lower, size_upper;
   size_lower = GetFileSize(fhandle, &size_upper);
   return UnlockFile(fhandle, 0, 0, size_lower, size_upper);
}

int flock(int fd, int operation)
{
   DWORD success;
   HANDLE fhandle = (HANDLE) _get_osfhandle(fd);
   
   if (fhandle == INVALID_HANDLE_VALUE) {
      errno = EBADF;
      return -1;
   }

   int nb_flag = operation & LOCK_NB;
   operation &= ~LOCK_NB;

   switch (operation) {
      case LOCK_SH:
         success = win_flock_hold(fhandle, nb_flag, 0);
         break;
      case LOCK_EX:
         success = win_flock_hold(fhandle, nb_flag, 1);
         break;
      case LOCK_UN:
         success = win_flock_release(fhandle);
         break;
      default:
         errno = EINVAL;
         return -1;
   }

   if (!success) {
      DWORD err = GetLastError();
      switch (err) {
         case ERROR_LOCK_VIOLATION:
            errno = EAGAIN;
            break;
         case ERROR_NOT_ENOUGH_MEMORY:
            errno = ENOMEM;
            break;
         case ERROR_BAD_COMMAND:
            errno = EINVAL;
            break;
         default:
            errno = err;
      }

      return -1;
   }

   return 0;
}

int fcntl(int fd, int cmd, long arg)
{
   int rval = 0;

   switch (cmd) {
   case F_GETFL:
      rval = O_NONBLOCK;
      break;

   case F_SETFL:
      rval = 0;
      break;

   default:
      errno = EINVAL;
      rval = -1;
      break;
   }

   return rval;
}

int
lstat(const char *file, struct stat *sb)
{
   return stat(file, sb);
}

void
sleep(int sec)
{
   Sleep(sec * 1000);
}

int
geteuid(void)
{
   return 0;
}

int
execvp(const char *, char *[]) {
   errno = ENOSYS;
   return -1;
}


int
fork(void)
{
   errno = ENOSYS;
   return -1;
}

int
pipe(int[])
{
   errno = ENOSYS;
   return -1;
}

int
waitpid(int, int*, int)
{
   errno = ENOSYS;
   return -1;
}

#ifndef HAVE_MINGW
int
strcasecmp(const char *s1, const char *s2)
{
   register int ch1, ch2;

   if (s1==s2)
      return 0;       /* strings are equal if same object. */
   else if (!s1)
      return -1;
   else if (!s2)
      return 1;
   do {
      ch1 = *s1;
      ch2 = *s2;
      s1++;
      s2++;
   } while (ch1 != 0 && tolower(ch1) == tolower(ch2));

   return(ch1 - ch2);
}
#endif //HAVE_MINGW

int
strncasecmp(const char *s1, const char *s2, int len)
{
   register int ch1 = 0, ch2 = 0;

   if (s1==s2)
      return 0;       /* strings are equal if same object. */
   else if (!s1)
      return -1;
   else if (!s2)
      return 1;

   while (len--) {
      ch1 = *s1;
      ch2 = *s2;
      s1++;
      s2++;
      if (ch1 == 0 || tolower(ch1) != tolower(ch2)) break;
   }

   return (ch1 - ch2);
}

int
gettimeofday(struct timeval *tv, struct timezone *)
{
    SYSTEMTIME now;
    FILETIME tmp;

    GetSystemTime(&now);

    if (tv == NULL) {
       errno = EINVAL;
       return -1;
    }
    if (!SystemTimeToFileTime(&now, &tmp)) {
       errno = b_errno_win32;
       return -1;
    }

    int64_t _100nsec = tmp.dwHighDateTime;
    _100nsec <<= 32;
    _100nsec |= tmp.dwLowDateTime;
    _100nsec -= WIN32_FILETIME_ADJUST;

    tv->tv_sec = (long)(_100nsec / 10000000);
    tv->tv_usec = (long)((_100nsec % 10000000)/10);
    return 0;

}

/*
 * Write in Windows System log
 */
void syslog(int type, const char *fmt, ...)
{
   va_list   arg_ptr;
   int len, maxlen;
   POOLMEM *msg;

   msg = get_pool_memory(PM_EMSG);

   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         msg = realloc_pool_memory(msg, maxlen + maxlen/2);
         continue;
      }
      break;
   }
   LogErrorMsg((const char *)msg);
   free_memory(msg);
}

void
closelog()
{
}

struct passwd *
getpwuid(uid_t)
{
    return NULL;
}

struct group *
getgrgid(uid_t)
{
    return NULL;
}

// implement opendir/readdir/closedir on top of window's API

typedef struct _dir
{
    WIN32_FIND_DATAW data_w;    // window's file info (wchar version)
    POOLMEM     *spec;           // the directory we're traversing
    HANDLE      dirh;           // the search handle
    bool        call_findnextfile; // use FindFirstFile data first
} _dir;

DIR *
opendir(const char *path)
{
    /* enough space for VSS !*/
    _dir *rval = NULL;
    POOL_MEM pwcBuf(PM_FNAME);

    if (path == NULL) {
       errno = ENOENT;
       return NULL;
    }
    if (!p_FindFirstFileW || !p_FindNextFileW) {
       errno = ENOMEM;
       return NULL;
    }

    Dmsg1(10, "Opendir path=%s\n", path);
    rval = (_dir *)get_pool_memory(PM_FNAME);
    rval = (_dir *)check_pool_memory_size((POOLMEM *)rval, sizeof(_dir));
    memset (rval, 0, sizeof (_dir));

    rval->spec = get_pool_memory(PM_FNAME);
    pm_strcpy(rval->spec, path);

    // convert to wchar_t
    make_win32_path_UTF8_2_wchar(&pwcBuf.addr(), path);

    // Add a "*" or "\\*" to the path, see documentation of FindFirstFile()
    // Do it after the call to make_win32_path_UTF8_2_wchar() that escape '*'
    int len = wcslen((wchar_t*)pwcBuf.c_str());
    pwcBuf.check_size((len+2)*sizeof(wchar_t));
    wchar_t *p = (wchar_t*)pwcBuf.c_str();
    if (p[len-1] != L'\\') {
       p[len++] = L'\\';
    }
    p[len++] = L'*';
    p[len] = L'\0';
    Dmsg2(10, "opendir XXX=%ls %s\n", p, path);

    rval->dirh = p_FindFirstFileW((LPCWSTR)pwcBuf.c_str(), &rval->data_w);
    rval->call_findnextfile = false;

    if (rval->dirh == INVALID_HANDLE_VALUE) {
       if (GetLastError() == ERROR_FILE_NOT_FOUND) {
          /* the directory is empty, continue with an INVALID_HANDLE_VALUE handle */
          rval->data_w.cFileName[0]='\0';
       } else {
          goto err;
       }
    }
    Dmsg4(10, "opendir(%s)\n\tspec=%s,\n\tFindFirstFile returns %p cFileName=%ls\n",
          path, rval->spec, rval->dirh, rval->data_w.cFileName);

    return (DIR *)rval;

err:
    if (rval) {
       if (rval->spec) {
          free_pool_memory(rval->spec);
       }
       free_pool_memory((POOLMEM *)rval);
    }
    errno = b_errno_win32;
    return NULL;
}

int
closedir(DIR *dirp)
{
    _dir *dp = (_dir *)dirp;
    if (dp->dirh != INVALID_HANDLE_VALUE) {
       FindClose(dp->dirh);
    }
    free_pool_memory(dp->spec);
    free_pool_memory((POOLMEM *)dp);
    return 0;
}

/*
  typedef struct _WIN32_FIND_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    TCHAR cFileName[MAX_PATH];
    TCHAR cAlternateFileName[14];
} WIN32_FIND_DATA, *PWIN32_FIND_DATA;
*/

int breaddir(DIR *dirp, POOLMEM *&dname)
{
   _dir *dp = (_dir *)dirp;

   if (dirp == NULL) {
      errno = EBADF;
      return EBADF;
   }

   if (dp->call_findnextfile) {
      if (p_FindNextFileW(dp->dirh, &dp->data_w)) {
      } else {
         if (GetLastError() == ERROR_NO_MORE_FILES) {
            Dmsg1(dbglvl, "breaddir(%p) ERROR_NO_MORE_FILES\n", dirp);
            return -1; // end of directory reached
         } else {
            errno = b_errno_win32;
            return b_errno_win32;
         }
      }
    } else {
       // use data from FindFirstFile first then next time call FindNextFileW
       if (dp->dirh == INVALID_HANDLE_VALUE) {
          return -1; // the directory is empty, no "." nor ".." (special case)
       }
       dp->call_findnextfile = true;
    }
    wchar_path_2_wutf8(&dname, dp->data_w.cFileName);
    Dmsg2(10, "breaddir %ls => %s\n", dp->data_w.cFileName, dname);
    return 0;
}

/*
 * Dotted IP address to network address
 *
 * Returns 1 if  OK
 *         0 on error
 */
int
inet_aton(const char *a, struct in_addr *inp)
{
   const char *cp = a;
   uint32_t acc = 0, tmp = 0;
   int dotc = 0;

   if (!isdigit(*cp)) {         /* first char must be digit */
      return 0;                 /* error */
   }
   do {
      if (isdigit(*cp)) {
         tmp = (tmp * 10) + (*cp -'0');
      } else if (*cp == '.' || *cp == 0) {
         if (tmp > 255) {
            return 0;           /* error */
         }
         acc = (acc << 8) + tmp;
         dotc++;
         tmp = 0;
      } else {
         return 0;              /* error */
      }
   } while (*cp++ != 0);
   if (dotc != 4) {              /* want 3 .'s plus EOS */
      return 0;                  /* error */
   }
   inp->s_addr = htonl(acc);     /* store addr in network format */
   return 1;
}


/*
 *    Convert from presentation format (which usually means ASCII printable)
 *     to network format (which is usually some kind of binary format).
 * return:
 *    1 if the address was valid for the specified address family
 *    0 if the address wasn't valid (`dst' is untouched in this case)
 */
int
binet_pton(int af, const char *src, void *dst)
{
   switch (af) {
   case AF_INET:
   case AF_INET6:
      if (p_InetPton) {
         return p_InetPton(af, src, dst);
      }
      return 0;
   default:
      return 0;
   }
}


int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (rem)
        rem->tv_sec = rem->tv_nsec = 0;
    Sleep((req->tv_sec * 1000) + (req->tv_nsec/1000000));
    return 0;
}

void
init_signals(void terminate(int sig))
{

}

void
init_stack_dump(void)
{

}


long
pathconf(const char *path, int name)
{
    switch(name) {
    case _PC_PATH_MAX :
        if (strncmp(path, "\\\\?\\", 4) == 0)
            return 32767;
    case _PC_NAME_MAX :
        return 255;
    }
    errno = ENOSYS;
    return -1;
}

int
WSA_Init(void)
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;

    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
       wVersionRequested = MAKEWORD(2, 0);
       err = WSAStartup(wVersionRequested, &wsaData);
       if (err != 0) {
          wVersionRequested = MAKEWORD(1, 1);
          err = WSAStartup(wVersionRequested, &wsaData);
       }
    }

    if (err != 0) {
        printf("Can not start Windows Sockets\n");
        errno = ENOSYS;
        return -1;
    }

    return 0;
}

static DWORD fill_attribute(DWORD attr, mode_t mode)
{
   Dmsg1(dbglvl, "  before attr=%lld\n", (uint64_t) attr);
   /* Use Bacula mappings define in stat() above */
   if (mode & (S_IRUSR|S_IRGRP|S_IROTH)) { // If file is readable
      attr &= ~FILE_ATTRIBUTE_READONLY;    // then this is not READONLY
   } else {
      attr |= FILE_ATTRIBUTE_READONLY;
   }
   if (mode & S_ISVTX) {                   // The sticky bit <=> HIDDEN
      attr |= FILE_ATTRIBUTE_HIDDEN;
   } else {
      attr &= ~FILE_ATTRIBUTE_HIDDEN;
   }
   if (mode & S_ISGID) {                   // The set group ID <=> ENCRYPTED
      attr |= FILE_ATTRIBUTE_ENCRYPTED;
   } else {
      attr &= ~FILE_ATTRIBUTE_ENCRYPTED;
   }
   if (mode & S_IRWXO) {              // Other can read/write/execute ?
      attr &= ~FILE_ATTRIBUTE_SYSTEM; // => Not system
   } else {
      attr |= FILE_ATTRIBUTE_SYSTEM;
   }
   Dmsg1(dbglvl, "  after attr=%lld\n", (uint64_t)attr);
   return attr;
}

int win32_chmod(const char *path, mode_t mode)
{
   bool ret=false;
   DWORD attr;

   Dmsg2(dbglvl, "win32_chmod(path=%s mode=%lld)\n", path, (uint64_t)mode);
   POOL_MEM pwszBuf(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), path);

   attr = p_GetFileAttributesW((LPCWSTR) pwszBuf.c_str());
   if (attr != INVALID_FILE_ATTRIBUTES) {
      /* Use Bacula mappings define in stat() above */
      attr = fill_attribute(attr, mode);
      ret = p_SetFileAttributesW((LPCWSTR)pwszBuf.c_str(), attr);
   }
   Dmsg0(dbglvl, "Leave win32_chmod. AttributesW\n");

   if (!ret) {
      const char *err = errorString();
      Dmsg2(dbglvl, "Get/SetFileAttributes(%s): %s\n", path, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      return -1;
   }
   return 0;
}


int
win32_chdir(const char *dir)
{
   POOL_MEM pwszBuf(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), dir);

   BOOL b=p_SetCurrentDirectoryW && p_SetCurrentDirectoryW((LPCWSTR)pwszBuf.c_str());

   if (!b) {
      errno = b_errno_win32;
      return -1;
   }

   return 0;
}

int
win32_mkdir(const char *dir)
{
   Dmsg1(dbglvl, "enter win32_mkdir. dir=%s\n", dir);
   if (p_wmkdir){
      POOL_MEM pwszBuf(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), dir);

      int n = p_wmkdir((LPCWSTR)pwszBuf.c_str());
      Dmsg0(dbglvl, "Leave win32_mkdir did wmkdir\n");
      return n;
   }

   Dmsg0(dbglvl, "Leave win32_mkdir did _mkdir\n");
   return _mkdir(dir);
}


char *
win32_getcwd(char *buf, int maxlen)
{
   if (!p_GetCurrentDirectoryW) {
      return NULL;
   }
   POOL_MEM pwszBuf(PM_FNAME);
   pwszBuf.check_size(maxlen*sizeof(wchar_t));

   if (0 == p_GetCurrentDirectoryW(maxlen, (LPWSTR) pwszBuf.c_str())) {
      return NULL;
   }

   POOL_MEM wtf8(PM_FNAME);
   int n = wchar_path_2_wutf8(&wtf8.addr(), (wchar_t *)pwszBuf.c_str());
   memcpy(buf, wtf8.c_str(), n<maxlen?n:maxlen);
   if (buf[n-2] != '\\') {
      // Add a trailing '\'
      if (n+1 > maxlen) {
         return NULL;
      }
      buf[n-1] = '\\';
      buf[n] = 0;
   }
   return buf;
}

int
win32_fputs(const char *string, FILE *stream)
{
   /* we use WriteConsoleA / WriteConsoleA
      so we can be sure that unicode support works on win32.
      with fallback if something fails
   */

   HANDLE hOut = GetStdHandle (STD_OUTPUT_HANDLE);
   if (hOut && (hOut != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte &&
       p_MultiByteToWideChar && (stream == stdout)) {

      POOLMEM* pwszBuf = get_pool_memory(PM_MESSAGE);

      DWORD dwCharsWritten;
      DWORD dwChars;

      dwChars = UTF8_2_wchar(&pwszBuf, string);

      /* try WriteConsoleW */
      if (WriteConsoleW (hOut, pwszBuf, dwChars-1, &dwCharsWritten, NULL)) {
         free_pool_memory(pwszBuf);
         return dwCharsWritten;
      }

      /* convert to local codepage and try WriteConsoleA */
      POOLMEM* pszBuf = get_pool_memory(PM_MESSAGE);
      pszBuf = check_pool_memory_size(pszBuf, dwChars+1);

      dwChars = p_WideCharToMultiByte(GetConsoleOutputCP(),0,(LPCWSTR)pwszBuf,-1,pszBuf,dwChars,NULL,NULL);
      free_pool_memory(pwszBuf);

      if (WriteConsoleA (hOut, pszBuf, dwChars-1, &dwCharsWritten, NULL)) {
         free_pool_memory(pszBuf);
         return dwCharsWritten;
      }
      free_pool_memory(pszBuf);
   }
   /* Fall back */
   return fputs(string, stream);
}

char*
win32_cgets (char* buffer, int len)
{
   /* we use console ReadConsoleA / ReadConsoleW to be able to read unicode
      from the win32 console and fallback if something fails */

   HANDLE hIn = GetStdHandle (STD_INPUT_HANDLE);
   if (hIn && (hIn != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte && p_MultiByteToWideChar) {
      DWORD dwRead;
      wchar_t wszBuf[1024];
      char  szBuf[1024];

      /* nt and unicode conversion */
      if (ReadConsoleW (hIn, wszBuf, 1024, &dwRead, NULL)) {

         /* null terminate at end */
         if (wszBuf[dwRead-1] == L'\n') {
            wszBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         if (wszBuf[dwRead-1] == L'\r') {
            wszBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         wchar_2_UTF8(buffer, wszBuf, len);
         return buffer;
      }

      /* win 9x and unicode conversion */
      if (ReadConsoleA (hIn, szBuf, 1024, &dwRead, NULL)) {

         /* null terminate at end */
         if (szBuf[dwRead-1] == L'\n') {
            szBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         if (szBuf[dwRead-1] == L'\r') {
            szBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         /* convert from ansii to wchar_t */
         p_MultiByteToWideChar(GetConsoleCP(), 0, szBuf, -1, wszBuf,1024);
         /* convert from wchar_t to UTF-8 */
         if (wchar_2_UTF8(buffer, wszBuf, len))
            return buffer;
      }
   }

   /* fallback */
   if (fgets(buffer, len, stdin))
      return buffer;
   else
      return NULL;
}

int
win32_unlink(const char *filename)
{
   int nRetCode;
   if (p_wunlink) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, filename);

      nRetCode = _wunlink((LPCWSTR) pwszBuf);

      /*
       * special case if file is readonly,
       * we retry but unset attribute before
       */
      if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesW && p_GetFileAttributesW) {
         DWORD dwAttr =  p_GetFileAttributesW((LPCWSTR)pwszBuf);
         if (dwAttr != INVALID_FILE_ATTRIBUTES) {
            if (p_SetFileAttributesW((LPCWSTR)pwszBuf, dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
               nRetCode = _wunlink((LPCWSTR) pwszBuf);
               /* reset to original if it didn't help */
               if (nRetCode == -1)
                  p_SetFileAttributesW((LPCWSTR)pwszBuf, dwAttr);
            }
         }
      }
      free_pool_memory(pwszBuf);
   } else {
      nRetCode = _unlink(filename);

      /* special case if file is readonly,
      we retry but unset attribute before */
      if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesA && p_GetFileAttributesA) {
         DWORD dwAttr =  p_GetFileAttributesA(filename);
         if (dwAttr != INVALID_FILE_ATTRIBUTES) {
            if (p_SetFileAttributesA(filename, dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
               nRetCode = _unlink(filename);
               /* reset to original if it didn't help */
               if (nRetCode == -1)
                  p_SetFileAttributesA(filename, dwAttr);
            }
         }
      }
   }
   return nRetCode;
}


#include "mswinver.h"

char WIN_VERSION_LONG[64];
char WIN_VERSION[32];
char WIN_RAWVERSION[32];

class winver {
public:
    winver(void);
};

static winver INIT;                     // cause constructor to be called before main()


winver::winver(void)
{
    const char *version = "";
    const char *platform = "";
    OSVERSIONINFO osvinfo;
    osvinfo.dwOSVersionInfoSize = sizeof(osvinfo);

    // Get the current OS version
    if (!GetVersionEx(&osvinfo)) {
        version = "Unknown";
        platform = "Unknown";
    }
        const int ver = _mkversion(osvinfo.dwPlatformId,
                                   osvinfo.dwMajorVersion,
                                   osvinfo.dwMinorVersion);
        snprintf(WIN_RAWVERSION, sizeof(WIN_RAWVERSION), "Windows %#08x", ver);
        switch (ver)
        {
        case MS_WINDOWS_95: (version =  "Windows 95"); break;
        case MS_WINDOWS_98: (version =  "Windows 98"); break;
        case MS_WINDOWS_ME: (version =  "Windows ME"); break;
        case MS_WINDOWS_NT4:(version =  "Windows NT 4.0"); platform = "NT"; break;
        case MS_WINDOWS_2K: (version =  "Windows 2000");platform = "NT"; break;
        case MS_WINDOWS_XP: (version =  "Windows XP");platform = "NT"; break;
        case MS_WINDOWS_S2003: (version =  "Windows Server 2003");platform = "NT"; break;
        default: version = WIN_RAWVERSION; break;
        }

    bstrncpy(WIN_VERSION_LONG, version, sizeof(WIN_VERSION_LONG));
    snprintf(WIN_VERSION, sizeof(WIN_VERSION), "%s %lu.%lu.%lu",
             platform, osvinfo.dwMajorVersion, osvinfo.dwMinorVersion, osvinfo.dwBuildNumber);

#if 0
    HANDLE h = CreateFile("G:\\foobar", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
#endif
#if 0
    BPIPE *b = open_bpipe("ls -l", 10, "r");
    char buf[1024];
    while (!feof(b->rfd)) {
        fgets(buf, sizeof(buf), b->rfd);
    }
    close_bpipe(b);
#endif
}

BOOL CreateChildProcess(VOID);
VOID WriteToPipe(VOID);
VOID ReadFromPipe(VOID);
VOID ErrorExit(LPCSTR);
VOID ErrMsg(LPTSTR, BOOL);

/**
 * Check for a quoted path,  if an absolute path name is given and it contains
 * spaces it will need to be quoted.  i.e.  "c:/Program Files/foo/bar.exe"
 * CreateProcess() says the best way to ensure proper results with executables
 * with spaces in path or filename is to quote the string.
 */
const char *
getArgv0(const char *cmdline)
{

    int inquote = 0;
    const char *cp;
    for (cp = cmdline; *cp; cp++)
    {
        if (*cp == '"') {
            inquote = !inquote;
        }
        if (!inquote && isspace(*cp))
            break;
    }


    int len = cp - cmdline;
    char *rval = (char *)malloc(len+1);

    cp = cmdline;
    char *rp = rval;

    while (len--)
        *rp++ = *cp++;

    *rp = 0;
    return rval;
}

/*
 * Extracts the executable or script name from the first string in
 * cmdline.
 *
 * If the name contains blanks then it must be quoted with double quotes,
 * otherwise quotes are optional.  If the name contains blanks then it
 * will be converted to a short name.
 *
 * The optional quotes will be removed.  The result is copied to a malloc'ed
 * buffer and returned through the pexe argument.  The pargs parameter is set
 * to the address of the character in cmdline located after the name.
 *
 * The malloc'ed buffer returned in *pexe must be freed by the caller.
 */
bool
GetApplicationName(const char *cmdline, char **pexe, const char **pargs)
{
   const char *pExeStart = NULL;    /* Start of executable name in cmdline */
   const char *pExeEnd = NULL;      /* Character after executable name (separator) */

   const char *pBasename = NULL;    /* Character after last path separator */
   const char *pExtension = NULL;   /* Period at start of extension */

   const char *current = cmdline;

   bool bQuoted = false;

   /* Skip initial whitespace */

   while (*current == ' ' || *current == '\t')
   {
      current++;
   }

   /* Calculate start of name and determine if quoted */

   if (*current == '"') {
      pExeStart = ++current;
      bQuoted = true;
   } else {
      pExeStart = current;
      bQuoted = false;
   }

   *pargs = NULL;
   *pexe = NULL;

   /*
    * Scan command line looking for path separators (/ and \\) and the
    * terminator, either a quote or a blank.  The location of the
    * extension is also noted.
    */

   for ( ; *current != '\0'; current++)
   {
      if (*current == '.') {
         pExtension = current;
      } else if (IsPathSeparator(*current) && current[1] != '\0') {
         pBasename = &current[1];
         pExtension = NULL;
      }

      /* Check for terminator, either quote or blank */
      if (bQuoted) {
         if (*current != '"') {
            continue;
         }
      } else {
         if (*current != ' ') {
            continue;
         }
      }

      /*
       * Hit terminator, remember end of name (address of terminator) and
       * start of arguments
       */
      pExeEnd = current;

      if (bQuoted && *current == '"') {
         *pargs = &current[1];
      } else {
         *pargs = current;
      }

      break;
   }

   if (pBasename == NULL) {
      pBasename = pExeStart;
   }

   if (pExeEnd == NULL) {
      pExeEnd = current;
   }

   if (*pargs == NULL)
   {
      *pargs = current;
   }

   bool bHasPathSeparators = pExeStart != pBasename;

   /* We have pointers to all the useful parts of the name */

   /* Default extensions in the order cmd.exe uses to search */

   static const char ExtensionList[][5] = { ".com", ".exe", ".bat", ".cmd" };
   DWORD dwBasePathLength = pExeEnd - pExeStart;

   DWORD dwAltNameLength = 0;
   char *pPathname = (char *)alloca(MAX_PATHLENGTH + 1);
   char *pAltPathname = (char *)alloca(MAX_PATHLENGTH + 1);

   pPathname[MAX_PATHLENGTH] = '\0';
   pAltPathname[MAX_PATHLENGTH] = '\0';

   memcpy(pPathname, pExeStart, dwBasePathLength);
   pPathname[dwBasePathLength] = '\0';

   if (pExtension == NULL) {
      /* Try appending extensions */
      for (int index = 0; index < (int)(sizeof(ExtensionList) / sizeof(ExtensionList[0])); index++) {

         if (!bHasPathSeparators) {
            /* There are no path separators, search in the standard locations */
            dwAltNameLength = SearchPathA(NULL, pPathname, ExtensionList[index], MAX_PATHLENGTH, pAltPathname, NULL);
            if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
               memcpy(pPathname, pAltPathname, dwAltNameLength);
               pPathname[dwAltNameLength] = '\0';
               break;
            }
         } else {
            bstrncpy(&pPathname[dwBasePathLength], ExtensionList[index], MAX_PATHLENGTH - dwBasePathLength);
            if (GetFileAttributesA(pPathname) != INVALID_FILE_ATTRIBUTES) {
               break;
            }
            pPathname[dwBasePathLength] = '\0';
         }
      }
   } else if (!bHasPathSeparators) {
      /* There are no path separators, search in the standard locations */
      dwAltNameLength = SearchPathA(NULL, pPathname, NULL, MAX_PATHLENGTH, pAltPathname, NULL);
      if (dwAltNameLength > 0 && dwAltNameLength < MAX_PATHLENGTH) {
         memcpy(pPathname, pAltPathname, dwAltNameLength);
         pPathname[dwAltNameLength] = '\0';
      }
   }

   if (strchr(pPathname, ' ') != NULL) {
      dwAltNameLength = GetShortPathNameA(pPathname, pAltPathname, MAX_PATHLENGTH);

      if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
         *pexe = (char *)malloc(dwAltNameLength + 1);
         if (*pexe == NULL) {
            return false;
         }
         memcpy(*pexe, pAltPathname, dwAltNameLength + 1);
      }
   }

   if (*pexe == NULL) {
      DWORD dwPathnameLength = strlen(pPathname);
      *pexe = (char *)malloc(dwPathnameLength + 1);
      if (*pexe == NULL) {
         return false;
      }
      memcpy(*pexe, pPathname, dwPathnameLength + 1);
   }

   return true;
}

/**
 * Create the process with WCHAR API
 */
static BOOL
CreateChildProcessW(const char *comspec, const char *cmdLine,
                    PROCESS_INFORMATION *hProcInfo,
                    HANDLE in, HANDLE out, HANDLE err)
{
   STARTUPINFOW siStartInfo;
   BOOL bFuncRetn = FALSE;

   // Set up members of the STARTUPINFO structure.
   ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
   siStartInfo.cb = sizeof(siStartInfo);
   // setup new process to use supplied handles for stdin,stdout,stderr

   siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
   siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

   siStartInfo.hStdInput = in;
   siStartInfo.hStdOutput = out;
   siStartInfo.hStdError = err;

   // Convert argument to WCHAR
   POOLMEM *cmdLine_wchar = get_pool_memory(PM_FNAME);
   POOLMEM *comspec_wchar = get_pool_memory(PM_FNAME);

   UTF8_2_wchar(&cmdLine_wchar, cmdLine);
   UTF8_2_wchar(&comspec_wchar, comspec);

   // Create the child process.
   Dmsg2(dbglvl, "Calling CreateProcess(%s, %s, ...)\n", comspec_wchar, cmdLine_wchar);

   // try to execute program
   bFuncRetn = p_CreateProcessW((WCHAR*)comspec_wchar,
                                (WCHAR*)cmdLine_wchar,// command line
                                NULL,      // process security attributes
                                NULL,      // primary thread security attributes
                                TRUE,      // handles are inherited
                                0,         // creation flags
                                NULL,      // use parent's environment
                                NULL,      // use parent's current directory
                                &siStartInfo,  // STARTUPINFO pointer
                                hProcInfo);   // receives PROCESS_INFORMATION
   free_pool_memory(cmdLine_wchar);
   free_pool_memory(comspec_wchar);

   return bFuncRetn;
}


/**
 * Create the process with ANSI API
 */
static BOOL
CreateChildProcessA(const char *comspec, char *cmdLine,
                    PROCESS_INFORMATION *hProcInfo,
                    HANDLE in, HANDLE out, HANDLE err)
{
   STARTUPINFOA siStartInfo;
   BOOL bFuncRetn = FALSE;

   // Set up members of the STARTUPINFO structure.
   ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
   siStartInfo.cb = sizeof(siStartInfo);
   // setup new process to use supplied handles for stdin,stdout,stderr
   siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
   siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

   siStartInfo.hStdInput = in;
   siStartInfo.hStdOutput = out;
   siStartInfo.hStdError = err;

   // Create the child process.
   Dmsg2(dbglvl, "Calling CreateProcess(%s, %s, ...)\n", comspec, cmdLine);

   // try to execute program
   bFuncRetn = p_CreateProcessA(comspec,
                                cmdLine,  // command line
                                NULL,     // process security attributes
                                NULL,     // primary thread security attributes
                                TRUE,     // handles are inherited
                                0,        // creation flags
                                NULL,     // use parent's environment
                                NULL,     // use parent's current directory
                                &siStartInfo,// STARTUPINFO pointer
                                hProcInfo);// receives PROCESS_INFORMATION
   return bFuncRetn;
}

/**
 * OK, so it would seem CreateProcess only handles true executables:
 * .com or .exe files.  So grab $COMSPEC value and pass command line to it.
 */
HANDLE
CreateChildProcess(const char *cmdline, HANDLE in, HANDLE out, HANDLE err)
{
   static const char *comspec = NULL;
   PROCESS_INFORMATION piProcInfo;
   BOOL bFuncRetn = FALSE;

   if (!p_CreateProcessA || !p_CreateProcessW)
      return INVALID_HANDLE_VALUE;

   if (comspec == NULL)
      comspec = getenv("COMSPEC");
   if (comspec == NULL) // should never happen
      return INVALID_HANDLE_VALUE;

   // Set up members of the PROCESS_INFORMATION structure.
   ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

   // if supplied handles are not used the send a copy of our STD_HANDLE
   // as appropriate
   if (in == INVALID_HANDLE_VALUE)
      in = GetStdHandle(STD_INPUT_HANDLE);

   if (out == INVALID_HANDLE_VALUE)
      out = GetStdHandle(STD_OUTPUT_HANDLE);

   if (err == INVALID_HANDLE_VALUE)
      err = GetStdHandle(STD_ERROR_HANDLE);

   char *exeFile;
   const char *argStart;

   if (!GetApplicationName(cmdline, &exeFile, &argStart)) {
      return INVALID_HANDLE_VALUE;
   }

   POOL_MEM cmdLine(PM_FNAME);
   Mmsg(cmdLine, "%s /c \"%s\"%s", comspec, exeFile, argStart);

   free(exeFile);

   // New function disabled
   if (p_CreateProcessW && p_MultiByteToWideChar) {
      bFuncRetn = CreateChildProcessW(comspec, cmdLine.c_str(), &piProcInfo,
                                      in, out, err);
   } else {
      bFuncRetn = CreateChildProcessA(comspec, cmdLine.c_str(), &piProcInfo,
                                      in, out, err);
   }

   if (bFuncRetn == 0) {
      ErrorExit("CreateProcess failed\n");
      Dmsg2(dbglvl, "  CreateProcess(%s, %s) failed\n",comspec,cmdLine.c_str());
      return INVALID_HANDLE_VALUE;
   }
   // we don't need a handle on the process primary thread so we close
   // this now.
   CloseHandle(piProcInfo.hThread);
   return piProcInfo.hProcess;
}

void
ErrorExit (LPCSTR lpszMessage)
{
    const char *err = errorString();
    Dmsg2(dbglvl, "%s: %s", lpszMessage, err);
    LocalFree((void *)err);
    errno = b_errno_win32;
}


/*
typedef struct s_bpipe {
   pid_t worker_pid;
   time_t worker_stime;
   int wait;
   btimer_t *timer_id;
   FILE *rfd;
   FILE *wfd;
} BPIPE;
*/

static void
CloseHandleIfValid(HANDLE handle)
{
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
}

#define MODE_READ 1
#define MODE_WRITE 2
#define MODE_SHELL 4
#define MODE_STDERR 8

BPIPE *
open_bpipe(char *prog, int wait, const char *mode, char *envp[])
{
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup,
        hChildStdoutRd, hChildStdoutWr, hChildStdoutRdDup,
        hChildStderrRd, hChildStderrWr, hChildStderrRdDup,
        hInputFile;

    SECURITY_ATTRIBUTES saAttr;

    BOOL fSuccess;

    hChildStdinRd = hChildStdinWr = hChildStdinWrDup =
        hChildStdoutRd = hChildStdoutWr = hChildStdoutRdDup =
        hChildStderrRd = hChildStderrWr = hChildStderrRdDup =
        hInputFile = INVALID_HANDLE_VALUE;

    BPIPE *bpipe = (BPIPE *)malloc(sizeof(BPIPE));
    memset((void *)bpipe, 0, sizeof(BPIPE));

    int mode_map(0);
    if (strchr(mode,'r')) mode_map|=MODE_READ;
    if (strchr(mode,'w')) mode_map|=MODE_WRITE;
    if (strchr(mode,'s')) mode_map|=MODE_SHELL;
    if (strchr(mode,'e')) mode_map|=MODE_STDERR;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (mode_map & MODE_READ) {

        // Create a pipe for the child process's STDOUT.
        if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
            ErrorExit("Stdout pipe creation failed\n");
            goto cleanup;
        }
        // Create noninheritable read handle and close the inheritable read
        // handle.

        fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
                                   GetCurrentProcess(), &hChildStdoutRdDup , 0,
                                   FALSE,
                                   DUPLICATE_SAME_ACCESS);
        if ( !fSuccess ) {
            ErrorExit("DuplicateHandle failed");
            goto cleanup;
        }

        CloseHandle(hChildStdoutRd);
        hChildStdoutRd = INVALID_HANDLE_VALUE;
    }

    if (mode_map & MODE_STDERR) {

        // Create a pipe for the child process's STDOUT.
        if (! CreatePipe(&hChildStderrRd, &hChildStderrWr, &saAttr, 0)) {
            ErrorExit("Stderr pipe creation failed\n");
            goto cleanup;
        }
        // Create noninheritable read handle and close the inheritable read
        // handle.

        fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStderrRd,
                                   GetCurrentProcess(), &hChildStderrRdDup , 0,
                                   FALSE,
                                   DUPLICATE_SAME_ACCESS);
        if ( !fSuccess ) {
            ErrorExit("DuplicateHandle failed");
            goto cleanup;
        }

        CloseHandle(hChildStderrRd);
        hChildStderrRd = INVALID_HANDLE_VALUE;
    }

    if (mode_map & MODE_WRITE) {

        // Create a pipe for the child process's STDIN.

        if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
            ErrorExit("Stdin pipe creation failed\n");
            goto cleanup;
        }

        // Duplicate the write handle to the pipe so it is not inherited.
        fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWr,
                                   GetCurrentProcess(), &hChildStdinWrDup,
                                   0,
                                   FALSE,                  // not inherited
                                   DUPLICATE_SAME_ACCESS);
        if (!fSuccess) {
            ErrorExit("DuplicateHandle failed");
            goto cleanup;
        }

        CloseHandle(hChildStdinWr);
        hChildStdinWr = INVALID_HANDLE_VALUE;
    }
    // spawn program with redirected handles as appropriate
    bpipe->worker_pid = (pid_t)
        CreateChildProcess(prog,             // commandline
                           hChildStdinRd,    // stdin HANDLE
                           hChildStdoutWr,   // stdout HANDLE
                           (mode_map & MODE_STDERR) ? hChildStderrWr:hChildStdoutWr);  // stderr HANDLE

    if ((HANDLE) bpipe->worker_pid == INVALID_HANDLE_VALUE) {
       ErrorExit("CreateChildProcess failed");
       goto cleanup;
    }

    bpipe->wait = wait;
    bpipe->worker_stime = time(NULL);

    if (mode_map & MODE_READ) {
        CloseHandle(hChildStdoutWr); // close our write side so when
                                     // process terminates we can
                                     // detect eof.
        // ugly but convert WIN32 HANDLE to FILE*
        int rfd = _open_osfhandle((intptr_t)hChildStdoutRdDup, O_RDONLY | O_BINARY);
        if (rfd >= 0) {
           bpipe->rfd = _fdopen(rfd, "rb");
        }
    }
    if (mode_map & MODE_STDERR) {
        CloseHandle(hChildStderrWr); // close our write side so when
                                     // process terminates we can
                                     // detect eof.
        // ugly but convert WIN32 HANDLE to FILE*
        int rfd = _open_osfhandle((intptr_t)hChildStderrRdDup, O_RDONLY | O_BINARY);
        if (rfd >= 0) {
           bpipe->efd = _fdopen(rfd, "rb");
        }
    }
    if (mode_map & MODE_WRITE) {
        CloseHandle(hChildStdinRd); // close our read side so as not
                                    // to interfre with child's copy
        // ugly but convert WIN32 HANDLE to FILE*
        int wfd = _open_osfhandle((intptr_t)hChildStdinWrDup, O_WRONLY | O_BINARY);
        if (wfd >= 0) {
           bpipe->wfd = _fdopen(wfd, "wb");
        }
    }

    if (wait > 0) {
        bpipe->timer_id = start_child_timer(NULL, bpipe->worker_pid, wait);
    }

    return bpipe;

cleanup:

    CloseHandleIfValid(hChildStdoutWr);
    CloseHandleIfValid(hChildStdoutRd);
    CloseHandleIfValid(hChildStdoutRdDup);
    CloseHandleIfValid(hChildStderrWr);
    CloseHandleIfValid(hChildStderrRd);
    CloseHandleIfValid(hChildStderrRdDup);
    CloseHandleIfValid(hChildStdinWr);
    CloseHandleIfValid(hChildStdinRd);
    CloseHandleIfValid(hChildStdinWrDup);

    free((void *)bpipe);
    errno = b_errno_win32;            /* do GetLastError() for error code */
    return NULL;
}


int
kill(pid_t pid, int signal)
{
   int rval = 0;
   if (!TerminateProcess((HANDLE)pid, (UINT)signal)) {
      rval = -1;
      errno = b_errno_win32;
   }
   CloseHandle((HANDLE)pid);
   return rval;
}


int
close_bpipe(BPIPE *bpipe)
{
   int rval = 0;
   int32_t remaining_wait = bpipe->wait;

   /* Close pipes */
   if (bpipe->rfd) {
      fclose(bpipe->rfd);
      bpipe->rfd = NULL;
   }
   if (bpipe->efd) {
      fclose(bpipe->efd);
      bpipe->efd = NULL;
   }
   if (bpipe->wfd) {
      fclose(bpipe->wfd);
      bpipe->wfd = NULL;
   }

   if (remaining_wait == 0) {         /* wait indefinitely */
      remaining_wait = INT32_MAX;
   }
   for ( ;; ) {
      DWORD exitCode;
      if (!GetExitCodeProcess((HANDLE)bpipe->worker_pid, &exitCode)) {
         const char *err = errorString();
         rval = b_errno_win32;
         Dmsg1(dbglvl, "GetExitCode error %s\n", err);
         LocalFree((void *)err);
         break;
      }
      if (exitCode == STILL_ACTIVE) {
         if (remaining_wait <= 0) {
            rval = ETIME;             /* timed out */
            break;
         }
         bmicrosleep(1, 0);           /* wait one second */
         remaining_wait--;
      } else if (exitCode != 0) {
         /* Truncate exit code as it doesn't seem to be correct */
         rval = (exitCode & 0xFF) | b_errno_exit;
         break;
      } else {
         break;                       /* Shouldn't get here */
      }
   }

   if (bpipe->timer_id) {
       stop_child_timer(bpipe->timer_id);
   }
   if (bpipe->rfd) fclose(bpipe->rfd);
   if (bpipe->efd) fclose(bpipe->efd);
   if (bpipe->wfd) fclose(bpipe->wfd);
   free((void *)bpipe);
   return rval;
}

int
close_wpipe(BPIPE *bpipe)
{
    int result = 1;

    if (bpipe->wfd) {
        fflush(bpipe->wfd);
        if (fclose(bpipe->wfd) != 0) {
            result = 0;
        }
        bpipe->wfd = NULL;
    }
    return result;
}

/* Close the stderror pipe only */
int close_epipe(BPIPE *bpipe)
{
   int stat = 1;

   if (bpipe->efd) {
      if (fclose(bpipe->efd) != 0) {
         stat = 0;
      }
      bpipe->efd = NULL;
   }
   return stat;
}

#ifndef MINGW64
int
utime(const char *fname, struct utimbuf *times)
{
    FILETIME acc, mod;
    char tmpbuf[5000];
    POOL_MEM pwszBuf(PM_FNAME);

    cvt_utime_to_ftime(times->actime, acc);
    cvt_utime_to_ftime(times->modtime, mod);

    make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), fname);

    HANDLE h = p_CreateFileW((LPCWSTR)pwszBuf.c_str(),
                     FILE_WRITE_ATTRIBUTES,
                     FILE_SHARE_WRITE|FILE_SHARE_READ|FILE_SHARE_DELETE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS, // required for directories
                     NULL);

    if (h == INVALID_HANDLE_VALUE) {
       const char *err = errorString();
       Dmsg2(dbglvl, "Cannot open file \"%s\" for utime(): ERR=%s", tmpbuf, err);
       LocalFree((void *)err);
       errno = b_errno_win32;
       return -1;
    }

    int rval = SetFileTime(h, NULL, &acc, &mod) ? 0 : -1;
    CloseHandle(h);
    if (rval == -1) {
       errno = b_errno_win32;
    }
    return rval;
}
#endif

#if 0
int
file_open(const char *file, int flags, int mode)
{
   DWORD access = 0;
   DWORD shareMode = 0;
   DWORD create = 0;
   DWORD msflags = 0;
   HANDLE foo = INVALID_HANDLE_VALUE;
   const char *remap = file;

   if (flags & O_WRONLY) access = GENERIC_WRITE;
   else if (flags & O_RDWR) access = GENERIC_READ|GENERIC_WRITE;
   else access = GENERIC_READ;

   if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
      create = CREATE_NEW;
   else if ((flags & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))
      create = CREATE_ALWAYS;
   else if (flags & O_CREAT)
      create = OPEN_ALWAYS;
   else if (flags & O_TRUNC)
      create = TRUNCATE_EXISTING;
   else
      create = OPEN_EXISTING;

   shareMode = 0;

   if (flags & O_APPEND) {
      printf("open...APPEND not implemented yet.");
      exit(-1);
   }

   POOL_MEM pwszBuf(PM_FNAME);
   make_win32_path_UTF8_2_wchar(&pwszBuf.addr(), file);
   foo = p_CreateFileW((LPCWSTR) pwszBuf.c_str(), access, shareMode, NULL, create, msflags, NULL);

   if (INVALID_HANDLE_VALUE == foo) {
      errno = b_errno_win32;
      return (int)-1;
   }
   return (int)foo;

}


int
file_close(int fd)
{
    if (!CloseHandle((HANDLE)fd)) {
        errno = b_errno_win32;
        return -1;
    }

    return 0;
}

ssize_t
file_write(int fd, const void *data, ssize_t len)
{
    BOOL status;
    DWORD bwrite;
    status = WriteFile((HANDLE)fd, data, len, &bwrite, NULL);
    if (status) return bwrite;
    errno = b_errno_win32;
    return -1;
}


ssize_t
file_read(int fd, void *data, ssize_t len)
{
    BOOL status;
    DWORD bread;

    status = ReadFile((HANDLE)fd, data, len, &bread, NULL);
    if (status) return bread;
    errno = b_errno_win32;
    return -1;
}

boffset_t
file_seek(int fd, boffset_t offset, int whence)
{
    DWORD method = 0;
    DWORD val;
    LONG  offset_low = (LONG)offset;
    LONG  offset_high = (LONG)(offset >> 32);

    switch (whence) {
    case SEEK_SET :
        method = FILE_BEGIN;
        break;
    case SEEK_CUR:
        method = FILE_CURRENT;
        break;
    case SEEK_END:
        method = FILE_END;
        break;
    default:
        errno = EINVAL;
        return -1;
    }


    if ((val=SetFilePointer((HANDLE)fd, offset_low, &offset_high, method)) == INVALID_SET_FILE_POINTER) {
       errno = b_errno_win32;
       return -1;
    }
    /* ***FIXME*** I doubt this works right */
    return val;
}

int
file_dup2(int, int)
{
    errno = ENOSYS;
    return -1;
}
#endif

#ifdef xxx
/*
 * Emulation of mmap and unmmap for tokyo dbm
 */
void *mmap(void *start, size_t length, int prot, int flags,
           int fd, off_t offset)
{
   DWORD fm_access = 0;
   DWORD mv_access = 0;
   HANDLE h;
   HANDLE mv;

   if (length == 0) {
      return MAP_FAILED;
   }
   if (!fd) {
      return MAP_FAILED;
   }

   if (flags & PROT_WRITE) {
      fm_access |= PAGE_READWRITE;
   } else if (flags & PROT_READ) {
      fm_access |= PAGE_READONLY;
   }

   if (flags & PROT_READ) {
      mv_access |= FILE_MAP_READ;
   }
   if (flags & PROT_WRITE) {
      mv_access |= FILE_MAP_WRITE;
   }

   h = CreateFileMapping((HANDLE)_get_osfhandle (fd),
                         NULL /* security */,
                         fm_access,
                         0 /* MaximumSizeHigh */,
                         0 /* MaximumSizeLow */,
                         NULL /* name of the file mapping object */);

   if (!h || h == INVALID_HANDLE_VALUE) {
      return MAP_FAILED;
   }

   mv = MapViewOfFile(h, mv_access,
                      0 /* offset hi */,
                      0 /* offset lo */,
                      length);
   CloseHandle(h);

   if (!mv || mv == INVALID_HANDLE_VALUE) {
      return MAP_FAILED;
   }

   return (void *) mv;
}

int munmap(void *start, size_t length)
{
   if (!start) {
      return -1;
   }
   UnmapViewOfFile(start);
   return 0;
}
#endif

#ifdef HAVE_MINGW
/* syslog function, added by Nicolas Boichat */
void openlog(const char *ident, int option, int facility) {}
#endif //HAVE_MINGW

/* Log an error message */
void LogErrorMsg(const char *message)
{
   HANDLE eventHandler;
   const char *strings[2];

   /* Use the OS event logging to log the error */
   eventHandler = RegisterEventSource(NULL, "Bacula");

   strings[0] = _("\n\nBacula ERROR: ");
   strings[1] = message;

   if (eventHandler) {
      ReportEvent(eventHandler, EVENTLOG_ERROR_TYPE,
              0,                      /* category */
              0,                      /* ID */
              NULL,                   /* SID */
              2,                      /* Number of strings */
              0,                      /* raw data size */
              (const char **)strings, /* error strings */
              NULL);                  /* raw data */
      DeregisterEventSource(eventHandler);
   }
}

int mkstemp(char *t)
{
   char *filename = mktemp(t);
   if (filename == NULL) {
      return -1;
   }
   return open(filename, O_RDWR | O_CREAT, 0600);
}

void malloc_trim(int)
{
   if (p_EmptyWorkingSet) {
      HANDLE hProcess = GetCurrentProcess();
      if (!p_EmptyWorkingSet(hProcess)) {
         const char *err = errorString();
         Dmsg1(dbglvl, "EmptyWorkingSet() = %s\n", err);
         LocalFree((void *)err);
      }
      CloseHandle( hProcess );
   }
}

/*
 * get win32 memory information.
 *    when buf is NULL then it return the process WorkingSetSize (used as Unix heap) only.
 */
uint64_t get_memory_info(char *buf, int buflen)
{
   char ed1[50], ed2[50], ed3[50], ed4[50];
   uint64_t ret=0;
   HANDLE hProcess = GetCurrentProcess();
   PROCESS_MEMORY_COUNTERS pmc;

   if (!buf) {
      return ret;
   }
   if (buf){
      buf[0] = '\0';
   }
   if (p_GetProcessMemoryInfo) {
      if (p_GetProcessMemoryInfo( hProcess, &pmc, sizeof(pmc))) {
         if (buf){
            bsnprintf(buf, buflen,
                      "WorkingSetSize: %s QuotaPagedPoolUsage: %s QuotaNonPagedPoolUsage: %s PagefileUsage: %s",
                      edit_uint64_with_commas(pmc.WorkingSetSize, ed1),
                      edit_uint64_with_commas(pmc.QuotaPagedPoolUsage, ed2),
                      edit_uint64_with_commas(pmc.QuotaNonPagedPoolUsage, ed3),
                      edit_uint64_with_commas(pmc.PagefileUsage, ed4));
         }
         ret = pmc.WorkingSetSize;

      } else {
         const char *err = errorString();
         bsnprintf(buf, buflen, "%s", err);
         LocalFree((void *)err);
      }
   }

   CloseHandle( hProcess );
   return ret;
}

/* normalize path provided by the fileset File directive
 * "f:" -> "f:/" add a ':' to "relative" to drive path
 */
void win32_normalize_fileset_path(POOLMEM *&fname)
{
   if (isalpha(fname[0]) && fname[1] == ':' && fname[2] == '\0') {
      fname = check_pool_memory_size(fname, 3);
      fname[2]='/';
      fname[3]='\0';
   }
}

/*
int test_wchar(const wchar_t *st, const char *comment)
{
   int err=0;
   char ascii[4096];
   char hexa[4096];
   POOL_MEM name(PM_FNAME);
   POOL_MEM name2(PM_FNAME);

   name.strcpy("");
   name2.strcpy("");
   int owsl=wcslen(st);
   printf(" ----- %s -- %s ----- %s\n", asciidump((char*)st, 2*owsl, ascii, sizeof(ascii)), hexdump((char*)st, 2*owsl, hexa, sizeof(hexa)), comment);
   int l=wchar_path_2_wutf8(&name.addr(), st);
   int sl=strlen(name.c_str());
   printf(" \t=> len=%d(%d+1) -- %s -- %s -- \n", l, sl, asciidump(name.c_str(), l, ascii, sizeof(ascii)), hexdump(name.c_str(), l, hexa, sizeof(hexa)));
   int wl=wutf8_path_2_wchar(&name2.addr(), name.c_str());
   int wsl=wcslen((wchar_t *)name2.c_str());
   printf(" \t<= len=%d(%d+1) -- %s -- %s -- \n", wl, wsl, asciidump(name2.c_str(), wl, ascii, sizeof(ascii)), hexdump(name2.c_str(), wl, hexa, sizeof(hexa)));
   if (memcmp((wchar_t *)name2.c_str(), st, 2*(wcslen(st)+1))!=0 ||
         owsl != wsl || l!=sl+1) {
      err=1;
      printf(" \tERROR !!!!!!!!!!\n");
   } else {
      printf(" \tOK\n");
   }
   return err;
}

int test_all_wchar()
{
   printf("test_all_wchar\n");
   test_wchar(L"Hello world", "Hello world (valid)");
   test_wchar(L"Hello*world", "must escape * in the middle");
   test_wchar(L"*Hello**world*", "must escape * in the middle");
   test_wchar(L"Hello\x63dfworld", "Hello world with a strange char in the middle");
   test_wchar(L"Hello\xdf63world", "Hello world with invalid char in the middle");
   test_wchar((const wchar_t*)"\x41\x00\x63\xd9\x63\xdd\x42\x00\x00\x00", "A<valide surrogate>B");
   test_wchar((const wchar_t*)"\x41\x00\x63\xdf\x42\x00\x00\x00", "A<invalid single char>B");
   test_wchar((const wchar_t*)"\x41\x00\x63\xdc\x63\xdd\x42\x00\x00\x00", "A<w1 is out of 0xD800 and 0xDBFF>B");
   test_wchar((const wchar_t*)"\x41\x00\x63\xd8\x63\x63\x42\x00\x00\x00", "A<w2 is out of 0xDC00 and 0xDBFF but only w1 will be escaped>B");
   return 0;
}
*/
