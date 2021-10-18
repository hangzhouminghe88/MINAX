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
 *  Bacula array list routines
 *
 *    alist is a simple malloc'ed array of pointers.  For the moment,
 *      it simply malloc's a bigger array controlled by num_grow.
 *      Default is to realloc the pointer array for each new member.
 *
 *    Note: the list can have holes (empty items). This is done by
 *      using get() and put().  If you are using this kind of indexed
 *      list, you cannot use: prepend() and remove() as they will
 *      reorder the list. So, in the ilist array, these functions are
 *      disabled and the put method is defined.
 *
 *   Kern Sibbald, June MMIII
 *
 */

#include "bacula.h"

/*
 * Private grow list function. Used to insure that
 *   at least one more "slot" is available.
 */
void baselist::grow_list()
{
   int i;
   int new_max_items;

   /* put() can insert and item anywhere in the list so
    * it's important to allocate at least last_item+1 items 
    */
   int min_grow = MAX(10, last_item+1);
   if (num_grow < min_grow) {
      num_grow = min_grow;               /* default if not initialized */
   }

   if (items == NULL) {
      items = (void **)malloc(num_grow * sizeof(void *));
      for (i=0; i<num_grow; i++) {
         items[i] = NULL;
      }
      max_items = num_grow;
   } else if (last_item >= max_items) {
      new_max_items = last_item + num_grow;
      items = (void **)realloc(items, new_max_items * sizeof(void *));
      for (i=max_items; i<new_max_items; i++) {
         items[i] = NULL;
      }
      max_items = new_max_items;
   }
}

void *alist::first()
{
   cur_item = 1;
   if (num_items == 0) {
      return NULL;
   } else {
      return items[0];
   }
}

void *alist::last()
{
   if (num_items == 0) {
      return NULL;
   } else {
      cur_item = last_item;
      return items[last_item-1];
   }
}

void *alist::next()
{
   if (cur_item >= last_item) {
      return NULL;
   } else {
      return items[cur_item++];
   }
}

/* Do not mix prev() and next() calls */
void *alist::prev()
{
   if (cur_item <= 1) {
      return NULL;
   } else {
      return items[--cur_item];
   }
}

/*
 * prepend an item to the list -- i.e. add to beginning
 */
void alist::prepend(void *item)
{
   grow_list();
   if (num_items == 0) {
      items[num_items++] = item;
      if (num_items > last_item) {
         last_item = num_items;
      }
      return;
   }
   for (int i=last_item; i > 0; i--) {
      items[i] = items[i-1];
   }
   items[0] = item;
   num_items++;
   last_item++;
}

/*
 * Append an item to the list
 */
void baselist::append(void *item)
{
   grow_list();
   items[last_item++] = item;
   num_items++;
}

/*
 * Remove an item from the list
 * Note: you must free the item when
 *   you are done with it.
 */
void * baselist::remove_item(int index)
{
   void *item;
   if (index < 0 || index >= last_item) {
      return NULL;
   }
   item = items[index];

   /* last_item is from 1..n, we work from 0..n-1 */
   for (int i=index; i < (last_item-1); i++) {
      items[i] = items[i+1];
   }

   items[last_item-1] = NULL;   /* The last item is shifted by one, the last slot is always free */

   last_item--;                 /* We have shifted all items by 1 */
   num_items--;                 /* We have 1 item less */

   return item;
}


/* Get the index item -- we should probably allow real indexing here */
void * baselist::get(int index)
{
   if (items == NULL || index < 0 || index >= last_item) {
      return NULL;
   }
   return items[index];
}

/* Destroy the list and its contents */
void baselist::destroy()
{
   if (items) {
      if (own_items) {
         for (int i=0; i<max_items; i++) {
            if (items[i]) {
               bfree(items[i]);
               items[i] = NULL;
            }
         }
      }
      bfree(items);
      items = NULL;
   }
   num_items = 0;
   last_item = 0;
   max_items = 0;
   num_grow = 0;
}

#ifdef TEST_PROGRAM
#include "unittests.h"

#define NUMITEMS        20
#define MORENUMITEMS    115

struct FILESET {
   alist mylist;
};

void check_all_alist_indexes2(alist *mlist)
{
   bool check_cont = true;
   char *bp;
   int i = 0;
   int nb;

   foreach_alist_index(i, bp, mlist) {
      nb = atoi(bp);
      if (nb != i){
         Dmsg2(0, "nb=%d != i=%d\n", nb, i);
         check_cont = false;
      }
   }
   ok(check_cont, "Check all alist indexes 2");
};

void check_all_alist_contents(alist *mlist)
{
   bool check_cont = true;
   char buf[30];
   int i;

   for (i = 0; i < mlist->size(); i++) {
      sprintf(buf, "This is item %d", i);
      if (strcmp(buf, (char*)mlist->get(i)) != 0){
         check_cont = false;
      }
   }
   ok(check_cont, "Checking alist contents");
};

void check_all_alist_indexes(alist *mlist)
{
   bool check_cont = true;
   char *bp;
   int i = 0;
   int nb;

   foreach_alist(bp, mlist) {
      nb = atoi(bp);
      if (nb != i++){
         check_cont = false;
      }
   }
   ok(check_cont, "Check all alist indexes");
};

void check_alist_destroy_and_delete(alist *mlist)
{
   mlist->destroy();
   ok(mlist->size() == 0, "Check alist size after destroy");
   ok(mlist->last() == NULL, "Check alist last after destroy");
   delete mlist;
};

