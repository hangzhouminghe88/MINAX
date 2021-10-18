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

#ifndef ORG_LIB_DEDUP_H
#define ORG_LIB_DEDUP_H
#include "bacula.h"


/* hardcoded default hash type (should move into bacula-sd.conf) */
#define DEDUP_DEFAULT_HASH_ID   1
/* size of static buffer that must handle hash */
#define DEDUP_MAX_HASH_SIZE     64  /* SHA512 */
/* before 8.2.8 some ref size were stored in 76 bytes in volumes instead of 44 */
#define DEDUP_BUGGY_REF_SIZE    76
/* a ref must contain at least a size and an address */
#define DEDUP_BASIC_REF_SIZE    (sizeof(uint32_t)+sizeof(blockaddr))
/* the offset of the address in a ref */
#define DEDUP_REF_ADDR_OFF      sizeof(uint32_t)
/* the maximum size of a reference, NOT including the OFFSET of sparse stream */
#define DEDUP_MAX_REF_SIZE      (DEDUP_BASIC_REF_SIZE+DEDUP_MAX_HASH_SIZE)
/* dedup don't have a lower limit, but it is ridiculous to try to dedup smallest block */
#ifdef DEVELOPER
   /* Try with a high value to amplify the use of raw data */
   #define DEDUP_MIN_BLOCK_SIZE    4096
#else
  /* TODO ASX choose the more appropriate value for this */
  #define DEDUP_MIN_BLOCK_SIZE    1024
#endif

/* the preferred block size for dedup */
#define DEDUP_IDEAL_BLOCK_SIZE  (64*1024)
/* the dedupengine cannot handle block bigger than this one */
#define DEDUP_MAX_BLOCK_SIZE    (65*1024-BLOCK_HEAD_SIZE)

#define DEDUP_MAX_ENCODED_SIZE  (DEDUP_MAX_BLOCK_SIZE+BLOCK_HEAD_SIZE)
#define DEDUP_MAX_MSG_SIZE      (DEDUP_MAX_BLOCK_SIZE+DEDUP_MAX_HASH_SIZE+sizeof(uint32_t)+OFFSET_FADDR_SIZE+100)


#define BLOCK_HEAD_SIZE               4       // only one int
#define BLOCK_HEAD_COMPRESSED         (1<<31)
#define BLOCK_HEAD_NOT_TRIED_COMPRESS (1<<30) // You could try to compress it
#define BLOCK_HEAD_CTRL_MASK          (0xFF000000)
#define BLOCK_HEAD_SIZE_MASK          (0x00FFFFFF)

/* access first bytes of any data block as an int, used to display short hash * in Dmsg()*/
inline int hash2int(const void *p) { return htonl(*(int *)p); }

int bhash_info(int hash_id, const char **hash_name);

bool is_deduplicable_stream(int stream);
bool is_client_rehydration_friendly_stream(int stream);
void dedup_get_limits(int64_t *nofile, int64_t *memlock);

#endif  /* ORG_LIB_DEDUP_H */
