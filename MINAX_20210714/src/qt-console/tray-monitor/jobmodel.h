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

#ifndef JOBMODEL_H
#define JOBMODEL_H

#include <QObject>
#include <QString>
#include "tray_conf.h"
#include "jcr.h"
#include "../util/fmtwidgetitem.h"
#include "android/log.h"

class JobModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ getId NOTIFY idChanged)
    Q_PROPERTY(QString name READ getName NOTIFY nameChanged)
    Q_PROPERTY(QString level READ getLevel NOTIFY levelChanged)
    Q_PROPERTY(QString fileInfo READ getFileInfo NOTIFY fileInfoChanged)
    Q_PROPERTY(QString errorCount READ getErrorCount NOTIFY errorCountChanged)

private:
   QString m_id;
   QString m_name;
   QString m_level;
   QString m_fileInfo;
   QString m_errorCount;

public:
    explicit JobModel(QObject *parent = nullptr);
    ~JobModel();

   //Job Id
   QString getId() {
       return m_id;
   }

   void setId(const QString &id) {
       if (m_id == id)
           return;

       m_id = id;
       emit idChanged();
   }

    //Job Name
    QString getName() {
        return m_name;
    }

    void setName(const QString &name) {
        if (m_name == name)
            return;

        m_name = name;
        emit nameChanged();
    }

    //Job Level
    QString getLevel() {
        return m_level;
    }

    void setLevel(const QString &level) {
        if (m_level == level)
            return;

        m_level = level;
        emit levelChanged();
    }

    //Job File Info
    QString getFileInfo() {
        return m_fileInfo;
    }

    void setFileInfo(uint64_t bytesInt, uint32_t fileCount) {
        char buf[50];
        qint64 bytes = QString(edit_uint64(bytesInt, buf))
                .trimmed()
                .toLongLong();
        QString bytesStr = convertBytesIEC(bytes);

        m_fileInfo = QString("%1 Files (%2)")
                .arg(fileCount)
                .arg(bytesStr);

        emit fileInfoChanged();
    }

    //Job Error Count
    QString getErrorCount() {
        return m_errorCount;
    }

    void setErrorCount(int32_t count) {

        QString temp;
        if(count == 1) {
            temp = "%1 Error";
        } else {
            temp = "%1 Errors";
        }

        m_errorCount = QString(temp).arg(count);
        emit errorCountChanged();
    }

    void setData(s_last_job *job) {
        setId(QString::number(job->JobId));
        setName(job->Job);
        setLevel(job_level_to_str(job->JobLevel));
        setFileInfo(job->JobBytes, job->JobFiles);

        if(job->Errors > 0) {
            setErrorCount(job->Errors);
        }
    }

    void setData(s_running_job *job) {
        setId(QString::number(job->JobId));
        setName(job->Job);
        setLevel(job_level_to_str(job->JobLevel));
        setFileInfo(job->JobBytes, job->JobFiles);

        if(job->Errors > 0) {
            setErrorCount(job->Errors);
        }
    }

    void setData(QList<QStandardItem *>jobFields) {
        setId(jobFields[0]->text());
        setName(jobFields[1]->text());

        if(jobFields[2] != NULL) {
            setLevel(jobFields[2]->text());
        }

        if(jobFields[4] != NULL && jobFields[5] != NULL) {
//            setFileInfo(jobFields[5]->text().toUInt(), jobFields[4]->text().toUInt());
        }
    }

signals:
    void idChanged();
    void nameChanged();
    void levelChanged();
    void fileInfoChanged();
    void errorCountChanged();

};

#endif // JOBMODEL_H
