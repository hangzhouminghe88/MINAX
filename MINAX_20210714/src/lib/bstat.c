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
 * This is a Bacula statistics internal collector.
 * Author: Radosław Korzeniewski, radekk@inteos.pl, Inteos Sp. z o.o.
 */

#include "bstat.h"

// #define TEST_PROGRAM_DEBUG

/*
 * Default bstatmetric constructor
 */
bstatmetric::bstatmetric()
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::bstatmetric(): %p\n", this);
#endif
   init(NULL, METRIC_UNDEF, METRIC_UNIT_EMPTY, NULL);
};

/*
 * This is a special initialization constructor. No value will be set.
 */
bstatmetric::bstatmetric(char *mname, metric_type_t mtype, metric_unit_t munit, char *descr)
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::bstatmetric(type/unit): %p\n", this);
#endif
   init(mname, mtype, munit, descr);
};

/*
 * This is a special initialization constructor for Boolean metric with a value.
 */
bstatmetric::bstatmetric(char *mname, metric_unit_t munit, bool mvalue, char *descr)
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::bstatmetric(bool): %p\n", this);
#endif
   init(mname, METRIC_BOOL, munit, descr);
   value.boolval = mvalue;
};

/*
 * This is a special initialization constructor for Integer metric with a value.
 */
bstatmetric::bstatmetric(char *mname, metric_unit_t munit, int64_t mvalue, char *descr)
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::bstatmetric(int64): %p\n", this);
#endif
   init(mname, METRIC_INT, munit, descr);
   value.int64val = mvalue;
};

/*
 * This is a special initialization constructor for Floating point metric with a value.
 */
bstatmetric::bstatmetric(char *mname, metric_unit_t munit, float mvalue, char *descr)
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::bstatmetric(float): %p\n", this);
#endif
   init(mname, METRIC_FLOAT, munit, descr);
   value.floatval = mvalue;
};

/*
 * The standard "copy" operator.
 *  Copies all values from source metric to this destination metric.
 */
bstatmetric& bstatmetric::operator=(const bstatmetric& orig)
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric& bstatmetric::operator=(const bstatmetric& orig): %p\n", this);
#endif
   if (name){
      free(name);
   }
   if (description){
      free(description);
   }
   init(orig.name, orig.type, orig.unit, orig.description);
   switch (type){
      case METRIC_BOOL:
         value.boolval = orig.value.boolval;
         break;
      case METRIC_INT:
         value.int64val = orig.value.int64val;
         break;
      case METRIC_FLOAT:
         value.floatval = orig.value.floatval;
         break;
      default:
         value.int64val = 0;
   }
   return *this;
};

/*
 * Default destructor. Frees all allocated resources.
 */
bstatmetric::~bstatmetric()
{
#ifdef TEST_PROGRAM_DEBUG
   Pmsg1(0, "bstatmetric::~bstatmetric(): %p\n", this);
#endif
   if (name){
      free(name);
   }
   if (description){
      free(description);
   }
};

/*
 * Class initialization method used in constructors.
 */
void bstatmetric::init(char* mname, metric_type_t mtype, metric_unit_t munit, char *descr)
{
   name = mname ? bstrdup(mname) : NULL;
   type = mtype;
   unit = munit;
   value.int64val = 0;
   description = descr ? bstrdup(descr) : NULL;
};

/*
 * Return a string representation of a metric type.
 */
const char *bstatmetric::metric_type_str()
{
   switch (type){
      case METRIC_INT:
         return "Integer";
      case METRIC_BOOL:
         return "Boolean";
      case METRIC_FLOAT:
         return "Float";
      default:
         return "Undefined";
   }
};

/*
 * Return a string representation of a metric unit.
 *  Note! Update the method below when add new unit type.
 */
