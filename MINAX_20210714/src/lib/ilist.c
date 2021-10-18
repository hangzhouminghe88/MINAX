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
 *    ilist is a simple malloc'ed array of pointers.  For the moment,
 *      it simply malloc's a bigger array controlled by num_grow.
 *      Default is to realloc the pointer array for each new member.
 *
 *    Note: the list can have holes (empty items). This is done by
 *      using get() and put().
 *
 *   From alist.c
 *
 */

#include "bacula.h"

/*
 * Private grow list function. Used to insure that
 *   at least one more "slot" is available.
 */
void ilist::grow_list()
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

void ilist::append(void *item)
{
   grow_list();
   items[last_item++] = item;
   num_items++;
}

/*
 * Put an item at a particular index
 */
void ilist::put(int index, void *item)
{
   if (index > last_item) {
      last_item = index; // FIXME: On alist, last_item is pointing after the last item
   }
   grow_list();
   if (items[index] == NULL) {
      num_items++;
   }
   items[index] = item;
}


/*
 * Remove an item from the list
 * Note: you must free the item when
 *   you are done with it.
 */
void * ilist::remove_item(int index)
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
void * ilist::get(int index)
{
   if (items == NULL || index < 0 || index > last_item) { // Difference with alist here
      return NULL;
   }
   return items[index];
}

/* Destroy the list and its contents */
void ilist::destroy()
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

void check_all_ilist_contents(ilist *vlist, int start)
{
   bool check_cont = true;
   char buf[30];
   int i;

   for (i = start; i< vlist->size(); i++) {
      sprintf(buf, "This is item %d", i);
      if (strcmp(buf, (char*)vlist->get(i)) != 0){
         check_cont = false;
      }
   }
   ok(check_cont, "Checking ilist contents");
};

void check_ilist_destroy_delete(ilist *vlist)
{
   vlist->destroy();
   ok(vlist->size() == 0, "Check ilist size after destroy");
   delete vlist;
}

int main()
{
   Unittests ilist_test("ilist_test");
   char buf[30];
   ilist *vlist;
   char *bp;
   int i;
   bool check_cont;
   bool check_indx;

   log("Initialize tests ...");
   log("Test ilist::put()");
   vlist = New(ilist(10, owned_by_ilist));
   sprintf(buf, "This is item 10");
   vlist->put(10, bstrdup(buf));
   ok(vlist && vlist->size() == 1, "Checking size after put()");
   ok(vlist->last_index() == 10, "Check last_index");
   check_ilist_destroy_delete(vlist);

   log("Test ilist with multiple put()");
   vlist = New(ilist(50, owned_by_ilist));
   sprintf(buf, "This is item 10");
   vlist->put(10, bstrdup(buf));
   ok(vlist && vlist->size() == 1, "Checking size after put()");
   ok(vlist->last_index() == 10, "Check last_index");
   sprintf(buf, "This is item 15");
   vlist->put(15, bstrdup(buf));
   ok(vlist->size() == 2, "Checking size after put()");
   ok(vlist->last_index() == 15, "Check last_index");
   for (i = NUMITEMS; i < NUMITEMS + MORENUMITEMS; i++) {
      sprintf(buf, "This is item %d", i);
      vlist->put(i, bstrdup(buf));
   }
   ok(vlist->size() == 2 + MORENUMITEMS, "Checking size after put()");
   ok(vlist->last_index() == NUMITEMS + MORENUMITEMS - 1, "Check last_index");
   /* check contents, first two sparse elements */
   ok(strcmp("This is item 10", (char *)vlist->get(10)) == 0, "Check ilist content at 10");
   ok(strcmp("This is item 15", (char *)vlist->get(15)) == 0, "Check ilist content at 15");
   check_all_ilist_contents(vlist, NUMITEMS);
   check_ilist_destroy_delete(vlist);

   return report();
}
#endif
