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

import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.1
import QtQuick.Dialogs 1.2
import io.qt.bmob.restorejobcontroller 1.0

Page {
    id: restoreJobPage
    visible: true
    height: parent ? parent.height : 0
    width: parent ? parent.width : 0

    property var model;

    RestoreUiController {
        id: restoreJobController

        Component.onCompleted: {
            if(typeof restoreJobPage.model !== "undefined") {
                model = restoreJobPage.model
            }
        }

        onFileModelChanged: {
            fileSelectTab.fileModel = restoreJobController.files
        }

        onDialogTextChanged: {
            restoreJobDialog.text = restoreJobController.dialogText
            restoreJobDialog.visible = true
        }
    }

    StackLayout {
        id: restorePageStack
        anchors.fill: parent
        currentIndex: 0

        JobSelectTab {
            id: jobSelectTab
        }

        FileSelectTab {
            id: fileSelectTab
        }

        RestoreConfirmTab {
            id: restoreConfirmTab
        }
    }

    MessageDialog {
        id: restoreJobDialog
        modality: Qt.ApplicationModal
        standardButtons: StandardButton.Ok
        visible: false

        onAccepted: {
            stackView.pop();
        }
    }
}

