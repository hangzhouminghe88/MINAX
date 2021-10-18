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

#include "bacula.h"

/*****************************************************************/
/* Events management in Bacula */

/* Generate an Event/Audit message */
void events_send_msg(JCR *jcr, const char *code, const char *type, const char *source,
                     intptr_t ref, const char *fmt, ...)
{
   POOL_MEM tmp(PM_MESSAGE), tmp2(PM_MESSAGE), rbuf(PM_MESSAGE);
   va_list arg_ptr;

   pm_strcpy(tmp, source);
   bash_spaces(tmp);

   pm_strcpy(tmp2, my_name);
   bash_spaces(tmp2);

   /* TODO: Handle references with Date+Id rather than pointers */
   Mmsg(rbuf, "Events: code=%s daemon=%s ref=0x%p type=%s source=%s text=",
        code, tmp2.c_str(), ref, type, tmp.c_str());

   va_start(arg_ptr, fmt);
   bvsnprintf(tmp.c_str(), tmp.size(), fmt, arg_ptr);
   va_end(arg_ptr);

   pm_strcat(rbuf, tmp.c_str());
   Dmsg1(5, "%s\n", rbuf.c_str());

   MSGS *msgs = get_current_MSGS(jcr);
   int mtype = msgs->get_custom_type((char *)type);
   if (mtype < 0) {
      mtype = M_EVENTS;
   }
   Jmsg(jcr, mtype, 0, "%s\n", rbuf.c_str());
}

/* Generate an Event/Audit message */
void events_send_msg(JCR *jcr, EVENTS_DBR *ev)
{
   POOL_MEM rbuf(PM_MESSAGE);
   bash_spaces(ev->EventsSource);
   bash_spaces(ev->EventsDaemon);

   Mmsg(rbuf, "Events: code=%s daemon=%s ref=0x%p type=%s source=%s text=%s",
        ev->EventsCode, ev->EventsDaemon, ev->EventsRef, ev->EventsType,
        ev->EventsSource, ev->EventsText);

   MSGS *msgs = get_current_MSGS(jcr);
   int mtype = msgs->get_custom_type((char *)ev->EventsType);
   if (mtype < 0) {
      mtype = M_EVENTS;
   }

   Jmsg(jcr, mtype, 0, "%s\n", rbuf.c_str());
   
   unbash_spaces(ev->EventsSource);
   unbash_spaces(ev->EventsDaemon);
}

bool EVENTS_DBR::scan_line(const char *line)
{
   if (scan_string(line, "Events: code=%127s daemon=%127s ref=%llx type=%127s source=%127s text=",
                   EventsCode, EventsDaemon, &EventsRef, EventsType, EventsSource) != 5) {
      Dmsg1(0, "Malformed Audit message [%s]\n", line);
      return false;             /* invalid format */
   }
   unbash_spaces(EventsSource);
   unbash_spaces(EventsDaemon);
   EventsText = bstrdup(strstr(line, "text=") + 5);
   strip_trailing_junk(EventsText);
   return true;
}

/**********************************************************************
 * Custom Events code - Used to let the user control Events in Messages
 **********************************************************************
 *
 * A custom event can be generated with the .events bconsole command. It is
 * also possible to generate a new event from the code with the
 * events_send_msg() function. The argument "Type" is not checked and is free.
 *
 * .events type="bweb" source="myscript" ref=1 text="This is an event"
 * 
 * Users can create events as many as they want, but we have some limits for
 * the Messages filtering system. We can drive 32 message type, and 17 are used
 * for regular job messages (warning, skipped, saved, error, info, warning...).
 *
 * It is possible to increase the number of events by changing the code (int)
 * to a int64 for example.
 *
 */

/* Stored in the MSGS resource in custom_type rblist */
struct CUSTOM_TYPE {
   rblink link;
   int    code;
   char   kw[1];
};

/* RBLIST function to look a custom event name */
static int custom_type_lookup(void *a, void *b)
{
   const char  *s = (const char *)a;
   CUSTOM_TYPE *t = (CUSTOM_TYPE*)b;
   return strcasecmp(s, t->kw);
}

