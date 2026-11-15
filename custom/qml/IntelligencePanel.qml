import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QGroundControl

Rectangle {
    id: root

    implicitWidth: 340
    implicitHeight: 440
    color: "#202833"
    border.color: "#536274"
    radius: 8

    function submitMessage() {
        if (bridge.sendChat(messageInput.text))
            messageInput.clear()
    }

    ListModel {
        id: conversation
    }

    IntelligenceBridge {
        id: bridge
        vehicle: QGroundControl.multiVehicleManager.activeVehicle

        onChatMessage: function(speaker, message) {
            conversation.append({
                speaker: speaker,
                body: message
            })

            if (conversation.count > 100)
                conversation.remove(0)

            Qt.callLater(function() {
                chatList.positionViewAtEnd()
            })
        }
    }

    // Prevent panel clicks from reaching the map.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: qsTr("Intelligence")
            color: "white"
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: bridge.connectionState
            color: bridge.connected ? "#80DEAA" : "#FFD080"
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: bridge.description
            textFormat: Text.PlainText
            color: "white"
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Camera analysis unavailable • Execution not connected")
            color: "#FFD080"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#536274"
        }

        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: conversation

            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                required property string speaker
                required property string body

                width: chatList.width - 12
                spacing: 3

                Label {
                    text: parent.speaker
                    color: parent.speaker === "You" ? "#80C7FF" : "#80DEAA"
                    font.bold: true
                }

                TextEdit {
                    width: parent.width
                    height: implicitHeight

                    text: parent.body
                    textFormat: TextEdit.PlainText
                    color: "white"
                    wrapMode: TextEdit.Wrap

                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                }

                Button {
                    text: qsTr("Copy response")

                    onClicked: {
                        responseCopy.text = parent.body
                        responseCopy.selectAll()
                        responseCopy.copy()
                        responseCopy.deselect()
                    }
                }

                TextEdit {
                    id: responseCopy
                    visible: false
                    readOnly: true
                }
            }
        }

        Label {
            visible: bridge.chatBusy
            text: qsTr("Waiting for AI…")
            color: "#B8C7D9"
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: messageInput
                Layout.fillWidth: true
                placeholderText: qsTr("Type an instruction…")
                maximumLength: 4000
                enabled: true
                onAccepted: root.submitMessage()
            }

            Button {
                text: qsTr("Send")
                enabled: bridge.connected
                    && !bridge.chatBusy
                    && messageInput.text.trim().length > 0
                onClicked: root.submitMessage()
            }
        }
    }
}
