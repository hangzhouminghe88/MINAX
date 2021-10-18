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

#include "respanel-ui-controller.h"

ResPanelUiController::ResPanelUiController(QObject *parent):
    QObject(parent)
{
    m_storage = &ConfigStorage::getInstance();
}

void ResPanelUiController::saveChanges() {
    const char *error_msg = NULL;
    QByteArray resNameBytes = m_resourceName.toUtf8();
    QByteArray resAddressBytes = m_resourceAddress.toUtf8();
    QByteArray resPasswordBytes = m_resourcePassword.toUtf8();

    RESMON *newRes = new RESMON();
    newRes->code = m_resCode;
    newRes->hdr.name = resNameBytes.data();
    newRes->address = resAddressBytes.data();
    newRes->password = resPasswordBytes.data();
    newRes->port = m_resourcePort.toUInt();
    newRes->use_remote = m_remoteClient;

    if(m_res == NULL) {
        error_msg = m_storage->addResource(newRes, m_resCode);
    } else {
        error_msg = m_storage->editResource(m_res, newRes, m_resCode);
    }

    if(error_msg == NULL) {
        setSuccessMessage("Changes Saved");
    } else {
        setErrorMessage(error_msg);
    }

    m_res = m_storage->getResourceByName(newRes->code, newRes->hdr.name);
    delete newRes;
}

ResPanelUiController::~ResPanelUiController() {}
