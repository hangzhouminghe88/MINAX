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

#include "resdetails-ui-controller.h"

ResDetailsUiController::ResDetailsUiController(QObject *parent):
    QObject(parent)
{
    m_storage = &ConfigStorage::getInstance();
}

void ResDetailsUiController::connectToResource()
{
    if(m_res == NULL) {
        return;
    }

    if (!isConnecting()) {
        setIsConnecting(true);

        if (m_res->managed) {
            AndroidFD::start();
        }

        task *t = new task();
        connect(t, SIGNAL(done(task *)), this, SLOT(connectCallback(task *)), Qt::QueuedConnection);

        //We get a version of the Resource that contains an encoded password
        m_storage->reloadResources(true);
        m_res = m_storage->getResourceByName(m_res->code, m_res->hdr.name);
        m_storage->reloadResources();

        t->init(m_res, TASK_STATUS);
        m_res->wrk->queue(t);
        emit connectionStarted();
   }
}

void ResDetailsUiController::connectCallback(task *t)
{

    if (!t->status) {
        setConnectionError(t->errmsg);
    } else {
        emit connectionSuccess();
        setStartedDate(m_res->started);
        setResourceVersion(m_res->version);
        setResourcePlugins(m_res->plugins);
        setBandwidthLimit(QString::number(m_res->bwlimit));

        QList<QObject *> *tJobs = new QList<QObject*>();
        struct s_last_job *job;

        foreach_dlist(job, m_res->terminated_jobs) {
            JobModel *model = new JobModel();
            model->setData(job);
            tJobs->append(model);
        }

        setTerminatedJobs(tJobs);

        QList<QObject *> *rJobs = new QList<QObject*>();
        struct s_running_job *rjob;

        foreach_alist(rjob, m_res->running_jobs) {
            JobModel *model = new JobModel();
            model->setData(rjob);
            rJobs->append(model);
        }

        setRunningJobs(rJobs);
    }

    t->deleteLater();
    setIsConnecting(false);
}

void ResDetailsUiController::createRunJobModel() {
    if (!isConnecting()) {
        setIsConnecting(true);
        task *t = new task();
        connect(t, SIGNAL(done(task *)), this, SLOT(createBackupModelCallback(task *)), Qt::QueuedConnection);
        t->init(m_res, TASK_RESOURCES);
        m_res->wrk->queue(t);
    }
}

void ResDetailsUiController::createBackupModelCallback(task *t) {
    RunJobModel *model = new RunJobModel();
    model->setDirector(t->res);
    t->deleteLater();
    setIsConnecting(false);
    emit runJobModelCreated(model);
}

void ResDetailsUiController::createRestoreJobModel() {
    if (!isConnecting()) {
        setIsConnecting(true);
        task *t = new task();
        connect(t, SIGNAL(done(task *)), this, SLOT(createRestoreModelCallback(task *)), Qt::QueuedConnection);
        t->init(m_res, TASK_RESOURCES);
        m_res->wrk->queue(t);
    }
}

void ResDetailsUiController::createRestoreModelCallback(task *t) {
    RestoreJobModel *model = new RestoreJobModel();
    model->setDirector(t->res);
    t->deleteLater();
    setIsConnecting(false);
    emit restoreModelCreated(model);
}

ResDetailsUiController::~ResDetailsUiController() {}
