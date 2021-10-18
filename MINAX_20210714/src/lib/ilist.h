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

#ifndef ILIST_H
#define ILIST_H

extern bool is_null(const void *ptr);

/* Second arg of init */
enum {
  owned_by_ilist = true,
  not_owned_by_ilist = false
};

/*
 * Array list -- much like a simplified STL vector
 *   array of pointers to inserted items. 
 */
class ilist : public SMARTALLOC {
protected:
   void **items;                /* from  0..n-1 */
   int num_items;               /* from  1..n   */

   int last_item;               /* maximum item index (1..n). */

   int max_items;               /* maximum possible items (array size) (1..n) */
   int num_grow;
   int cur_item;                /* from 1..n */
   bool own_items;
   void grow_list(void);
   void *remove_item(int index);

public:
   ilist(int num = 100, bool own=true);
   ~ilist();
   void init(int num = 100, bool own=true);
   void append(void *item);
   void *get(int index);
   bool empty() const;
   int last_index() const { return last_item; };
   int max_size() const { return max_items; };
   void * operator [](int index) const;
   int size() const;
   void destroy();
   void grow(int num);

   void put(int index, void *item);
};

inline bool ilist::empty() const
{
   return num_items == 0;
}

/*
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void ilist::init(int num, bool own)
{
   items = NULL;
   num_items = 0;
   last_item = 0;
   max_items = 0;
   num_grow = num;
   own_items = own;
}

/*
 * Define index operator []
 */
inline void * ilist::operator [](int index) const {
   if (index < 0 || index >= max_items) {
      return NULL;
   }
   return items[index];
}

/* Constructor */
inline ilist::ilist(int num, bool own)
{
   init(num, own);
}

/* Destructor */
inline ilist::~ilist()
{
   destroy();
}

/* Current size of list */
inline int ilist::size() const
{
   return num_items;
}

/* How much to grow by each time */
inline void ilist::grow(int num)
{
   num_grow = num;
}

#endif  // ILIST_H
