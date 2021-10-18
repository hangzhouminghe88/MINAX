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

#ifndef RUNJOBUICONTROLLER_H
#define RUNJOBUICONTROLLER_H

#include <QObject>
#include <QString>
#include <QDate>
#include <fstream>
#include "conf.h"
#include "tray_conf.h"
#include "runjobmodel.h"
#include "task.h"

/*
   RunJobUiController - controls the screen where the user specifies which Job it wants to run
*/
class RunJobUiController : public QObject
{
   Q_OBJECT
   // Contains the Director that will run the Job
   Q_PROPERTY(RunJobModel *model WRITE setModel)
   Q_PROPERTY(bool loadedModel READ loadedModel)

   // Lists of options that the user can choose for the desired Job
   Q_PROPERTY(QStringList jobNames READ getJobNames NOTIFY jobNamesChanged())
   Q_PROPERTY(QStringList jobLevels READ getJobLevels NOTIFY jobLevelsChanged())
   Q_PROPERTY(QStringList clientNames READ getClientNames NOTIFY clientNamesChanged())
   Q_PROPERTY(QStringList filesetNames READ getFilesetNames NOTIFY filesetNamesChanged())
   Q_PROPERTY(QStringList poolNames READ getPoolNames NOTIFY poolNamesChanged())
   Q_PROPERTY(QStringList storageNames READ getStorageNames NOTIFY storageNamesChanged())
   Q_PROPERTY(QStringList catalogNames READ getCatalogNames NOTIFY catalogNamesChanged())

   // Indexes of user selected options
   Q_PROPERTY(int selectedLevel READ getLevel WRITE setLevel NOTIFY levelChanged())
   Q_PROPERTY(int selectedClient READ getClient WRITE setClient NOTIFY clientChanged())
   Q_PROPERTY(int selectedFileset READ getFileset WRITE setFileset NOTIFY filesetChanged())
   Q_PROPERTY(int selectedPool READ getPool WRITE setPool NOTIFY poolChanged())
   Q_PROPERTY(int selectedStorage READ getStorage WRITE setStorage NOTIFY storageChanged())
   Q_PROPERTY(int selectedCatalog READ getCatalog WRITE setCatalog NOTIFY catalogChanged())

   // Information about the Job files (total size, number of files)
   Q_PROPERTY(QString filesDescription READ getFilesDescription WRITE setFilesDescription NOTIFY filesDescriptionChanged())

   // Job priority
   Q_PROPERTY(QString priority READ getPriority WRITE setPriority NOTIFY priorityChanged())

   Q_PROPERTY(bool isConnecting READ isConnecting WRITE setIsConnecting NOTIFY isConnectingChanged)
   //    Q_PROPERTY(QString startDate READ getStartDate WRITE setStartDate NOTIFY startDateChanged())

private:
   int m_selectedJobIndex;
   int m_selectedLevel = 0;
   int m_selectedClient= 0;
   int m_selectedFileset = 0;
   int m_selectedPool= 0;
   int m_selectedStorage = 0;
   int m_selectedCatalog = 0;

   ConfigStorage *m_config;
   QStringList m_jobNames;
   QStringList m_jobLevel;

   QStringList m_clientNames;
   QStringList m_filesetNames;
   QStringList m_poolNames;
   QStringList m_storageNames;
   QStringList m_catalogNames;

   QString m_startDate;
   QString m_filesDescription;
   QString m_priority = "10";

   RunJobModel *m_rJobModel;
   bool m_loadedModel = false;
   bool m_connecting = false;

   int findIndex(QStringList list, QString str);

public:
   explicit RunJobUiController(QObject *parent = nullptr);
   ~RunJobUiController();

   // Getters / Setters for data that is used by our GUI
   bool loadedModel() { return m_loadedModel; }
   QString getPriority() { return m_priority; }
   QString getFilesDescription() { return m_filesDescription; }
   QStringList getJobNames() { return m_jobNames; }
   QStringList getJobLevels() { return m_jobLevel; }
   QStringList getFilesetNames() { return m_filesetNames; }
   QStringList getClientNames() { return m_clientNames; }
   QStringList getPoolNames() { return m_poolNames; }
   QStringList getStorageNames() { return m_storageNames; }
   QStringList getCatalogNames() { return m_catalogNames; }
   int getLevel() { return m_selectedLevel; }
   int getClient() { return m_selectedClient; }
   int getFileset() { return m_selectedFileset; }
   int getPool() { return m_selectedPool; }
   int getStorage() { return m_selectedStorage; }
   int getCatalog() { return m_selectedCatalog; }
   bool isConnecting() { return m_connecting; }
   // QString getStartDate() { return m_startDate; }

