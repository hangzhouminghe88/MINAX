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
import io.qt.bmob.runjobcontroller 1.0


Page {
    id: runJobPage
    visible: true
    height: parent ? parent.height : 0
    width: parent ? parent.width : 0

    property var model;

    RunJobUiController {
        id: runJobController

        Component.onCompleted: {
            if(typeof runJobPage.model !== "undefined") {
                runJobController.model = runJobPage.model
            }
        }

        onJobSuccess: {
            successDialog.text = "Job Started"
            successDialog.visible = true
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
                text: "Run Job"
                color: "white"
                font.pixelSize: 18
                anchors.centerIn: parent
            }

        }
    }

    Text {
        id: selectJobLabel
        text: "<b>Job:</b>"
        anchors.verticalCenter: jobsCombo.verticalCenter
        anchors.right: jobsCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: jobsCombo
        model: runJobController.jobNames
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: storageCombo.left
        anchors.right: parent.right
        anchors.rightMargin: 32

        delegate: ItemDelegate {
            width: jobsCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: jobsCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: jobsCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            leftPadding: 8
            rightPadding: jobsCombo.indicator.width + jobsCombo.spacing

            text: jobsCombo.displayText
            font: jobsCombo.font
            color: jobsCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: jobsCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: jobsCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: jobsCombo.height - 1
            width: jobsCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: jobsCombo.popup.visible ? jobsCombo.delegateModel : null
                currentIndex: jobsCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }

        onCurrentIndexChanged: {
            runJobController.handleSelectedJobChange(
                        jobsCombo.currentIndex,
                        selectedLevel.text
            )
        }
    }

    Text {
        id: levelLabel
        text: "<b>Level:</b>"
        anchors.verticalCenter: levelCombo.verticalCenter
        anchors.right: levelCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: levelCombo
        currentIndex: runJobController.selectedLevel
        model: runJobController.jobLevels
        anchors.top: jobsCombo.bottom
        anchors.topMargin: 8
        anchors.left: storageCombo.left
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setLevel(currentIndex)
        }

        delegate: ItemDelegate {
            width: levelCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: levelCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: levelCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedLevel
            leftPadding: 8
            rightPadding: levelCombo.indicator.width + levelCombo.spacing

            text: levelCombo.displayText
            font: levelCombo.font
            color: levelCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {
                if(runJobController.loadedModel) {
                    runJobController.handleSelectedJobChange(
                                jobsCombo.currentIndex,
                                selectedLevel.text
                    )
                }
            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: levelCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: levelCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: levelCombo.height - 1
            width: levelCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: levelCombo.popup.visible ? levelCombo.delegateModel : null
                currentIndex: levelCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: filesLabel
        text: "<b>Files:</b>"
        anchors.top: levelCombo.bottom
        anchors.topMargin: 12
        anchors.right: files.left
        anchors.rightMargin: 8
    }

    Text {
        id: files
        text: runJobController.filesDescription
        anchors.top: levelCombo.bottom
        anchors.topMargin: 12
        anchors.left: storageCombo.left
    }

    Rectangle {
        id: divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: filesLabel.bottom
        anchors.topMargin: 16
        height: 1
        color: "#e0e0e0"
    }

    Text {
        id: clientLabel
        text: "<b>Client:</b>"
        anchors.verticalCenter: clientCombo.verticalCenter
        anchors.right: clientCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: clientCombo
        currentIndex: runJobController.selectedClient
        model: runJobController.clientNames
        anchors.top: divider.bottom
        anchors.topMargin: 16
        anchors.left: storageLabel.right
        anchors.leftMargin: 8
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setClient(currentIndex)
        }

        delegate: ItemDelegate {
            width: clientCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: clientCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: clientCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedClient
            leftPadding: 8
            rightPadding: clientCombo.indicator.width + clientCombo.spacing

            text: clientCombo.displayText
            font: clientCombo.font
            color: clientCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {

            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: clientCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: clientCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: clientCombo.height - 1
            width: clientCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: clientCombo.popup.visible ? clientCombo.delegateModel : null
                currentIndex: clientCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: filesetLabel
        text: "<b>FileSet:</b>"
        anchors.verticalCenter: filesetCombo.verticalCenter
        anchors.right: filesetCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: filesetCombo
        currentIndex: runJobController.selectedFileset
        model: runJobController.filesetNames
        anchors.top: clientCombo.bottom
        anchors.topMargin: 8
        anchors.left: storageCombo.left
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setFileset(currentIndex)
        }

        delegate: ItemDelegate {
            width: filesetCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: filesetCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: filesetCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedFileset
            leftPadding: 8
            rightPadding: filesetCombo.indicator.width + filesetCombo.spacing

            text: filesetCombo.displayText
            font: filesetCombo.font
            color: filesetCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {

            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: filesetCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: filesetCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: filesetCombo.height - 1
            width: filesetCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: filesetCombo.popup.visible ? filesetCombo.delegateModel : null
                currentIndex: filesetCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: poolLabel
        text: "<b>Pool:</b>"
        anchors.verticalCenter: poolCombo.verticalCenter
        anchors.right: poolCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: poolCombo
        currentIndex: runJobController.selectedPool
        model: runJobController.poolNames
        anchors.top: filesetCombo.bottom
        anchors.topMargin: 8
        anchors.left: storageCombo.left
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setPool(currentIndex)
        }

        delegate: ItemDelegate {
            width: poolCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: poolCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: poolCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedPool
            leftPadding: 8
            rightPadding: poolCombo.indicator.width + poolCombo.spacing

            text: poolCombo.displayText
            font: poolCombo.font
            color: poolCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {

            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: poolCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: poolCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: poolCombo.height - 1
            width: poolCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: poolCombo.popup.visible ? poolCombo.delegateModel : null
                currentIndex: poolCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: storageLabel
        text: "<b>Storage:</b>"
        anchors.verticalCenter: storageCombo.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 16
    }

    ComboBox {
        id: storageCombo
        currentIndex: runJobController.selectedStorage
        model: runJobController.storageNames
        anchors.top: poolCombo.bottom
        anchors.topMargin: 8
        anchors.left: storageLabel.right
        anchors.leftMargin: 8
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setStorage(currentIndex)
        }

        delegate: ItemDelegate {
            width: storageCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: storageCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: storageCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedStorage
            leftPadding: 8
            rightPadding: storageCombo.indicator.width + storageCombo.spacing

            text: storageCombo.displayText
            font: storageCombo.font
            color: storageCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {

            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: storageCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: storageCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: storageCombo.height - 1
            width: storageCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: storageCombo.popup.visible ? storageCombo.delegateModel : null
                currentIndex: storageCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: catalogLabel
        text: "<b>Catalog:</b>"
        anchors.verticalCenter: catalogCombo.verticalCenter
        anchors.right: catalogCombo.left
        anchors.rightMargin: 8
    }

    ComboBox {
        id: catalogCombo
        currentIndex: runJobController.selectedCatalog
        model: runJobController.catalogNames
        anchors.top: storageCombo.bottom
        anchors.topMargin: 8
        anchors.left: storageCombo.left
        anchors.right: jobsCombo.right

        onCurrentIndexChanged: {
            runJobController.setCatalog(currentIndex)
        }

        delegate: ItemDelegate {
            width: catalogCombo.width
            contentItem: Text {
                text: model.modelData
                color: "#000000"
                font: catalogCombo.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            highlighted: catalogCombo.highlightedIndex === index
        }

        indicator: Canvas {
        }

        contentItem: Text {
            id: selectedCatalog
            leftPadding: 8
            rightPadding: catalogCombo.indicator.width + catalogCombo.spacing

            text: catalogCombo.displayText
            font: catalogCombo.font
            color: catalogCombo.pressed ? "#ef9a9a" : "#000000"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            onTextChanged: {

            }
        }

        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            border.color: catalogCombo.pressed ? "#ef9a9a" : "#d32f2f"
            border.width: catalogCombo.visualFocus ? 2 : 1
            radius: 2
        }

        popup: Popup {
            y: catalogCombo.height - 1
            width: catalogCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: catalogCombo.popup.visible ? catalogCombo.delegateModel : null
                currentIndex: catalogCombo.highlightedIndex

                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                border.color: "#d32f2f"
                radius: 2
            }
        }
    }

    Text {
        id: priorityLabel
        text: "<b>Priority:</b>"
        anchors.verticalCenter: priorityBox.verticalCenter
        anchors.right: priorityBox.left
        anchors.rightMargin: 8
    }

    TextField {
        id: priorityBox
        text: runJobController.priority
        maximumLength: 2
        anchors.top: catalogCombo.bottom
        anchors.topMargin: 12
        anchors.left: jobsCombo.left
        anchors.right: jobsCombo.right
        inputMethodHints: Qt.ImhDigitsOnly

        topPadding: 10
        bottomPadding: 10
        background: Rectangle {
            radius: 2
            border.color: "#d32f2f"
            border.width: 1
        }

        onTextChanged: runJobController.priority = text
    }

    Rectangle {
        id: runButton
        height: childrenRect.height
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: clickArea.pressed ? "#b71c1c" : "#d32f2f"

        Text {
            text: qsTr("Run >")
            topPadding: 8
            bottomPadding: 8
            anchors.horizontalCenter: parent.horizontalCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 18
            color: "white"
        }
    }

    MouseArea {
        id: clickArea
        anchors.left: runButton.left
        anchors.right: runButton.right
        anchors.top: runButton.top
        anchors.bottom: runButton.bottom

        onClicked: runJobController.runJob()
    }

    MessageDialog {
        id: successDialog
        modality: Qt.ApplicationModal
        standardButtons: StandardButton.Ok
        visible: false

        onAccepted: {
           stackView.pop()
        }
    }

    PulseLoader {
       useDouble: true
       visible: runJobController.isConnecting
       radius: 28
       color: "#d32f2f"
       anchors.horizontalCenter: parent.horizontalCenter
       anchors.bottom: runButton.top
       anchors.bottomMargin: 24
    }
//    Text {
//        id: startDateLabel
//        text: "<b>When:</b>"
//        anchors.verticalCenter: startDate.verticalCenter
//        anchors.left: parent.left
//        anchors.leftMargin: 16
//    }

//    TextField {
//        id: startDate
//        text: runJobController.startDate
//        placeholderText: qsTr("When")
//        anchors.top: catalogCombo.bottom
//        anchors.topMargin: 8
//        anchors.left: startDateLabel.right
//        anchors.leftMargin: 8
//        anchors.right: poolCombo.right
//    }

}