const char *bstatmetric::metric_unit_str()
{
   switch (unit){
      case METRIC_UNIT_BYTE:
         return "Bytes";
      case METRIC_UNIT_BYTESEC:
         return "Bytes/Sec";
      case METRIC_UNIT_JOB:
         return "Jobs";
      case METRIC_UNIT_CLIENT:
         return "Clients";
      case METRIC_UNIT_FILESET:
         return "FileSets";
      case METRIC_UNIT_POOL:
         return "Pools";
      case METRIC_UNIT_STORAGE:
         return "Storages";
      case METRIC_UNIT_SCHEDULE:
         return "Schedules";
      case METRIC_UNIT_FILE:
         return "Files";
      case METRIC_UNIT_VOLUME:
         return "Volumes";
      case METRIC_UNIT_SEC:
         return "Seconds";
      case METRIC_UNIT_MSEC:
         return "milliseconds";
      case METRIC_UNIT_NUMBER:
         return "Number";
      case METRIC_UNIT_PERCENT:
         return "Percent";
      case METRIC_UNIT_DEVICE:
         return "Device";
      case METRIC_UNIT_STATUS:
         return "Status";
      case METRIC_UNIT_AUTOCHANGER:
         return "Autochanger";
      default:
         return "Undefined";
   }
};

/*
 * Render and return a string representation of a metric value.
 *
 * in:
 *    buf - a pointer to POLLMEM buffer where value will be rendered
 *    bstr - defines a Boolean value rendering behavior; when True then rendered as True/False else as 1/0
 * out:
 *    buf - rendered string
 */
void bstatmetric::render_metric_value(POOLMEM **buf, bool bstr)
{
   if (buf && *buf){
      switch(type){
         case METRIC_BOOL:
            if (bstr){
               Mmsg(buf, "%s", value.boolval ? "True":"False");
            } else {
               Mmsg(buf, "%s", value.boolval ? "1":"0");
            }
            break;
         case METRIC_INT:
            Mmsg(buf, "%lld", value.int64val);
            break;
         case METRIC_FLOAT:
            Mmsg(buf, "%f", value.floatval);
            break;
         default:
            pm_strcpy(buf, NULL);
            break;
      }
   }
};

/*
 * Render and return a string representation of a metric value.
 *
 * in:
 *    buf - a pointer to POLLMEM buffer where value will be rendered
 *    bstr - defines a Boolean value rendering behavior; when True then rendered as True/False else as 1/0
 * out:
 *    buf - rendered string
 */
void bstatmetric::render_metric_value(POOL_MEM &buf, bool bstr)
{
   switch(type){
      case METRIC_BOOL:
         if (bstr){
            Mmsg(buf, "%s", value.boolval ? "True":"False");
         } else {
            Mmsg(buf, "%s", value.boolval ? "1":"0");
         }
         break;
      case METRIC_INT:
         Mmsg(buf, "%lld", value.int64val);
         break;
      case METRIC_FLOAT:
         Mmsg(buf, "%f", value.floatval);
         break;
      default:
         pm_strcpy(buf, NULL);
         break;
   }
};

/*
 * Default cache collector class.
 */
bstatcollect::bstatcollect()
{
   metrics = NULL;
   nrmetrics = 0;
   maxindex = 0;
   size = 0;
   if (pthread_mutex_init(&mutex, NULL) != 0){
      /* leave uninitialized */
      return;
   }
   metrics = (bstatmetric**)malloc(BSTATCOLLECT_NR * sizeof(bstatmetric*));
   memset(metrics, 0, BSTATCOLLECT_NR * sizeof(bstatmetric*));
   size = BSTATCOLLECT_NR;
};

/*
 * Default destructor.
 */
bstatcollect::~bstatcollect()
{
   int a;

   if (metrics){
      for (a = 0; a < maxindex; a++){
         if (metrics[a]){
            /* the metric is allocated at this index, free it */
            delete(metrics[a]);
         }
      }
      free(metrics);
   }
   pthread_mutex_destroy(&mutex);
};

/*
 * The cache collector lock synchronization for metrics table.
 *
 *  This is a "global" lock for a particular class instance,
 *  so a single lock for the whole table. It could be a bottleneck
 *  on concurrent metric registration (first) and update (next)
 *  when a metric update will has to wait until registration
 *  complete with O(n).
 *  The solution/mitigation to this problem could be a number
 *  of independent locks that guards fixed parts of the metric
 *  table (i.e. a single lock for every 16 metrics). This allows
 *  to change the time when a lock is held before update to a fixed
 *  and limited latency. The registration process could take longer
 *  in this case as it could be required to acquire a number of locks
 *  instead of the one currently.
 *
 *  As we do not expect more then a few hundreds metrics in the table
 *  above bottleneck should not be a problem.
 */