int main()
{
   Unittests alist_test("alist_test");
   FILESET *fileset;
   char buf[30];
   alist *mlist;
   char *bp;
   int i;
   bool check_cont;
   bool check_indx;

   log("Initialize tests ...");
   fileset = (FILESET *)malloc(sizeof(FILESET));
   bmemzero(fileset, sizeof(FILESET));
   fileset->mylist.init();
   ok(fileset && fileset->mylist.empty() && fileset->mylist.max_size() == 0,
      "Default initialization");

   log("Automatic allocation/destruction of alist:");

   for (int i = 0; i < NUMITEMS; i++) {
      sprintf(buf, "This is item %d", i);
      fileset->mylist.append(bstrdup(buf));
   }
   ok(fileset->mylist.size() == NUMITEMS, "Checking size");

   check_all_alist_contents(&fileset->mylist);

   fileset->mylist.destroy();
   ok(fileset->mylist.size() == 0, "Check size after delete");
   ok(fileset->mylist.last() == NULL, "Check last after delete");
   free(fileset);

   log("Allocation/destruction using new delete");

   mlist = New(alist(50));
   ok(mlist && mlist->empty() && mlist->max_size() == 0,
      "Constructor initialization");
   for (i = 0; i < NUMITEMS; i++) {
      sprintf(buf, "This is item %d", i);
      mlist->append(bstrdup(buf));
   }
   ok(mlist->size() == NUMITEMS, "Checking size");
   check_all_alist_contents(mlist);
   check_alist_destroy_and_delete(mlist);

   log("Test alist::remove(0)");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("trash"));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("3"));
   ok(mlist && mlist->size() == 5, "Checking size");
   ok(mlist->last_index() == 5, "Check last_index");
   free(mlist->remove(0));
   ok(mlist->size() == 4, "Remove test size");
   ok(mlist->last_index() == 4, "Check last_index");
   check_all_alist_indexes(mlist);
   check_alist_destroy_and_delete(mlist);

   log("Test alist::remove(3)");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("trash"));
   mlist->append(bstrdup("3"));
   ok(mlist && mlist->size() == 5, "Checking size");
   ok(mlist->last_index() == 5, "Check last_index");
   free(mlist->remove(3));
   ok(mlist->size() == 4, "Remove test size");
   ok(mlist->last_index() == 4, "Check last_index");
   check_all_alist_indexes(mlist);
   check_alist_destroy_and_delete(mlist);

   log("Test alist::remove(last)");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("3"));
   mlist->append(bstrdup("trash"));
   ok(mlist && mlist->size() == 5, "Checking size");
   ok(mlist->last_index() == 5, "Check last_index");
   free(mlist->remove(4));
   ok(mlist->size() == 4, "Remove test size");
   check_all_alist_indexes(mlist);
   check_alist_destroy_and_delete(mlist);

   log("Test alist::remove(last+1)");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("3"));
   mlist->append(bstrdup("4"));
   check_all_alist_indexes2(mlist);
   ok(mlist && mlist->size() == 5, "Checking size");
   ok(mlist->last_index() == 5, "Check last_index");
   ok(mlist->remove(5) == NULL, "Check remove returns null");
   ok(mlist->size() == 5, "Remove test size");
   check_all_alist_indexes(mlist);
   check_all_alist_indexes2(mlist);   
   check_alist_destroy_and_delete(mlist);

   log("Test alist::pop()");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("3"));
   mlist->append(bstrdup("trash"));
   ok(mlist && mlist->size() == 5, "Checking size");
   ok(mlist->last_index() == 5, "Check last_index");
   free(mlist->pop());
   ok(mlist->size() == 4, "Check last_index after pop()");
   check_all_alist_indexes(mlist);
   check_alist_destroy_and_delete(mlist);

   log("Test alist::push()");
   mlist = New(alist(10, owned_by_alist));
   check_cont = true;
   check_indx = true;
   for (i = 0; i < NUMITEMS; i++) {
      sprintf(buf, "This is item %d", i);
      mlist->push(bstrdup(buf));
      if (mlist->size() != i + 1){
         check_cont = false;
      }
      if (mlist->last_index() != i + 1){
         check_indx = false;
      }
   }
   ok(check_cont, "Check all sizes after push");
   ok(check_indx, "Check all last_indexes after push");
   log("Test alist::pop()");
   check_cont = true;
   for (i = NUMITEMS-1; (bp = (char *)mlist->pop()); i--) {
      sprintf(buf, "This is item %d", i);
      if (strcmp(buf, bp) != 0){
         check_cont = false;
      }
      free(bp);
   }
   ok(check_cont, "Check alist content after pop()");
   ok(mlist->size() == 0, "Check alist size after pop()");
   ok(mlist->last_index() == 0, "Check alist last_index after pop()");
   /* check get after pop, it should be NULL */
   check_cont = true;
   for (int i=0; i<mlist->max_size(); i++) {
      bp = (char *) mlist->get(i);
      if (bp != NULL){
         check_cont = false;
      }
   }
   ok(check_cont, "Check get() after pop() contents.");
   check_alist_destroy_and_delete(mlist);

   log("Test alist::foreach_alist_index()");
   mlist = New(alist(10, owned_by_alist));
   mlist->append(bstrdup("0"));
   mlist->append(bstrdup("1"));
   mlist->append(bstrdup("2"));
   mlist->append(bstrdup("3"));
   mlist->append(bstrdup("4"));
   mlist->append(bstrdup("5"));
   mlist->append(bstrdup("6"));
   mlist->append(bstrdup("7"));
   mlist->append(bstrdup("8"));
   mlist->append(bstrdup("9"));
   check_all_alist_indexes2(mlist);
   ok(mlist && mlist->size() == 10, "Checking size");
   ok(mlist->last_index() == 10, "Check last_index");
   foreach_alist_index(i, bp, mlist) {
      ok(i < mlist->size(), "check index");
      ok(bp != NULL, "check element of alist");
   }
   check_alist_destroy_and_delete(mlist);
   return report();
}
#endif
