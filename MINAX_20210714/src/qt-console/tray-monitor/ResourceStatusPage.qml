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
   width: parent.width
   height: childrenRect.height

   property bool connected: false

   Text {
      id: versionLabel
      text: "<b>Version:</b>"
      visible: connected
      anchors.top: parent.top
      anchors.topMargin: 16
      anchors.left: parent.left
      anchors.leftMargin: 16
   }

   Text {
      id: version
      visible: connected
      text: resDetailsController.resourceVersion
      anchors.top: parent.top
      anchors.topMargin: 16
      anchors.left: versionLabel.right
      anchors.leftMargin: 8
   }

   Text {
      id: startedAtLabel
      visible: connected
      text: "<b>Started:</b>"
      anchors.top: versionLabel.bottom
      anchors.left: parent.left
      anchors.leftMargin: 16
      anchors.topMargin: 8
   }

   Text {
      id: startedAt
      visible: connected
      text: resDetailsController.startedDate
      anchors.top: versionLabel.bottom
      anchors.left: startedAtLabel.right
      anchors.leftMargin: 8
      anchors.topMargin: 8
   }

   Text {
      id: pluginsLabel
      visible: connected
      text: "<b>Plugins:</b>"
      anchors.top: startedAtLabel.bottom
      anchors.left: parent.left
      anchors.leftMargin: 16
      anchors.topMargin: 8
   }

   Text {
      id: plugins
      visible: connected
      text: resDetailsController.resourcePlugins
      anchors.top: startedAtLabel.bottom
      anchors.left: pluginsLabel.right
      anchors.leftMargin: 8
      anchors.topMargin: 8
   }

   Text {
      id: bandwidthLabel
      visible: connected
      text: "<b>Bandwidth Limit:</b>"
      anchors.top: pluginsLabel.bottom
      anchors.left: parent.left
      anchors.leftMargin: 16
      anchors.topMargin: 8
   }

   Text {
      id: bandwidthLimit
      visible: connected
      text: resDetailsController.bandwidthLimit
      anchors.top: pluginsLabel.bottom
      anchors.left: bandwidthLabel.right
      anchors.leftMargin: 8
      anchors.topMargin: 8
   }

   // Bottom Margin
   Item {
      height: 16
      anchors.top: bandwidthLabel.bottom
   }

   Button {
      id: runJobButton
      visible: connected && resDetailsController.canRunJobs()
      text: "Run Job"
      onClicked: resDetailsController.createRunJobModel()

      anchors.right: parent.right
      anchors.rightMargin: 8
      anchors.verticalCenter: bandwidthLimit.verticalCenter

      contentItem: Text {
         text: runJobButton.text
         font: runJobButton.font
         color: runJobButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   Button {
      id: restoreJobButton
      visible: connected && resDetailsController.canRunJobs()
      text: "Restore"
      onClicked: resDetailsController.createRestoreJobModel()

      anchors.right: runJobButton.left
      anchors.rightMargin: 8
      anchors.verticalCenter: bandwidthLimit.verticalCenter

      contentItem: Text {
         text: restoreJobButton.text
         font: restoreJobButton.font
         color: restoreJobButton.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }
}
