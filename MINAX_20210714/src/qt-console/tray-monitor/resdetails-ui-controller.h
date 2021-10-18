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

#ifndef RESCONFIGUICONTROLLER_H
#define RESCONFIGUICONTROLLER_H

#include <QObject>
#include <QString>
#include "tray_conf.h"
#include "config-storage.h"
#include "resmodel.h"
#include <android/log.h>
#include "jcr.h"
#include "task.h"
#include "common.h"
#include "jobmodel.h"
#include "runjobmodel.h"
#include "restorejobmodel.h"

/*
   TrayUiController - Controls the screen displayed when a user selects one resource
   (Director, File Daemon or Storage Daemon). It allows the user to:

   1 - Change the data that's required to connect with the resource (name, password, address and port)
   2 - Connect to the resource, and therefore see it's terminated jobs and running jobs

   Also, if the resource is a director, it allows the user to start new backup jobs or restore jobs
*/
class ResDetailsUiController : public QObject
{
   Q_OBJECT

   // Data related to the resource that the user selected
   Q_PROPERTY(ResourceModel *resModel WRITE setResModel)

   Q_PROPERTY(QString resourceName READ resourceName WRITE setResourceName NOTIFY resourceNameChanged)

   //These are loaded after successfully connecting to the resource
   Q_PROPERTY(QString startedDate READ startedDate WRITE setStartedDate NOTIFY startedDateChanged)
   Q_PROPERTY(QString resourceVersion READ resourceVersion WRITE setResourceVersion NOTIFY resourceVersionChanged)
   Q_PROPERTY(QString resourcePlugins READ resourcePlugins WRITE setResourcePlugins NOTIFY resourcePluginsChanged)
   Q_PROPERTY(QString bandwidthLimit READ bandwidthLimit WRITE setBandwidthLimit NOTIFY bandwidthLimitChanged)
   Q_PROPERTY(QList<QObject *> terminatedJobs READ getTerminatedJobs NOTIFY terminatedJobsChanged())
   Q_PROPERTY(QList<QObject *> runningJobs READ getRunningJobs NOTIFY runningJobsChanged())

   //Loaded if the connection was not successful
   Q_PROPERTY(QString connectionError READ getConnectionError WRITE setConnectionError NOTIFY connectionError)

   //TODO merge both into a variable 'dialogMsg'
   // Message displayed on the GUI dialogs
   Q_PROPERTY(QString successMsg READ successMsg  WRITE setSuccessMessage NOTIFY successMessageChanged)
   Q_PROPERTY(QString errorMsg READ errorMsg  WRITE setErrorMessage NOTIFY errorMessageChanged)

   Q_PROPERTY(bool isConnecting READ isConnecting WRITE setIsConnecting NOTIFY isConnectingChanged)

private:
   QString m_resourceName;
   QString m_startedDate;
   QString m_resourceVersion;
   QString m_resourcePlugins;
   QString m_bandwidthLimit;
   QList<QObject *> *m_terminatedJobs = new QList<QObject *>();
   QList<QObject *> *m_runningJobs = new QList<QObject *>();

   QString m_successMsg;
   QString m_errorMsg;
   QString m_connError;

   ConfigStorage *m_storage = NULL;

   rescode m_resCode;
   RESMON *m_res = NULL;

   bool m_connecting = false;

public:
   explicit ResDetailsUiController(QObject *parent = nullptr);
   ~ResDetailsUiController();

   // Getters / Setters for data that is used by our GUI
   bool isConnecting() { return m_connecting; }

   // Resource Name
   QString resourceName() {
      return m_resourceName;
   }

   void setResourceName(const QString &resourceName) {
      if (resourceName == m_resourceName)
         return;

      m_resourceName = resourceName;
      emit resourceNameChanged();
   }

   // Resource Started Date
   QString startedDate() {
      return m_startedDate;
   }

   void setStartedDate(const QString &startedDate) {
      if (startedDate == m_startedDate)
         return;

      m_startedDate = startedDate;
      emit startedDateChanged();
   }