int bstatcollect::lock()
{
   int stat;

   if (metrics){
      if ((stat = pthread_mutex_lock(&mutex)) != 0){
         return stat;
      }
   } else {
      return EIO;
   }
   return 0;
};

/*
 * The cache collector unlock synchronization for metrics table.
 */
int bstatcollect::unlock()
{
   int stat;

   if (metrics){
      if ((stat = pthread_mutex_unlock(&mutex)) != 0){
         return stat;
      }
   } else {
      return EIO;
   }
   return 0;
};

/*
 * Verifies the size of metrics table and grows if required.
 */
void bstatcollect::check_size(int newsize)
{
   int a;
   bstatmetric **newtable;

   if (size < newsize + BSTATCOLLECT_STEP){
      /* we have to extend our table */
      newtable = (bstatmetric**)malloc((size + BSTATCOLLECT_STEP) * sizeof(bstatmetric*));
      memset(newtable, 0, (size + BSTATCOLLECT_STEP) * sizeof(bstatmetric*));
      for (a = 0; a < size; a++){
         newtable[a] = metrics[a];
      }
      free(metrics);
      metrics = newtable;
      size += BSTATCOLLECT_STEP;
   }
};

/*
 * Finds the metric index for already existent metric or assigns the new index.
 *
 * in:
 *    metric - the metric name to find or index assignment
 * out:
 *    metric index number
 *    ifalloc - False if index already allocated, True if new index assignments
 */
int bstatcollect::checkreg(char *metric, bool &ifalloc)
{
   int a;
   int firstfree = METRIC_INVALID;
   int index = METRIC_INVALID;

   if (nrmetrics){
      /* we have some metrics, so check it */
      for (a = 0; a < maxindex; a++){
         if (metrics[a]){
            if (metrics[a]->name && bstrcmp(metrics[a]->name, metric)){
               /* metric found return index */
               ifalloc = false;
               return a;
            }
         } else {
            if (!(firstfree > METRIC_INVALID)){
               firstfree = a;
            }
         }
      }
      /* not found, allocate new one */
      index = firstfree > METRIC_INVALID ? firstfree : maxindex++;
      check_size(index);
      nrmetrics++;
   } else {
      /* no metrics, this will be the first one */
      nrmetrics = 1;
      maxindex = 1;
      index = 0;
   }
   ifalloc = true;
   return index;
};

/*
 * Metric unregistration. Removes the metric from metrics table and frees resources.
 *  The metric index becomes invalid and can be reused on any subsequent metric registration.
 *  You should not use unregistered metrics index any more.
 *
 * in:
 *    metric - metric index to unregistration
 * out:
 *    unregistered metric, when metric index was already invalid it no changes are made
 */
void bstatcollect::unregistration(int metric)
{
   if (metric > METRIC_INVALID && metric < maxindex && metrics[metric]){
      delete(metrics[metric]);
      metrics[metric] = NULL;
      nrmetrics--;
   }
};

/*
 * Generic metric registration method. Allocates the new bstatmetric class if required.
 *
 * in:
 *    metric - metric name to register
 *    type - metric type
 *    unit - metric unit
 *    descr - description of the metric
 * out:
 *    registered metric index - when metric already exist it return its index without allocating the new one
 *    you can get METRIC_INVALID value on any error
 */
int bstatcollect::registration(char *metric, metric_type_t type, metric_unit_t unit, char *descr)
{
   int index;
   bstatmetric *data;
   bool ifalloc;

   if (lock() != 0){
      return METRIC_INVALID;
   }

   index = checkreg(metric, ifalloc);
   if (ifalloc){
      data = New(bstatmetric(metric, type, unit, descr));
      metrics[index] = data;
   }

   if (unlock() != 0){
      return METRIC_INVALID;
   }

   return index;
};

