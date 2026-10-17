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
            text: "Profile Inspector — action catalog"
            color: "white"
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: "Development profiles — not a connected vehicle"
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
            color: ProfileStore.error ? "#FF9999" : "white"
            wrapMode: Text.WordWrap

            text: {
                if (ProfileStore.error)
                    return "Profile error: " + ProfileStore.error

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
                ? "Configured accessories: "
                    + root.profile.accessories.length
                    + "\nDeclared accessory actions: "
                    + ProfileStore.actions.length
                : ""
        }

        Label {
            width: parent.width
            visible: ProfileStore.loaded
                && ProfileStore.actions.length === 0

            text: "No accessory actions configured"
            color: "white"
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: ProfileStore.actions

            delegate: Column {
                id: actionSection

                required property var modelData

                width: content.width
                spacing: 4

                Label {
                    width: parent.width
                    text: actionSection.modelData.accessoryName
                    color: "white"
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Button {
                    width: parent.width
                    text: actionSection.modelData.displayName

                    // This inspector has no command dispatcher.
                    enabled: false
                }

                Label {
                    width: parent.width
                    text: "Action ID: "
                        + actionSection.modelData.actionId
                    color: "#B8C7D9"
                    wrapMode: Text.WordWrap
                }

                Label {
                    width: parent.width
                    text: actionSection.modelData.blockedReason
                    color: "#FFD080"
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: root.loadSelectedProfile()
}
