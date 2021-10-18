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
import io.qt.bmob.fdconfigcontroller 1.0


Page {
    id: fdConfigPage
    visible: true
    height: parent.height
    width: parent.width

    FdConfigUiController {
        id: controller

        onFileDaemonConfigRead: {
            fdConfigFilePage.text = text
        }
    }

    header: ToolBar {
        id: toolbar
        height: 48

        background: Rectangle {
            color: "#d32f2f"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            ToolButton {
                id: backButton
                onClicked: stackView.pop()
                anchors.left: parent.left

                contentItem: Text {
                    text: qsTr("<")
                    font.pixelSize: 24
                    opacity: enabled ? 1.0 : 0.3
                    color: "white"
                }

                background: Rectangle {
                    color: backButton.down ? "#b71c1c" : "#d32f2f"
                }
            }

            Label {
                id: titleLabel
                text: "bacula-fd.conf"
                color: "white"
                font.pixelSize: 18
                anchors.centerIn: parent
            }

        }
    }

    Flickable {
        id: fdConfigFilePage
        flickableDirection: Flickable.VerticalFlick
        anchors.fill: parent

        property alias text: textArea.text

        TextArea.flickable: TextArea {
            id: textArea
            text: controller.readFileDaemonConfig()
            textFormat: Qt.PlainText
            wrapMode: TextArea.Wrap
            readOnly: false
            persistentSelection: true
            leftPadding: 6
            rightPadding: 6
            topPadding: 0
            bottomPadding: 0
            background: null
        }

        ScrollBar.vertical: ScrollBar {}

        onVisibleChanged: {
            if (!visible) {
                controller.writeFileDaemonConfig(textArea.text)
            }
        }
    }

}
