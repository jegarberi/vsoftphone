import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MicrosipClone 1.0

Item {
    id: root

    signal dialRequested(string number)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: contactsModel

            delegate: ItemDelegate {
                width: ListView.view.width
                text: model.name + " (" + model.number + ")"
                
                contentItem: RowLayout {
                    Label {
                        text: model.name
                        Layout.fillWidth: true
                    }
                     Label {
                        text: model.number
                        Layout.preferredWidth: 100
                    }
                    Label {
                        text: model.status
                        color: model.status === "Online" ? "green" : "gray"
                    }
                    Button {
                        text: "Call"
                        onClicked: root.dialRequested(model.number)
                    }
                    Button {
                        text: "Message"
                        onClicked: {
                            var component = Qt.createComponent("MessageWindow.qml")
                            var win = component.createObject(root, {
                                "remoteParty": model.number
                            })
                            win.show()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Add Contact"
                onClicked: addContactDialog.open()
            }
        }
    }

    Dialog {
        id: addContactDialog
        title: "Add Contact"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            TextField {
                id: nameField
                placeholderText: "Name"
            }
            TextField {
                id: numberField
                placeholderText: "Number"
            }
        }

        onAccepted: {
            contactsModel.addContact(nameField.text, numberField.text)
            nameField.clear()
            numberField.clear()
        }
    }
}
