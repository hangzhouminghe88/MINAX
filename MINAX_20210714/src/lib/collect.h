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
 * This is a Bacula statistics collector support utilities.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#ifndef __COLLECT_H_
#define __COLLECT_H_

# include "bstat.h"

/* all supported display formats */
typedef enum {
   COLLECT_SIMPLE = 0,
   COLLECT_FULL,
   COLLECT_JSON,
} display_format_t;

/* scanning commands for remote metrics query */
const char collect_all_cmd[] = "statistics all format=%127s";
const char collect_metrics_cmd[] = "statistics format=%127s %127s";
const char collect_all_send_cmd[] = "statistics all format=%s\n";
const char collect_metrics_send_cmd[] = "statistics format=%s";

/* forward reference only */
class bstatmetric;
/* forward reference only */
class COLLECTOR;
class OutputWriter;

const char *displayformat2str(display_format_t format);
display_format_t scandisplayformat(POOLMEM *buf);
//void rendervalue(POOL_MEM &buf, bstatmetric *m);
//void rendermetricsimple(POOL_MEM &out, bstatmetric *m);
//void rendermetricjson(POOL_MEM &out, bstatmetric *m);
//void rendermetricfull(POOL_MEM &out, bstatmetric *m);
void rendermetric(POOL_MEM &out, bstatmetric *m, display_format_t format, int nr);
void render_metric_prefix(COLLECTOR *collector, POOL_MEM &buf, bstatmetric *item);
int render_collector_status(COLLECTOR &res_collector, POOL_MEM &buf);
void api_render_collector_status(COLLECTOR &res_collector, OutputWriter &ow);

#endif  /* __COLLECT_H */
