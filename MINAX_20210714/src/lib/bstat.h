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

#ifndef __BSTAT_H_
#define __BSTAT_H_
#include "bacula.h"


#define     METRIC_INVALID          -1
#define     BSTATCOLLECT_NR         100
#define     BSTATCOLLECT_STEP       10

/* metric types supported */
typedef enum {
    METRIC_UNDEF = 0,
    METRIC_INT,
    METRIC_BOOL,
    METRIC_FLOAT,
} metric_type_t;

/* metric units supported */
typedef enum {
    METRIC_UNIT_EMPTY = 0,
    METRIC_UNIT_BYTE,
    METRIC_UNIT_BYTESEC,
    METRIC_UNIT_JOB,
    METRIC_UNIT_CLIENT,
    METRIC_UNIT_FILESET,
    METRIC_UNIT_POOL,
    METRIC_UNIT_STORAGE,
    METRIC_UNIT_SCHEDULE,
    METRIC_UNIT_FILE,
    METRIC_UNIT_VOLUME,
    METRIC_UNIT_SEC,
    METRIC_UNIT_MSEC,
    METRIC_UNIT_NUMBER,
    METRIC_UNIT_STATUS,
    METRIC_UNIT_PERCENT,
    METRIC_UNIT_DEVICE,
    METRIC_UNIT_AUTOCHANGER,
} metric_unit_t;
// some aliases
#define  METRIC_UNIT_BYTES    METRIC_UNIT_BYTE

/* the metric value */
typedef union {
    int64_t int64val;
    float floatval;
    bool boolval;
} metric_value_t;

/* This is a metric class */
class bstatmetric : public SMARTALLOC {
public:
    char *name;                             /* this is a metric name */
    metric_type_t type;                     /* this is a metric type */
    metric_unit_t unit;                     /* this is a metric unit */
    metric_value_t value;                   /* this is a metric value */
    char *description;                      /* this is a metric description */
    bstatmetric();
    bstatmetric(char *mname, metric_type_t mtype, metric_unit_t munit, char *descr);
    bstatmetric(char *mname, metric_unit_t munit, bool mvalue, char *descr);
    bstatmetric(char *mname, metric_unit_t munit, int64_t mvalue, char *descr);
    bstatmetric(char *mname, metric_unit_t munit, float mvalue, char *descr);
    ~bstatmetric();
    bstatmetric& operator=(const bstatmetric& orig);
    void render_metric_value(POOLMEM **buf, bool bstr=false);
    void render_metric_value(POOL_MEM &buf, bool bstr=false);
    const char *metric_type_str();
    const char *metric_unit_str();
    void dump();
private:
    void init(char *mname, metric_type_t mtype, metric_unit_t munit, char *descr);
};

/* update statcollector with macros */
#define  collector_update_set_value_bool(sc, m, v)                (sc && sc->set_value_bool(m, v))
#define  collector_update_set_value_int64(sc, m, v)               (sc && sc->set_value_int64(m, v))
#define  collector_update_add_value_int64(sc, m, v)               (sc && sc->add_value_int64(m, v))
#define  collector_update_add2_value_int64(sc, m1, v1, m2, v2)    (sc && sc->add2_value_int64(m1, v1, m2, v2))
#define  collector_update_sub_value_int64(sc, m, v)               (sc && sc->sub_value_int64(m, v))
#define  collector_update_set_value_float(sc, m, v)               (sc && sc->set_value_float(m, v))
#define  collector_update_inc_value_int64(sc, m)                  (sc && sc->inc_value_int64(m))
#define  collector_update_dec_value_int64(sc, m)                  (sc && sc->dec_value_int64(m))
#define  collector_update_dec_inc_values_int64(sc, md, mi)        (sc && sc->dec_inc_values_int64(md, mi))

/* This is an internal collector class */
class bstatcollect : public SMARTALLOC {
    bstatmetric **metrics;
    int size;
    int nrmetrics;
    int maxindex;
    pthread_mutex_t mutex;

    int lock();
    int unlock();
    void check_size(int newsize);
    int checkreg(char *metric, bool &ifalloc);

public:
    bstatcollect();
    ~bstatcollect();

    /* registration return a metric index */
    int registration(char *metric, metric_type_t type, metric_unit_t unit, char *descr);
    int registration(const char *metric, metric_type_t type, metric_unit_t unit, const char *descr){
       return registration((char*)metric, type, unit, (char*)descr);
    };
    int registration(char *metric, metric_type_t type, metric_unit_t unit, const char *descr){
       return registration(metric, type, unit, (char*)descr);
    };
    int registration_bool(char *metric, metric_unit_t unit, bool value, char *descr);
    int registration_int64(char *metric, metric_unit_t unit, int64_t value, char *descr);
    int registration_int64(const char *metric, metric_unit_t unit, int64_t value, const char *descr){
       return registration_int64((char*)metric, unit, value, (char*)descr);
    };
    int registration_int64(char *metric, metric_unit_t unit, int64_t value, const char *descr){
       return registration_int64(metric, unit, value, (char*)descr);
    };
    int registration_float(char *metric, metric_unit_t unit, float value, char *descr);
    /* unregistration */
    void unregistration(int metric);
    /* update/set the metric value */
    int set_value_bool(int metric, bool value);
    int set_value_int64(int metric, int64_t value);
    int add_value_int64(int metric, int64_t value);
    int add2_value_int64(int metric1, int64_t value1, int metric2, int64_t value2);
    int sub_value_int64(int metric, int64_t value);
    int set_value_float(int metric, float value);
    int inc_value_int64(int metric);
    int dec_value_int64(int metric);
    int dec_inc_values_int64(int metricd, int metrici);
    /* get data */
    bool get_bool(int metric);
    int64_t get_int(int metric);
    float get_float(int metric);
    alist *get_all();
    bstatmetric *get_metric(int metric);
    bstatmetric *get_metric(char *metric);
    /* utility */
    void dump();
    /* for unit tests only */
    int _check_size(int n) { return size == n; };
    int _check_nrmetrics(int n) { return nrmetrics == n; };
    int _get_maxindex(int n) { return maxindex == n; };
};

/* supported utilities */
void free_metric_alist(alist *list);

#endif /* __BSTAT_H_ */
