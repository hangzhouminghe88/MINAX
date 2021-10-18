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

Item {
    id: listItem
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

    property alias name: resName.text
    property alias address: resAddress.text
    property alias port: resPort.text
    property alias delButton: deleteButton
    property alias clickArea: clickArea
    property alias deletable: deleteButton.visible

    Text {
        id: nameLabel
        text: "<b>Name:</b>"
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 16
    }

    Text {
        id: resName
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: nameLabel.right
        anchors.leftMargin: 8
    }


    Text {
        id: addressLabel
        text: "<b>Address:</b>"
        anchors.top: resName.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.topMargin: 8
    }

    Text {
        id: resAddress
        anchors.top: nameLabel.bottom
        anchors.left: addressLabel.right
        anchors.leftMargin: 8
        anchors.topMargin: 8
    }

    Text {
        id: portLabel
        text: "<b>Port:</b>"
        anchors.top: addressLabel.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.topMargin: 8
    }

    Text {
        id: resPort
        anchors.top: addressLabel.bottom
        anchors.left: portLabel.right
        anchors.leftMargin: 8
        anchors.topMargin: 8
    }

    Rectangle {
        id: divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: resPort.bottom
        anchors.topMargin: 16
        height: 1
        color: "#e0e0e0"
    }

    MouseArea {
        id: clickArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: divider.bottom
    }

    ToolButton {
        id: deleteButton
        anchors.right: parent.right
        anchors.rightMargin: 10

        anchors {
            top: parent.top
            topMargin: 8
            right: parent.right
            rightMargin: 8
        }

        contentItem: Text {
            text: qsTr("x")
            font.pixelSize: 20
            color: deleteButton.down ? "#ffcdd2" : "#d32f2f"
        }

        background: Rectangle {
            color: "#ffffff"
        }

    }
}
