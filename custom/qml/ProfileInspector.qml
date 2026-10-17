import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    implicitWidth: 340
    implicitHeight: content.implicitHeight + 24
    color: "#202833"
    border.color: "#536274"
    radius: 8

    property var profile: null
    property string loadError: ""
    property bool loading: false
    property int requestNumber: 0

    function loadProfile(fileName) {
        const thisRequest = ++requestNumber

        // Clear previous data immediately, so a failed load cannot
        // leave another profile displayed as the selected one.
        profile = null
        loadError = ""
        loading = true

        const request = new XMLHttpRequest()
        request.open("GET", "qrc:/control-center/profiles/" + fileName)

        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE)
                return

            // Ignore an older response if the selection has changed.
            if (thisRequest !== requestNumber)
                return

            loading = false

            try {
                if (request.status !== 0 && request.status !== 200)
                    throw new Error("Unable to read profile")

                const data = JSON.parse(request.responseText)

                if (!data || data.schemaVersion !== 1)
                    throw new Error("Unsupported profile format")

                if (data.developmentOnly !== true)
                    throw new Error("Inspector accepts development profiles only")

                if (typeof data.displayName !== "string"
                        || !Array.isArray(data.accessories))
                    throw new Error("Missing profile display information")

                profile = data
            } catch (error) {
                loadError = String(error)
            }
        }

        request.send()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
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
            text: "Profile Inspector — action check"
            color: "white"
            font.bold: true
        }

        Label {
            width: parent.width
            text: "Development data — no hardware commands"
            color: "#FFD080"
            wrapMode: Text.WordWrap
        }

        ComboBox {
            width: parent.width
            model: ["Basic aircraft", "Aircraft with gripper"]

            onActivated: {
                root.loadProfile(currentIndex === 0
                    ? "basic-aircraft.json"
                    : "aircraft-with-gripper.json")
            }
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: root.loadError ? "#FF9999" : "white"

            text: {
                if (root.loading)
                    return "Loading profile..."
                if (root.loadError)
                    return "Profile error: " + root.loadError
                if (!root.profile)
                    return "No profile loaded"
                return root.profile.displayName
            }
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: "white"

            text: {
                if (!root.profile)
                    return ""

                const accessories = root.profile.accessories
                if (accessories.length === 0)
                    return "No configured accessories"

                return accessories.map(function(accessory) {
                    return accessory.displayName
                        + " — " + accessory.verificationState
                }).join("\n")
            }
        } // End of the information Label

        // This Repeater belongs to the main Column, not the Label.
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

                Repeater {
                    model: accessorySection.modelData.actions || []

                    delegate: Button {
                        required property var modelData

                        width: accessorySection.width
                        text: modelData.displayName
                        enabled: false
                    }
                }

        Label {
                    width: parent.width
                    text: "Development profile: commands disabled.\n"
                        + "Hardware mapping and verification required."
                    color: "#FFD080"
                    wrapMode: Text.WordWrap
                }
            }
        }
    } // End of the main Column

    Component.onCompleted: loadProfile("basic-aircraft.json")
} // End of the Rectangle
