import QtQuick
import QtWebSockets

Item {
    id: root
    visible: false

    property var vehicle: null
    property url endpoint: "ws://127.0.0.1:8765/bridge"

    readonly property bool connected: socket.status === WebSocket.Open
    readonly property bool chatBusy: chatRequest !== ""

    property string connectionState: "Connecting"
    property string description: "Waiting for vehicle status."
    property string statusRequest: ""
    property string chatRequest: ""
    property int sequence: 0
    property double statusSentAt: 0
    property double chatSentAt: 0

    signal chatMessage(string speaker, string message)

    function nextId(prefix) {
        sequence += 1
        return prefix + "-" + sequence
    }

    function sendChat(text) {
        const clean = text.trim()
        if (!connected || chatBusy || clean.length === 0)
            return false

        if (clean.length > 4000) {
            chatMessage("System", "Keep messages under 4,000 characters.")
            return false
        }

        chatRequest = nextId("chat")
        chatSentAt = Date.now()
        chatMessage("You", clean)

        socket.sendTextMessage(JSON.stringify({
            version: 1,
            type: "chat_request",
            request_id: chatRequest,
            message: clean
        }))
        return true
    }

    function sendStatus() {
        if (!connected || statusRequest !== "")
            return

        const current = vehicle
        const live = current !== null && current !== undefined
            && !current.vehicleLinkManager.communicationLost

        statusRequest = nextId("status")
        statusSentAt = Date.now()

        socket.sendTextMessage(JSON.stringify({
            version: 1,
            type: "vehicle_status",
            request_id: statusRequest,
            connected: live,
            vehicle_id: current ? current.id : null,
            flight_mode: live ? String(current.flightMode) : null
        }))
    }

    function clearPendingChat(message) {
        if (chatRequest !== "") {
            chatRequest = ""
            chatMessage("System", message)
        }
    }

    onVehicleChanged: {
        statusRequest = ""
        description = "Vehicle changed; awaiting fresh status."
        clearPendingChat("Vehicle changed. The previous reply was discarded.")
    }

    WebSocket {
        id: socket
        url: root.endpoint
        active: true

        onStatusChanged: {
            if (status === WebSocket.Open) {
                root.connectionState = "Connected to Python"
                root.statusRequest = ""
                root.sendStatus()
            } else if (status === WebSocket.Error
                       || status === WebSocket.Closed) {
                root.connectionState = "Disconnected; retrying"
                root.description = "No fresh vehicle status."
                root.statusRequest = ""
                root.clearPendingChat(
                    "Connection lost. Your message was not resent.")
                socket.active = false
                reconnect.restart()
            } else if (status === WebSocket.Connecting) {
                root.connectionState = "Connecting"
            }
        }

        onTextMessageReceived: function(message) {
            if (message.length > 262144)
                return

            let reply
            try {
                reply = JSON.parse(message)
            } catch (error) {
                return
            }

            if (!reply || reply.version !== 1)
                return

            if (reply.type === "panel_update"
                    && root.statusRequest !== ""
                    && reply.request_id === root.statusRequest
                    && typeof reply.description === "string") {
                root.statusRequest = ""
                root.description = reply.description.slice(0, 2000)
                return
            }

            if (root.chatRequest === ""
                    || reply.request_id !== root.chatRequest)
                return

            if ((reply.type === "chat_response"
                    || reply.type === "chat_error")
                    && typeof reply.message === "string") {
                root.chatRequest = ""
                root.chatMessage(
                    reply.type === "chat_response" ? "AI" : "System",
                    reply.message)
            }
        }
    }

    Timer {
        id: reconnect
        interval: 2000
        onTriggered: socket.active = true
    }

    Timer {
        interval: 1000
        running: true
        repeat: true

        onTriggered: {
            const now = Date.now()

            if (root.chatRequest !== ""
                    && now - root.chatSentAt > 60000) {
                root.clearPendingChat(
                    "Reply timed out. No vehicle action was executed.")
            }

            if (root.statusRequest !== ""
                    && now - root.statusSentAt > 5000) {
                root.statusRequest = ""
                root.description = "Vehicle status response timed out."
                socket.active = false
                reconnect.restart()
                return
            }

            root.sendStatus()
        }
    }
}
