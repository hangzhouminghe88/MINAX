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

#include "enterprise-tray-ui-controller.h"

EnterpriseTrayUiController::EnterpriseTrayUiController(QObject *parent):
    TrayUiController(parent)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/etc";
    ConfigStorage *storage = &ConfigStorage::getInstance();
    m_bweb = new BWebService(configDir, storage->config_path);
    m_bweb->m_fdPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_bweb->m_monitorName = AndroidFD::deviceId();
    m_bweb->m_monitorPwd = AndroidFD::devicePwd();
    connect(this, SIGNAL(onQRCodeRead(QString)),
            this, SLOT(handleQRCodeData(QString)));
}

void EnterpriseTrayUiController::handleActivityResult(int receiverRequestCode, int resultCode, const QAndroidJniObject & data) {
    jint RESULT_OK = QAndroidJniObject::getStaticField<jint>("android/app/Activity", "RESULT_OK");

    if (resultCode == RESULT_OK && data.isValid()) {
        QAndroidJniObject key = QAndroidJniObject::fromString("barcode.data");
        QString bwebUrl = data.callObjectMethod("getStringExtra"
                        , "(Ljava/lang/String;)Ljava/lang/String;"
                        , key.object<jstring>()
                        ).toString();
        // We can't use the network here
        // because this method is called from a secondary thread.
        // So we just emit a signal and do the network call elsewhere;
        emit onQRCodeRead(bwebUrl);
    }
}

void EnterpriseTrayUiController::handleQRCodeData(QString bwebUrl) {
    if (!isConnecting()) {
        setIsConnecting(true);
        m_bweb->doLinkRegistration(bwebUrl, this);
    }
}

void EnterpriseTrayUiController::onBWebSuccess(QString msg) {
    setIsConnecting(false);
    setInfoDialogText(msg);
}

void EnterpriseTrayUiController::onBWebError(QString msg) {
    setIsConnecting(false);
    setInfoDialogText(msg);
}

EnterpriseTrayUiController::~EnterpriseTrayUiController()
{
    delete m_bweb;
}