/*
 * Boolean metric registration and initialization. Allocates the new bstatmetric class if required.
 *  If metric already exist it will update the metric value.
 *
 * in:
 *    metric - metric name to register
 *    type - metric type
 *    unit - metric unit
 *    descr - description of the metric
 * out:
 *    registered metric index - when metric already exist it return its index without allocating the new one
 *    you can get METRIC_INVALID value on any error
 */
int bstatcollect::registration_bool(char *metric, metric_unit_t unit, bool value, char *descr)
{
   int index;
   bstatmetric *data;
   bool ifalloc;

   if (lock() != 0){
      return METRIC_INVALID;
   }

   index = checkreg(metric, ifalloc);
   if (ifalloc){
      data = New(bstatmetric(metric, unit, value, descr));
      metrics[index] = data;
   } else {
      metrics[index]->value.boolval = value;
   }

   if (unlock() != 0){
      return METRIC_INVALID;
   }

   return index;
};

/*
 * Integer metric registration and initialization. Allocates the new bstatmetric class if required.
 *  If metric already exist it will update the metric value.
 *
 * in:
 *    metric - metric name to register
 *    type - metric type
 *    unit - metric unit
 *    descr - description of the metric
 * out:
 *    registered metric index - when metric already exist it return its index without allocating the new one
 *    you can get METRIC_INVALID value on any error
 */
int bstatcollect::registration_int64(char *metric, metric_unit_t unit, int64_t value, char *descr)
{
   int index;
   bstatmetric *data;
   bool ifalloc;

   if (lock() != 0){
      return METRIC_INVALID;
   }

   index = checkreg(metric, ifalloc);
   if (ifalloc){
      data = New(bstatmetric(metric, unit, value, descr));
      metrics[index] = data;
   } else {
      metrics[index]->value.int64val = value;
   }

   if (unlock() != 0){
      return METRIC_INVALID;
   }

   return index;
};

/*
 * Floating point metric registration and initialization. Allocates the new bstatmetric class if required.
 *  If metric already exist it will update the metric value.
 *
 * in:
 *    metric - metric name to register
 *    type - metric type
 *    unit - metric unit
 *    descr - description of the metric
 * out:
 *    registered metric index - when metric already exist it return its index without allocating the new one
 *    you can get METRIC_INVALID value on any error
 */
int bstatcollect::registration_float(char *metric, metric_unit_t unit, float value, char *descr)
{
   int index;
   bstatmetric *data;
   bool ifalloc;

   if (lock() != 0){
      return METRIC_INVALID;
   }

   index = checkreg(metric, ifalloc);
   if (ifalloc){
      data = New(bstatmetric(metric, unit, value, descr));
      metrics[index] = data;
   } else {
      metrics[index]->value.floatval = value;
   }

   if (unlock() != 0){
      return METRIC_INVALID;
   }

   return index;
};

/*
 * Updates the Boolean metric value.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (set)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::set_value_bool(int metric, bool value)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_BOOL){
      metrics[metric]->value.boolval = value;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates the Integer metric value.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (set)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::set_value_int64(int metric, int64_t value)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_INT){
      metrics[metric]->value.int64val = value;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates the Integer metric value by adding the number.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (add)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::add_value_int64(int metric, int64_t value)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_INT){
      metrics[metric]->value.int64val += value;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates two Integer metric values by adding corresponding numbers.
 *
 * in:
 *    metric1 - the metric index to update
 *    value1 - value to update (add)
 *    metric2 - the metric index to update
 *    value2 - value to update (add)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::add2_value_int64(int metric1, int64_t value1, int metric2, int64_t value2)
{
   int stat;
   int rc = 0;

   if (!metrics && ((!(metric1 > METRIC_INVALID) && !(metric1 < maxindex))
         || (!(metric2 > METRIC_INVALID) && !(metric2 < maxindex)))){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric1] && metrics[metric1]->type == METRIC_INT){
      metrics[metric1]->value.int64val += value1;
   } else {
      rc = EINVAL;
   }
   if (metrics[metric2] && metrics[metric2]->type == METRIC_INT){
      metrics[metric2]->value.int64val += value2;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates the Integer metric value by substitute the number.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (substitute)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::sub_value_int64(int metric, int64_t value)
{
   return add_value_int64(metric, -value);
};

/*
 * Updates the Floating point metric value.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (set)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::set_value_float(int metric, float value)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_FLOAT){
      metrics[metric]->value.floatval = value;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates the Integer metric value by incrementing it.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (set)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::inc_value_int64(int metric)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_INT){
      metrics[metric]->value.int64val++;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 * Updates the Integer metric value by incrementing it.
 *
 * in:
 *    metric - the metric index to update
 *    value - value to update (set)
 * out:
 *    0 when update was successful
 *    EINVAL when metric index is invalid or points to different metric type
 *    errno on any other error
 */
