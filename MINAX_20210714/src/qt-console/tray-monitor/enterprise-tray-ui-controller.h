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

#ifndef ENTR_TRAYUICONTROLLER_H
#define ENTR_TRAYUICONTROLLER_H

#include <QAndroidActivityResultReceiver>
#include "tray-ui-controller.h"
#include "bweb-service.h"

/*
   EnterpriseTrayUiController - A subclass of TrayUiController that adds the QR Code configuration feature
   for our Android bacula-fd.conf file
*/
class EnterpriseTrayUiController : public TrayUiController, QAndroidActivityResultReceiver, BWebCallback
{
    Q_OBJECT

    Q_PROPERTY(bool isConnecting READ isConnecting WRITE setIsConnecting NOTIFY isConnectingChanged)

private:
    BWebService *m_bweb;
    bool m_connecting = false;

public:
    explicit EnterpriseTrayUiController(QObject *parent = nullptr);
    ~EnterpriseTrayUiController();

    // Callback containing the scanned QR Code data
    virtual void handleActivityResult(int receiverRequestCode, int resultCode, const QAndroidJniObject &data);

    // Callbacks for the Link Registration HTTP request sent to BWeb
    virtual void onBWebSuccess(QString msg);
    virtual void onBWebError(QString msg);

    // Getters / Setters for data that is used by our GUI
    bool isConnecting() { return m_connecting; }

    void setIsConnecting(bool isConnecting) {
       if (m_connecting != isConnecting) {
           m_connecting = isConnecting;
           emit isConnectingChanged();
       }
   }

public slots:
    void handleQRCodeData(QString bwebUrl);

    void startQRCodeReader() {
        AndroidFD::startQRCodeReader(this);
    }

signals:
    void onQRCodeRead(QString bwebUrl);
    void isConnectingChanged();
};

#endif // ENTR_TRAYUICONTROLLER_H
