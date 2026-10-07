import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MicrosipClone 1.0

Item {
    id: root


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: accountModel

            delegate: ItemDelegate {
                width: ListView.view.width
                text: model.username + "@" + model.server
                
                contentItem: RowLayout {
                    Label {
                        text: (model.accountName || (model.username + "@" + model.domain))
                              + (model.active && model.status ? " — " + model.status : "")
                    }
                    Button {
                        text: model.active ? "Unregister" : "Register"
                        onClicked: accountModel.toggleActive(index)
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Edit"
                        onClicked: root.openEdit(index)
                    }
                    Button {
                        text: "Remove"
                        onClicked: accountModel.removeAccount(index)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Add Account"
                onClicked: {
                    addAccountDialog.editIndex = -1
                    addAccountDialog.open()
                }
            }
        }
    }

    Dialog {
        id: addAccountDialog
        title: editIndex >= 0 ? "Edit Account" : "Add Account"
        width: 400
        height: 600
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ScrollView {
            anchors.fill: parent
            contentWidth: parent.width
            contentHeight: formLayout.height
            clip: true

            ColumnLayout {
                id: formLayout
                width: parent.width
                spacing: 10

                TextField { id: accountNameField; placeholderText: "Account Name"; Layout.fillWidth: true }
                TextField { id: serverField; placeholderText: "SIP Server"; Layout.fillWidth: true }
                TextField { id: proxyField; placeholderText: "SIP Proxy"; Layout.fillWidth: true }
                TextField { id: usernameField; placeholderText: "Username"; Layout.fillWidth: true }
                TextField { id: domainField; placeholderText: "Domain"; Layout.fillWidth: true }
                TextField { id: loginField; placeholderText: "Login"; Layout.fillWidth: true }
                TextField { id: passwordField; placeholderText: "Password"; echoMode: TextInput.Password; Layout.fillWidth: true }
                TextField { id: displayNameField; placeholderText: "Display Name"; Layout.fillWidth: true }
                
                RowLayout {
                    Label { text: "Transport" }
                    ComboBox {
                        id: transportCombo
                        model: ["Auto", "UDP", "TCP", "TLS"]
                    }
                }
                
                RowLayout {
                    Label { text: "IP Version" }
                    ComboBox {
                        id: ipVersionCombo
                        model: ["IPv4 Only", "IPv6 Only", "Auto"]
                    }
                }

                RowLayout {
                    Label { text: "Register Refresh" }
                    SpinBox {
                        id: refreshSpin
                        value: 300
                        to: 3600
                        editable: true
                    }
                }

                TextField { id: blfField; placeholderText: "BLF list URI (BroadWorks busy lamp field)"; Layout.fillWidth: true }
                CheckBox { id: presenceCheck; text: "Publish Presence" }
                CheckBox { id: enableStunCheck; text: "Enable STUN" }
                TextField { id: stunField; placeholderText: "STUN Server (host:port, empty = the one in Settings)"; Layout.fillWidth: true; visible: enableStunCheck.checked }
                CheckBox { id: iceCheck; text: "ICE: try direct media first, then STUN, then TURN" }
                TextField { id: turnField; placeholderText: "TURN Server (host:port, empty = the one in Settings)"; Layout.fillWidth: true }
                TextField { id: turnUserField; placeholderText: "TURN Username"; Layout.fillWidth: true; visible: turnField.text !== "" }
                TextField { id: turnPassField; placeholderText: "TURN Password"; echoMode: TextInput.Password; Layout.fillWidth: true; visible: turnField.text !== "" }

                RowLayout {
                    Label { text: "SRTP" }
                    ComboBox {
                        id: srtpCombo
                        model: ["Disabled", "Optional", "Mandatory"]
                    }
                }

                CheckBox { id: rewriteCheck; text: "Allow IP Rewrite" }
                CheckBox { id: autoNatCheck; text: "Automatic NAT Handling" }
            }
        }

        property int editIndex: -1

        onOpened: {
            // An account is identified by username@domain: both are required
            // and the pair must be unique
            standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return usernameField.text.trim() !== "" && domainField.text.trim() !== ""
                    && !accountModel.isDuplicate(editIndex, usernameField.text, domainField.text)
            })
            if (editIndex >= 0) {
                var data = accountModel.getAccount(editIndex)
                accountNameField.text = data["accountName"]
                serverField.text = data["server"]
                proxyField.text = data["proxy"]
                usernameField.text = data["username"]
                domainField.text = data["domain"]
                loginField.text = data["login"]
                passwordField.text = data["password"]
                displayNameField.text = data["displayName"]
                
                var transp = data["transport"]
                for(var i=0; i<transportCombo.model.length; i++) {
                    if(transportCombo.model[i] === transp) transportCombo.currentIndex = i
                }
                
                ipVersionCombo.currentIndex = data["ipVersion"]

                refreshSpin.value = data["registerRefresh"]
                blfField.text = data["blfUri"]
                presenceCheck.checked = data["publishPresence"]
                enableStunCheck.checked = data["enableStun"]
                iceCheck.checked = data["ice"]

                rewriteCheck.checked = data["allowIpRewrite"]
                autoNatCheck.checked = data["autoNat"]
                
                title = "Edit Account"
            } else {
                title = "Add Account"
                accountNameField.clear()
                serverField.clear()
                proxyField.clear()
                usernameField.clear()
                domainField.clear()
                loginField.clear()
                passwordField.clear()
                displayNameField.clear()
                transportCombo.currentIndex = 0
                ipVersionCombo.currentIndex = 2
                refreshSpin.value = 300
                blfField.text = ""
                presenceCheck.checked = false
                enableStunCheck.checked = true
                iceCheck.checked = false

                rewriteCheck.checked = false
                autoNatCheck.checked = false
            }
        }

        onAccepted: {
            var data = {
                "accountName": accountNameField.text,
                "server": serverField.text,
                "proxy": proxyField.text,
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
            }
        }
    }

    function openEdit(index) {
        addAccountDialog.editIndex = index
        addAccountDialog.open()
    }
}
