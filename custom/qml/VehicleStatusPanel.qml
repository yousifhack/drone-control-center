import QtQuick
import QtQuick.Controls
import QGroundControl

Rectangle {
    id: panel

    // Follow whichever vehicle QGroundControl currently selects.
    readonly property var vehicle:
        QGroundControl.multiVehicleManager.activeVehicle

    // A vehicle can remain selected after communication is lost.
    readonly property bool linkLost:
        vehicle ? vehicle.vehicleLinkManager.communicationLost : false

    readonly property bool connected:
        vehicle !== null && vehicle !== undefined && !linkLost

    implicitWidth: 320
    implicitHeight: content.implicitHeight + 24

    color: "#202833"
    radius: 8
    border.color: "#536274"
    border.width: 1

    // Prevent clicks on the panel from reaching the map underneath.
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
        spacing: 8

        Label {
            width: parent.width
            text: qsTr("Vehicle Status")
            color: "white"
            font.bold: true
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: !panel.vehicle
                  ? qsTr("No vehicle connected")
                  : panel.linkLost
                    ? qsTr("Connection lost")
                    : qsTr("Connected")
            color: panel.connected ? "#80DEAA" : "#FFD080"
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: panel.vehicle
                  ? qsTr("Vehicle ID: %1").arg(panel.vehicle.id)
                  : qsTr("Vehicle ID: unavailable")
            color: "white"
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: panel.connected && panel.vehicle.flightMode
                  ? qsTr("Flight mode: %1").arg(panel.vehicle.flightMode)
                  : qsTr("Flight mode: unavailable")
            color: "white"
        }
    }
}
