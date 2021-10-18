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

#include "bacula.h"
#include "lib/unittests.h"
#include "filed/fd_plugins.h"
#include "findlib/bfile.h"

/* Pointers to Bacula functions */
static const  char *working="./";

#define USE_JOB_LIST
#define UNITTESTS
#include "plugins/fd/fd_common.h"

void *start_heap;

int main (int argc, char *argv[])
{
   Unittests t("joblist_test");
   POOL_MEM tmp;
   const char *k = "netapp:/vol/vol1";
   const char *k2 = "netapp:/vol/vol2";
   const char *k3 = "netapp:/vol/vol2xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
   const char *k4 = "netapp:/vol/vol2xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxz";
   char key1[128], key2[128], key3[128], key4[128], key5[128],
      jobF[128], jobI[128], jobI2[128], jobD[128], jobI3[128];
   strcpy(key1, "ID=704ba403-fa5d-45f9-8681-859eec950723 DATE=1:15f18c5c-3824-40d2-8843-e6d161f77504:15f18c5c-3824-40d2-8843-e6d161f77504");
   strcpy(key2, "ID=704ba403-fa5d-45f9-8681-859eec950723 DATE=2:15f18c5c-3824-40d2-8843-e6d161f77504:15f18c5c-3824-40d2-8843-e6d161f77504");
   strcpy(key3, "ID=704ba403-fa5d-45f9-8681-859eec950723 DATE=3:15f18c5c-3824-40d2-8843-e6d161f77504:15f18c5c-3824-40d2-8843-e6d161f77504");
   strcpy(key4, "ID=704ba403-fa5d-45f9-8681-859eec950723 DATE=4:15f18c5c-3824-40d2-8843-e6d161f77504:15f18c5c-3824-40d2-8843-e6d161f77504");
   strcpy(key5, "ID=704ba403-fa5d-45f9-8681-859eec950723 DATE=5:15f18c5c-3824-40d2-8843-e6d161f77504:15f18c5c-3824-40d2-8843-e6d161f77504");

   strcpy(jobF, "Test1.2020-03-12_09.45.31_24");
   strcpy(jobI, "Test1.2020-03-12_09.45.31_25");
   strcpy(jobI2, "Test1.2020-03-12_09.45.31_26");
   strcpy(jobD, "Test1.2020-03-12_09.45.31_27");
   strcpy(jobI3, "Test1.2020-03-12_09.45.31_28");

   if (argc != 1) {
      exit (1);
   }
   unlink("test.dat");
   {
      joblist job_history(NULL, k, jobF, NULL, 'F');
      job_history.set_base("test.dat");
      job_history.store_job(key1);
      job_history.prune_jobs(NULL, NULL, NULL);
   }
   {
      joblist job_history(NULL, k, jobI, jobF, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key2);
   }
   {
      joblist job_history(NULL, k, jobI2, jobI, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      ok(job_history.find_job(jobI, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key2) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key3);
   }
   {
      joblist job_history(NULL, k, jobD, jobF, 'D');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key4);
   }
   {
      joblist job_history(NULL, k, jobI3, jobD, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key5);
      nok(job_history.find_job(jobI, tmp.handle()), "Find pruned job");
   }

   {
      joblist job_history(NULL, k2, jobF, NULL, 'F');
      job_history.set_base("test.dat");
      job_history.store_job(key1);
      job_history.prune_jobs(NULL, NULL, NULL);
   }
   {
      joblist job_history(NULL, k2, jobI, jobF, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key2);
   }
   {
      joblist job_history(NULL, k2, jobI2, jobI, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      ok(job_history.find_job(jobI, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key2) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key3);
   }
   {
      joblist job_history(NULL, k2, jobD, jobF, 'D');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key4);
   }
   {
      joblist job_history(NULL, k2, jobI3, jobD, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key5);
      nok(job_history.find_job(jobI, tmp.handle()), "Find pruned job");
   }
   {
      joblist job_history(NULL, k3, jobF, NULL, 'F');
      job_history.set_base("test.dat");
      job_history.store_job(key1);
      job_history.prune_jobs(NULL, NULL, NULL);
   }
   {
      joblist job_history(NULL, k3, jobI, jobF, 'I');
      job_history.set_base("test.dat");
      ok(job_history.find_job(jobF, tmp.handle()), "Find previous very long job");
      ok(strcmp(tmp.c_str(), key1) == 0, "test key");
      job_history.prune_jobs(NULL, NULL, NULL);
      job_history.store_job(key2);
   }
   {
      joblist job_history(NULL, k4, jobI, jobF, 'I');
      job_history.set_base("test.dat");
      nok(job_history.find_job(jobF, tmp.handle()), "Find previous very long job");
   }
   return report();
}
