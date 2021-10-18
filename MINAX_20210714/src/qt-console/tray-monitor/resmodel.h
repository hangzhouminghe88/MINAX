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

#ifndef RESMODEL_H
#define RESMODEL_H

#include <QObject>
#include <QString>
#include "tray_conf.h"

/*
   ResourceModel - This model represents one resource that is present in the
   bacula-tray-monitor.conf file (either a Director, File Daemon or Storage Daemon).
*/
class ResourceModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString resourceName READ resourceName WRITE setResourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(QString resourceAddress READ resourceAddress WRITE setResourceAddress NOTIFY resourceAddressChanged)
    Q_PROPERTY(QString resourcePort READ resourcePort WRITE setResourcePort NOTIFY resourcePortChanged)
    Q_PROPERTY(bool deletableResource READ isResourceDeletable)

private:
   QString m_resourceName;
   QString m_resourceAddress;
   QString m_resourcePort;
   RESMON *m_res;
   rescode m_resCode;

public:
    explicit ResourceModel(QObject *parent = nullptr);
    ~ResourceModel();

    //Resource Name
    QString resourceName() {
        return m_resourceName;
    }

    void setResourceName(const QString &resourceName) {
        if (resourceName == m_resourceName)
            return;

        m_resourceName = resourceName;
        emit resourceNameChanged();
    }


    //Resource Address
    QString resourceAddress() {
        return m_resourceAddress;
    }

    void setResourceAddress(const QString &resourceAddress) {
        if (resourceAddress == m_resourceAddress)
            return;

        m_resourceAddress = resourceAddress;
        emit resourceAddressChanged();
    }


    //Resource Port
    QString resourcePort() {
        return m_resourcePort;
    }

    void setResourcePort(const QString &resourcePort) {
        if (resourcePort == m_resourcePort)
            return;

        m_resourcePort = resourcePort;
        emit resourcePortChanged();
    }

    bool isResourceDeletable() {
        return !m_res->managed;
    }

    // Resource Struct
    RESMON *resource() {
        return m_res;
    }

    rescode resCode() {
        return m_resCode;
    }

    void setResource(RESMON * res, rescode code) {
        m_resCode = code;
        m_res = res;
        setResourceName(res->hdr.name);
        setResourceAddress(res->address);
        setResourcePort(QString::number(res->port));
    }

signals:
    void resourceNameChanged();
    void resourceAddressChanged();
    void resourcePortChanged();

};

#endif // RESMODEL_H
