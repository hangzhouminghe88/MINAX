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

#ifndef RESTOREUICONTROLLER_H
#define RESTOREUICONTROLLER_H

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
#include "filesmodel.h"
#include <QAbstractItemModel>
#include <android/log.h>

/*
   RestoreJobUiController - controls the screen where the user specifies which Job it wants to restore
*/
class RestoreUiController : public QObject
{
    Q_OBJECT

    // Contains the Director that will restore the Job
    Q_PROPERTY(RestoreJobModel *model WRITE setModel)

    // List of Jobs that can be restored
    Q_PROPERTY(QList<QObject *> jobs READ getJobs NOTIFY jobsChanged())

    // List of clients that the user can choose
    // The Job will be restored into the selected client
    Q_PROPERTY(QStringList clients READ getClients NOTIFY clientsChanged())

    // Restore Options
    Q_PROPERTY(QString where READ getWhereParam WRITE setWhereParam NOTIFY whereParamChanged())
    Q_PROPERTY(int replaceIndex READ getReplaceIndex WRITE setReplaceIndex NOTIFY replaceIndexChanged())
    Q_PROPERTY(QStringList replaceOptions READ getReplaceOptions NOTIFY replaceOptionsChanged())

    // Optional comment
    Q_PROPERTY(QString comment READ getComment WRITE setComment NOTIFY commentChanged())

    // Model usued to show the whole directory structure associated with the selected Job
    // It starts at "/", and the user can move down into the directory tree
    // to select specific files
    Q_PROPERTY(QStandardItemModel *files READ getFileModel NOTIFY fileModelChanged())

    //Information dialog message
    Q_PROPERTY(QString dialogText READ getDialogText NOTIFY dialogTextChanged())

    Q_PROPERTY(bool isConnecting READ isConnecting WRITE setIsConnecting NOTIFY isConnectingChanged)

private:
    ConfigStorage *m_config;
    RestoreJobModel *m_model;
    FileSourceModel *m_sourceModel = new FileSourceModel();

    QStringList *m_clients = new QStringList();
    QStringList m_replaceOptions;
    QList<QObject *> *m_jobs = new QList<QObject *>();

    QString m_where;
    QString m_comment;
    QString m_selectedJobId;
    QString m_dialogText;

    int m_replaceIndex = 0;
    int m_selectedClient = 0;
    int m_targetClient = 0;

    int m_currentSourceId;
    QString m_currentPathStr;

    QMap<int, QModelIndex> m_selectedFileIds;
    QMap<int, QModelIndex> m_selectedDirIds;
    bool m_connecting = false;

    void setDefaultValues(RESMON *dir);

public:
    explicit RestoreUiController(QObject *parent = nullptr);
    ~RestoreUiController();

    // Getters / Setters for data that is used by our GUI
    bool isConnecting() { return m_connecting; }

    void setIsConnecting(bool isConnecting) {
       if (m_connecting != isConnecting) {
           m_connecting = isConnecting;
           emit isConnectingChanged();
       }
    }

    QString getDialogText() {
        return m_dialogText;
    }

    void setDialogText(QString dialogText) {
        if (m_dialogText == dialogText) {
            return;
        }

        m_dialogText = dialogText;
        emit dialogTextChanged();
    }

    // Clients
    QStringList getClients() {
        return *m_clients;
    }

    void setClients(alist *clients) {
        char *cli;
        foreach_alist(cli, clients) {
            m_clients->append(QString(cli));
        }

        emit clientsChanged();
    }

    // Model
    void setModel(RestoreJobModel *model) {
        m_model = model;
        RESMON *dir = model->getDirector();
        setClients(dir->clients);

        QStringList replaceOptions;
        replaceOptions << "Never" << "Always" << "IfNewer" << "IfOlder";
        setReplaceOptions(replaceOptions);
    }

    // Jobs
    QList<QObject *> getJobs() {
        return *m_jobs;
    }

    void setJobs(QStandardItemModel *jobs) {
        JobModel *job;
        m_jobs->clear();
        for(;;) {
            QList<QStandardItem *> jobFields = jobs->takeRow(0);
            if (jobFields.size() < 2) {
                break;
            }

            job = new JobModel();
            job->setData(jobFields);
            m_jobs->append(job);
        }
        emit jobsChanged();
    }

    // Where Param
    QString getWhereParam() {
        return m_where;
    }

    void setWhereParam(QString where) {
        if (m_where == where) {
            return;
        }

        m_where = where;
        emit whereParamChanged();
    }

    // Replace Options
    QStringList getReplaceOptions() {
        return m_replaceOptions;
    }

    void setReplaceOptions(QStringList replaceOptions) {
        m_replaceOptions = replaceOptions;
        emit replaceOptionsChanged();
    }

    // Replace Index
    int getReplaceIndex() {
        return m_replaceIndex;
    }

    // Comment
    QString getComment() {
        return m_comment;
    }

    void setComment(QString comment) {
        if (comment == m_comment)
            return;

        m_comment = comment;
        emit commentChanged();
    }

    QStandardItemModel *getFileModel() {
        return m_sourceModel;
    }

    void fetchDefaultValues();
    void fetchPath();

signals:
    void clientsChanged();
    void jobsChanged();
    void whereParamChanged();
    void replaceOptionsChanged();
    void replaceIndexChanged();
    void commentChanged();
    void fileModelChanged();
    void dialogTextChanged();
    void isConnectingChanged();

public slots:
    void setTargetClient(int targetClient) {
        m_targetClient = targetClient;
    }

    void setReplaceIndex(int index) {
        if(index == m_replaceIndex)
            return;

        m_replaceIndex = index;
        emit replaceIndexChanged();
    }

    // Called when the user selects a client. Calls the Director to fetch Jobs associated with it
    void handleClientChange(int clientIndex);
    void clientJobsCallback(task *t);

    // Called when the user selects a job. Calls the Director to fetch Files associated with it
    void handleJobSelection(QString jobId, QString jobName);
    void jobFilesCallback(task *t);

    // Called when the user navigates under the directory tree
    // to selected files to be restored
    void handleFileTreeNodeClick(int fileIndex);
    void handleFileSelection(int fileIndex, bool checked);
    bool fileIsSelected(int fileIndex);

    // Called when the user wishes to start the restore
    void restore();

    // Called after we asked the resource to start the restore
    void restoreJobCallback(task *t);
};

#endif // RESTOREUICONTROLLER_H
