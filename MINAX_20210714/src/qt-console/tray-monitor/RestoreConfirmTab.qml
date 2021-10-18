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
            onClicked: restorePageStack.currentIndex = 1
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
            text: "3 - Destination"
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }

      }
   }

   Flickable {
      id: scrollableBox
      anchors.top: parent.top
      anchors.bottom: restoreButton.top
      width: parent.width
      contentWidth: parent.width
      contentHeight: body.height
      clip: true

      Item {
         id: body
         width: parent.width
         height: childrenRect.height

         Text {
            id: clientsLabel
            text: "<b>Client:</b>"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 24
            anchors.leftMargin: 16
         }

         ComboBox {
            id: clientsCombo
            model: restoreJobController.clients

            anchors.top: clientsLabel.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 24
            anchors.rightMargin: 24

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
               restoreJobController.setTargetClient(clientsCombo.currentIndex)
            }
         }

         Text {
            id: whereLabel
            text: "<b>Where:</b>"

            anchors.top: clientsCombo.bottom
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.topMargin: 24
         }

         TextField {
            id: whereField
            text: restoreJobController.where

            anchors.top: whereLabel.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 24
            anchors.rightMargin: 24

            topPadding: 10
            bottomPadding: 10
            background: Rectangle {
               radius: 2
               border.color: "#d32f2f"
               border.width: 1
            }

            onTextChanged: restoreJobController.where = text
         }

         Text {
            id: replaceLabel
            text: "<b>Replace:</b>"

            anchors.top: whereField.bottom
            anchors.left: parent.left
            anchors.topMargin: 24
            anchors.leftMargin: 16
         }

         ComboBox {
            id: replaceCombo
            model: restoreJobController.replaceOptions

            anchors.top: replaceLabel.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 24
            anchors.rightMargin: 24

            delegate: ItemDelegate {
               width: replaceCombo.width
               contentItem: Text {
                  text: model.modelData
                  color: "#000000"
                  font: replaceCombo.font
                  elide: Text.ElideRight
                  verticalAlignment: Text.AlignVCenter
               }
               highlighted: replaceCombo.highlightedIndex === index
            }

            indicator: Canvas {
            }

            contentItem: Text {
               leftPadding: 8
               rightPadding: replaceCombo.indicator.width + replaceCombo.spacing

               text: replaceCombo.displayText
               font: replaceCombo.font
               color: replaceCombo.pressed ? "#ef9a9a" : "#000000"
               verticalAlignment: Text.AlignVCenter
               elide: Text.ElideRight
            }

            background: Rectangle {
               implicitWidth: 120
               implicitHeight: 40
               border.color: replaceCombo.pressed ? "#ef9a9a" : "#d32f2f"
               border.width: replaceCombo.visualFocus ? 2 : 1
               radius: 2
            }

            popup: Popup {
               y: replaceCombo.height - 1
               width: replaceCombo.width
               implicitHeight: contentItem.implicitHeight
               padding: 1

               contentItem: ListView {
                  clip: true
                  implicitHeight: contentHeight
                  model: replaceCombo.popup.visible ? replaceCombo.delegateModel : null
                  currentIndex: replaceCombo.highlightedIndex

                  ScrollIndicator.vertical: ScrollIndicator { }
               }

               background: Rectangle {
                  border.color: "#d32f2f"
                  radius: 2
               }
            }

            onCurrentIndexChanged: {
               restoreJobController.setReplaceIndex(replaceCombo.currentIndex)
            }
         }

         Text {
            id: commentLabel
            text: "<b>Comment:</b>"
            anchors.top: replaceCombo.bottom
            anchors.left: parent.left
            anchors.topMargin: 24
            anchors.leftMargin: 16
         }

         TextArea {
            id: commentField
            text: restoreJobController.comment
            height: 200

            anchors.top: commentLabel.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 24
            anchors.rightMargin: 24

            topPadding: 10
            bottomPadding: 10
            background: Rectangle {
               radius: 2
               border.color: "#d32f2f"
               border.width: 1
            }

            wrapMode: TextEdit.WrapAnywhere
            onTextChanged: restoreJobController.comment = text
         }

         // Visual margin in the bottom
         Item {
            width: parent.width
            height: 32
            anchors.top: commentField.bottom
         }
      }
   }

   Rectangle {
      id: restoreButton
      height: childrenRect.height
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      color: clickArea.pressed ? "#b71c1c" : "#d32f2f"

      Text {
         text: qsTr("Restore >")
         topPadding: 8
         bottomPadding: 8
         anchors.horizontalCenter: parent.horizontalCenter
         verticalAlignment: Text.AlignVCenter
         font.pixelSize: 18
         color: "white"
      }
   }

   PulseLoader {
      useDouble: true
      visible: restoreJobController.isConnecting
      radius: 28
      color: "#d32f2f"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: restoreButton.top
      anchors.bottomMargin: 24
   }

   MouseArea {
      id: clickArea
      anchors.left: restoreButton.left
      anchors.right: restoreButton.right
      anchors.top: restoreButton.top
      anchors.bottom: restoreButton.bottom

      onClicked: restoreJobController.restore()
   }
}

