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

#include "config-storage.h"
#include <android/log.h>
#include <iostream>
#include <fstream>
#include "resmodel.h"

extern URES res_all;
extern char *configfile;        // defined in tray-monitor.cpp

/* Check for \ at the end */
static char *is_str_valid(POOLMEM **buf, const char *p)
{
   char *p1;
   if (!p || !*p) {
      return NULL;
   }
   p1 = *buf = check_pool_memory_size(*buf, (strlen(p) + 1));
   for (; *p ; p++) {
      if (*p == '\\') {
         *p1++ = '/';

      } else if (*p == '"') {
         return NULL;

      } else {
         *p1++ = *p;
      }
   }
   *p1 = 0;
   return *buf;
}

bool ConfigStorage::reloadResources()
{    
   reloadResources(false);
}

bool ConfigStorage::reloadResources(bool encodePwd)
{
   bool ret;
   config = New(CONFIG());
   config->encode_password(encodePwd);
   config->init(configfile, NULL, M_ERROR, (void *)&res_all, res_all_size,
                r_first, r_last, resources, &rhead);
   ret = config->parse_config();
   return ret;
}


MONITOR *ConfigStorage::getMonitor() {
    MONITOR *mon = NULL;
    mon = (MONITOR *) GetNextRes(rhead, R_MONITOR, NULL);
    return mon;
}

const char *ConfigStorage::saveMonitor(const char *deviceId) {
    MONITOR mon;
    POOLMEM *monName = get_pool_memory(PM_FNAME);
    Mmsg(monName, "%s-mon", deviceId);
    mon.hdr.name = monName;
    const char *err_msg = saveMonitor(&mon);
    free_and_null_pool_memory(monName);
    return err_msg;
}

const char *ConfigStorage::saveMonitor(MONITOR *mon)
{
    const char *error_msg = NULL;
    POOL_MEM tmp_fname;
    Mmsg(tmp_fname, "%s.temp", configfile);
    FILE *fp = fopen(tmp_fname.c_str(), "w");

    if(!fp) {
        return "Error - unable to open configuration file";
    }

    writeMonitor(fp, mon);

    QList<RESMON *> *resources = getAllResources();
    for(RESMON *res : *resources)  {
        writeResource(fp, res, res->code);
    }

    fclose(fp);
    unlink(configfile);

    int result = rename(tmp_fname.c_str(), configfile);

    if (result != 0) {
        error_msg = "Error - unable to write to the configuration file";
    }

    if(error_msg == NULL) {
      reloadResources();
      dump_storage();
    }

    return error_msg;
}

RESMON *ConfigStorage::getResourceByName(rescode resCode, const char* resName) {
   RESMON *res = NULL;
   QList<RESMON *> *resources = getResources(resCode);

   for(RESMON *r : *resources)  {
       if(strcmp(r->hdr.name, resName) == 0) {
            res = r;
       }
   }

   return res;
}

QList<RESMON *> *ConfigStorage::getResources(rescode resCode) {
    QList<RESMON *> *resources = new QList<RESMON *>();
    RESMON *res;

    for(res=NULL; (res=(RESMON *)GetNextRes(rhead, resCode, (RES*)res));) {
        res->code = resCode;
        resources->push_back(res);
    }

    return resources;
}

QList<RESMON *> *ConfigStorage::getAllResources() {
    QList<RESMON *> *resources = new QList<RESMON *>();
    RESMON *res;

    for(res=NULL; (res=(RESMON *)GetNextRes(rhead, R_CLIENT, (RES*)res));) {
        res->code = R_CLIENT;
        resources->push_back(res);
    }

    for(res=NULL; (res=(RESMON *)GetNextRes(rhead, R_DIRECTOR, (RES*)res));) {
        res->code = R_DIRECTOR;
        resources->push_back(res);
    }

    for(res=NULL; (res=(RESMON *)GetNextRes(rhead, R_STORAGE, (RES*)res));) {
        res->code = R_STORAGE;
        resources->push_back(res);
    }

    return resources;
}

const char *ConfigStorage::saveResources(
        QList<QObject *> *clients,
        QList<QObject *> *directors,
        QList<QObject *> *storages
        )
{
    const char *error_msg = NULL;

    POOL_MEM tmp_fname;
    Mmsg(tmp_fname, "%s.temp", configfile);
    FILE *fp = fopen(tmp_fname.c_str(), "w");

    if(!fp) {
        return "Unable to open configuration file";
    }

    MONITOR *mon = getMonitor();
    writeMonitor(fp, mon);

    for(QObject *obj : *clients)  {
        ResourceModel *model = (ResourceModel *) obj;
        RESMON *res = model->resource();
        writeResource(fp, res, R_CLIENT);
    }

    for(QObject *obj : *directors)  {
        ResourceModel *model = (ResourceModel *) obj;
        RESMON *res = model->resource();
        writeResource(fp, res, R_DIRECTOR);
    }

    for(QObject *obj : *storages)  {
        ResourceModel *model = (ResourceModel *) obj;
        RESMON *res = model->resource();
        writeResource(fp, res, R_STORAGE);
    }

    fclose(fp);
    unlink(configfile);
    int result = rename(tmp_fname.c_str(), configfile);

    if (result != 0) {
        error_msg = "Unable to write to the configuration file";
    }

    reloadResources();
    return error_msg;
}

