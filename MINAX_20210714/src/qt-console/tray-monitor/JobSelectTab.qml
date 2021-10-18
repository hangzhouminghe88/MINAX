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
   height: parent ? parent.height : 0
   width: parent ? parent.width : 0

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
            text: "1 - Select a Job"
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }

      }
   }

   Text {
      id: clientsLabel
      text: "<b>Client:</b>"
      anchors.verticalCenter: clientsCombo.verticalCenter
      anchors.left: parent.left
      anchors.leftMargin: 16
   }

   ComboBox {
      id: clientsCombo
      model: restoreJobController.clients
      anchors.top: parent.top
      anchors.topMargin: 16
      anchors.left: clientsLabel.right
      anchors.leftMargin: 8
      anchors.right: parent.right
      anchors.rightMargin: 16

      delegate: ItemDelegate {
         width: clientsCombo.width
         contentItem: Text {
            text: model.modelData
            color: "#000000"
            font: clientsCombo.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
         }
         highlighted: clientsCombo.highlightedIndex === index
      }

      indicator: Canvas {
      }

      contentItem: Text {
         leftPadding: 8
         rightPadding: clientsCombo.indicator.width + clientsCombo.spacing

         text: clientsCombo.displayText
         font: clientsCombo.font
         color: clientsCombo.pressed ? "#ef9a9a" : "#000000"
         verticalAlignment: Text.AlignVCenter
         elide: Text.ElideRight
      }

      background: Rectangle {
         implicitWidth: 120
         implicitHeight: 40
         border.color: clientsCombo.pressed ? "#ef9a9a" : "#d32f2f"
         border.width: clientsCombo.visualFocus ? 2 : 1
         radius: 2
      }

      popup: Popup {
         y: clientsCombo.height - 1
         width: clientsCombo.width
         implicitHeight: contentItem.implicitHeight
         padding: 1

         contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: clientsCombo.popup.visible ? clientsCombo.delegateModel : null
            currentIndex: clientsCombo.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
         }

         background: Rectangle {
            border.color: "#d32f2f"
            radius: 2
         }
      }

      onCurrentIndexChanged: {
         restoreJobController.handleClientChange(
                  clientsCombo.currentIndex
                  )
      }
   }

   Rectangle {
      id: divider
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: clientsCombo.bottom
      anchors.topMargin: 16
      height: 1
      color: "#e0e0e0"
   }


   ListView {
      id: jobsListView
      clip: true
      anchors.top: divider.bottom
      anchors.right: parent.right
      anchors.left: parent.left
      anchors.bottom: parent.bottom
      model:restoreJobController.jobs
      boundsBehavior: Flickable.StopAtBounds

      delegate: JobListItem {
         jobId: model.modelData.id
         name: model.modelData.name
         level: model.modelData.level
         finfo: model.modelData.fileInfo
         errorCount: model.modelData.errorCount

         clickArea.onClicked: {
            restoreJobController.handleJobSelection(
                     model.modelData.id,
                     model.modelData.name
                     )
            restorePageStack.currentIndex = 1
         }
      }

   }

   PulseLoader {
      useDouble: true
      visible: restoreJobController.isConnecting
      radius: 28
      color: "#d32f2f"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: 24
   }
}

