import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    function openEdit(index) {
        accountDialog.editIndex = index
        accountDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: accountModel

            // Click a row to edit the account; the switch registers it
            delegate: ItemDelegate {
                width: ListView.view.width
                enabled: accountModel.ready
                onClicked: root.openEdit(index)

                contentItem: RowLayout {
                    spacing: 10

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: !model.active ? Theme.muted
                             : model.status === "Registered" ? Theme.success
                             : model.status === "" ? Theme.warning
                             : Theme.danger
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: model.accountName || (model.username + "@" + model.domain)
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: !model.active ? "Off" : (model.status || "Connecting…")
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Switch {
                        checked: model.active
                        // The model may refuse (account in a call), so show
                        // what it says rather than what was clicked
                        onClicked: {
                            accountModel.toggleActive(index)
                            checked = Qt.binding(function() { return model.active })
                        }
                    }

                    ToolButton {
                        text: "✕"
                        onClicked: {
                            removeDialog.row = index
                            removeDialog.accountLabel = model.username + "@" + model.domain
                            removeDialog.open()
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: "No accounts yet.\nYou can still call an IP address directly."
                color: Theme.muted
            }
        }

        PillButton {
            Layout.fillWidth: true
            Layout.margins: 8
            text: "Add account"
            enabled: accountModel.ready
            onClicked: root.openEdit(-1)
        }
    }

    // --- Add / edit ---------------------------------------------------------

    component Heading: Label {
        Layout.fillWidth: true
        Layout.topMargin: 8
        color: Theme.accent
        font.pixelSize: 11
        font.bold: true
    }

    Dialog {
        id: accountDialog
        property int editIndex: -1

        title: editIndex >= 0 ? "Edit account" : "Add account"
        // Apply stores the account and keeps the dialog open; Save also closes it
        standardButtons: Dialog.Save | Dialog.Apply | Dialog.Cancel
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 16, 380)
        height: Math.min(root.height - 16, 620)

        onOpened: {
            // An account is identified by username@domain: both are required
            // and the pair must be unique
            var valid = function() {
                return usernameField.text.trim() !== "" && domainField.text.trim() !== ""
                    && !accountModel.isDuplicate(editIndex, usernameField.text, domainField.text)
            }
            standardButton(Dialog.Save).enabled = Qt.binding(valid)
            standardButton(Dialog.Apply).enabled = Qt.binding(valid)

            var adding = editIndex < 0
            var data = adding ? {} : accountModel.getAccount(editIndex)
            accountNameField.text = data["accountName"] || ""
            usernameField.text = data["username"] || ""
            domainField.text = data["domain"] || ""
            passwordField.text = data["password"] || ""
            loginField.text = data["login"] || ""
            displayNameField.text = data["displayName"] || ""
            serverField.text = data["server"] || ""
            proxyField.text = data["proxy"] || ""
            proxy2Field.text = data["proxy2"] || ""
            transportCombo.currentIndex = Math.max(0, transportCombo.model.indexOf(data["transport"]))
            ipVersionCombo.currentIndex = adding ? 2 : data["ipVersion"]
            refreshSpin.value = adding ? 300 : data["registerRefresh"]
            blfField.text = data["blfUri"] || ""
            presenceCheck.checked = adding ? false : data["publishPresence"]
            enableStunCheck.checked = adding ? true : data["enableStun"]
            stunField.text = data["stunServer"] || ""
            iceCheck.checked = adding ? false : data["ice"]
            turnField.text = data["turnServer"] || ""
            turnUserField.text = data["turnUser"] || ""
            turnPassField.text = data["turnPass"] || ""
            srtpCombo.currentIndex = adding ? 0 : data["srtpMode"]
            rewriteCheck.checked = adding ? false : data["allowIpRewrite"]
            autoNatCheck.checked = adding ? false : data["autoNat"]
            usernameField.forceActiveFocus()
        }

        onAccepted: store()
        onApplied: store()

        function store() {
            var data = {
                "accountName": accountNameField.text,
                "server": serverField.text,
                "proxy": proxyField.text,
                "proxy2": proxy2Field.text,
                "username": usernameField.text,
                "domain": domainField.text,
                "login": loginField.text,
                "password": passwordField.text,
                "displayName": displayNameField.text,
                "transport": transportCombo.currentText,
                "ipVersion": ipVersionCombo.currentIndex,
                "registerRefresh": refreshSpin.value,
                "publishPresence": presenceCheck.checked,
                "blfUri": blfField.text,
                "enableStun": enableStunCheck.checked,
                "ice": iceCheck.checked,
                "stunServer": stunField.text,
                "turnServer": turnField.text,
                "turnUser": turnUserField.text,
                "turnPass": turnPassField.text,
                "srtpMode": srtpCombo.currentIndex,
                "allowIpRewrite": rewriteCheck.checked,
                "autoNat": autoNatCheck.checked
            }

            if (editIndex >= 0) {
                accountModel.updateAccount(editIndex, data)
            } else {
                accountModel.addAccount(data)
                // A second Apply must update this account, not add another
                editIndex = accountModel.rowCount() - 1
            }
        }

        ScrollView {
            id: formScroll
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: formScroll.availableWidth
                spacing: 4

                Heading { text: "ACCOUNT"; Layout.topMargin: 0 }
                TextField { id: usernameField; placeholderText: "Username"; Layout.fillWidth: true }
                TextField { id: domainField; placeholderText: "Domain"; Layout.fillWidth: true }
                TextField { id: passwordField; placeholderText: "Password"; echoMode: TextInput.Password; Layout.fillWidth: true }
                TextField { id: accountNameField; placeholderText: "Name shown in the list (optional)"; Layout.fillWidth: true }
                TextField { id: displayNameField; placeholderText: "Display name sent to others (optional)"; Layout.fillWidth: true }
                TextField { id: loginField; placeholderText: "Login, if different from the username"; Layout.fillWidth: true }

                Heading { text: "SERVER" }
                TextField { id: serverField; placeholderText: "SIP server, if different from the domain"; Layout.fillWidth: true }
                TextField { id: proxyField; placeholderText: "Outbound proxy (optional)"; Layout.fillWidth: true }
                TextField { id: proxy2Field; placeholderText: "Second proxy, used when the first fails"; Layout.fillWidth: true; visible: proxyField.text.trim() !== "" || text !== "" }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Transport"; Layout.fillWidth: true }
                    ComboBox { id: transportCombo; model: ["Auto", "UDP", "TCP", "TLS"] }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "IP version"; Layout.fillWidth: true }
                    ComboBox { id: ipVersionCombo; model: ["IPv4 only", "IPv6 only", "Auto"] }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Register every (seconds)"; Layout.fillWidth: true }
                    SpinBox { id: refreshSpin; from: 30; to: 3600; value: 300; editable: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "SRTP"; Layout.fillWidth: true }
                    ComboBox { id: srtpCombo; model: ["Disabled", "Optional", "Mandatory"] }
                }

                Heading { text: "PRESENCE" }
                CheckBox { id: presenceCheck; text: "Publish my presence" }
                TextField { id: blfField; placeholderText: "BLF list URI (BroadWorks busy lamps)"; Layout.fillWidth: true }

                Heading { text: "NAT" }
                CheckBox { id: enableStunCheck; text: "Use STUN" }
                TextField { id: stunField; placeholderText: "STUN server (empty: the one in Settings)"; Layout.fillWidth: true; visible: enableStunCheck.checked }
                CheckBox { id: iceCheck; text: "ICE: direct media first, then STUN, then TURN" }
                TextField { id: turnField; placeholderText: "TURN server (empty: the one in Settings)"; Layout.fillWidth: true }
                TextField { id: turnUserField; placeholderText: "TURN username"; Layout.fillWidth: true; visible: turnField.text !== "" }
                TextField { id: turnPassField; placeholderText: "TURN password"; echoMode: TextInput.Password; Layout.fillWidth: true; visible: turnField.text !== "" }
                CheckBox { id: rewriteCheck; text: "Use the address the server sees (IP rewrite)" }
                CheckBox { id: autoNatCheck; text: "Automatic NAT handling" }
            }
        }
    }

    Dialog {
        id: removeDialog
        property int row: -1
        property string accountLabel: ""
        title: "Remove account"
        standardButtons: Dialog.Yes | Dialog.No
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 24, 320)
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Remove " + removeDialog.accountLabel + "?"
        }
        onAccepted: accountModel.removeAccount(row)
    }

    // An account in a call cannot be removed, switched off or renamed
    Dialog {
        id: busyDialog
        title: "Account in use"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 24, 320)
        Label {
            id: busyLabel
            width: parent.width
            wrapMode: Text.Wrap
        }
    }
    Connections {
        target: accountModel
        function onAccountBusy(message) {
            busyLabel.text = message
            busyDialog.open()
        }
    }
}
