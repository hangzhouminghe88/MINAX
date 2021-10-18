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
 * This is a Bacula File Daemon OS suspension routines.
 *
 * Author: Radosław Korzeniewski, MMXIX
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 */

#include "bacula.h"

#ifdef HAVE_WIN32
#include "compat.h"
#endif

#include "suspend.h"

/*
 * Don't allow OS to idle suspend while backup/restore is running.
 */
void prevent_os_suspensions(suspendres_t& suspend)
{
   suspend.status = false;
#ifdef HAVE_WIN32
/* Note, the OS automatically tracks these for each thread */
   SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
   suspend.status = true;
#endif
#ifdef HAVE_DARWIN_OS
   /* IOPMAssertionCreateWithName limits the string to 128 characters. */
   CFStringRef reasonForActivity = CFSTR("Bacula backup/restore job");

   if (kIOReturnSuccess == IOPMAssertionCreateWithName(
         kIOPMAssertionTypeNoIdleSleep,
         kIOPMAssertionLevelOn,
         reasonForActivity,
         &suspend.assertionID)){
      suspend.status = true;
   }
#endif
};

/*
 * Allow OS to idle suspend after job finish.
 */
void allow_os_suspensions(suspendres_t& suspend)
{
   if (suspend.status){
#ifdef HAVE_WIN32
      SetThreadExecutionState(ES_CONTINUOUS);
#endif
#ifdef HAVE_DARWIN_OS
      IOPMAssertionRelease(suspend.assertionID);
#endif
   }
};
