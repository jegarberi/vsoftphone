import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root

    signal dialRequested(string number)
    signal chatRequested(string party)

    // Presence (Online/Offline) and busy lamps (Idle/Ringing/Busy) share the dot
    function statusColor(status) {
        if (status === "Busy")
            return Theme.danger
        if (status === "Ringing")
            return Theme.warning
        if (status === "Online" || status === "Idle")
            return Theme.success
        return Theme.muted
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: contactsModel

            section.property: "group"
            // Contacts without a group come first and get no heading
            section.delegate: Item {
                width: ListView.view.width
                height: section !== "" ? 30 : 0
                Label {
                    visible: section !== ""
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    text: section
                    color: Theme.accent
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // Click a row to put the number on the dial pad
            delegate: ItemDelegate {
                width: ListView.view.width
                onClicked: root.dialRequested(model.number)

                contentItem: RowLayout {
                    spacing: 10

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: root.statusColor(model.status)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: model.name
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: model.number + "  ·  " + model.status
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    ToolButton {
                        text: "Chat"
                        font.pixelSize: 11
                        onClicked: root.chatRequested(model.number)
                    }

                    // Busy-lamp rows come from the server and cannot be removed
                    ToolButton {
                        text: "✕"
                        visible: model.id !== ""
                        onClicked: contactsModel.removeContact(index)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: "No contacts yet"
                color: Theme.muted
            }
        }

        PillButton {
            Layout.fillWidth: true
            Layout.margins: 8
            text: "Add contact"
            onClicked: addContactDialog.open()
        }
    }

    Dialog {
        id: addContactDialog
        title: "Add contact"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 24, 320)

        onOpened: {
            standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return nameField.text.trim() !== "" && numberField.text.trim() !== ""
            })
            nameField.forceActiveFocus()
        }

        ColumnLayout {
            width: parent.width
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "Name"
            }
            TextField {
                id: numberField
                Layout.fillWidth: true
                placeholderText: "Number or user@host"
            }
            TextField {
                id: groupField
                Layout.fillWidth: true
                placeholderText: "Group (optional)"
            }
        }

        onAccepted: {
            contactsModel.addContact(nameField.text.trim(), numberField.text.trim(), groupField.text.trim())
            nameField.clear()
            numberField.clear()
            groupField.clear()
        }
    }
}
