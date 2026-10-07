import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MicrosipClone 1.0

Item {
    id: root
    
    property string remoteParty: ""
    property bool isRemoteTyping: false
    property var conversations: storageService.conversations()

    onRemotePartyChanged: recipientBox.editText = remoteParty

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
        anchors.margins: 10

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
            model: messageModel
            delegate: ItemDelegate {
                width: listView.width
                
                contentItem: ColumnLayout {
                    Label {
                        font.bold: true
                        font.pixelSize: 10
                        text: model.direction === "Failed" ? "Not delivered" : model.direction
                        color: model.direction === "Incoming" ? "blue"
                             : model.direction === "Failed" ? "red" : "green"
                    }
                    Label {
                        text: model.content
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: model.timestamp
                        font.pixelSize: 8
                        color: "gray"
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }
            onCountChanged: listView.positionViewAtEnd()
            
            Component.onCompleted: listView.positionViewAtEnd()
        }

        Label {
            text: remoteParty + " is typing..."
            visible: isRemoteTyping
            font.italic: true
            font.pixelSize: 10
            color: "gray"
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: messageInput
                Layout.fillWidth: true
                placeholderText: "Type a message..."
                onAccepted: {
                    if (text.length > 0) {
                        messageModel.sendMessage(text)
                        text = ""
                    }
                }
            }
            Button {
                text: "Send"
                enabled: messageInput.text.length > 0
                onClicked: {
                    messageModel.sendMessage(messageInput.text)
                    messageInput.text = ""
                }
            }
        }
    }

    Timer {
        id: typingTimer
        interval: 3000
        onTriggered: {
            sipService.sendTypingIndication(accountModel.defaultAccountId, remoteParty, false)
        }
    }

    Connections {
        target: messageInput
        function onTextChanged() {
            if (messageInput.text.length > 0) {
                if (!typingTimer.running) {
                    sipService.sendTypingIndication(accountModel.defaultAccountId, remoteParty, true)
                }
                typingTimer.restart()
            }
        }
    }

    Connections {
        target: sipService
        function onTypingIndicationReceived(fromUri, isTyping) {
            if (remoteParty !== "" && fromUri.includes(remoteParty)) {
                isRemoteTyping = isTyping
            }
        }
    }
}
