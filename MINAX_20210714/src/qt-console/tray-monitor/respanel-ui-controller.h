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

#ifndef RESPANELGUICONTROLLER_H
#define RESPANELGUICONTROLLER_H

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

*/
class ResPanelUiController : public QObject
{
   Q_OBJECT

   // Data related to the resource that the user selected
   Q_PROPERTY(ResourceModel *resModel WRITE setResModel)
   Q_PROPERTY(QString resType WRITE setResType)

   //These can be edited by the user
   Q_PROPERTY(QString resourceName READ resourceName WRITE setResourceName NOTIFY resourceNameChanged)
   Q_PROPERTY(QString resourcePassword READ resourcePassword WRITE setResourcePassword NOTIFY resourcePasswordChanged)
   Q_PROPERTY(QString resourceAddress READ resourceAddress WRITE setResourceAddress NOTIFY resourceAddressChanged)
   Q_PROPERTY(QString resourcePort READ resourcePort WRITE setResourcePort NOTIFY resourcePortChanged)
   Q_PROPERTY(bool remoteClient READ isRemoteClient WRITE setIsRemoteClient NOTIFY isRemoteClientChanged)

   //TODO merge both into a variable 'dialogMsg'
   // Message displayed on the GUI dialogs
   Q_PROPERTY(QString successMsg READ successMsg  WRITE setSuccessMessage NOTIFY successMessageChanged)
   Q_PROPERTY(QString errorMsg READ errorMsg  WRITE setErrorMessage NOTIFY errorMessageChanged)

private:
   QString m_resourceName;
   QString m_resourcePassword;
   QString m_resourceAddress;
   QString m_resourcePort;
   QString m_successMsg;
   QString m_errorMsg;
   bool m_remoteClient;
   ConfigStorage *m_storage = NULL;
   rescode m_resCode;
   RESMON *m_res = NULL;

public:
   explicit ResPanelUiController(QObject *parent = nullptr);
   ~ResPanelUiController();

   void setResCode(rescode code) {
       if (m_resCode == code) {
           return;
       }

       m_resCode = code;
       emit resCodeChanged();
   }

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

   // Resource Password
   QString resourcePassword() {
      return m_resourcePassword;
   }

   void setResourcePassword(const QString &resourcePassword) {
      if (resourcePassword == m_resourcePassword)
         return;

      m_resourcePassword = resourcePassword;
      emit resourcePasswordChanged();
   }

   // Resource Address
   QString resourceAddress() {
      return m_resourceAddress;
   }

   void setResourceAddress(const QString &resourceAddress) {
      if (resourceAddress == m_resourceAddress)
         return;

      m_resourceAddress = resourceAddress;
      emit resourceAddressChanged();
   }

   // Resource Port
   QString resourcePort() {
      return m_resourcePort;
   }

   void setResourcePort(const QString &resourcePort) {
      if (resourcePort == m_resourcePort)
         return;

      m_resourcePort = resourcePort;
      emit resourcePortChanged();
   }

   // Remote Client
   bool isRemoteClient() {
       return m_remoteClient;
   }

   void setIsRemoteClient(bool remote) {
       if (m_remoteClient == remote) {
           return;
       }

       m_remoteClient = remote;
       emit isRemoteClientChanged();
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

   // Model (If Edition Mode)
   void setResModel(ResourceModel *resModel) {
      m_res = resModel->resource();
      setResCode(m_res->code);
      setResourceName(m_res->hdr.name);
      setResourcePassword(m_res->password);
      setResourceAddress(m_res->address);
      setResourcePort(QString::number(m_res->port));
      setIsRemoteClient(m_res->use_remote);
   }

   // Called if Save Mode
   void setResType(QString resType) {
      m_res = NULL;
      setResourceName("");
      setResourcePassword("");
      setResourceAddress("");
      setIsRemoteClient(false);

      if(resType == "1") {
         setResCode(R_DIRECTOR);
         setResourcePort("9101");
      } else if(resType == "2") {
         setResCode(R_STORAGE);
         setResourcePort("9103");
      } else {
         setResCode(R_CLIENT);
         setResourcePort("9102");
      }
   }

signals:
   // Events that are emitted to our GUI code to inform changes in our data
   void resourceNameChanged();
   void resourcePasswordChanged();
   void resourceAddressChanged();
   void resourcePortChanged();
   void successMessageChanged();
   void errorMessageChanged();
   void isRemoteClientChanged();
   void resCodeChanged();

public slots:
   // Save the changes made to this resource on bacula-tray-monitor.conf
   void saveChanges();

   bool isClient() {
       return m_resCode == R_CLIENT;
   }

};

#endif // RESPANELGUICONTROLLER_H
