pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root

    implicitWidth: ScreenTools.defaultFontPixelWidth * 62
    implicitHeight: ScreenTools.defaultFontPixelHeight * 32
    color: qgcPal.window
    border.color: qgcPal.groupBorder
    radius: ScreenTools.defaultFontPixelHeight / 2

    property bool showHistory: false
    property var selectedObject: null
    readonly property real gap: ScreenTools.defaultFontPixelHeight / 2
    readonly property var displayedObjects: bridge.objects.filter(function(item) {
        return root.showHistory || (bridge.cameraLive && item.visible)
    })

    function submitMessage() {
        if (bridge.sendChat(messageInput.text))
            messageInput.clear()
    }

    function openDetails() {
        const id = selectedObject ? selectedObject.id : ""
        // Only application-owned routes may be opened, never model-provided URLs.
        if (/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(id))
            Qt.openUrlExternally("http://127.0.0.1:8765/objects/" + id)
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: root.enabled }
    ListModel { id: conversation }

    IntelligenceBridge {
        id: bridge
        vehicle: QGroundControl.multiVehicleManager.activeVehicle

        onChatMessage: function(speaker, message) {
            conversation.append({speaker: speaker, body: message})
            if (conversation.count > 100)
                conversation.remove(0)
            Qt.callLater(function() { chatList.positionViewAtEnd() })
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Dialog {
        id: detailsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(ScreenTools.defaultFontPixelWidth * 54, parent.width - root.gap * 4)
        modal: true
        title: root.selectedObject ? root.selectedObject.display_id : qsTr("Object details")
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            spacing: root.gap
            QGCLabel {
                Layout.fillWidth: true
                text: root.selectedObject ? qsTr("Tentative label: %1").arg(root.selectedObject.label) : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            TextEdit {
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                text: root.selectedObject ? root.selectedObject.id : ""
                color: qgcPal.text
                font.pointSize: ScreenTools.defaultFontPointSize
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.WrapAnywhere
            }
            QGCLabel {
                Layout.fillWidth: true
                text: root.selectedObject
                    ? qsTr("Last seen: %1").arg(new Date(root.selectedObject.last_seen * 1000).toLocaleString()) : ""
                wrapMode: Text.WordWrap
            }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Identity is tentative. No object location is established.")
                wrapMode: Text.WordWrap
                color: qgcPal.warningText
            }
            QGCButton {
                text: qsTr("Open details file")
                enabled: bridge.connected
                onClicked: root.openDetails()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.gap * 1.5
        spacing: root.gap

        RowLayout {
            Layout.fillWidth: true
            QGCLabel { text: qsTr("Intelligence"); font.bold: true; Layout.fillWidth: true }
            QGCLabel {
                text: bridge.connected ? qsTr("● Connected") : qsTr("● Offline")
                color: bridge.connected ? qgcPal.colorGreen : qgcPal.warningText
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: bridge.description
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height * 0.38
            Layout.maximumHeight: root.height * 0.38
            spacing: root.gap

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                Layout.fillHeight: true
                QGCLabel { text: qsTr("Observations & advice"); font.bold: true }
                ListView {
                    id: adviceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: root.gap
                    model: bridge.connected ? bridge.advisories : []
                    ScrollBar.vertical: ScrollBar {}
                    delegate: QGCLabel {
                        required property var modelData
                        width: adviceList.width - root.gap
                        text: (modelData.severity === "warning" ? "⚠ " : "• ") + modelData.text
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: modelData.severity === "warning" ? qgcPal.warningText : qgcPal.text
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    text: bridge.cameraLive ? qsTr("Camera: live detections") : qsTr("Camera: no fresh observations")
                    color: bridge.cameraLive ? qgcPal.colorGreen : qgcPal.warningText
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: qgcPal.groupBorder
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.fillHeight: true
                QGCLabel { text: qsTr("Objects · tentative"); font.bold: true }
                CheckBox {
                    text: qsTr("Include history")
                    checked: root.showHistory
                    onToggled: root.showHistory = checked
                }
                ListView {
                    id: objectList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: root.gap / 2
                    model: root.displayedObjects
                    ScrollBar.vertical: ScrollBar {}

                    delegate: RowLayout {
                        id: objectRow
                        required property var modelData
                        width: objectList.width - root.gap
                        QGCLabel {
                            Layout.fillWidth: true
                            text: (bridge.cameraLive && objectRow.modelData.visible ? "" : "◷ ")
                                + objectRow.modelData.label + " ?"
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                        }
                        QGCButton {
                            Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 2
                            Layout.maximumWidth: ScreenTools.defaultFontPixelHeight * 2
                            text: qsTr("ⓘ")
                            Accessible.name: qsTr("Details for %1").arg(objectRow.modelData.display_id)
                            onClicked: {
                                root.selectedObject = objectRow.modelData
                                detailsDialog.open()
                            }
                        }
                    }

                    QGCLabel {
                        anchors.fill: parent
                        visible: objectList.count === 0
                        text: root.showHistory ? qsTr("No saved objects yet.") : qsTr("No live tracked objects.")
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: qsTr("Sampled evidence only · Video recording off · Execution unavailable")
            color: qgcPal.warningText
            font.pointSize: ScreenTools.smallFontPointSize
            wrapMode: Text.WordWrap
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: qgcPal.groupBorder }

        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: ScreenTools.defaultFontPixelHeight * 3
            clip: true
            spacing: root.gap
            model: conversation
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: chatEntry
                required property string speaker
                required property string body
                width: chatList.width - root.gap
                spacing: root.gap / 3

                QGCLabel {
                    text: chatEntry.speaker
                    color: chatEntry.speaker === "You" ? qgcPal.colorBlue : qgcPal.text
                    font.bold: true
                }
                TextEdit {
                    id: responseText
                    width: parent.width
                    height: implicitHeight
                    text: chatEntry.body
                    textFormat: TextEdit.PlainText
                    color: qgcPal.text
                    font.pointSize: ScreenTools.defaultFontPointSize
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                }
                QGCButton {
                    text: qsTr("Copy")
                    onClicked: {
                        responseText.selectAll()
                        responseText.copy()
                        responseText.deselect()
                    }
                }
            }
        }

        QGCLabel { visible: bridge.chatBusy; text: qsTr("Waiting for AI…") }

        RowLayout {
            Layout.fillWidth: true
            QGCTextField {
                id: messageInput
                Layout.fillWidth: true
                placeholderText: qsTr("Type an instruction…")
                maximumLength: 4000
                enabled: true
                onAccepted: root.submitMessage()
            }
            QGCButton {
                text: qsTr("Send")
                enabled: bridge.connected && !bridge.chatBusy && messageInput.text.trim().length > 0
                onClicked: root.submitMessage()
            }
        }
    }
}