int bstatcollect::dec_value_int64(int metric)
{
   int stat;
   int rc = 0;

   if (!metrics && !(metric > METRIC_INVALID) && !(metric < maxindex)){
      return EINVAL;
   }
   if ((stat = lock()) != 0){
      return stat;
   }

   if (metrics[metric] && metrics[metric]->type == METRIC_INT){
      metrics[metric]->value.int64val--;
   } else {
      rc = EINVAL;
   }

   if ((stat = unlock()) != 0){
      return stat;
   }
   return rc;
};

/*
 *
 */
int bstatcollect::dec_inc_values_int64(int metricd, int metrici)
{
   int rc = 0;

   lock();
   if (!metrics && !(metricd > METRIC_INVALID) && !(metricd < maxindex)
                && !(metrici > METRIC_INVALID) && !(metrici < maxindex)){
      rc = EINVAL;
   } else
      if (metrics[metricd] && metrics[metricd]->type == METRIC_INT &&
          metrics[metrici] && metrics[metrici]->type == METRIC_INT){
            metrics[metricd]->value.int64val--;
            metrics[metrici]->value.int64val++;
      } else {
         rc = EINVAL;
      }
   unlock();

   return rc;
};

/*
 * Return the Boolean value from metric.
 *  If metric index is invalid it will always return false.
 *  It does not check if the metric type is Boolean.
 */
bool bstatcollect::get_bool(int metric)
{
   bool val = false;

   lock();
   if (metrics && metric > METRIC_INVALID && metric < maxindex && metrics[metric]){
      val = metrics[metric]->value.boolval;
   }
   unlock();
   return val;
};

/*
 * Return the Integer value from metric.
 *  If metric index is invalid it will always return zero.
 *  It does not check if the metric type is Integer.
 */
int64_t bstatcollect::get_int(int metric)
{
   int64_t val = 0;

   lock();
   if (metrics && metric > METRIC_INVALID && metric < maxindex && metrics[metric]){
      val = metrics[metric]->value.int64val;
   }
   unlock();
   return val;
};

/*
 * Return the Floating point value from metric.
 *  If metric index is invalid it will always return zero.
 *  It does not check if the metric type is Floating point.
 */
float bstatcollect::get_float(int metric)
{
   float val = 0;

   lock();
   if (metrics && metric > METRIC_INVALID && metric < maxindex && metrics[metric]){
      val = metrics[metric]->value.floatval;
   }
   unlock();
   return val;
};

/*
 * Return all available (registered) metrics from metrics table as an array list.
 *
 * in:
 * out:
 *    alist - a newly allocated array list with all current metrics (bstatmetric); every metrics
 *          in the array list returned are newly allocated; all returned resources (array list and bstat metrics)
 *          should be freed when no needed. you can use free_metric_alist() for that.
 */
alist *bstatcollect::get_all()
{
   int a;
   alist *list = NULL;
   bstatmetric *data;

   if (metrics){
      list = New(alist(BSTATCOLLECT_NR, not_owned_by_alist));
      lock();
      for (a = 0; a < maxindex; a++){
         if (metrics[a]){
            data = New(bstatmetric);
#ifdef TEST_PROGRAM_DEBUG
            Pmsg0(0, "copy bstatmetric ...\n");
#endif
            *data = *metrics[a];
            list->append(data);
         }
      }
      unlock();
   }
   return list;
};

/*
 * Return the bstatmetric class pointed by a metric name.
 *  If metric name is invalid or not found or collector class has no metrics allocated
 *  it will always return Null.
 *
 * in:
 *    metric - a metric name to search
 * out:
 *    bstatmetric - a newly allocated and copied bstatmetric class when metric was found
 *    Null when metric not found
 */
