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

#ifndef VERSION_H
#define VERSION_H
#undef  VERSION

#define COMMUNITY 1      /* Define to create a Windows community binary */

/* Note: there can be only *one* VERSION in this file */
#define VERSION "11.0.1"
#define BDATE   "05 February 2020"
#define LSMDATE "05Feb20"

#define RELEASE 1   /* Use ONLY in rpms */

#define PROG_COPYRIGHT "Copyright (C) %d-2020 Kern Sibbald.\n"
#define BYEAR "2020"       /* year for copyright messages in progs */

/*
 * Versions of packages needed to build Bacula components
 */
#define DEPKGS_QT_VERSION  "30Nov17"
#define DEPKGS_VERSION     "16Oct19"
#define BQT4_VERSION       "5.9.3"
#define VIX_VERSION        "30Apr20"
#define JAVA_VERSION       "19Mar20"
#define NDMP_VERSION       "20Apr20"
#define EXTRAJS_VERSION    "20Apr20"
#define EXPAT_VERSION      "1.95.8"
#define ACSLS_VERSION      "20Mar18"

/* Normally included in the base depkgs */
#define LIBRSYNC_VERSION   "0.9.7b"
#define LIBLZO_VERSION     "2.06"
#define TOKYOCABINET_VERSION "1.4.48a"
#define MSSQL_VERSION      "18Sep19"
#define DOCKER_TAR_IMAGE   "19Aug19"
#define KUBERNETES_IMAGE_VERSION "29Oct19"
#define HADOOP_VERSION "07Feb20"

/* Debug flags */
#undef  DEBUG
#define DEBUG 1
#define TRACEBACK 1
#define TRACE_FILE 1
#define ENTER_LEAVE 1
//#define FORCE_ALIGNED 1

/* If this is set stdout will not be closed on startup */
/* #define DEVELOPER 1 */

/* adjust DEVELOPER_MODE for status command */
#ifdef DEVELOPER
# define DEVELOPER_MODE 1
#else
# define DEVELOPER_MODE 0
#endif

/*
 * SMCHECK does orphaned buffer checking (memory leaks)
 *  it can always be turned on, but has some minor performance
 *  penalties.
 */
#ifdef DEVELOPER
# define SMCHECK
#endif

/*
 * _USE_LOCKMGR does lock/unlock mutex tracking (dead lock)
 *   it can always be turned on, but we advise to use it only
 *   for debug
 */
# ifndef USE_LOCKMGR
#  define USE_LOCKMGR
# endif /* USE_LOCKMGR */
/*
 * Enable priority management with the lock manager
 *
 * Note, turning this on will cause the Bacula SD to abort if
 *  mutexes are executed out of order, which could lead to a
 *  deadlock.  However, note that this is not necessarily a
 *  deadlock, so turn this on only for debugging.
 */
#define USE_LOCKMGR_PRIORITY

/*
 * Enable thread verification before kill
 *
 * Note, this extra check have a high cost when using
 * dozens of thread, so turn this only for debugging.
 */
#define USE_LOCKMGR_SAFEKILL

#if !HAVE_LINUX_OS && !HAVE_SUN_OS && !HAVE_DARWIN_OS && !HAVE_FREEBSD_OS && !HAVE_KFREEBSD_OS
# undef USE_LOCKMGR
#endif

/*
 * USE_VTAPE is a dummy tape driver. This is useful to
 *  run regress test.
 */
#ifdef HAVE_LINUX_OS
# define USE_VTAPE
#endif

/*
 * USE_FTP is a ftp driver for the FD using curl.
 */
// #define USE_FTP

/*
 * for fastest speed but you must have a UPS to avoid unwanted shutdowns
 */
//#define SQLITE3_INIT_QUERY "PRAGMA synchronous = OFF"

/*
 * for more safety, but is 30 times slower than above
 */
#define SQLITE3_INIT_QUERY "PRAGMA synchronous = NORMAL"

/*
 * This should always be on. It enables data encryption code
 *  providing it is configured.
 */
#define DATA_ENCRYPTION 1

/*
 * This uses a Bacula specific bsnprintf rather than the sys lib
 *  version because it is much more secure. It should always be
 *  on.
 */
#define USE_BSNPRINTF 1

/* Debug flags not normally turned on */

/* #define TRACE_JCR_CHAIN 1 */
/* #define TRACE_RES 1 */
/* #define DEBUG_MEMSET 1 */
/* #define DEBUG_MUTEX 1 */
/* #define DEBUG_BLOCK_CHECKSUM 1 */

