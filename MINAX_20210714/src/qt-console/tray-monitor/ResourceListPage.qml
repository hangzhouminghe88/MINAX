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
   anchors.fill: parent

   property alias model: resList.model
   property alias panelVisible: resDetailsPanel.visible
   property alias resType: resDetailsPanel.resType

   ListView {
      id: resList
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: parent.top
      anchors.bottom: resDetailsPanel.visible? resDetailsPanel.top : monitorName.top
      boundsBehavior: Flickable.StopAtBounds
      clip: true

      delegate: ResourceListItem {
         name: model.modelData.resourceName
         address: model.modelData.resourceAddress
         port: model.modelData.resourcePort
         deletable: model.modelData.deletableResource

         delButton.onClicked: {
            confirmDialog.text = "Delete resource <b>"
                  + model.modelData.resourceName + "</b> ?"
            confirmDialog.res = model.modelData
            confirmDialog.open()
         }

         clickArea.onClicked: {
            resDetailsPanel.resModel = model.modelData
            resDetailsPanel.visible = true
         }
      }
   }

   ResourcePanel {
      id: resDetailsPanel

      onVisibleChanged: {
         if (!visible) {
            controller.fetchClients()
            controller.fetchDirectors()
            controller.fetchStorages()
         }
      }
   }

   TextField {
      id: monitorName
      height: childrenRect.height
      width: parent.width
      anchors.bottom: parent.bottom
      placeholderText: qsTr("Monitor name")
      text: controller.getMonitorName()
      visible: !resDetailsPanel.visible
   }

   Button {
      id: saveMonitorBtn
      text: "Save"
      onClicked: controller.setMonitorName(monitorName.text)

      anchors.right: parent.right
      anchors.rightMargin: 16
      anchors.verticalCenter: monitorName.verticalCenter
      visible: !resDetailsPanel.visible

      contentItem: Text {
         text: saveMonitorBtn.text
         font: saveMonitorBtn.font
         color: saveMonitorBtn.down ? "#ef9a9a" : "#d32f2f"
      }

      background: Rectangle {
         color: "white"
      }
   }

   onVisibleChanged: {
      if (visible) {
         monitorName.text = controller.getMonitorName()
      }
   }
}


