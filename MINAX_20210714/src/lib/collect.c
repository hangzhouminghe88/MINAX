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
 * Radosław Korzeniewski, MMXVIII
 * radoslaw@korzeniewski.net, radekk@inteos.pl
 * Inteos Sp. z o.o. http://www.inteos.pl/
 *
 * This is a Bacula statistics support utilities.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bacula.h"

/*
 * Scans a display format parameter and return appropriate enum value.
 *
 * in:
 *    buf - the command argument string to check
 * out:
 *    appropriate display_format_t value, defaults to COLLECT_SIMPLE
 */
display_format_t scandisplayformat(POOLMEM *buf)
{
   if (bstrcmp(buf, "json")){
      return COLLECT_JSON;
   }
   if (bstrcmp(buf, "full")){
      return COLLECT_FULL;
   }
   return COLLECT_SIMPLE;
};

/*
 * Render a simple representation of the metric into a buffer.
 *
 * in:
 *    out - a memory buffer where render to
 *    m - a bstatmetric object to render its value
 * out:
 *    rendered metric name and value in simple format at buf
 */
void rendermetricsimple(POOL_MEM &out, bstatmetric *m)
{
   POOL_MEM buf(PM_MESSAGE);

   m->render_metric_value(buf);
   Mmsg(out, "%s=%s\n", m->name, buf.c_str());
};

/*
 * Render a JSON representation of the metric into a buffer.
 *
 * in:
 *    out - a memory buffer where render to
 *    m - a bstatmetric object to render its value
 * out:
 *    rendered metric in JSON format at buf
 *
 * JSON example output implemented:
 * [
 *    {
 *       name: "bacula.jobs.all",
 *       value: 228,
 *       type: "Integer",
 *       unit: "Jobs",
 *       description: "Number of all jobs."
 *    },
 * ]
 * - the array brackets are delivered outside this function
 */
void rendermetricjson(POOL_MEM &out, bstatmetric *m, int nr)
{
   POOL_MEM buf(PM_MESSAGE);

   m->render_metric_value(buf, true);
   Mmsg(out, "%s  {\n    \"name\": \"%s\",\n    \"value\": %s,\n    \"type\": \"%s\",\n    \"unit\": \"%s\",\n    \"description\": \"%s\"\n  }",
         nr > 0 ? ",\n":"\n", m->name, buf.c_str(), m->metric_type_str(), m->metric_unit_str(), m->description);
};

/*
 * Render a full representation of the metric into a buffer.
 *
 * in:
 *    out - a memory buffer where render to
 *    m - a bstatmetric object to render its value
 * out:
 *    rendered metric at buf
 */
void rendermetricfull(POOL_MEM &out, bstatmetric *m)
{
   POOL_MEM buf(PM_MESSAGE);

   m->render_metric_value(buf);
   Mmsg(out, "name=\"%s\" value=%s type=%s unit=%s descr=\"%s\"\n", m->name, buf.c_str(), m->metric_type_str(),
            m->metric_unit_str(), m->description);
};

/*
 * Render metric into a buffer based on display format provided.
 *
 * in:
 *    out - a memory buffer where render to
 *    m - a bstatmetric object to render its value
 *    format - display format enum
 * out:
 *    rendered metric at buf
 */
void rendermetric(POOL_MEM &out, bstatmetric *m, display_format_t format, int nr)
{
   switch (format){
      case COLLECT_SIMPLE:
         rendermetricsimple(out, m);
         break;
      case COLLECT_JSON:
         rendermetricjson(out, m, nr);
         break;
      case COLLECT_FULL:
         rendermetricfull(out, m);
         break;
   }
};

/*
 * Return a string representation of the display format enum.
 */
const char *displayformat2str(display_format_t format)
{
   switch (format){
      case COLLECT_SIMPLE:
         return "simple";
      case COLLECT_JSON:
         return "json";
      case COLLECT_FULL:
         return "full";
      default:
         return "simple";
   }
};

/*
 * Return a string representation of the collector status.
 *
 * in:
 *    res_collector - a COLLECTOR resource for collector backend
 * out:
 *    string representation of the collector status
 */
const char *str_collector_status(COLLECTOR &res_collector)
{
   const char *status;

   if (res_collector.valid){
      status = res_collector.running?"running":"stopped";
   } else {
      status = res_collector.running?"waiting to exit":"stopped";
   }
   return status;
};

/*
 * Return a string representation of the collector spooling status.
 *
 * in:
 *    res_collector - a COLLECTOR resource for collector backend
 * out:
 *    string representation of the collector spooling status
 */
const char *str_collector_spooling(COLLECTOR &res_collector)
{
   const char *spool;

   if (res_collector.spool_directory){
      /* spooling defined */
      switch (res_collector.spooled){
         case BCOLLECT_SPOOL_YES:
            spool = "in progress";
            break;
         case BCOLLECT_SPOOL_DESPOOL:
            spool = "despooling now";
            break;
         case BCOLLECT_SPOOL_NO:
            spool = "enabled";
            break;
         default:
            spool = "unknown (enabled)";
      }
   } else {
      spool = "disabled";
   }
   return spool;
};

