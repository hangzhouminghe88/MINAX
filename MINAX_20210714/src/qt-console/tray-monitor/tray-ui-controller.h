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

#ifndef TRAYUICONTROLLER_H
#define TRAYUICONTROLLER_H

#include <QtAndroid>
#include <QObject>
#include <QString>
#include <QAndroidActivityResultReceiver>
#include <QFileSystemWatcher>
#include <android/log.h>
#include <fstream>
#include "conf.h"
#include "tray_conf.h"
#include "resmodel.h"
#include "config-storage.h"
#include "android-fd-service.h"

/*
   TrayUiController - Controls the first screen displayed by our Qt App
*/
class TrayUiController : public QObject
{
    Q_OBJECT

    // Where to find the Android File Daemon trace file
    Q_PROPERTY(QString fdTracePath READ getDaemonTracePath)

    // Where to find the Tray Monitor trace file
    Q_PROPERTY(QString trayTracePath READ getTrayTracePath)

    // Controls the Tray Monitor log level
    Q_PROPERTY(QString trayLogLevel READ getTrayLogLevel WRITE setTrayLogLevel)

    // Controls the File Daemon log level
    Q_PROPERTY(QString fdLogLevel READ getFdLogLevel WRITE setFdLogLevel)

    // Text that is displayed in an information dialog
    Q_PROPERTY(QString infoDialogText READ getInfoDialogText WRITE setInfoDialogText)

    // Resources that were obtained from the bacula-tray-monitor.conf file
    Q_PROPERTY(QList<QObject *> clients READ getClients NOTIFY clientsChanged())
    Q_PROPERTY(QList<QObject *> directors READ getDirectors NOTIFY directorsChanged())
    Q_PROPERTY(QList<QObject *> storages READ getStorages NOTIFY storagesChanged())

private:
    QString m_monitorName;
    QString m_infoDialogText;
    QFileSystemWatcher *m_traceWatcher;
    ConfigStorage *m_config;
    QList<QObject *> *m_clients;
    QList<QObject *> *m_directors;
    QList<QObject *> *m_storages;

    QList<QObject *> *fetchResources(rescode resCode);

public:
    explicit TrayUiController(QObject *parent = nullptr);
    ~TrayUiController();

    // Getters / Setters for data that is used by our GUI
    QString getDaemonTracePath() {
        return AndroidFD::tracePath();
    }

    QString getTrayTracePath() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tray-monitor.trace";
    }

    QString getTrayLogLevel() {
        return QString::number(debug_level);
    }

    void setTrayLogLevel(QString level) {
         debug_level = level.toInt();
    }

    QString getFdLogLevel() {
        char buf[50];
        edit_int64(AndroidFD::logLevel, buf);
        return QString(buf);
    }

    void setFdLogLevel(QString level) {
        AndroidFD::logLevel = level.toInt();
    }

    QList<QObject *> getClients() {
        return *m_clients;
    }

    void setClients(QList<QObject *> *clients) {
        if(m_clients != NULL) {
            delete m_clients;
        }

        m_clients = clients;
        emit clientsChanged();
    }

    QList<QObject *> getDirectors() {
        return *m_directors;
    }

    void setDirectors(QList<QObject *> *directors) {
        if(m_directors != NULL) {
            delete m_directors;
        }

        m_directors = directors;
        emit directorsChanged();
    }

    QList<QObject *> getStorages() {
        return *m_storages;
    }

    void setStorages(QList<QObject *> *storages) {
        if(m_storages != NULL) {
            delete m_storages;
        }

        m_storages = storages;
        emit storagesChanged();
    }

    QString getInfoDialogText() {
        return m_infoDialogText;
    }

    void setInfoDialogText(const QString &text) {
        m_infoDialogText = text;
        emit infoDialogTextChanged();
    }

public slots:
    // Reads and updates data from bacula-tray-monitor.conf
    void fetchClients();
    void fetchDirectors();
    void fetchStorages();
    void deleteResource(ResourceModel* resModel);
    QString getMonitorName();
    void setMonitorName(QString name);

    // Controls the execution of our Android File Daemon Service
    void startFileDaemon();
    void stopFileDaemon();

    // Send FD Trace to a Mail App
    void composeTraceMail();

    // Reads and returns the contents of a file in the device
    QString fetchFile(QString fpath);

    // Called when the File Daemon updates it's trace file
    void handleFdTraceChange(QString path);

signals:
    // Events that are emitted to our GUI code to inform changes in our data
    void clientsChanged();
    void directorsChanged();
    void storagesChanged();
    void fdStatusChanged();
    void fdTraceChanged();
    void infoDialogTextChanged();
};

#endif // TRAYUICONTROLLER_H
