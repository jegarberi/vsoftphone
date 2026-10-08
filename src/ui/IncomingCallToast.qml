import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as M
import QtQuick.Layouts
import QtQuick.Window

// A small notice for a ringing call, on top of everything and independent of
// the main window, so it also shows while that is hidden in the tray.
// The call window opens once the call is answered.
Window {
    id: toast
    objectName: "incomingToast"

    property int callId: -1
    property string remoteUri: ""

    // The call was answered, here or by auto answer; `state` is its call state
    signal answered(string state)

    width: 320
    height: content.implicitHeight + 28
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: null
    color: "transparent"
    // Bottom right of the screen, where the compositor lets windows choose
    x: Screen.virtualX + Screen.desktopAvailableWidth - width - 24
    y: Screen.virtualY + Screen.desktopAvailableHeight - height - 24

    M.Material.theme: Theme.dark ? M.Material.Dark : M.Material.Light
    M.Material.background: Theme.surface
    M.Material.foreground: Theme.foreground
    M.Material.accent: Theme.accent

    function answer() {
        // Answering holds any call already in progress (CallService does it);
        // the model then reports this one connected, which closes the toast
        callService.answerCall(callId)
    }

    function decline() {
        settled = true
        callService.hangupCall(callId)
        close()
    }

    onClosing: destroy()

    // Follow the call: answered (here or by auto answer), cancelled by the
    // caller, or gone. Model updates keep arriving while the window is being
    // torn down, so it settles only once.
    property bool settled: false

    function sync() {
        if (settled)
            return
        var state = "DISCONNECTED" // a call no longer listed has ended
        for (var i = 0; i < callModel.rowCount(); i++) {
            var row = callModel.index(i, 0)
            if (callModel.data(row, Qt.UserRole + 1) === callId) {
                state = callModel.data(row, Qt.UserRole + 3)
                break
            }
        }
        if (state === "CONNECTING" || state === "CONFIRMED") {
            settled = true
            toast.answered(state)
            toast.close()
        } else if (state === "DISCONNECTED") {
            settled = true
            toast.close()
        }
    }

    Connections {
        target: callModel
        function onDataChanged() { toast.sync() }
        function onRowsRemoved() { toast.sync() }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.look === "" ? 12 : 0 // the old desktops had square windows
        color: Theme.surface
        border.color: Theme.accent
        border.width: 1

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Avatar {
                    name: toast.remoteUri
                    size: 40
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        text: "Incoming call"
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                    Label {
                        Layout.fillWidth: true
                        text: toast.remoteUri
                        color: Theme.foreground
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: callService.hasConnectedCall(toast.callId)
                text: "Answering puts your current call on hold."
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                PillButton {
                    Layout.fillWidth: true
                    kind: "danger"
                    text: "Decline"
                    onClicked: toast.decline()
                }
                PillButton {
                    Layout.fillWidth: true
                    kind: "success"
                    text: "Answer"
                    onClicked: toast.answer()
                }
                PillButton {
                    visible: callService.offersVideo(toast.callId)
                    kind: "success"
                    text: "Video"
                    onClicked: callService.answerCall(toast.callId, true)
                }
            }
        }
    }
}
