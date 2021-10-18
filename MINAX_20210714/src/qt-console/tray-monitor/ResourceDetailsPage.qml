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
import io.qt.bmob.resdetailscontroller 1.0

Page {
   id: resDetailsPage
   visible: true
   height: parent ? parent.height : 0
   width: parent ? parent.width : 0

   property var resModel;
   property var resType;

   ResDetailsUiController {
      id: resDetailsController

      onSuccessMessageChanged: {
         titleLabel.text = resDetailsController.resourceName
         dialog.text = resDetailsController.successMsg
         dialog.open()
      }

      onErrorMsgChanged: {
         dialog.text = resDetailsController.errorMsg
         dialog.open()
      }

      Component.onCompleted: {
         resModel = resDetailsPage.resModel
         connectToResource();
      }

      onConnectionStarted: {
         connStatus.text = "Connecting..."
         connStatus.color = "#fff176"
      }

      onConnectionError: {
         connStatus.text = connectionError
         connStatus.color = "#ffcdd2"
      }

      onConnectionSuccess: {
         connStatus.text = "Connected"
         connStatus.color = "#66bb6a"
         resStatusPage.connected = true
         bar.visible = true
      }

      // "model" here is received as an argument
      onRunJobModelCreated: {
         stackView.push(
                  Qt.resolvedUrl("RunJobPage.qml"),
                  {"model" : model}
                  )
      }

      onRestoreModelCreated: {
         stackView.push(
                  Qt.resolvedUrl("RestoreJobPage.qml"),
                  {"model" : model}
                  )
      }
   }

   onVisibleChanged: {
      if (visible) {
         resDetailsController.connectToResource()
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
            text: resDetailsController.resourceName
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }

      }
   }

   Rectangle {
      id: connStatus
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: childrenRect.height
      color: "#ffcdd2"

      property alias text: statusLabel.text

      Label {
         id: statusLabel
         text: "Not Connected"
         anchors.horizontalCenter: parent.horizontalCenter
         topPadding: 4
         bottomPadding: 4
         color: "#000000"
      }
   }

   Flickable {
      id: scrollableBox
      anchors.top: connStatus.bottom
      anchors.bottom: parent.bottom
      width: parent.width
      contentWidth: parent.width
      contentHeight: scrollContent.height
      clip: true

      Item {
         id: scrollContent
         width: parent.width
         height: childrenRect.height

         ResourceStatusPage {
            id: resStatusPage
         }

         Rectangle {
            id: divider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: resStatusPage.bottom
            anchors.topMargin: 16
            height: 1
            color: "#e0e0e0"
         }

         Text {
            id: rjobsLabel
            text: "Running Jobs"
            font.pixelSize: 18
            anchors.top: divider.bottom
            anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
         }

         ListView {
            id: runningJobsPage
            model: resDetailsController.runningJobs
            height: childrenRect.height
            anchors.top: rjobsLabel.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            interactive: false
            boundsBehavior: Flickable.StopAtBounds

            delegate: JobListItem {
               jobId: model.modelData.id
               name: model.modelData.name
               level: model.modelData.level
               finfo: model.modelData.fileInfo
               errorCount: model.modelData.errorCount
            }

            Text {
               text: "No jobs are running"
               anchors.left: parent.left
               anchors.leftMargin: 24
               anchors.top: parent.top
               anchors.topMargin: 8
               visible: parent.count == 0
               font.pixelSize: 18
               color: "#d32f2f"
            }
         }

         Rectangle {
            id: divider2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: runningJobsPage.bottom
            anchors.topMargin: 24
            height: 1
            color: "#e0e0e0"
         }

         Text {
            id: tjobsLabel
            text: "Terminated Jobs"
            font.pixelSize: 18
            anchors.top: divider2.bottom
            anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
         }

         ListView {
            id: terminatedJobsPage
            model: resDetailsController.terminatedJobs
            height: childrenRect.height
            anchors.top: tjobsLabel.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            interactive: false
            boundsBehavior: Flickable.StopAtBounds

            delegate: JobListItem {
               jobId: model.modelData.id
               name: model.modelData.name
               level: model.modelData.level
               finfo: model.modelData.fileInfo
               errorCount: model.modelData.errorCount
            }

            Text {
               text: "No terminated jobs"
               anchors.left: parent.left
               anchors.leftMargin: 24
               anchors.top: parent.top
               anchors.topMargin: 8
               visible: parent.count == 0
               font.pixelSize: 18
               color: "#d32f2f"
            }
         }
      }

   }

   PulseLoader {
      useDouble: true
      visible: resDetailsController.isConnecting
      radius: 28
      color: "#d32f2f"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: 24
   }

   MessageDialog {
      id: dialog
      modality: Qt.ApplicationModal
      standardButtons: StandardButton.Ok
      visible: false
   }
}
