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

#ifndef FDCONFIGUICONTROLLER_H
#define FDCONFIGUICONTROLLER_H

#include <QObject>
#include <QString>
#include <fstream>
#include "conf.h"
#include "tray_conf.h"
#include "resmodel.h"
#include "config-storage.h"

class FdConfigUiController : public QObject
{
    Q_OBJECT

public:
    explicit FdConfigUiController(QObject *parent = nullptr);
    ~FdConfigUiController();

public slots:
   //FD Config File
   void readFileDaemonConfig();
   void writeFileDaemonConfig(QString fcontents);

signals:
  void fileDaemonConfigRead(const QString &text);
};

#endif // FDCONFIGUICONTROLLER_H
