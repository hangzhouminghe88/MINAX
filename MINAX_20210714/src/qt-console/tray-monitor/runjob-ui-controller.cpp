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

#include "runjob-ui-controller.h"

RunJobUiController::RunJobUiController(QObject *parent):
   QObject(parent)
{
   m_config = &ConfigStorage::getInstance();
}

void RunJobUiController::handleSelectedJobChange(int jobIndex, QString jobLevel)
{
   if (isConnecting()) {
       return;
   }

   setIsConnecting(true);
   RESMON *dir = m_rJobModel->getDirector();
   POOL_MEM jobName;
   POOL_MEM level;

   char *p = (char *) dir->jobs->get(jobIndex);
   pm_strcpy(jobName, p);

   p = jobLevel.toUtf8().data();
   pm_strcpy(level, p);

   task *t = new task();
   t->init(dir, TASK_INFO);
   connect(t, SIGNAL(done(task *)), this, SLOT(jobInfoCallback(task *)), Qt::QueuedConnection);
   t->arg = jobName.c_str();
   t->arg2 = level.c_str();
   dir->wrk->queue(t);

   m_selectedJobIndex = jobIndex;
}

void RunJobUiController::jobInfoCallback(task *t)
{
   RESMON *dir = m_rJobModel->getDirector();
   char buf[50];
   edit_uint64_with_suffix(dir->infos.JobBytes, buf);
   bstrncat(buf, "B", sizeof(buf));
   setFilesDescription(
            QString("%1 Files (%2)")
            .arg(dir->infos.JobFiles)
            .arg(QString(buf))
            );

   delete t;

   // Now fetch job defaults
   t = new task();
   QString selectedJob = m_jobNames[m_selectedJobIndex];
   char *job = bstrdup(selectedJob.toUtf8().data());

   Dmsg1(10, "get defaults for %s\n", job);
   dir->mutex->lock();
   bfree_and_null(dir->defaults.job);
   dir->defaults.job = job;
   dir->mutex->unlock();

   connect(t, SIGNAL(done(task *)), this, SLOT(jobDefaultsCallback(task *)), Qt::QueuedConnection);
   t->init(dir, TASK_DEFAULTS);
   dir->wrk->queue(t);
   setIsConnecting(false);
}

int RunJobUiController::findIndex(QStringList list, QString str)
{
   for (int i = 0; i < list.size(); i++) {
      if (list[i] == str) {
         return i;
      }
   }

   Dmsg1(0, "Error: could not find default index. Value: %s\n", str.toLatin1().constData());
   return 0;
}

void RunJobUiController::jobDefaultsCallback(task *t)
{
   if (t->status == true) {
      RESMON *dir = m_rJobModel->getDirector();
      dir->mutex->lock();
      int levelIndex = this->findIndex(m_jobLevel, dir->defaults.level);
      int filesetIndex = this->findIndex(m_filesetNames, dir->defaults.fileset);
      int clientIndex = this->findIndex(m_clientNames, dir->defaults.client);
      int storageIndex = this->findIndex(m_storageNames, dir->defaults.storage);
      int poolIndex = this->findIndex(m_poolNames, dir->defaults.pool);
      int catalogIndex = this->findIndex(m_catalogNames, dir->defaults.catalog);
      setLevel(levelIndex);
      setFileset(filesetIndex);
      setClient(clientIndex);
      setStorage(storageIndex);
      setPool(poolIndex);
      setCatalog(catalogIndex);
      dir->mutex->unlock();
   }

   t->deleteLater();
   setIsConnecting(false);
}

void RunJobUiController::runJob()
{
   if (isConnecting()) {
       return;
   }

   setIsConnecting(true);
   RESMON *dir = m_rJobModel->getDirector();
   POOL_MEM command;
   POOL_MEM tmp;
   char *p;

   p = (char *) dir->jobs->get(m_selectedJobIndex);
   Mmsg(command, "run job=\"%s\" yes", p);

   p = (char *) dir->storages->get(m_selectedStorage);
   Mmsg(tmp, " storage=\"%s\"", p);
   pm_strcat(command, tmp.c_str());

   p = (char *) dir->clients->get(m_selectedClient);

   RESMON *client = m_config->getResourceByName(R_CLIENT, p);
   if (client != NULL && client->managed) {
       AndroidFD::start();
   }

   Mmsg(tmp, " client=\"%s\"", p);
   pm_strcat(command, tmp.c_str());

   p = m_jobLevel[m_selectedLevel].toUtf8().data();
   Mmsg(tmp, " level=\"%s\"", p);
   pm_strcat(command, tmp.c_str());

   p = (char *) dir->pools->get(m_selectedPool);
   Mmsg(tmp, " pool=\"%s\"", p);
   pm_strcat(command, tmp.c_str());

   p = (char *) dir->filesets->get(m_selectedFileset);
   Mmsg(tmp, " fileset=\"%s\"", p);
   pm_strcat(command, tmp.c_str());

   Mmsg(tmp, " priority=\"%d\"", m_priority.toUInt());
   pm_strcat(command, tmp.c_str());

   //    if (res->type == R_CLIENT) {
   //       pm_strcat(command, " fdcalled=1");
   //    }

   task *t = new task();
   t->init(dir, TASK_RUN);
   connect(t, SIGNAL(done(task *)), this, SLOT(runJobCallback(task *)), Qt::QueuedConnection);
   t->arg = command.c_str();
   dir->wrk->queue(t);
}

void RunJobUiController::runJobCallback(task *t)
{
   emit jobSuccess();
   delete t;
   setIsConnecting(false);
}

RunJobUiController::~RunJobUiController()
{}
