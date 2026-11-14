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

    readonly property var preferences: {
        // Re-evaluate when any profile preference is saved.
        const revision = ProfilePreferences.revision

        if (!root.profile) {
            return {
                displayName: "",
                showAccessoryControls: true,
                error: ""
            }
        }

        return ProfilePreferences.read(root.profile.profileId)
    }



    ProfileSettingsDialog {
        id: settingsDialog
        parent: Overlay.overlay
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
            color: '#ffd980'
            wrapMode: Text.WordWrap
        }

        ComboBox {
            id: profileSelector

            width: parent.width
            model: ["Basic aircraft", "Aircraft with gripper"]

            onActivated: root.loadSelectedProfile()
        }

        Button {
            width: parent.width
            text: "Profile settings"
            enabled: ProfileStore.loaded

            onClicked: {
                settingsDialog.profileId = root.profile.profileId
                settingsDialog.defaultDisplayName = root.profile.displayName
                settingsDialog.open()
            }
        }

        Label {
            width: parent.width
            visible: root.preferences.error.length > 0
            text: root.preferences.error
            color: "#FF7777"
            wrapMode: Text.WordWrap
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

                return root.preferences.displayName.length > 0
                    ? root.preferences.displayName
                    : root.profile.displayName
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

        Label {
            width: parent.width

            visible: ProfileStore.loaded
                && ProfileStore.actions.length > 0
                && !root.preferences.showAccessoryControls

            text: "Accessory controls are hidden. "
                + "Enable them in Profile settings."

            color: "#FFD080"
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.preferences.showAccessoryControls
                ? ProfileStore.actions
                : []

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
