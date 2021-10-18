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
   id: restoreJobPage
   visible: true
   height: parent ? parent.height : 0
   width: parent ? parent.width : 0

   property alias fileModel: fileTree.model

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
            onClicked: restorePageStack.currentIndex = 0
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
            text: "2 - Select Files"
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }

      }
   }

   ListView {
      id: fileTree
      clip: true
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: parent.top
      anchors.bottom: nextButton.top
      model: restoreJobController.files
      boundsBehavior: Flickable.StopAtBounds

      delegate: Item {
         width: parent.width
         height: childrenRect.height

         Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: divider.bottom
            color: "#e0e0e0"
            visible: clickArea.pressed
         }

         Text {
            id: node
            text: model.display
            padding: 12
            font.pixelSize: 18
         }

         MouseArea {
            id: clickArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: divider.bottom

            onClicked: {
               restoreJobController.handleFileTreeNodeClick(index)
            }
         }


         CheckBox {
            id: checkbox
            visible: node.text !== ".."
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: node.verticalCenter

            indicator: Rectangle {
               implicitWidth: 20
               implicitHeight: 20
               x: checkbox.leftPadding
               y: parent.height / 2 - height / 2
               radius: 3
               border.color: checkbox.down ? "#ef9a9a" : "#d32f2f"

               Rectangle {
                  width: 8
                  height: 8
                  x: 6
                  y: 6
                  radius: 2
                  color: checkbox.down ? "#ef9a9a" : "#d32f2f"
                  visible: checkbox.checked
               }
            }

            onCheckedChanged: {
               nextButton.visible = true
               nextClickArea.visible = true
               restoreJobController.handleFileSelection(index, checked)
            }

            Component.onCompleted: {
               checked = restoreJobController.fileIsSelected(index)
            }
         }

         Rectangle {
            id: divider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: node.bottom
            height: 1
            color: "#e0e0e0"
         }

      }

   }

   PulseLoader {
      useDouble: true
      visible: restoreJobController.isConnecting
      radius: 28
      color: "#d32f2f"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: nextButton.top
      anchors.bottomMargin: 24
   }

   Rectangle {
      id: nextButton
      visible: false
      height: childrenRect.height
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      color: nextClickArea.pressed ? "#b71c1c" : "#d32f2f"

      Text {
         text: qsTr("Next >")
         topPadding: 8
         bottomPadding: 8
         anchors.horizontalCenter: parent.horizontalCenter
         verticalAlignment: Text.AlignVCenter
         font.pixelSize: 18
         color: "white"
      }
   }

   MouseArea {
      id: nextClickArea
      visible: false
      anchors.left: nextButton.left
      anchors.right: nextButton.right
      anchors.top: nextButton.top
      anchors.bottom: nextButton.bottom

      onClicked: restorePageStack.currentIndex = 2
   }
}

