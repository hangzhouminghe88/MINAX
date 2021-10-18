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
 * Includes specific to the Director
 *
 *     Kern Sibbald, December MM
 */

#include "lib/ini.h"
#include "lib/runscript.h"
#include "lib/breg.h"
#include "dird_conf.h"

#define DIRECTOR_DAEMON 1

#include "dir_plugins.h"
#include "cats/cats.h"

#include "jcr.h"
#include "bsr.h"
#include "ua.h"
#include "jobq.h"

/* Globals that dird.c exports */
extern DIRRES *director;                     /* Director resource */
extern int FDConnectTimeout;
extern int SDConnectTimeout;

/* Used in ua_prune.c and ua_purge.c */

struct s_count_ctx {
   int count;
};

#define MAX_DEL_LIST_LEN 2000000

struct del_ctx {
   JobId_t *JobId;                    /* array of JobIds */
   char *PurgedFiles;                 /* Array of PurgedFile flags */
   int num_ids;                       /* ids stored */
   int max_ids;                       /* size of array */
   int num_del;                       /* number deleted */
   int tot_ids;                       /* total to process */
};

/* Flags for find_next_volume_for_append() */
enum {
  fnv_create_vol    = true,
  fnv_no_create_vol = false,
  fnv_prune         = true,
  fnv_no_prune      = false
};

typedef struct {
   char    *plugin_name;
   POOLMEM *content;
} plugin_config_item;

struct idpkt {
   POOLMEM *list;
   uint32_t count;
};

typedef struct {
   int bacula_dir_config_clients;
   int bacula_dir_config_jobs;
   int bacula_dir_config_filesets;
   int bacula_dir_config_pools;
   int bacula_dir_config_schedules;
   int bacula_dir_config_storages;
   int bacula_dir_memory_bufs;
   int bacula_dir_memory_heap;
   int bacula_dir_memory_maxbufs;
   int bacula_dir_memory_maxbytes;
   int bacula_dir_memory_smbytes;
   int bacula_jobs_queued_all;
   int bacula_jobs_running_all;
   int bacula_jobs_all;
   int bacula_jobs_bytes;
   int bacula_jobs_files;
   int bacula_jobs_success_all;
   int bacula_jobs_errors_all;
   int bacula_jobs_warning_all;
   int bacula_volumes_all;
   int bacula_volumes_available;
   int bacula_volumes_bytes;
   int bacula_volumes_errors_all;
   int bacula_volumes_full_all;
   int bacula_volumes_used_all;
} dirdstatmetrics_t;

void free_plugin_config_item(plugin_config_item *lst);
void free_plugin_config_items(alist *lst);
bool update_permanent_stats(void *data);

extern bstatcollect *statcollector;
extern dirdstatmetrics_t dirstatmetrics;

#include "protos.h"