/*
 * A support function renders a Statistics resource status into a buffer.
 *
 * in:
 *    res_collector - a COLLECTOR resource class to display
 *    buf - a POLL_MEM buffer to render into
 * out:
 *    the length of rendered text
 */
int render_collector_status(COLLECTOR &res_collector, POOL_MEM &buf)
{
   const char *status, *spool;
   char dt[MAX_TIME_LENGTH];
   time_t t;
   utime_t i;
   int len;
   POOL_MEM errmsg(PM_MESSAGE);

   res_collector.lock();
   status = str_collector_status(res_collector);
   t = res_collector.timestamp;
   i = res_collector.interval;
   spool = str_collector_spooling(res_collector);
   if (res_collector.errmsg && strlen(res_collector.errmsg)){
      Mmsg(errmsg, " lasterror=%s\n", res_collector.errmsg);
   } else {
      pm_strcpy(errmsg, "");
   }
   res_collector.unlock();

   bstrftime_nc(dt, sizeof(dt), t);
   len = Mmsg(buf, "Statistics backend: %s is %s\n type=%i lasttimestamp=%s\n interval=%d secs\n spooling=%s\n%s\n",
         res_collector.hdr.name, status,
         res_collector.type, dt,
         i, spool,
         errmsg.c_str());
   return len;
};

/*
 * A support function renders a Statistics resource status into an OutputWriter for APIv2.
 *
 * in:
 *    res_collector - a COLLECTOR resource class to display
 *    ow - OutputWriter for apiv2
 * out:
 *    rendered status in OutputWritter buffer
 */
void api_render_collector_status(COLLECTOR &res_collector, OutputWriter &ow)
{
   const char *status, *spool;
   time_t t;
   utime_t i;

   res_collector.lock();
   status = str_collector_status(res_collector);
   t = res_collector.timestamp;
   i = res_collector.interval;
   spool = str_collector_spooling(res_collector);
   res_collector.unlock();
   ow.get_output(
         OT_START_OBJ,
         OT_STRING,  "name",           res_collector.hdr.name,
         OT_STRING,  "status",         status,
         OT_INT,     "interval",       i,
         OT_UTIME,   "lasttimestamp",  t,
         OT_STRING,  "spooling",       spool,
         OT_STRING,  "lasterror",      NPRTB(res_collector.errmsg),
         OT_END_OBJ,
         OT_END
   );
   return;
};

/*
 * Replace dot '.' character for "%32" to avoid metric level split
 */
char *replace_dot_metric_name(POOL_MEM &out, const char *name)
{
   char *p, *q;
   POOL_MEM tmp(PM_NAME);

   pm_strcpy(out, NULL);
   pm_strcpy(tmp, name);
   p = tmp.c_str();
   while((q = strchr(p, '.')) != NULL){
      /* q is the dot char and p is a start of the substring to copy */
      *q = 0;  // temporary terminate substring
      pm_strcat(out, p);
      pm_strcat(out, "%32");
      // printf(">%s<", out.c_str());
      p = q + 1;  // next substring
   }
   pm_strcat(out, p);
   return out.c_str();
};

/*
 * Return the metric name updated with Statistics Prefix parameter if defined.
 */
void render_metric_prefix(COLLECTOR *collector, POOL_MEM &buf, bstatmetric *item)
{
   POOL_MEM name(PM_NAME);

   if (collector && item){
      if (collector->mangle_name){
         replace_dot_metric_name(name, item->name);
      } else {
         Mmsg(name, "%s", item->name);
      }
      if (collector->prefix){
         /* $prefix.$metric */
         Mmsg(buf, "%s.%s", collector->prefix, name.c_str());
      } else {
         /* $metric */
         Mmsg(buf, "%s", name.c_str());
      }
      Dmsg2(1500, "Statistics: %s met&prefix: %s\n", collector->hdr.name, buf.c_str());
   }
};


#ifndef TEST_PROGRAM
#define TEST_PROGRAM_A
#endif

#ifdef TEST_PROGRAM
#include "unittests.h"

struct test {
   const int nr;
   const char *in;
   const char *out;
};

static struct test tests[] = {
   {1, "abc.def", "abc%32def"},
   {2, "", ""},
   {3, ".abc", "%32abc"},
   {4, "abc.", "abc%32"},
   {5, "abc..def", "abc%32%32def"},
   {6, "abc..", "abc%32%32"},
   {7, "..def", "%32%32def"},
   {8, ".......", "%32%32%32%32%32%32%32"},
   {0, NULL, NULL},
};

#define ntests ((int)(sizeof(tests)/sizeof(struct test)))

int main()
{
   Unittests collect_test("collect_test");
   POOL_MEM name(PM_NAME);
   char buf[64];

   for (int i = 0; i < ntests; i++) {
      if (tests[i].nr > 0){
         snprintf(buf, 64, "Checking mangle test: %d - '%s'", tests[i].nr, tests[i].in);
         ok(strcmp(replace_dot_metric_name(name, tests[i].in), tests[i].out) == 0, buf);
      }
   }

   return report();
}
#endif /* TEST_PROGRAM */
