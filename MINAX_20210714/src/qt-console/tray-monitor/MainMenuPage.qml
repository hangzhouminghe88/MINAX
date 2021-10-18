import QtQuick 2.10
import QtQuick.Window 2.10
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.1
import QtQuick.Dialogs 1.2
import io.qt.bmob.traycontroller 1.0

Page {
   id: mainMenuPage
   visible: true
   width: parent.width
   height: parent.height
   property bool main: true

   // Our C++ component
   TrayUiController {
      id: controller

      // Events triggered by our c++ code
      onClientsChanged: {
         clientsGrid.model = controller.clients
      }

      onDirectorsChanged: {
         directorsGrid.model = controller.directors
      }

      onStoragesChanged: {
         storagesGrid.model = controller.storages
      }

      onInfoDialogTextChanged: {

      }
   }

   onVisibleChanged: {
      if(visible) {
         controller.fetchClients()
         controller.fetchDirectors()
         controller.fetchStorages()
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

         Label {
            id: title
            text: "Main Menu"
            color: "white"
            font.pixelSize: 18
            anchors.centerIn: parent
         }
      }
   } 

   Flickable {
      id: scrollableBox
      anchors.top: parent.top
      anchors.bottom: resDetailsPanel.visible? resDetailsPanel.top : divider.top
      width: parent.width
      contentWidth: parent.width
      contentHeight: scrollContent.height
      clip: true

      Item {
         id: scrollContent
         width: parent.width
         height: childrenRect.height

         Text {
            id: clientsLabel
            text: "Clients:"
            font.pixelSize: 18
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 24
         }

         Rectangle {
            id: clientsDivider
            width: parent.width
            height: 1
            color: "#d32f2f"
            anchors.top: clientsLabel.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
         }

         GridView {
            id: clientsGrid
            height: childrenRect.height
            interactive: false
            anchors.top: clientsDivider.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right

            model: controller.clients

            Button {
               id: emptyClientsButton
               text: "+ Add a Client"
               onClicked: {
                   resDetailsPanel.resType = 0
                   resDetailsPanel.visible = true
               }
               anchors.left: parent.left
               anchors.leftMargin: 24
               anchors.top: parent.top
               anchors.topMargin: 8
               visible: parent.count == 0

               contentItem: Text {
                  text: emptyClientsButton.text
                  font.pixelSize: 18
                  color: emptyClientsButton.down ? "#ef9a9a" : "#d32f2f"
               }

               background: Rectangle {
                  color: "white"
               }
            }

            delegate: Item {
               id: gridItem
               width: clientsGrid.cellWidth
               height: clientsGrid.cellHeight

               Rectangle {
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom
                  color: "#e0e0e0"
                  visible: clickArea.pressed
               }

               Item {
                  height: childrenRect.height
                  width: parent.width
                  anchors.verticalCenter: parent.verticalCenter

                  Image {
                     id: resIcon
                     width: 48
                     height: 48
                     source: "images/client_icon_48dp.png"
                     anchors.horizontalCenter: parent.horizontalCenter
                  }

                  Text {
                     anchors.top: resIcon.bottom
                     anchors.topMargin: 8
                     anchors.right: parent.right
                     anchors.left: parent.left
                     anchors.rightMargin: 8
                     anchors.leftMargin: 8
                     font.pixelSize: 13
                     horizontalAlignment: Text.AlignHCenter
                     text: model.modelData.resourceName
                     elide: Text.ElideRight
                  }
               }

               MouseArea {
                  id: clickArea
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom

                  onClicked: {
                     stackView.push(
                              Qt.resolvedUrl("ResourceDetailsPage.qml"),
                              {"resModel" : model.modelData}
                              )
                  }
               }
            }
         }

         Text {
            id: directorsLabel
            text: "Directors:"
            font.pixelSize: 18
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: clientsGrid.bottom
            anchors.topMargin: 24
         }

         Rectangle {
            id: directorsDivider
            width: parent.width
            height: 1
            color: "#d32f2f"
            anchors.top: directorsLabel.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
         }

         GridView {
            id: directorsGrid
            height: childrenRect.height
            interactive: false
            anchors.top: directorsDivider.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right

            model: controller.directors

            Button {
               id: emptyDirsButton
               text: "+ Add a Director"
               onClicked: {
                   resDetailsPanel.resType = 1
                   resDetailsPanel.visible = true
               }
               anchors.left: parent.left
               anchors.leftMargin: 24
               anchors.top: parent.top
               anchors.topMargin: 8
               visible: parent.count == 0

               contentItem: Text {
                  text: emptyDirsButton.text
                  font.pixelSize: 18
                  color: emptyDirsButton.down ? "#ef9a9a" : "#d32f2f"
               }

               background: Rectangle {
                  color: "white"
               }
            }

            delegate: Item {
               id: dirGridItem
               width: directorsGrid.cellWidth
               height: directorsGrid.cellHeight

               Rectangle {
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom
                  color: "#e0e0e0"
                  visible: dirClickArea.pressed
               }

               Item {
                  height: childrenRect.height
                  width: parent.width
                  anchors.verticalCenter: parent.verticalCenter

                  Image {
                     id: dirResIcon
                     width: 48
                     height: 48
                     source: "images/director_icon_48dp.png"
                     anchors.horizontalCenter: parent.horizontalCenter
                  }

                  Text {
                     anchors.top: dirResIcon.bottom
                     anchors.topMargin: 8
                     anchors.right: parent.right
                     anchors.left: parent.left
                     anchors.rightMargin: 8
                     anchors.leftMargin: 8
                     font.pixelSize: 13
                     horizontalAlignment: Text.AlignHCenter
                     text: model.modelData.resourceName
                     elide: Text.ElideRight
                  }
               }

               MouseArea {
                  id: dirClickArea
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom

                  onClicked: {
                     stackView.push(
                              Qt.resolvedUrl("ResourceDetailsPage.qml"),
                              {"resModel" : model.modelData}
                              )
                  }
               }
            }
         }

         Text {
            id: storagesLabel
            text: "Storages:"
            font.pixelSize: 18
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: directorsGrid.bottom
            anchors.topMargin: 24
         }

         Rectangle {
            id: storagesDivider
            width: parent.width
            height: 1
            color: "#d32f2f"
            anchors.top: storagesLabel.bottom
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
         }

         GridView {
            id: storagesGrid
            height: childrenRect.height
            interactive: false
            anchors.top: storagesDivider.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right

            model: controller.storages

            Button {
               id: emptyStoragesButton
               text: "+ Add a Storage"
               onClicked: {
                   resDetailsPanel.resType = 2
                   resDetailsPanel.visible = true
               }
               anchors.left: parent.left
               anchors.leftMargin: 24
               anchors.top: parent.top
               anchors.topMargin: 8
               visible: parent.count == 0

               contentItem: Text {
                  text: emptyStoragesButton.text
                  font.pixelSize: 18
                  color: emptyStoragesButton.down ? "#ef9a9a" : "#d32f2f"
               }

               background: Rectangle {
                  color: "white"
               }
            }

            delegate: Item {
               id: sdGridItem
               width: storagesGrid.cellWidth
               height: storagesGrid.cellHeight

               Rectangle {
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom
                  color: "#e0e0e0"
                  visible: sdClickArea.pressed
               }

               Item {
                  height: childrenRect.height
                  width: parent.width
                  anchors.verticalCenter: parent.verticalCenter

                  Image {
                     id: sdResIcon
                     width: 48
                     height: 48
                     source: "images/storage_icon_48dp.png"
                     anchors.horizontalCenter: parent.horizontalCenter
                  }

                  Text {
                     anchors.top: sdResIcon.bottom
                     anchors.topMargin: 8
                     anchors.right: parent.right
                     anchors.left: parent.left
                     anchors.rightMargin: 8
                     anchors.leftMargin: 8
                     font.pixelSize: 13
                     horizontalAlignment: Text.AlignHCenter
                     text: model.modelData.resourceName
                     elide: Text.ElideRight
                  }
               }

               MouseArea {
                  id: sdClickArea
                  anchors.left: parent.left
                  anchors.right: parent.right
                  anchors.top: parent.top
                  anchors.bottom: parent.bottom

                  onClicked: {
                     stackView.push(
                              Qt.resolvedUrl("ResourceDetailsPage.qml"),
                              {"resModel" : model.modelData}
                              )
                  }
               }
            }
         }

         // Visual margin in the bottom
         Item {
            width: parent.width
            height: 32
            anchors.top: storagesGrid.bottom
         }

      } // Item (ScrollView contents)
   } // ScrollView

   Rectangle {
       id: divider
       visible: !resDetailsPanel.visible
       width: parent.width
       height: 1
       color: "#d32f2f"
       anchors.bottom: tconfigButton.top
   }

   Button {
       id: tconfigButton
       visible: !resDetailsPanel.visible
       text: "Tray Config"
       onClicked: stackView.push(Qt.resolvedUrl("TrayUiPage.qml"))
       anchors.leftMargin: 12
       anchors.left: parent.left
       anchors.bottom: parent.bottom

       contentItem: Text {
           text: tconfigButton.text
           font.pixelSize: 18
           color: tconfigButton.down ? "#ef9a9a" : "#d32f2f"
       }

       background: Rectangle {
           color: "white"
       }
   }

   Button {
       id: fdButton
       visible: !resDetailsPanel.visible
       text: "File Daemon"
       onClicked: stackView.push(Qt.resolvedUrl("FileDaemonPage.qml"))
       anchors.rightMargin: 12
       anchors.right: parent.right
       anchors.bottom: parent.bottom

       contentItem: Text {
           text: fdButton.text
           font.pixelSize: 18
           color: fdButton.down ? "#ef9a9a" : "#d32f2f"
       }

       background: Rectangle {
           color: "white"
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
}
