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

#ifndef RESTOREJOBMODEL_H
#define RESTOREJOBMODEL_H

#include <QObject>
#include <QString>
#include "tray_conf.h"

/*
   RestoreJobModel - contains the director that the App is connected with.
   The only reason for the existence of this class is the fact we can't
   send a RESMON struct between QML screens. Therefore, we need to wrap it into a
   class that extends QObject.
*/
class RestoreJobModel : public QObject
{
    Q_OBJECT

private:

   RESMON *m_director;

public:
    explicit RestoreJobModel(QObject *parent = nullptr);
    ~RestoreJobModel();

    void setDirector(RESMON *dir) {
        m_director = dir;
    }

    RESMON *getDirector() {
        return m_director;
    }

signals:


};

#endif // RESTOREJOBMODEL_H
