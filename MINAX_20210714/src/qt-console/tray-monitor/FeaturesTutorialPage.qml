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

Page {
    id: featuresTutorialPage
    visible: true

    background: Rectangle {
       color: "#d32f2f"
    }

    GridLayout {
       id: featuresGrid
       columns: 2
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.top: parent.top
       anchors.bottom: featureText.top
       anchors.topMargin: 24
       anchors.bottomMargin: 24
       anchors.leftMargin: 16
       anchors.rightMargin: 16

       Image {
          source: "images/ss_main.jpg"
          Layout.preferredWidth: featuresGrid.width / 2
          Layout.preferredHeight:  featuresGrid.height / 2

          Rectangle {
             anchors.fill: parent
             anchors.margins: -border.width
             z: -1
             border.width: 1
             color: "#000"
          }
       }

       Image {
          source: "images/ss_dir.jpg"
          Layout.preferredWidth: featuresGrid.width / 2
          Layout.preferredHeight:  featuresGrid.height / 2

          Rectangle {
             anchors.fill: parent
             anchors.margins: -border.width
             z: -1
             border.width: 1
             color: "#000"
          }
       }

       Image {
          source: "images/ss_backup.jpg"
          Layout.preferredWidth: featuresGrid.width / 2
          Layout.preferredHeight:  featuresGrid.height / 2

          Rectangle {
             anchors.fill: parent
             anchors.margins: -border.width
             z: -1
             border.width: 1
             color: "#000"
          }
       }

       Image {
          source: "images/ss_restore.jpg"
          Layout.preferredWidth: featuresGrid.width / 2
          Layout.preferredHeight:  featuresGrid.height / 2

          Rectangle {
             anchors.fill: parent
             anchors.margins: -border.width
             z: -1
             border.width: 1
             color: "#000"
          }
       }
    }

    Text {
       id: featureText
       text: "With the Bacula® Mobile app, users can backup their phones and manage Bacula resources."
       wrapMode: TextEdit.WordWrap
       font.pixelSize: 18
       color: "white"

       anchors.bottom: nextButton.top
       anchors.left: parent.left
       anchors.right: parent.right
       anchors.bottomMargin: 16
       anchors.leftMargin: 16
       anchors.rightMargin: 16
    }

    ToolButton {
       id: nextButton
       onClicked: {
          tutorialSwipe.currentIndex = 1
       }
       anchors.right: parent.right
       anchors.bottom: parent.bottom
       anchors.rightMargin: 16
       anchors.bottomMargin: 16

       contentItem: Text {
          text: qsTr(">")
          font.pixelSize: 28
          opacity: enabled ? 1.0 : 0.3
          color: "white"
       }

       background: Rectangle {
          color: parent.down ? "#b71c1c" : "#d32f2f"
       }
    }
}
