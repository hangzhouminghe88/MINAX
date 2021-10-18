import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.1
import QtQuick.Dialogs 1.2
import io.qt.bmob.respanelcontroller 1.0

Item {
    id: resPanel
    width: parent.width
    height: resPanel.visible ? childrenRect.height : 0
    visible: false
    anchors.bottom: parent.bottom

    property alias resModel: resPanelController.resModel;
    property alias resType: resPanelController.resType;

    ResPanelUiController {
       id: resPanelController

       onSuccessMessageChanged: {
          dialog.text = resPanelController.successMsg
          dialog.open()
       }

       onErrorMsgChanged: {
          dialog.text = resPanelController.errorMsg
          dialog.open()
       }

       onResCodeChanged: {
           labelRemote.visible = isClient()
           checkbox.visible = isClient()
       }
    }

    Rectangle {
       id: panelDivider
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.top: parent.top
       height: 1
       color: "#d32f2f"
    }

    ToolButton {
       id: closePanelButton
       anchors.top: panelDivider.bottom
       anchors.right: parent.right
       anchors.rightMargin: 10
       anchors.topMargin: 8

       anchors {
          top: parent.top
          topMargin: 8
          right: parent.right
          rightMargin: 8
       }

       contentItem: Text {
          text: qsTr("x")
          font.pixelSize: 20
          color: closePanelButton.down ? "#ffcdd2" : "#d32f2f"
       }

       background: Rectangle {
          color: "#ffffff"
       }

       onClicked: {
          resPanel.visible = false
       }
    }

    Text {
       id: nameLabel
       text: "<b>Name:</b>"
       anchors.top: panelDivider.bottom
       anchors.left: parent.left
       anchors.topMargin: 16
       anchors.leftMargin: 16
    }

    TextField {
       id: resName
       text: resPanelController.resourceName
       placeholderText: qsTr("Resource name")
       anchors.top: nameLabel.bottom
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.topMargin: 8
       anchors.leftMargin: 24
       anchors.rightMargin: 48
       onTextChanged: resPanelController.resourceName = text
    }

    Text {
       id: passwordLabel
       text: "<b>Password:</b>"
       anchors.top: resName.bottom
       anchors.left: parent.left
       anchors.topMargin: 16
       anchors.leftMargin: 16
    }

    TextField {
       id: resPassword
       echoMode: TextInput.PasswordEchoOnEdit
       text: resPanelController.resourcePassword
       placeholderText: qsTr("Resource password")
       anchors.top: passwordLabel.bottom
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.topMargin: 8
       anchors.leftMargin: 24
       anchors.rightMargin: 48
       onTextChanged: resPanelController.resourcePassword = text
    }

    Text {
       id: addressLabel
       text: "<b>Address:</b>"
       anchors.top: resPassword.bottom
       anchors.left: parent.left
       anchors.topMargin: 16
       anchors.leftMargin: 16
    }

    TextField {
       id: resAddress
       text: resPanelController.resourceAddress
       placeholderText: qsTr("Resource address")
       anchors.top: addressLabel.bottom
       anchors.left: resPassword.left
       anchors.right: parent.right
       anchors.topMargin: 8
       anchors.rightMargin: 48
       onTextChanged: resPanelController.resourceAddress = text
    }

    Text {
       id: portLabel
       text: "<b>Port:</b>"
       anchors.top: resAddress.bottom
       anchors.left: parent.left
       anchors.topMargin: 16
       anchors.leftMargin: 16
    }

    TextField {
       id: resPort
       text: resPanelController.resourcePort
       placeholderText: qsTr("Resource port")
       anchors.top: portLabel.bottom
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.topMargin: 8
       anchors.leftMargin: 24
       anchors.rightMargin: 48
       onTextChanged: resPanelController.resourcePort = text
    }

    Text {
       id: labelRemote
       text: "<b>Remote:</b>"
       color: "black"
       visible: resPanelController.isClient()
       anchors.left: parent.left
       anchors.leftMargin: 16
       anchors.verticalCenter: checkbox.verticalCenter
    }

    CheckBox {
        id: checkbox
        checked: resPanelController.remoteClient
        visible: resPanelController.isClient()
        anchors.top: resPort.bottom
        anchors.topMargin: 16
        anchors.left: labelRemote.right
        anchors.leftMargin: 8

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
            resPanelController.remoteClient = checked
        }
    }

    Button {
       id: saveButton
       text: "Save"
       onClicked: resPanelController.saveChanges()
       anchors.top: labelRemote.visible? labelRemote.bottom : resPort.bottom
       anchors.topMargin: 16
       anchors.horizontalCenter: parent.horizontalCenter

       contentItem: Text {
          text: saveButton.text
          font.pixelSize: 20
          color: saveButton.down ? "#ef9a9a" : "#d32f2f"
       }

       background: Rectangle {
          color: "white"
       }
    }

    // Bottom Margin
    Item {
       height: 16
       anchors.top: saveButton.bottom
    }

    MessageDialog {
       id: dialog
       modality: Qt.ApplicationModal
       standardButtons: StandardButton.Ok
       visible: false

       onAccepted: resPanel.visible = false
    }
}