void ConfigStorage::writeMonitor(FILE *fp, MONITOR *mon) {
    fprintf(fp, "Monitor {\n Name = \"%s\"\n", mon->hdr.name);
    fprintf(fp, " Refresh Interval = %d\n", 120);
    fprintf(fp, "}\n");
}


const char *ConfigStorage::addResource(const char *resName, rescode code) {
    return addResource(resName, code, false);
}

const char *ConfigStorage::addResource(const char *resName, rescode code, bool managed) {
    RESMON res;
    res.address = "localhost";
    res.password = (char *) AndroidFD::devicePwd().toUtf8().constData();
    res.managed = managed;
    POOLMEM *pm_resName = get_pool_memory(PM_FNAME);

    switch (code) {
    case R_CLIENT:
        Mmsg(pm_resName, "%s-fd", resName);
        res.port = 9102;
        res.use_remote = false;
        break;
    case R_DIRECTOR:
        Mmsg(pm_resName, "%s-dir", resName);
        res.port = 9101;
        break;
    case R_STORAGE:
        Mmsg(pm_resName, "%s-sd", resName);
        res.port = 9103;
        break;
    default:
        return NULL;
    }

    res.hdr.name = pm_resName;
    const char *err_msg = addResource(&res, code);
    free_and_null_pool_memory(pm_resName);
    return err_msg;
}

const char *ConfigStorage::addResource(RESMON *newRes, rescode code) {
    const char *error_msg = validateResource(newRes);

    if(error_msg != NULL) {
        return error_msg;
    }

    FILE *fp = fopen(configfile, "a");

    if(!fp) {
        return "Unable to open configuration file";
    }

    writeResource(fp, newRes, code);
    fclose(fp);
    reloadResources();
    return error_msg;
}

const char *ConfigStorage::editResource(RESMON *oldRes, RESMON *newRes, rescode resCode) {
    const char *error_msg = validateResource(newRes);

    if(error_msg != NULL) {
        return error_msg;
    }

    POOL_MEM tmp_fname;
    Mmsg(tmp_fname, "%s.temp", configfile);
    FILE *fp = fopen(tmp_fname.c_str(), "w");

    if(!fp) {
        return "Unable to open configuration file";
    }

    MONITOR *mon = getMonitor();
    writeMonitor(fp, mon);

    QList<RESMON *> *resources = getAllResources();
    for(RESMON *res : *resources)  {
        if(strcmp(res->hdr.name, oldRes->hdr.name) == 0) {
            writeResource(fp, newRes, resCode);
        }
        else {
            writeResource(fp, res, res->code);
        }
    }

    fclose(fp);
    unlink(configfile);
    int result = rename(tmp_fname.c_str(), configfile);

    if (result != 0) {
        error_msg = "Unable to write to the configuration file";
    }

    if(error_msg == NULL) {
      reloadResources();
      dump_storage();
    }

    return error_msg;
}

const char *ConfigStorage::validateResource(RESMON *res) {
    POOLMEM *buf1 = get_pool_memory(PM_FNAME);
    POOLMEM *buf2 = get_pool_memory(PM_FNAME);
    POOLMEM *buf3 = get_pool_memory(PM_FNAME);

    res->hdr.name = is_str_valid(&buf1, res->hdr.name);

    if(!res->hdr.name) {
        return "The name of the Resource should be set";
    }

    res->address = is_str_valid(&buf2, res->address);

    if(!res->address) {
        return "The address of the Resource should be set";
    }

    res->password = is_str_valid(&buf3, res->password);

    if(!res->password) {
        return "The password of the Resource should be set";
    }

    return NULL;
}

void ConfigStorage::writeResource(FILE *fp, RESMON *res, rescode code) {
    const char *resType;

    switch (code) {
    case R_CLIENT:
        resType = "Client";
        break;
    case R_DIRECTOR:
        resType = "Director";
        break;
    case R_STORAGE:
        resType = "Storage";
        break;
    default:
        return;
    }

    fprintf(fp, "%s {\n Name = \"%s\"\n Address = \"%s\"\n", resType, res->hdr.name, res->address);
    fprintf(fp, " Password = \"%s\"\n", res->password);
    fprintf(fp, " Port = %d\n", res->port);
    fprintf(fp, " Connect Timeout = %d\n", 10);

    if (res->use_remote) {
       fprintf(fp, " Remote = yes\n");
    }

    if (res->managed) {
       fprintf(fp, " Managed = yes\n");
    }

    fprintf(fp, "}\n");
}


void ConfigStorage::dump_storage() {
    __android_log_write(ANDROID_LOG_DEBUG, "Configuration File:", configfile);
    std::string line;
    std::ifstream myfile(configfile);
    if (myfile.is_open())
    {
        __android_log_write(ANDROID_LOG_DEBUG, "Success!", "File was opened");
      while ( getline (myfile, line) )
      {
          __android_log_write(ANDROID_LOG_DEBUG, "Line:", line.c_str());
      }
      myfile.close();
    } else {
        __android_log_write(ANDROID_LOG_DEBUG, "Error!", "Unable to open file");
    }
}



