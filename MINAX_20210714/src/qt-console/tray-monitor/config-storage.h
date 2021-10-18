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

#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "common.h"
#include "tray_conf.h"
#include "android-fd-service.h"

/*
   ConfigStorage - Responsible for managing all operations (reads, updates, writes) in
   the bacula-tray-monitor.conf file.
*/
class ConfigStorage
{

public:
    static ConfigStorage& getInstance() {
        static ConfigStorage instance;
        return instance;
    }

    // Creates the singleton instance
    static bool init(const char *config_path) {
       struct stat statp;
       ConfigStorage *storage = &ConfigStorage::getInstance();
       storage->config_path = config_path;

       /**
        * If tray-monitor.conf does not exist yet,
        * create it with default resources.
       */
       if (stat(config_path, &statp) != 0) {
           const char *err_msg;
           char *deviceId = bstrdup(AndroidFD::deviceId().toLatin1().constData());
           err_msg = storage->addResource(deviceId, R_CLIENT, true);

           if (err_msg != NULL) {
               Dmsg1(0, "Error - could not save default client resource - %s\n", err_msg);
               return false;
           }

           err_msg = storage->saveMonitor(deviceId);

           if (err_msg != NULL) {
               Dmsg1(0, "Error - could not save default monitor resource - %s\n", err_msg);
               return false;
           }

           free(deviceId);
       }

       return storage->reloadResources();
    }

private:
    CONFIG *config = NULL;
    RES_HEAD **rhead;

    explicit ConfigStorage() {}
    void writeMonitor(FILE *fp, MONITOR *mon);
    void writeResource(FILE *fp, RESMON *res, rescode code);
    const char *validateResource(RESMON *res);

public:
    QString config_path;
    bool reloadResources();
    bool reloadResources(bool encodePassword);

    const char *saveMonitor(const char *deviceId);
    const char *saveMonitor(MONITOR *mon);
    MONITOR *getMonitor();

    const char *addResource(const char *resName, rescode code);
    const char *addResource(const char *resName, rescode code, bool managed);
    const char *addResource(RESMON *res, rescode code);
    const char *editResource(RESMON *oldRes, RESMON *newRes, rescode code);
    const char *saveResources(
            QList<QObject *> *clients,
            QList<QObject *> *directors,
            QList<QObject *> *storages
            );

    QList<RESMON *> *getAllResources();
    QList<RESMON *> *getResources(rescode resCode);
    RESMON *getResourceByName(rescode resCode, const char* resName);

    //Debug Only
    void dump_storage();
};

#endif // CONFIG_STORAGE_H
