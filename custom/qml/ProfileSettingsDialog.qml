import QtQuick
import QtQuick.Controls
import Yousif.ControlCenter 1.0

Dialog {
    id: dialog

    property string profileId: ""
    property string defaultDisplayName: ""

    title: "Profile settings"
    modal: true
    anchors.centerIn: parent

    width: Math.min(380, parent.width - 24)

    standardButtons: Dialog.NoButton

    function refreshFields() {
        const saved = ProfilePreferences.read(profileId)

        nameField.text = saved.displayName
        showControls.checked = saved.showAccessoryControls
        errorLabel.text = saved.error
    }

    onOpened: refreshFields()

    contentItem: Column {
        spacing: 12

        Label {
            width: parent.width
            text: dialog.defaultDisplayName
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: "Custom display name"
        }

        TextField {
            id: nameField

            width: parent.width
            maximumLength: 80
            placeholderText: "Leave empty to use the original name"
        }

        CheckBox {
            id: showControls

            text: "Show accessory controls"
        }

        Label {
            width: parent.width
            text: "This changes the interface only. "
                + "It does not enable hardware commands."
            wrapMode: Text.WordWrap
        }

        Label {
            id: errorLabel

            width: parent.width
            visible: text.length > 0
            color: "#FF7777"
            wrapMode: Text.WordWrap
        }

        Button {
            width: parent.width
            text: "Save"

            onClicked: {
                const error = ProfilePreferences.save(
                    dialog.profileId,
                    nameField.text,
                    showControls.checked)

                errorLabel.text = error

                if (error.length === 0)
                    dialog.close()
            }
        }

        Button {
            width: parent.width
            text: "Reset to defaults"

            onClicked: {
                const error = ProfilePreferences.reset(dialog.profileId)

                if (error.length > 0) {
                    errorLabel.text = error
                } else {
                    dialog.refreshFields()
                }
            }
        }

        Button {
            width: parent.width
            text: "Cancel"
            onClicked: dialog.close()
        }
    }
}