/* RBLIST function to insert a custom event name */
static int custom_type_insert(void *a, void *b)
{
   CUSTOM_TYPE *e1 = (CUSTOM_TYPE *)a;
   CUSTOM_TYPE *e2 = (CUSTOM_TYPE *)b;
   return strcasecmp(e1->kw, e2->kw);
}

/* MESSAGES resource is copied for each job. We need
 * to make sure the memory is not shared between instances
 */
void custom_type_copy(MSGS *dest, MSGS *src)
{
   CUSTOM_TYPE *elt=NULL;
   dest->custom_type_current_index = src->custom_type_current_index;
   if (src->custom_type) {
      dest->custom_type = New(rblist(elt, &elt->link));
      foreach_rblist(elt, src->custom_type) {
         CUSTOM_TYPE *elt2 = (CUSTOM_TYPE *)malloc(sizeof(CUSTOM_TYPE)+strlen(elt->kw)+1);
         elt2->code = elt->code;
         strcpy(elt2->kw, elt->kw);
         dest->custom_type->insert(elt2, custom_type_insert);
      }
   } else {
      dest->custom_type = NULL;
   }
}

/* Add a custom event type
 * > M_MAX => new custom type
 * M_DEBUG => ignored
 * -2      => incorrect format
 * -1      => too much element
 */
int MSGS::add_custom_type(bool is_not, char *type)
{
   CUSTOM_TYPE *t = NULL;
   if (!type || *type == 0) {
      return -2;                /* Incorrect format */
   }
   
   if (custom_type == NULL) {
      custom_type = New(rblist(t, &t->link));
   }

   if (custom_type_current_index >= (int)M_EVENTS_LIMIT) {
      return -1;                /* Too much elements */
   }

   int len = strlen(type);
   t = (CUSTOM_TYPE*) malloc(sizeof(CUSTOM_TYPE)+len+1);
   bstrncpy(t->kw, type, len+1);
   CUSTOM_TYPE *t2 = (CUSTOM_TYPE*) custom_type->insert(t, custom_type_insert);
   if (t2 == t) {
      custom_type_current_index = MAX(M_ALL, custom_type_current_index);
      t2->code = ++custom_type_current_index;
      Dmsg2(50, "Add custom type [Events.%s] = %d\n", t2->kw, t2->code);
   } else {
      free(t);                 /* Already in */
   }
   return t2->code;
}

/* Get an existing custom event name */
int MSGS::get_custom_type(char *type)
{
   CUSTOM_TYPE *t = NULL;
   if (custom_type == NULL) {
      return -1;
   }
   t = (CUSTOM_TYPE *)custom_type->search(type, custom_type_lookup);
   if (t) {
      return t->code;
   }
   return -1;
}

void edit_custom_type(POOLMEM **edbuf, MSGS *msgs, char *msg_types)
{
   CUSTOM_TYPE *elt;
   bool first_time = (*edbuf)[0] == '\0' || ((*edbuf)[0] == '[' && (*edbuf)[1] == '\0');

   if (msgs->custom_type == NULL) {
      return;
   }
   foreach_rblist(elt, msgs->custom_type) {
      if (bit_is_set(M_EVENTS, msg_types) == 0 &&
          bit_is_set(elt->code, msg_types))
      {
         if (!first_time) {
            pm_strcat(edbuf, ",");

         } else {
            first_time = false;
         }
         pm_strcat(edbuf, "\"Events.");
         pm_strcat(edbuf, elt->kw);
         pm_strcat(edbuf, "\"");

      } else if (bit_is_set(M_EVENTS, msg_types) &&
                 bit_is_set(elt->code, msg_types) == 0)
      {
         if (!first_time) {
            pm_strcat(edbuf, ",");
         } else {
            first_time = false;
         }
         pm_strcat(edbuf, "\"!Events.");
         pm_strcat(edbuf, elt->kw);
         pm_strcat(edbuf, "\"");
      }
   }
}
