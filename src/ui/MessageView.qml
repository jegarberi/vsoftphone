import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import vsoftphone 1.0

Item {
    id: root

    property string remoteParty: ""
    property bool isRemoteTyping: false
    property var conversations: storageService.conversations()

    onRemotePartyChanged: recipientBox.editText = remoteParty

    function send() {
        if (messageInput.text.length === 0 || root.remoteParty === "")
            return
        messageModel.sendMessage(messageInput.text)
        messageInput.text = ""
    }

    Connections {
        target: storageService
        function onMessagesChanged() {
            root.conversations = storageService.conversations()
            recipientBox.editText = root.remoteParty
        }
    }

    MessageModel {
        id: messageModel
        currentRemoteParty: root.remoteParty
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Pick an existing conversation or type a new recipient
        ComboBox {
            id: recipientBox
            Layout.fillWidth: true
            editable: true
            model: root.conversations
            onActivated: root.remoteParty = currentText
            onAccepted: root.remoteParty = editText
            Component.onCompleted: editText = root.remoteParty
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: messageModel

            // Our messages on the right, theirs on the left
            delegate: Item {
                id: row
                readonly property bool mine: model.direction !== "Incoming"
                readonly property bool failed: model.direction === "Failed"
                width: ListView.view.width
                height: bubble.height

                Rectangle {
                    id: bubble
                    x: row.mine ? row.width - width : 0
                    width: Math.min(row.width * 0.82, Math.max(body.implicitWidth, stamp.implicitWidth) + 20)
                    height: body.height + stamp.height + 14
                    radius: Theme.look === "" ? 10 : 0
                    color: row.failed ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.22)
                         : row.mine ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                         : Theme.surface

                    Label {
                        id: body
                        x: 10
                        y: 6
                        width: bubble.width - 20
                        text: model.content
                        wrapMode: Text.Wrap
                    }
                    Label {
                        id: stamp
                        x: 10
                        y: body.y + body.height + 2
                        text: (row.failed ? "Not delivered  ·  " : "") + model.timestamp
                        color: row.failed ? Theme.danger : Theme.muted
                        font.pixelSize: 9
                    }
                }
            }

            onCountChanged: positionViewAtEnd()
            Component.onCompleted: positionViewAtEnd()

            Label {
                anchors.centerIn: parent
                visible: listView.count === 0
                text: root.remoteParty === "" ? "Choose or type a recipient" : "No messages yet"
                color: Theme.muted
            }
        }

        Label {
            text: root.remoteParty + " is typing…"
            visible: root.isRemoteTyping
            font.italic: true
            font.pixelSize: 11
            color: Theme.muted
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            TextField {
                id: messageInput
                Layout.fillWidth: true
                placeholderText: "Message"
                enabled: root.remoteParty !== ""
                onAccepted: root.send()
            }
            PillButton {
                text: "Send"
                kind: "accent"
                enabled: messageInput.text.length > 0 && root.remoteParty !== ""
                onClicked: root.send()
            }
        }
    }

    Timer {
        id: typingTimer
        interval: 3000
        onTriggered: sipService.sendTypingIndication(accountModel.defaultAccountId, root.remoteParty, false)
    }

    Connections {
        target: messageInput
        function onTextChanged() {
            if (messageInput.text.length > 0) {
                if (!typingTimer.running)
                    sipService.sendTypingIndication(accountModel.defaultAccountId, root.remoteParty, true)
                typingTimer.restart()
            }
        }
    }

    Connections {
        target: sipService
        function onTypingIndicationReceived(fromUri, isTyping) {
            if (root.remoteParty !== "" && fromUri.includes(root.remoteParty))
                root.isRemoteTyping = isTyping
        }
    }
}