#define BEEF 0
#define BDEMO ""

/*
 * SD_VERSION history Enterprise
 * Note: Enterprise versions now numbered in 30000
 *       and community is at SD version 3
 *     None prior to 06Aug13
 *      1 06Aug13 - added comm line compression
 *      2 13Dec13 - added api version to status command
 *      3 22Feb14 - Added SD->SD with SD_Calls_Client
 *      4 22Jun14 - Added capabilities comm protocol
 *  30005 04Jun15 - Added JobMedia queueing
 *  30006 11Apr17 - Added PoolBytes, MaxPoolBytes and Recycle
 *  30007 06Feb20 - Added can_create to the Find media request
 *
 * Community:
 *    305 04Jun15 - Added JobMedia queueing
 *    306 20Mar15 - Added comm line compression
 *    307 06Feb20 - Added can_create to the Find media request
 *  30007 02Dec20 - Sync with Enterprise
 */

#ifdef COMMUNITY
#define SD_VERSION 30007   /* Community SD version */
#else
#define SD_VERSION 30007   /* Enterprise SD version */
#endif

/* FD_VERSION history Enterprise
 *   None prior to 10Mar08
 *   1 10Mar08
 *   2 13Mar09 - added the ability to restore from multiple storages
 *   3 03Sep10 - added the restore object command for vss plugin 4.0
 *   4 25Nov10 - added bandwidth command 5.1
 *   5 24Nov11 - added new restore object command format (pluginname) 6.0
 *   6 15Feb12 - added Component selection information list
 *   7 19Feb12 - added Expected files to restore
 *   8 22Mar13 - added restore options + version for SD
 *   9 06Aug13 - added comm line compression
 *  10 01Jan14 - added SD Calls Client and api version to status command
 *  11 O4May14 - added dedup aware FD
 *  12 22Jun14 - added new capabilities comm protocol with the SD
 *  13 04Feb15 - added snapshot protocol with the DIR
 *  14 06Sep17 - added send file list during restore
 *
 *  Community:
 * 213 04Feb15 - added snapshot protocol with the DIR
 * 214 20Mar17 - added comm line compression
 *  14 02Dec20 - Sync with Enterprise
 */

#ifdef COMMUNITY
#define FD_VERSION 14  /* make same as community Linux FD */
#else
#define FD_VERSION 14 /* Enterprise FD version */
#endif

/*
 * Set SMALLOC_SANITY_CHECK to zero to turn off, otherwise
 *  it is the maximum memory malloced before Bacula will
 *  abort.  Except for debug situations, this should be zero
 */
#define SMALLOC_SANITY_CHECK 0  /* 500000000  0.5 GB max */


/* Check if header of tape block is zero before writing */
/* #define DEBUG_BLOCK_ZEROING 1 */

/* #define FULL_DEBUG 1 */   /* normally on for testing only */

/* Turn this on ONLY if you want all Dmsg() to append to the
 *   trace file. Implemented mainly for Win32 ...
 */
/*  #define SEND_DMSG_TO_FILE 1 */


/* The following are turned on for performance testing */
/*
 * If you turn on the NO_ATTRIBUTES_TEST and rebuild, the SD
 *  will receive the attributes from the FD, will write them
 *  to disk, then when the data is written to tape, it will
 *  read back the attributes, but they will not be sent to
 *  the Director. So this will eliminate: 1. the comm time
 *  to send the attributes to the Director. 2. the time it
 *  takes the Director to put them in the catalog database.
 */
/* #define NO_ATTRIBUTES_TEST 1 */

/*
* If you turn on NO_TAPE_WRITE_TEST and rebuild, the SD
*  will do all normal actions, but will not write to the
*  Volume.  Note, this means a lot of functions such as
*  labeling will not work, so you must use it only when
*  Bacula is going to append to a Volume. This will eliminate
*  the time it takes to write to the Volume (not the time
*  it takes to do any positioning).
*/
/* #define NO_TAPE_WRITE_TEST 1 */

/*
 * If you turn on FD_NO_SEND_TEST and rebuild, the FD will
 *  not send any attributes or data to the SD. This will
 *  eliminate the comm time sending to the SD.
 */
/* #define FD_NO_SEND_TEST 1 */

#ifndef COMMUNITY
#include "bee_version.h"
#endif

#endif  /* VERSION_H */
