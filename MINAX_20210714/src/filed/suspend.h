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
#ifndef _SUSPEND_H_
#define _SUSPEND_H_

#ifdef HAVE_DARWIN_OS
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

struct suspendres {
#ifdef HAVE_DARWIN_OS
   IOPMAssertionID assertionID;
#endif
   bool status;
};
typedef struct suspendres suspendres_t;

void allow_os_suspensions(suspendres_t&);
void prevent_os_suspensions(suspendres_t&);

#endif /* _SUSPEND_H_ */