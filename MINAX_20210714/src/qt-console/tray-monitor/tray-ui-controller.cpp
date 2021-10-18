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

#include "tray-ui-controller.h"

TrayUiController::TrayUiController(QObject *parent):
    QObject(parent),
    m_monitorName(""),
    m_infoDialogText(""),
    m_config(NULL),
    m_clients(NULL),
    m_directors(NULL),
    m_storages(NULL)
{
    m_config = &ConfigStorage::getInstance();
    m_config->dump_storage();
    m_traceWatcher = new QFileSystemWatcher(this);
    connect(m_traceWatcher, SIGNAL(fileChanged(QString)), this, SLOT(handleFdTraceChange(QString)), Qt::QueuedConnection);
    m_traceWatcher->addPath(AndroidFD::tracePath());
    MONITOR *mon = m_config->getMonitor();
    m_monitorName = QString(mon->hdr.name);
    fetchClients();
    fetchDirectors();
    fetchStorages();
}

QString TrayUiController::fetchFile(QString fpath) {
    QByteArray fileBytes;
    QFile file(fpath);
    file.open(QIODevice::ReadOnly);
    file.seek(file.size() - 20000);
    fileBytes = file.read(20000);
    return QString(fileBytes);
}

void TrayUiController::fetchClients() {
    QList<QObject *> *clients = fetchResources(R_CLIENT);
    setClients(clients);
}

void TrayUiController::fetchDirectors() {
    QList<QObject *> *directors = fetchResources(R_DIRECTOR);
    setDirectors(directors);
}

void TrayUiController::fetchStorages() {
    QList<QObject *> *storages = fetchResources(R_STORAGE);
    setStorages(storages);
}

QList<QObject *> *TrayUiController::fetchResources(rescode resCode) {
    QList<RESMON *> *resources = m_config->getResources(resCode);
    QList<QObject *> *models = new QList<QObject*>();

    for(RESMON *res : *resources) {
        ResourceModel *model = new ResourceModel();
        model->setResource(res, resCode);
        models->append(model);
    }

    delete resources;
    return models;
}

void TrayUiController::deleteResource(ResourceModel *resModel) {
    const char *error_msg = NULL;

    switch(resModel->resource()->code) {
    case R_CLIENT:
        m_clients->removeAll(resModel);
        error_msg = m_config->saveResources(m_clients, m_directors, m_storages);
        emit clientsChanged();
        break;
    case R_DIRECTOR:
        m_directors->removeAll(resModel);
        error_msg = m_config->saveResources(m_clients, m_directors, m_storages);
        emit directorsChanged();
        break;
    case R_STORAGE:
        m_storages->removeAll(resModel);
        error_msg = m_config->saveResources(m_clients, m_directors, m_storages);
        emit storagesChanged();
        break;
    default:
        break;
    }
}

void TrayUiController::startFileDaemon() {
    AndroidFD::start();
}

void TrayUiController::stopFileDaemon() {
    AndroidFD::stop();
}

void TrayUiController::composeTraceMail() {
    AndroidFD::composeTraceMail();
}

QString TrayUiController::getMonitorName() {
    return m_monitorName;
}

void TrayUiController::setMonitorName(QString name) {
    MONITOR *newMon = new MONITOR();
    const char *err_msg = NULL;
    newMon->hdr.name = bstrdup(name.toLatin1().constData());
    err_msg = m_config->saveMonitor(newMon);

    if (err_msg != NULL) {
        Dmsg1(0, "Error - could not save monitor name - %s\n", err_msg);
    }

    m_monitorName = name;
    free(newMon->hdr.name);
}

void TrayUiController::handleFdTraceChange(QString) {
    emit fdTraceChanged();
}

TrayUiController::~TrayUiController()
{
    if (m_clients != NULL) {
        delete m_clients;
    }

    if (m_directors != NULL) {
        delete m_directors;
    }

    if (m_storages != NULL) {
        delete m_storages;
    }

    if (m_traceWatcher != NULL) {
        m_traceWatcher->removePath(AndroidFD::tracePath());
        delete m_traceWatcher;
    }
}
