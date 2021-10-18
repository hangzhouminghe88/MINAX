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
/* Written by Eric Bollengier June 2020 */

#ifndef BITMAP_H
#define BITMAP_H

/* Small class to check bitmap and determine when they are set or not */
void       set_assert_msg        (const char *file, int line, const char *msg);

class bitmap: public SMARTALLOC
{
   int      len;
   uint32_t max;
   char *array;
   char *isset;

   void init(uint32_t nb_bits)
   {
      max = MAX(1, nb_bits);
      len = nbytes_for_bits(max);
      Dmsg2(0, "max=%d len=%d\n", max, len);
      if (array) {
         free(array);
      }
      if (isset) {
         free(isset);
      }
      array = (char *)malloc(len);
      isset = (char *)malloc(len);
      bmemset(array, 0, len);
      bmemset(isset, 0, len);
   };

public:
   bitmap(uint32_t nb_bits): len(0), max(0), array(NULL), isset(NULL) {
      init(nb_bits);
   };
   ~bitmap() {
      free(array);
      free(isset);
   };
   void copy(bitmap *from) {
      init(from->max);
      memcpy(isset, from->isset, len);
      memcpy(array, from->array, len);
   };
   void resize(uint32_t nb_bits) {
      if (nb_bits <= max) {
         max = nb_bits;

      } else {
         max = nb_bits;
         int tlen = nbytes_for_bits(max);
         char *tarray = (char *) malloc(len);
         char *tisset = (char *) malloc(len);
         bmemset(tarray, 0, tlen);
         bmemset(tisset, 0, tlen);
         memcpy(tarray, array, len);
         memcpy(tisset, isset, len);
         free(array);
         free(tisset);
         array = tarray;
         isset = isset;
         len = tlen;
      }
   };

   void dump() {
      POOL_MEM tmp;
      for (uint32_t i = 0 ; i < max ; i++) {
         switch(is_set(i)) {
         case -1:
            pm_strcat(tmp, ".");
            break;
         case 0:
            pm_strcat(tmp, "0");
            break;
         case 1:
            pm_strcat(tmp, "1");
            break;
         }
      }
      Dmsg2(0, "bitmap %p: [%s]\n", this, tmp.c_str());
   };

   int32_t get_max_bit() {
      return max - 1;
   };

   /* -1: not set
    *  0: cleared
    *  1: set
    */
   int is_set(uint32_t b) {
      if (b >= max) {
         ASSERTD(b >= max, "Should not query so much bit in bitmap");
         return -1;
      }
      if (bit_is_set(b, isset)) {
         return bit_is_set(b, array);

      } else {
         return -1;
      }
   };

   bool bit_set(uint32_t b) {
      if (b >= max) {
         ASSERTD(b >= max, "Should not set so much bit in bitmap");
         return false;
      }
      set_bit(b, isset);
      set_bit(b, array);
      return true;
   };

   bool bit_clear(uint32_t b) {
      if (b >= max) {
         ASSERTD(b >= max, "Should not clear so much bit in bitmap");
         return false;
      }
      clear_bit(b, array);
      set_bit(b, isset);
      return true;
   };

   bool bit_reset(uint32_t b) {
      if (b >= max) {
         ASSERTD(b >= max, "Should not reset so much bit in bitmap");
         return false;
      }
      clear_bit(b, isset);
      clear_bit(b, array);
      return true;
   };
};

#endif  /* BITMAP_H */
