import QtQuick
import QtQuick.Controls
import Yousif.ControlCenter 1.0

Rectangle {
    id: root

    implicitWidth: 340
    implicitHeight: content.implicitHeight + 24

    color: "#202833"
    border.color: "#536274"
    radius: 8

    readonly property var profile:
        ProfileStore.loaded ? ProfileStore.profile : null

    readonly property string loadError: ProfileStore.error

    function loadSelectedProfile() {
        const fileName = profileSelector.currentIndex === 0
            ? "basic-aircraft.json"
            : "aircraft-with-gripper.json"

        ProfileStore.loadExample(fileName)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons

        onWheel: function(wheel) {
            wheel.accepted = true
        }
    }

    Column {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 10

        Label {
            width: parent.width
            text: "Profile Inspector — shared store"
            color: "white"
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: "Development data — no hardware commands"
            color: "#FFD080"
            wrapMode: Text.WordWrap
        }

        ComboBox {
            id: profileSelector

            width: parent.width
            model: ["Basic aircraft", "Aircraft with gripper"]

            onActivated: root.loadSelectedProfile()
        }

        Label {
            width: parent.width
            color: root.loadError ? "#FF9999" : "white"
            wrapMode: Text.WordWrap

            text: {
                if (root.loadError)
                    return "Profile error: " + root.loadError

                if (!root.profile)
                    return "No profile loaded"

                return root.profile.displayName
            }
        }

        Label {
            width: parent.width
            color: "#B8C7D9"
            wrapMode: Text.WordWrap

            text: root.profile
                ? "Configured accessories: " + root.profile.accessories.length
                : ""
        }

        Label {
            width: parent.width
            color: "white"
            wrapMode: Text.WordWrap

            visible: root.profile !== null
                && root.profile.accessories.length === 0

            text: "No configured accessories"
        }

        Repeater {
            model: root.profile ? root.profile.accessories : []

            delegate: Column {
                id: accessorySection

                required property var modelData

                width: content.width
                spacing: 6

                Label {
                    width: parent.width
                    text: accessorySection.modelData.displayName
                    color: "white"
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    width: parent.width
                    text: "Verification: "
                        + accessorySection.modelData.verificationState
                    color: "#FFD080"
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: accessorySection.modelData.actions

                    delegate: Button {
                        required property var modelData

                        width: accessorySection.width
                        text: modelData.displayName
                        enabled: false
                    }
                }

                Label {
                    width: parent.width
                    text: "Commands disabled. Hardware mapping "
                        + "and verification are required."
                    color: "#FFD080"
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: root.loadSelectedProfile()
}