   // Resource Version
   QString resourceVersion() {
      return m_resourceVersion;
   }

   void setResourceVersion(const QString &resourceVersion) {
      if (resourceVersion == m_resourceVersion)
         return;

      m_resourceVersion = resourceVersion;
      emit resourceVersionChanged();
   }

   // Resource Plugins
   QString resourcePlugins() {
      return m_resourcePlugins;
   }

   void setResourcePlugins(const QString &resourcePlugins) {
      if (resourcePlugins == m_resourcePlugins)
         return;

      m_resourcePlugins = resourcePlugins;
      emit resourcePluginsChanged();
   }

   // Resource Bandwidth Limit
   QString bandwidthLimit() {
      return m_bandwidthLimit;
   }

   void setBandwidthLimit(const QString &bandwidthLimit) {
      if (bandwidthLimit == m_bandwidthLimit)
         return;

      m_bandwidthLimit = bandwidthLimit;
      emit bandwidthLimitChanged();
   }

   // Terminated Jobs
   QList<QObject *> getTerminatedJobs() {
      return *m_terminatedJobs;
   }

   void setTerminatedJobs(QList<QObject *> *jobs) {
      if(m_terminatedJobs != NULL) {
         delete m_terminatedJobs;
      }

      m_terminatedJobs = jobs;
      emit terminatedJobsChanged();
   }

   // Running Jobs
   QList<QObject *> getRunningJobs() {
      return *m_runningJobs;
   }

   void setRunningJobs(QList<QObject *> *jobs) {
      if(m_runningJobs != NULL) {
         delete m_runningJobs;
      }

      m_runningJobs = jobs;
      emit runningJobsChanged();
   }

   // Dialog Success Message
   QString successMsg() {
      return m_successMsg;
   }

   void setSuccessMessage(const QString &successMsg) {
      m_successMsg = successMsg;
      emit successMessageChanged();
   }

   // Dialog Error Message
   QString errorMsg() {
      return m_errorMsg;
   }

   void setErrorMessage(const QString &errorMsg) {
      m_errorMsg = errorMsg;
      emit errorMessageChanged();
   }

   // Connection Error Message
   QString getConnectionError() {
      return m_connError;
   }

   void setConnectionError(const QString &connError) {
      m_connError = connError;
      emit connectionError();
   }

   void setIsConnecting(bool isConnecting) {
       if (m_connecting != isConnecting) {
           m_connecting = isConnecting;
           emit isConnectingChanged();
       }
   }

   // Model (If Edition Mode)
   void setResModel(ResourceModel *resModel) {
      m_res = resModel->resource();
      m_resCode = resModel->resCode();
      setResourceName(m_res->hdr.name);
   }

signals:
   // Events that are emitted to our GUI code to inform changes in our data
   void resourceNameChanged();
   void startedDateChanged();
   void resourceVersionChanged();
   void resourcePluginsChanged();
   void bandwidthLimitChanged();
   void terminatedJobsChanged();
   void runningJobsChanged();
   void successMessageChanged();
   void errorMessageChanged();
   void isConnectingChanged();

   // Events emitted to our GUI about the connection status with the resource
   void connectionStarted();
   void connectionError();
   void connectionSuccess();

   // Tells the GUI to start the screen related to running a job
   void runJobModelCreated(RunJobModel *model);

   // Tells the GUI to start the screen related to restoring a job
   void restoreModelCreated(RestoreJobModel *model);

public slots:
   // Called when the user wants to connect to a resource
   void connectToResource();
   void connectCallback(task *t);

   // Called when the user wants to run a job
   void createRunJobModel();
   void createBackupModelCallback(task *t);

   // Called when the user wants to restore a job
   void createRestoreJobModel();
   void createRestoreModelCallback(task *t);

   bool canRunJobs() {
       if (m_res == NULL) {
           return false;
       }

       return m_resCode == R_DIRECTOR || m_res->use_remote;
   }
};

#endif // RESCONFIGUICONTROLLER_H
