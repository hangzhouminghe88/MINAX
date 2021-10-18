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

    property alias jobId: jobId.text
    property alias name: name.text
    property alias level: level.text
    property alias finfo: finfo.text
    property alias errorCount: errors.text
    property alias clickArea: clickArea

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: divider.bottom
        color: "#e0e0e0"
        visible: clickArea.pressed
    }

    Text {
        id: idLabel
        text: "<b>Id:</b>"
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 16
    }

    Text {
        id: jobId
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: idLabel.right
        anchors.leftMargin: 8
    }

    Text {
        id: errors
        color: "#d32f2f"
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 12
    }

    Text {
        id: nameLabel
        text: "<b>Name:</b>"
        anchors.top: idLabel.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.topMargin: 8
    }

    Text {
        id: name
        elide: Text.ElideRight
        anchors.top: idLabel.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.left: nameLabel.right
        anchors.leftMargin: 8
    }

    Text {
        id: levelLabel
        text: "<b>Level:</b>"
        anchors.top: nameLabel.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.topMargin: 8
    }

    Text {
        id: level
        anchors.top: nameLabel.bottom
        anchors.left: levelLabel.right
        anchors.leftMargin: 8
        anchors.topMargin: 8
    }

    Text {
        id: finfo
        anchors.top: nameLabel.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.leftMargin: 8
    }

    MouseArea {
        id: clickArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: divider.bottom
    }

    Rectangle {
        id: divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: levelLabel.bottom
        anchors.topMargin: 16
        height: 1
        color: "#e0e0e0"
    }

}
