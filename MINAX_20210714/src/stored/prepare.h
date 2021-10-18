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
 * Code to do a Percona prepare on a MySQL Percona backup
 *  during the backup but prior to putting it on the volume.
 *
 *  Kern Sibbald, February MMIIXX
 */

#ifndef __PREPARE_H__
#define __PREPARE_H__ 1

#include "findlib/find.h"

struct attr_record {
   int32_t stream;     /* masked stream */
   int32_t Stream;     /* full stream */
   uint32_t data_len;
   int32_t  FileIndex;
   /* *** last item in struct */
   char data[0];       /* malloced at end of this record */
};

class prepare_ctx {
public:
   int32_t stream;      /* current stream */
   ATTR *attr;
   BFILE bfd;
   char *wbuf;
   int temp_fd;
   FILE *temp_fp;
   uint32_t wsize;              /* write size */
   uint64_t fileAddr;           /* file write address */
   uint32_t num_files;
   bool prepare;                /* set when running prepare job */
   bool created;                /* set for a single initialization */
   POOLMEM *tmp;
   POOLMEM *tmp2;
   POOLMEM *result;             /* run_program results */
   alist *attr_list;            /* list of attr records */
   const char *working;
   struct stat statp;           /* stat() of prepared .xbstream */
   const char* xtrabackupconf;
   BPIPE* bpipe;

   /* Methods */
   prepare_ctx() :
      stream(0),
      attr(NULL),
      bfd(),                    /* will be zero initialized */
      wbuf(NULL),
      temp_fd(0),
      temp_fp(NULL),
      wsize(0),
      fileAddr(0),
      num_files(0),
      prepare(false),
      created(false),
      tmp(NULL),
      tmp2(NULL),
      result(NULL),
      attr_list(NULL),
      working(NULL),
      statp(),                  /* will be zero initialized */
      xtrabackupconf(NULL),
      bpipe(NULL)
      {
      };
   ~prepare_ctx() { };
};

bool prepare(JCR *jcr, prepare_ctx &pctx, DEV_RECORD &rec);
bool prepare_sd_end(JCR *jcr, prepare_ctx &pctx, DEV_RECORD &rec);

#endif
