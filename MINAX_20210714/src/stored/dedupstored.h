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

#ifndef DEDUPSTORED_H_
#define DEDUPSTORED_H_

bool is_dedup_server_side(DEVICE *dev, int32_t stream, uint64_t stream_len);
bool is_dedup_ref(DEV_RECORD *rec, bool lazy);
void list_dedupengines(char *cmd, STATUS_PKT *sp);
void dedup_get_limits(int64_t *nofile, int64_t *memlock);
bool dedup_parse_filter(char *fltr);
void dedup_filter_record(int verbose, DCR *dcr, DEV_RECORD *rec, char *dedup_msg, int len);

class DedupEngine;

/* Interface between DEDUP and SD */
class DedupStoredInterfaceBase
{
public:

   DedupStoredInterfaceBase(JCR *jcr, DedupEngine *dedupengine) {};
   virtual ~DedupStoredInterfaceBase() {};

      // deduplication
   virtual int start_deduplication() {return -1;};
   virtual void *wait_deduplication(bool emergency=false) {return NULL;};
   virtual void *do_deduplication_thread(void){return NULL;};

   // rehydration
   virtual int start_rehydration(){return -1;};
   virtual void *wait_rehydration(bool emergency=false){return NULL;};
   virtual void *do_rehydration_thread(void){return NULL;};
   virtual int handle_rehydration_command(BSOCK *fd){return -1;};
   virtual bool wait_flowcontrol_rehydration(int free_rec_count, int timeoutms){return false;};
   virtual bool do_flowcontrol_rehydration(int free_rec_count, int retry_timeoutms=250){return false;};
   virtual void warn_rehydration_eod() {};

   virtual int record_rehydration(DCR *dcr, DEV_RECORD *rec, char *buf, POOLMEM *&errmsg, bool despite_of_error, int *chunk_size){return -1;};
   virtual int add_circular_buf(DCR *dcr, DEV_RECORD *rec){return -1;};
   virtual void set_checksum_after_rehydration(bool val) {};
   virtual void set_use_index_for_recall(bool val) {};

   virtual POOLMEM *get_msgbuf() { return NULL; };
   virtual void unset_rehydration_srvside() { return; };
   virtual bool is_rehydration_srvside() { return false; };
   virtual bool is_thread_started() { return false; };

};

#endif /* DEDUPSTORED_H_ */
