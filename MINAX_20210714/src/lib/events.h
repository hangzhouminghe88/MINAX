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
/* Written by Eric Bollengier Apr 2020 */

#ifndef BEVENTS_H
#define BEVENTS_H

/* Events record -- same format as database */
class EVENTS_DBR: public SMARTALLOC {
public:
   EVENTS_DBR(): EventsId(0), EventsTime(0), EventsRef(0), EventsText(NULL), limit(100), order(0)
      {
         *EventsDaemon = *EventsType = *EventsSource = *EventsCode = *start = *end = 0;
      };

   ~EVENTS_DBR() { bfree_and_null(EventsText); };
   bool scan_line(const char *line);
   void debug() {
      Dmsg6(10, "daemon=%s ref=%lx code=[%s] type=%s source=%s text=%s", EventsDaemon, EventsRef, EventsCode, EventsType, EventsSource, EventsText);
   };
   
   DBId_t EventsId;
   btime_t EventsTime;                 /* Events time */
   uint64_t EventsRef;                 /* Reference used to track the message */
   char EventsDaemon[MAX_NAME_LENGTH];   /* Events from (daemon name)  */
   char EventsType[MAX_NAME_LENGTH];   /* Events type (security, audit, access, daemon)  */
   char EventsSource[MAX_NAME_LENGTH]; /* Events source (console, daemon name, etc...) */ 
   char EventsCode[MAX_NAME_LENGTH];   /* Code for the Event */
   char *EventsText;                   /* Events message */

   /* Extra stuff not in DB */
   int limit;                   /* limit the records to display */
   int order;                   /* ASC/DESC 0/1 */
   char start[MAX_TIME_LENGTH]; /* Search between start and end */
   char end[MAX_TIME_LENGTH];
};

void custom_type_copy(MSGS *dest, MSGS *src);

/* Get JSON array for custom types */
void edit_custom_type(POOLMEM **edbuf, MSGS *msgs, char *msg_types);

/* Event structure */
#define EVENTS_TYPE_SECURITY   "security"
#define EVENTS_TYPE_CONNECTION "connection"
#define EVENTS_TYPE_COMMAND    "command"
#define EVENTS_TYPE_DAEMON     "daemon"
#define EVENTS_TYPE_JOB        "job"
#define EVENTS_TYPE_VOLUME     "volume"

void events_send_msg(JCR *jcr, const char *code, const char *type, const char *source, intptr_t ref, const char *fmt, ...);
void events_send_msg(JCR *jcr, EVENTS_DBR *ev);

#endif  /* !BEVENTS_H */