bstatmetric *bstatcollect::get_metric(char* metric)
{
   int a;
   bstatmetric *data = NULL;

   if (nrmetrics && metrics && metric){
      /* we have some metrics, so check it */
      lock();
      for (a = 0; a < maxindex; a++){
         if (metrics[a]){
            if (metrics[a]->name && bstrcmp(metrics[a]->name, metric)){
               /* metric found return a copy of it */
               data = New(bstatmetric);
               *data = *metrics[a];
               break;
            }
         }
      }
      unlock();
   }
   return data;
};

/*
 * Return the bstatmetric class pointed by a metric name.
 *  If metric name is invalid or not found or collector class has no metrics allocated
 *  it will always return Null.
 *
 * in:
 *    metric - a metric name to search
 * out:
 *    bstatmetric - a newly allocated and copied bstatmetric class when metric was found
 *    Null when metric not found
 */
bstatmetric *bstatcollect::get_metric(int metric)
{
   bstatmetric *data = NULL;

   lock();
   if (nrmetrics && metrics && metric > METRIC_INVALID && metric < maxindex && metrics[metric]){
      data = New(bstatmetric);
      *data = *metrics[metric];
   }
   unlock();
   return data;
};

/*
 * Supported function releases array list of metric returned by get_all().
 */
void free_metric_alist(alist *list)
{
   bstatmetric *item;

   if (list){
      foreach_alist(item, list){
         delete (item);
      }
      delete(list);
   }
};

#ifndef TEST_PROGRAM
#define TEST_PROGRAM_A
#endif

/*
 * Dumps the content of bstatmetric class.
 *    Used for debugging only outside Bacula core.
 */
void bstatmetric::dump()
{
#ifdef TEST_PROGRAM
   char ed1[50];

   Pmsg4(-1, "name=\"%s\" type=%s unit=%s descr=\"%s\" value=",
      name ? name : "<*NULL*>",
      metric_type_str(),
      metric_unit_str(),
      description ? description : "<*NULL*>");
      switch (type){
         case METRIC_BOOL:
            Pmsg1(-1, "%s\n", value.boolval ? "True" : "False");
            break;
         case METRIC_INT:
            Pmsg1(-1, "%s\n", edit_uint64(value.int64val, ed1));
            break;
         case METRIC_FLOAT:
            Pmsg1(-1, "%f\n", value.floatval);
            break;
         default:
            Pmsg0(-1, "<unknown>\n");
            break;
      }
#endif
};

/*
 * Dumps the content of collector class including all metrics dump.
 *    Used for debugging only outside Bacula core.
 */
void bstatcollect::dump()
{
#ifdef TEST_PROGRAM
   int a;

   Pmsg1(-1, "\tbstatcollect::size %d\n", size);
   Pmsg1(-1, "\tbstatcollect::nrmetrics %d\n", nrmetrics);
   Pmsg1(-1, "\tbstatcollect::maxindex %d\n", maxindex);
   if (metrics){
      for (a = 0; a < maxindex; a++){
         if (metrics[a]){
            Pmsg1(-1, "\tbstatcollect::metric[%d]: ", a);
            metrics[a]->dump();
         } else {
            Pmsg1(-1, "\tbstatcollect::metric[%d]: EMPTY SLOT\n", a);
         }
      }
   } else {
      Pmsg0(-1, "\tbstatcollect uninitialized\n");
   }
#endif
};

#ifdef TEST_PROGRAM
#include "unittests.h"

