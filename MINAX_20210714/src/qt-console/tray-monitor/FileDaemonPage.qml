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

   // Our C++ component
   TrayUiController {
      id: controller

      // Events triggered by our c++ code
      onClientsChanged: {
         //
      }

      onDirectorsChanged: {
         //
      }

      onStoragesChanged: {
         //
      }

      onInfoDialogTextChanged: {
         infoDialog.text = controller.infoDialogText
         infoDialog.open()
      }

      onFdTraceChanged: {
         scrollableBox.text = controller.fetchFile(controller.fdTracePath);
      }

      Component.onCompleted: {
         scrollableBox.text = controller.fetchFile(controller.fdTracePath)
         scrollableBox.scrollToBottom()
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
            text: "File Daemon"
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }

      }
   }

   Text {
      id: labelLogLevel
      text: "Log Level:"
      color: "black"
      anchors.left: parent.left
      anchors.leftMargin: 16
      anchors.verticalCenter: fdLogLevel.verticalCenter
   }

   TextField {
      id: fdLogLevel
      height: 44
      anchors.top: parent.top
      anchors.right: parent.right
      anchors.left: labelLogLevel.right
      placeholderText: qsTr("value")
      text: controller.fdLogLevel
      inputMethodHints: Qt.ImhDigitsOnly
      maximumLength: 3

      onTextChanged: controller.fdLogLevel = text

      background: Rectangle {
          implicitHeight: 48
          implicitWidth: 100
          color: "transparent"
          border.color: "transparent"
      }
   }

   Button {
      id: reportButton
      text: "Report"
      anchors.verticalCenter: fdLogLevel.verticalCenter
      anchors.right: parent.right
      anchors.rightMargin: 12
      onClicked: controller.composeTraceMail()

      contentItem: Text {
         text: reportButton.text
         font.pixelSize: 18
         color: reportButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   Rectangle {
      id: divider2
      width: parent.width
      height: 1
      color: "#d32f2f"
      anchors.bottom: fdLogLevel.bottom
   }

   Rectangle {
      id: divider
      width: parent.width
      height: 1
      color: "#d32f2f"
      anchors.bottom: startButton.top
   }

   Button {
      id: startButton
      text: "Start"
      onClicked: controller.startFileDaemon()
      anchors.rightMargin: 12
      anchors.right: parent.right
      anchors.bottom: parent.bottom

      contentItem: Text {
         text: startButton.text
         font.pixelSize: 18
         color: startButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   Button {
      id: stopButton
      text: "Stop"
      onClicked: controller.stopFileDaemon()
      anchors.rightMargin: 12
      anchors.right: startButton.left
      anchors.bottom: parent.bottom

      contentItem: Text {
         text: stopButton.text
         font.pixelSize: 18
         color: stopButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   Button {
      id: configButton
      text: "Config"
      onClicked: stackView.push(Qt.resolvedUrl("FileDaemonConfigPage.qml"))
      anchors.rightMargin: 12
      anchors.right: stopButton.left
      anchors.bottom: parent.bottom

      contentItem: Text {
         text: configButton.text
         font.pixelSize: 18
         color: configButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   Button {
      visible: IS_ENTERPRISE
      id: qrcodeButton
      text: "QR Code"
      onClicked: controller.startQRCodeReader()
      anchors.leftMargin: 12
      anchors.left: parent.left
      anchors.bottom: parent.bottom

      contentItem: Text {
         text: qrcodeButton.text
         font.pixelSize: 18
         color: qrcodeButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   ScrollView {
      id: scrollableBox
      anchors.top: divider2.bottom
      anchors.bottom: divider.top
      width: parent.width
      contentHeight: content.height
      contentWidth: parent.width
      ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
      ScrollBar.horizontal.interactive: false
      clip: true

      property alias text: content.text

      Text {
         id: content
         width: scrollableBox.width
         leftPadding: 12
         rightPadding: 12
         topPadding: 8
         bottomPadding: 12
         wrapMode: Text.WrapAnywhere
      }

      onVisibleChanged: {
         if (visible) {
            scrollableBox.text = controller.fetchFile(controller.fdTracePath);
         }
      }

      function scrollToBottom() {
          if (content.height > contentItem.height) {
             contentItem.contentY = content.height - contentItem.height
          }
      }
   }

   PulseLoader {
      useDouble: true
      visible: controller.isConnecting
      radius: 28
      color: "#d32f2f"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: divider.top
      anchors.bottomMargin: 24
   }

   MessageDialog {
      id: infoDialog
      modality: Qt.ApplicationModal
      standardButtons: StandardButton.Ok
      visible: false
   }
}
