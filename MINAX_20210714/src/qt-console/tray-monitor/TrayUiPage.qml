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
import io.qt.bmob.traycontroller 1.0

Page {
    visible: true
    width: parent.width
    height: parent.height

    property variant tabTitles: [qsTr("Clients"), qsTr("Directors"), qsTr("Storages"), qsTr("Log")]

    // Our C++ component
    TrayUiController {
        id: controller

        // Events triggered by our c++ code
        onClientsChanged: {
            clientsPage.model = controller.clients
        }

        onDirectorsChanged: {
            directorsPage.model = controller.directors
        }

        onStoragesChanged: {
            storagesPage.model = controller.storages
        }

        onInfoDialogTextChanged: {
            infoDialog.text = controller.infoDialogText
            infoDialog.open()
        }

        onFdTraceChanged: {
            traymonLogView.text = controller.fetchFile(controller.trayTracePath)
            traymonLogView.scrollToBottom()
        }

        Component.onCompleted: {
            traymonLogView.text = controller.fetchFile(controller.trayTracePath)
            traymonLogView.scrollToBottom()
        }
    }

    onVisibleChanged: {
        if(visible) {
            controller.fetchClients()
            controller.fetchDirectors()
            controller.fetchStorages()
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

            Label {
                id: title
                text: tabTitles[bar.currentIndex]
                color: "white"
                font.pixelSize: 18
                anchors.centerIn: parent
            }

            ToolButton {
                id: newResButton
                anchors.right: parent.right

                property int resType: bar.currentIndex

                onClicked: {
                    if (resType == 0) {
                        clientsPage.resType = 0
                        clientsPage.panelVisible = true
                    } else if (resType == 1) {
                        directorsPage.resType = 1
                        directorsPage.panelVisible = true
                    } else if (resType == 2) {
                        storagesPage.resType = 2
                        storagesPage.panelVisible = true
                    }
                }

                contentItem: Text {
                    text: bar.currentIndex >= 3 ? qsTr("") : qsTr("+")
                    font.pixelSize: 24
                    opacity: enabled ? 1.0 : 0.3
                    color: "white"
                }

                background: Rectangle {
                    color: newResButton.down ? "#b71c1c" : "#d32f2f"
                }
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: bar.currentIndex

        ResourceListPage {
            id: clientsPage
            model: controller.clients
        }

        ResourceListPage {
            id: directorsPage
            model: controller.directors
        }

        ResourceListPage {
            id: storagesPage
            model: controller.storages
        }

        Page {
            id: trayMonLogPage
            height: parent.height
            width: parent.width

            Text {
               id: labelLogLevel
               text: "Log Level:"
               color: "black"
               anchors.left: parent.left
               anchors.leftMargin: 16
               anchors.verticalCenter: monLogLevel.verticalCenter
            }

            TextField {
               id: monLogLevel
               height: 44
               anchors.top: parent.top
               anchors.right: parent.right
               anchors.left: labelLogLevel.right
               placeholderText: qsTr("value")
               text: controller.trayLogLevel
               inputMethodHints: Qt.ImhDigitsOnly
               maximumLength: 3
               onTextChanged: controller.trayLogLevel = text

               background: Rectangle {
                   implicitHeight: 48
                   implicitWidth: 100
                   color: "transparent"
                   border.color: "transparent"
               }
            }

            Rectangle {
               id: divider
               width: parent.width
               height: 1
               color: "#d32f2f"
               anchors.bottom: monLogLevel.bottom
            }

            ScrollView {
                id: traymonLogView
                anchors.top: divider.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                contentWidth: parent.width
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.interactive: false
                clip: true

                property alias text: traymonLog.text

                Text {
                    id: traymonLog
                    width: traymonLogView.width
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 12
                    wrapMode: Text.WrapAnywhere
                }

                onVisibleChanged: {
                    if(visible) {
                        traymonLogView.text = controller.fetchFile(controller.trayTracePath);
                    }
                }

                function scrollToBottom() {
                   if (traymonLog.height > contentItem.height) {
                      contentItem.contentY = traymonLog.height - contentItem.height
                   }
                }
            }

        }
    }

    footer: TabBar {
        id: bar
        width: parent.width

        background: Rectangle {
            color: "#d32f2f"
        }

        TabButton {
            text: tabTitles[0]

            background: Rectangle {
                anchors.fill: parent
                color: bar.currentIndex === 0 ? "#ffcdd2" : "#d32f2f"
            }
        }

        TabButton {
            text: tabTitles[1]

            background: Rectangle {
                anchors.fill: parent
                color: bar.currentIndex === 1 ? "#ffcdd2" : "#d32f2f"
            }
        }

        TabButton {
            text: tabTitles[2]

            background: Rectangle {
                anchors.fill: parent
                color: bar.currentIndex === 2 ? "#ffcdd2" : "#d32f2f"
            }
        }

        TabButton {
            text: tabTitles[3]

            background: Rectangle {
                anchors.fill: parent
                color: bar.currentIndex === 3 ? "#ffcdd2" : "#d32f2f"
            }
        }
    }

    MessageDialog {
        id: confirmDialog
        property var res

        modality: Qt.ApplicationModal
        standardButtons: StandardButton.No | StandardButton.Yes
        onYes: controller.deleteResource(res)
        visible: false
    }

    MessageDialog {
        id: infoDialog
        modality: Qt.ApplicationModal
        standardButtons: StandardButton.Ok
        visible: false
    }
}
