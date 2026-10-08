import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as M
import QtQuick.Layouts

ApplicationWindow {
    id: inCallWindow
    objectName: "callWindow"
    width: 320
    height: 460
    minimumWidth: 280
    minimumHeight: 420
    title: remoteUri
    visible: true
    // Its own window, not tied to the main one, which may be hidden in the tray
    transientParent: null
    color: Theme.background

    M.Material.theme: Theme.dark ? M.Material.Dark : M.Material.Light
    M.Material.background: Theme.background
    M.Material.foreground: Theme.foreground
    M.Material.accent: Theme.accent
    M.Material.primary: Theme.accent

    property int callId
    property string remoteUri
    property string callState
    property bool isIncoming: false
    property bool hasVideo: false
    property bool isHeld: false
    property bool isInConference: false
    property bool isMuted: false
    property bool isRecording: false
    property int callDuration: 0
    property int lastStatus: 0
    property string lastReason: ""

    readonly property bool connected: callState === "CONFIRMED"
    // An incoming call that has not been answered yet
    readonly property bool ringing: isIncoming && (callState === "INCOMING" || callState === "EARLY")

    // Closing the window ends the call
    onClosing: {
        if (callState !== "DISCONNECTED")
            callService.hangupCall(callId)
        destroy()
    }

    Timer {
        interval: 1000
        running: inCallWindow.connected
        repeat: true
        onTriggered: callDuration++
    }

    // A failed call stays up for a moment so the reason can be read
    Timer {
        id: closeTimer
        interval: 3000
        onTriggered: inCallWindow.close()
    }

    function formatDuration(seconds) {
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = seconds % 60
        var mm = (h > 0 && m < 10 ? "0" : "") + m
        return (h > 0 ? h + ":" : "") + mm + ":" + (s < 10 ? "0" : "") + s
    }

    function statusText() {
        if (callState === "DISCONNECTED")
            return lastStatus >= 400 ? lastStatus + " " + lastReason : "Call ended"
        if (ringing)
            return "Incoming call"
        if (connected)
            return (isHeld ? "On hold  ·  " : "") + formatDuration(callDuration)
        if (callState === "EARLY")
            return "Ringing…"
        return "Calling…"
    }

    // Quiet when off, accent-coloured when on
    component CallButton: PillButton {
        property bool on: false
        Layout.fillWidth: true
        kind: on ? "accent" : "tonal"
        font.pixelSize: 12
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Avatar {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            name: inCallWindow.remoteUri
            size: 72
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            text: inCallWindow.remoteUri
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: inCallWindow.statusText()
            color: inCallWindow.connected ? Theme.success
                 : inCallWindow.callState === "DISCONNECTED" && inCallWindow.lastStatus >= 400 ? Theme.danger
                 : Theme.muted
        }

        // Android: the other side's picture is drawn here, on a surface of
        // its own placed over this gap (the desktop gets a separate window)
        Item {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property bool shown: Qt.platform.os === "android" && inCallWindow.hasVideo
                                          && inCallWindow.callState !== "DISCONNECTED"
            function place() {
                var at = mapToItem(null, 0, 0)
                var px = Screen.devicePixelRatio
                if (shown)
                    callService.setVideoArea((inCallWindow.x + at.x) * px, (inCallWindow.y + at.y) * px,
                                             width * px, height * px)
                else
                    callService.setVideoArea(0, 0, 0, 0)
            }
            onShownChanged: place()
            onWidthChanged: place()
            onHeightChanged: place()
            onYChanged: place()
            Component.onDestruction: callService.setVideoArea(0, 0, 0, 0)
        }

        // Android: earpiece, loudspeaker, or a headset when one is connected
        RowLayout {
            Layout.fillWidth: true
            visible: hardwareService.audioOutputs.length > 0
            Label { text: "Sound" }
            ComboBox {
                Layout.fillWidth: true
                model: hardwareService.audioOutputs
                onActivated: hardwareService.setAudioOutput(currentText)
            }
            // A headset can be plugged in or switched on at any moment
            Timer {
                interval: 2000
                repeat: true
                running: parent.visible
                onTriggered: hardwareService.refreshAudioOutputs()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 4
            columnSpacing: 4
            enabled: inCallWindow.connected

            CallButton {
                text: "Mute"
                on: isMuted
                onClicked: {
                    callService.toggleMute(callId)
                    isMuted = !isMuted
                }
            }
            CallButton {
                text: isHeld ? "Resume" : "Hold"
                on: isHeld
                onClicked: isHeld ? callService.resumeCall(callId) : callService.holdCall(callId)
            }
            CallButton {
                text: "Keypad"
                onClicked: dtmfPopup.open()
            }
            CallButton {
                text: "Transfer"
                onClicked: transferDialog.open()
            }
            CallButton {
                text: "Conference"
                on: isInConference
                onClicked: isInConference ? callService.removeFromConference(callId)
                                          : callService.addToConference(callId)
            }
            CallButton {
                text: "Record"
                on: isRecording
                onClicked: isRecording = callService.toggleRecording(callId)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            PillButton {
                Layout.fillWidth: true
                visible: inCallWindow.ringing
                text: "Answer"
                kind: "success"
                onClicked: callService.answerCall(callId)
            }
            PillButton {
                visible: inCallWindow.ringing && callService.offersVideo(callId)
                text: "Video"
                kind: "success"
                onClicked: callService.answerCall(callId, true)
            }
            PillButton {
                Layout.fillWidth: true
                text: inCallWindow.ringing ? "Decline" : "Hang up"
                kind: "danger"
                onClicked: {
                    callService.hangupCall(callId)
                    inCallWindow.close()
                }
            }
        }
    }

    Popup {
        id: dtmfPopup
        anchors.centerIn: parent
        modal: true
        padding: 8

        GridLayout {
            columns: 3
            rowSpacing: 2
            columnSpacing: 2
            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]
                Button {
                    flat: true
                    text: modelData
                    font.pixelSize: 18
                    implicitWidth: 64
                    onClicked: callService.sendDtmf(callId, modelData)
                }
            }
        }
    }

    Dialog {
        id: transferDialog
        title: "Transfer call"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: parent
        width: Math.min(inCallWindow.width - 24, 300)

        onOpened: {
            standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return transferTargetField.text.trim() !== ""
            })
            transferTargetField.forceActiveFocus()
        }
        onAccepted: {
            callService.transferCall(callId, transferTargetField.text.trim())
            transferTargetField.clear()
        }

        TextField {
            id: transferTargetField
            width: parent.width
            placeholderText: "Number or user@host"
            onAccepted: if (text.trim() !== "") transferDialog.accept()
        }
    }

    // The call's row in the model is the source of truth for its state
    Connections {
        target: callModel
        function onDataChanged(topLeft, bottomRight, roles) {
            for (var i = 0; i < callModel.rowCount(); i++) {
                var row = callModel.index(i, 0)
                if (callModel.data(row, Qt.UserRole + 1) !== callId)
                    continue
                callState = callModel.data(row, Qt.UserRole + 3)
                hasVideo = callModel.data(row, Qt.UserRole + 4)
                isHeld = callModel.data(row, Qt.UserRole + 5)
                isInConference = callModel.data(row, Qt.UserRole + 6)
                lastStatus = callModel.data(row, Qt.UserRole + 7)
                lastReason = callModel.data(row, Qt.UserRole + 8)

                if (callState === "DISCONNECTED") {
                    if (lastStatus >= 400)
                        closeTimer.start()
                    else
                        inCallWindow.close()
                }
                break
            }
        }
    }
}