   void setIsConnecting(bool isConnecting) {
       if (m_connecting != isConnecting) {
           m_connecting = isConnecting;
           emit isConnectingChanged();
       }
   }

   void setJobLevels(QStringList jobLevels) {
      m_jobLevel = jobLevels;
      emit jobLevelsChanged();
   }

   void setFilesDescription(QString filesDesc) {
      m_filesDescription = filesDesc;
      emit filesDescriptionChanged();
   }

   void setJobNames(alist *jobNames) {
      char *name;
      foreach_alist(name, jobNames) {
         m_jobNames << QString(name);
      }

      emit jobNamesChanged();
   }

   void setClientNames(alist *clientNames) {
      char *name;
      foreach_alist(name, clientNames) {
         m_clientNames << QString(name);
      }

      emit clientNamesChanged();
   }

   void setFilesetNames(alist *filesetNames) {
      char *name;
      foreach_alist(name, filesetNames) {
         m_filesetNames << QString(name);
      }

      emit filesetNamesChanged();
   }

   void setPoolNames(alist *poolNames) {
      char *name;
      foreach_alist(name, poolNames) {
         m_poolNames << QString(name);
      }

      emit poolNamesChanged();
   }

   void setStorageNames(alist *storageNames) {
      char *name;
      foreach_alist(name, storageNames) {
         m_storageNames << QString(name);
      }

      emit storageNamesChanged();
   }

   void setCatalogNames(alist *catalogNames) {
      char *name;
      foreach_alist(name, catalogNames) {
         m_catalogNames << QString(name);
      }

      emit catalogNamesChanged();
   }

   void setPriority(const QString &priority) {
      m_priority = priority;
   }

   //    void setStartDate(QString startDate) {
   //        m_startDate = startDate;
   //        emit startDateChanged();
   //    }

   // Model provided by previous screen
   void setModel(RunJobModel *model) {
      RESMON *dir = model->getDirector();
      m_rJobModel = model;
      setJobNames(model->getDirector()->jobs);

      //        int year = QDate::currentDate().year();
      //        int month = QDate::currentDate().month();
      //        int day = QDate::currentDate().day();
      //        QString date("%1 / %2 / %3");
      //        date = date.arg(year).arg(month).arg(day);
      //        QString time = QTime::currentTime().toString();
      //        setStartDate(date.append(" - ").append(time));

      setClientNames(dir->clients);
      setFilesetNames(dir->filesets);
      setPoolNames(dir->pools);
      setStorageNames(dir->storages);
      setCatalogNames(dir->catalogs);

      QStringList levels;
      levels << "Full" << "Incremental" << "Differential";
      setJobLevels(levels);
      m_loadedModel = true;
   }

public slots:

   // When the User selects a Job, we ask the Director for information related to it
   void handleSelectedJobChange(int newIndex, QString jobLevel);
   void jobInfoCallback(task *t);
   void jobDefaultsCallback(task *t);

   void setLevel(int level) {
      if (m_selectedLevel != level) {
         m_selectedLevel = level;
         emit levelChanged();
      }
   }

   void setClient(int client) {
      if (m_selectedClient != client) {
         m_selectedClient = client;
         emit clientChanged();
      }
   }

   void setFileset(int fs) {
      if (m_selectedFileset != fs) {
         m_selectedFileset = fs;
         emit filesetChanged();
      }
   }

   void setPool(int pool) {
      if (m_selectedPool != pool) {
         m_selectedPool = pool;
         emit poolChanged();
      }
   }

   void setStorage(int storage) {
      if (m_selectedStorage != storage) {
         m_selectedStorage = storage;
         emit storageChanged();
      }
   }

   void setCatalog(int catalog) {
      if(m_selectedCatalog != catalog) {
         m_selectedCatalog = catalog;
         emit catalogChanged();
      }
   }

   // Called when the user wishes to run the Job
   void runJob();

   // Called after we ask the Director to run the Job
   void runJobCallback(task *t);

signals:
   //    void startDateChanged();

   // Events that are emitted when the lists changed
   void jobNamesChanged();
   void jobLevelsChanged();
   void clientNamesChanged();
   void filesetNamesChanged();
   void poolNamesChanged();
   void storageNamesChanged();
   void catalogNamesChanged();

   // Events that are emitted when the User selects one option
   void levelChanged();
   void clientChanged();
   void filesetChanged();
   void poolChanged();
   void storageChanged();
   void catalogChanged();
   void filesDescriptionChanged();
   void priorityChanged();

   // Emitted when we successfully requested the Director to run the Job
   void jobSuccess();

   void isConnectingChanged();
};

#endif // RUNJOBUICONTROLLER_H