int main()
{
   Unittests bstat_test("bstat_test", true);
   bstatcollect *collector;
   int m1, m1a, m2, m2a, m3, m4;
   alist *all;
   int rc;
   bstatmetric *item;
   char *metric1 = (char*)"bacula.test.metric";
   char *metric2 = (char*)"bacula.test.other";
   char *metric3 = (char*)"bacula.test.third";
   char *metric4 = (char*)"bacula.test.four.bool";
   char *descr1 = (char*)"Test bacula.test.metric description";
   char *descr2 = (char*)"Test bacula.test.other description";
   char *descr3 = (char*)"Test bacula.test.third description";
   char *descr4 = (char*)"Test bacula.test.four.bool description";

   Pmsg0(0, "Initialize tests ...\n");

   collector = New(bstatcollect);
   ok(collector->_check_size(BSTATCOLLECT_NR) &&
         collector->_check_nrmetrics(0) &&
         collector->_get_maxindex(0),
         "Default initialization");

   Pmsg0(0, "Simple registration tests ...\n");

   m1 = collector->registration_int64(metric1, METRIC_UNIT_JOB, (int64_t)123, descr1);
   ok(collector->_check_nrmetrics(1), "Registration int64_t nrmetrics");
   ok(collector->_get_maxindex(1), "Registration int64_t maxindex");
   ok(m1 == 0, "Registration int64_t metricid");

   m2 = collector->registration_float(metric2, METRIC_UNIT_PERCENT, (float)90.0, descr2);
   ok(collector->_check_nrmetrics(2), "Registration float nrmetrics");
   ok(collector->_get_maxindex(2), "Registration float maxindex");
   ok(m2 == 1, "Registration float metricid");

   m4 = collector->registration(metric4, METRIC_BOOL, METRIC_UNIT_EMPTY, descr4);
   ok(collector->_check_nrmetrics(3), "Registration METRIC_BOOL nrmetrics");
   ok(collector->_get_maxindex(3), "Registration METRIC_BOOL maxindex");
   ok(m4 == 2, "Registration METRIC_BOOL metricid");

   Pmsg0(0, "Update tests ...\n");

   collector->set_value_int64(m1, 234);
   ok(collector->get_int(m1), "Set int64_t");

   collector->set_value_float(m2, 10.0);
   ok(collector->get_float(m2), "Set float");

   collector->set_value_bool(m4, true);
   ok(collector->get_bool(m4), "Set bool");

   rc = collector->set_value_float(m1, -1.234);
   ok(rc, "Update int64_t by float return status");
   ok(collector->get_int(m1) == 234, "Update int64_t by float value");

   rc = collector->add_value_int64(m2, 1234);
   ok(rc, "Add int64_t on float return status");
   ok(collector->get_float(m2) == 10.0, "Add int64_t on float value");

   Pmsg0(0, "Double Registration tests ...\n");

   m1a = collector->registration_int64(metric1, METRIC_UNIT_JOB, (int64_t)321, descr2);
   ok(m1 == m1a, "Registration metricid");
   item = collector->get_metric(m1);
   ok(item->value.int64val == 321, "New metric value");
   ok(strcmp(descr2, item->description), "New metric descr");
   delete(item);

   m2a = collector->registration(metric2, METRIC_INT, METRIC_UNIT_JOB, descr3);
   ok(m2 == m2a, "Registration metricid");

   Pmsg0(0, "Unregistration tests ...\n");
   collector->unregistration(m1);
   ok(collector->_check_nrmetrics(2), "Unregistration nrmetrics");
   ok(collector->_get_maxindex(3), "Unregistration maxindex");
   item = collector->get_metric(m1);
   nok(item != NULL, "Get unregistered metric");
   if (item){
      item->dump();
      delete(item);
   }
   m3 = collector->registration_int64(metric3, METRIC_UNIT_BYTE, (int64_t)999, descr3);
   ok(collector->_check_nrmetrics(3), "Registration again nrmetrics");
   ok(collector->_get_maxindex(3), "Registration again maxindex");
   ok(m3 == 0, "Registration again metricid");

   Pmsg0(0, "Get value tests ...\n");
   ok(collector->get_float(m2) == 10.0, "Get value float");
   ok(collector->get_int(m3) == 999, "Get value int64_t");
   ok(collector->get_bool(m4), "Get value bool");
   nok(collector->get_int(m3+1000), "Get value from nonexistent metric");

   Pmsg0(0, "Get all values list tests ...\n");
   all = collector->get_all();
   ok(all != NULL, "Return list pointer");
   ok(all->size() == 3, "Return list size");

   Pmsg0(0, "All metrics list:\n");
   foreach_alist(item, all){
      item->dump();
   }
   free_metric_alist(all);
   delete(collector);
   return report();
};

#endif /* TEST_PROGRAM */
