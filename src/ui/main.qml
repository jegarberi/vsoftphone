import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Qt.labs.platform as Platform

ApplicationWindow {
    id: root
    width: 360
    height: 600
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("Microsip")
    color: Theme.background

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.background: Theme.background
    Material.foreground: Theme.foreground
    Material.accent: Theme.accent
    Material.primary: Theme.accent

    // --- Closing: quit, or keep running in the system tray -------------------

    function showFromTray() {
        show()
        raise()
        requestActivate()
    }

    onClosing: (close) => {
        if (Theme.closeToTray && tray.available) {
            close.accepted = false
            hide()
        } else {
            Qt.quit()
        }
    }

    Platform.SystemTrayIcon {
        id: tray
        visible: Theme.closeToTray
        icon.source: "qrc:/ui/icon.png"
        tooltip: root.title + (accountModel.defaultAccountId !== "" ? " – " + accountModel.defaultAccountId : "")
        onActivated: root.visible ? root.hide() : root.showFromTray()

        menu: Platform.Menu {
            Platform.MenuItem {
                text: "Show"
                onTriggered: root.showFromTray()
            }
            Platform.MenuItem {
                text: sipService.doNotDisturb ? "Turn off Do Not Disturb" : "Do Not Disturb"
                onTriggered: sipService.doNotDisturb = !sipService.doNotDisturb
            }
            Platform.MenuItem {
                text: "Quit"
                onTriggered: Qt.quit()
            }
        }
    }

    // --- Header: which account is in use and how it is doing ---------------

    header: ToolBar {
        Keys.onPressed: (event) => root.handleDialKey(event)
        Material.primary: Theme.surface
        Material.foreground: Theme.foreground
        Material.elevation: 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 4
            spacing: 8

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: accountModel.defaultAccountId === "" ? Theme.muted
                     : accountModel.defaultAccountStatus === "Registered" ? Theme.success
                     : accountModel.defaultAccountStatus === "" ? Theme.warning
                     : Theme.danger
            }

            // Click to switch between the active accounts
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: accountModel.defaultAccountId === "" ? "No account"
                    : accountModel.defaultAccountId
                ToolTip.visible: accountMouse.containsMouse
                ToolTip.delay: 600
                ToolTip.text: accountModel.defaultAccountId === ""
                    ? "Add an account, or dial an IP address"
                    : (accountModel.defaultAccountStatus || "Connecting") + " (click for the next account)"

                MouseArea {
                    id: accountMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: accountModel.selectNextAccount()
                }
            }

            ToolButton {
                text: "DND"
                font.pixelSize: 11
                highlighted: sipService.doNotDisturb
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: sipService.doNotDisturb ? "Do Not Disturb is on" : "Do Not Disturb"
                onClicked: sipService.doNotDisturb = !sipService.doNotDisturb
            }

            ToolButton {
                text: "⚙︎" // text form, not the colour emoji
                font.pixelSize: 18
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: "Settings"
                onClicked: settingsWindowLoader.active = true
            }
        }
    }

    // --- Pages ------------------------------------------------------------

    // Keys nothing else wanted arrive here, whatever has the focus: digits,
    // * and # go to the dial pad, Enter calls. A text field keeps what is
    // typed into it, so chatting and searching are unaffected.
    function handleDialKey(event) {
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
            return
        if (/^[0-9*#+]$/.test(event.text)) {
            tabs.currentIndex = 0
            dialpadTab.type(event.text)
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (tabs.currentIndex !== 0)
                return
            dialpadTab.call(false)
        } else if (event.key === Qt.Key_Backspace && tabs.currentIndex === 0) {
            dialpadTab.backspace()
        } else {
            return
        }
        event.accepted = true
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex
        focus: true
        Keys.onPressed: (event) => root.handleDialKey(event)

        DialpadView {
            id: dialpadTab
        }

        ContactsView {
            onDialRequested: (number) => {
                dialpadTab.dial(number)
                tabs.currentIndex = 0
            }
            onChatRequested: (party) => {
                messagesTab.remoteParty = party
                tabs.currentIndex = 3
            }
        }

        HistoryView {
            onDialRequested: (number) => {
                dialpadTab.dial(number)
                tabs.currentIndex = 0
            }
        }

        MessageView {
            id: messagesTab
        }

        AccountsView {
        }
    }

    footer: TabBar {
        id: tabs
        objectName: "tabs" // found by the test when it takes screenshots
        Keys.onPressed: (event) => root.handleDialKey(event)
        position: TabBar.Footer
        Material.background: Theme.surface

        Repeater {
            model: ["Dial", "Contacts", "History", "Chat", "Accounts"]
            TabButton {
                text: modelData
                font.pixelSize: 12
                font.capitalization: Font.MixedCase
                // Five labels have to fit a 320 pixel window
                leftPadding: 2
                rightPadding: 2
            }
        }
    }

    // --- Windows and notices ------------------------------------------------

    Loader {
        id: settingsWindowLoader
        active: false
        source: "SettingsWindow.qml"
        onLoaded: {
            item.show()
            item.closing.connect(function() { settingsWindowLoader.active = false })
        }
    }

    Dialog {
        id: callFailedDialog
        property alias text: callFailedLabel.text
        title: "Call failed"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 32, 320)
        Label {
            id: callFailedLabel
            width: parent.width
            wrapMode: Text.Wrap
        }
    }

    // Incoming message notice; click to open the conversation
    Popup {
        id: messageNotice
        property string from: ""
        x: parent.width - width - 12
        y: 12
        padding: 12
        closePolicy: Popup.NoAutoClose
        Timer { id: messageNoticeTimer; interval: 8000; onTriggered: messageNotice.close() }
        contentItem: Label {
            text: "New message from " + messageNotice.from
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    messagesTab.remoteParty = messageNotice.from
                    tabs.currentIndex = 3
                    messageNotice.close()
                }
            }
        }
    }

    Connections {
        target: sipService
        function onMessageReceived(from, text) {
            if (tabs.currentIndex === 3 && messagesTab.remoteParty !== "" && from.startsWith(messagesTab.remoteParty))
                return // already looking at this conversation
            messageNotice.from = from
            messageNotice.open()
            messageNoticeTimer.restart()
        }
    }

    // One window per call
    Connections {
        target: callService
        // A ringing call shows a toast; its window opens when it is answered
        function onIncomingCall(callId, remoteUri) {
            var component = Qt.createComponent("IncomingCallToast.qml")
            if (component.status !== Component.Ready) {
                console.error(component.errorString())
                return
            }
            var toast = component.createObject(root, {"callId": callId, "remoteUri": remoteUri})
            toast.answered.connect(function(state) {
                root.openCallWindow(callId, remoteUri, state, true)
            })
            toast.show()
            toast.sync() // auto answer may already have picked it up
        }
        function onOutgoingCall(callId, remoteUri) {
            root.openCallWindow(callId, remoteUri, "CALLING", false)
        }
        function onCallFailed(destination, reason) {
            callFailedDialog.text = "Could not call " + destination + ": " + reason
            callFailedDialog.open()
        }
    }

    function openCallWindow(callId, remoteUri, state, incoming) {
        var component = Qt.createComponent("InCallWindow.qml")
        if (component.status !== Component.Ready) {
            console.error(component.errorString())
            return
        }
        component.createObject(root, {
            "callId": callId,
            "remoteUri": remoteUri,
            "callState": state,
            "isIncoming": incoming
        }).show()
    }
}
