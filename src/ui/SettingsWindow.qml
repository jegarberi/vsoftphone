import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: settingsWindow
    // Apply keeps the current values and stays open; Save also closes
    function applySettings() {
        Theme.saveSettings()
        hardwareService.setAudioDevice(micDevCombo.currentIndex, speakerDevCombo.currentIndex)
        hardwareService.speakerVolume = speakerVolSlider.value
        hardwareService.micVolume = micVolSlider.value
        hardwareService.echoCancellation = ecCheckBox.checked
        hardwareService.saveSettings()

        sipService.stunEnabled = stunCheck.checked
        sipService.stunServer = stunField.text
        sipService.turnEnabled = turnCheck.checked
        sipService.turnServer = turnField.text
        sipService.turnUser = turnUserField.text
        sipService.turnPassword = turnPassField.text
        sipService.rport = rportCheck.checked
        sipService.mediaRport = mediaRportCheck.checked
        sipService.applyNetworkSettings()
    }

    // Changes apply live; closing without Save goes back to the saved ones
    onClosing: {
        Theme.loadSettings()
        hardwareService.restoreSettings()
    }
    width: 600
    height: 700
    title: "Settings"
    visible: true
    color: Theme.background

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            id: settingsLayout
            width: parent.width - 20
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: 10
            spacing: 20

            GroupBox {
                title: "General"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.spacing2

                    CheckBox { 
                        text: "Dark Mode"
                        checked: Theme.isDarkMode
                        onToggled: Theme.isDarkMode = checked
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "UI Scale (" + Theme.uiScale.toFixed(2) + "x)" }
                        Slider {
                            id: uiScaleSlider
                            from: 0.75
                            to: 2.0
                            value: Theme.uiScale
                            Layout.fillWidth: true
                            onMoved: Theme.uiScale = value
                        }
                    }

                    CheckBox {
                        text: "Do Not Disturb"
                        checked: sipService.doNotDisturb
                        onToggled: sipService.doNotDisturb = checked
                    }
                    CheckBox {
                        text: "Auto Answer"
                        checked: sipService.autoAnswer
                        onToggled: sipService.autoAnswer = checked
                    }

                    CheckBox {
                        text: "Verify TLS server certificates (applies after restart)"
                        checked: sipService.tlsVerify
                        onToggled: sipService.tlsVerify = checked
                    }
                    Button {
                        text: "Open recordings folder"
                        onClicked: Qt.openUrlExternally("file://" + callService.recordingsDir())
                    }

                    RowLayout {
                        Label { text: "Presence" }
                        ComboBox {
                            model: ["Online", "Away", "Busy", "Offline"]
                            currentIndex: Math.max(0, model.indexOf(presenceService.status))
                            onActivated: presenceService.status = currentText
                        }
                    }
                }
            }

            GroupBox {
                title: "Audio"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Label { 
                            text: "Speaker"
                            color: Theme.textPrimary
                        }
                        ComboBox {
                            id: speakerDevCombo
                            currentIndex: hardwareService.playbackIndex
                            model: hardwareService.audioPlaybackDevices.length > 0 ? hardwareService.audioPlaybackDevices : ["No devices available"]
                            Layout.fillWidth: true
                            enabled: hardwareService.audioPlaybackDevices.length > 0
                        }
                        Label { 
                            text: "Microphone"
                            color: Theme.textPrimary
                        }
                        ComboBox {
                            id: micDevCombo
                            currentIndex: hardwareService.captureIndex
                            model: hardwareService.audioCaptureDevices.length > 0 ? hardwareService.audioCaptureDevices : ["No devices available"]
                            Layout.fillWidth: true
                            enabled: hardwareService.audioCaptureDevices.length > 0
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Speaker Volume" }
                        Slider {
                            id: speakerVolSlider
                            from: 0.0
                            to: 1.0
                            value: hardwareService.speakerVolume
                            Layout.fillWidth: true
                            onMoved: hardwareService.speakerVolume = value
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Mic Volume" }
                        Slider {
                            id: micVolSlider
                            from: 0.0
                            to: 1.0
                            value: hardwareService.micVolume
                            Layout.fillWidth: true
                            onMoved: hardwareService.micVolume = value
                        }
                    }

                    CheckBox {
                        id: ecCheckBox
                        text: "Echo Cancellation"
                        checked: hardwareService.echoCancellation
                        onToggled: hardwareService.echoCancellation = checked
                    }

                    RowLayout {
                        Button {
                            text: "Test Audio"
                            onClicked: hardwareService.startAudioTest(micDevCombo.currentIndex, speakerDevCombo.currentIndex)
                        }
                        Button {
                            text: "Stop Test"
                            onClicked: hardwareService.stopAudioTest()
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Refresh Devices"
                            icon.name: "view-refresh"
                            onClicked: hardwareService.refreshDevices()
                        }
                    }
                }
            }

            GroupBox {
                title: "Audio Codecs"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    Repeater {
                        model: hardwareService.audioCodecs
                        CheckBox {
                            text: modelData
                            checked: hardwareService.isCodecEnabled(modelData)
                            onToggled: hardwareService.setCodecEnabled(modelData, checked)
                        }
                    }
                }
            }

            GroupBox {
                title: "Network"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    Label {
                        text: "STUN and TURN here are used by accounts that name no server of their own. They reach the accounts on Apply or Save."
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            id: stunCheck
                            text: "STUN"
                            checked: sipService.stunEnabled
                        }
                        TextField {
                            Layout.fillWidth: true
                            enabled: stunCheck.checked
                            id: stunField
                            placeholderText: "host:port"
                            text: sipService.stunServer
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            id: turnCheck
                            text: "TURN"
                            checked: sipService.turnEnabled
                        }
                        TextField {
                            Layout.fillWidth: true
                            enabled: turnCheck.checked
                            id: turnField
                            placeholderText: "host:port"
                            text: sipService.turnServer
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: turnCheck.checked
                        TextField {
                            Layout.fillWidth: true
                            id: turnUserField
                            placeholderText: "TURN username"
                            text: sipService.turnUser
                        }
                        TextField {
                            Layout.fillWidth: true
                            id: turnPassField
                            placeholderText: "TURN password"
                            echoMode: TextInput.Password
                            text: sipService.turnPassword
                        }
                    }
                    CheckBox {
                        id: rportCheck
                        text: "rport: ask servers to reply to the port our requests come from"
                        checked: sipService.rport
                    }
                    CheckBox {
                        id: mediaRportCheck
                        text: "rport for media: advertise in SDP the address the server saw us at"
                        checked: sipService.mediaRport
                    }
                }
            }

            GroupBox {
                title: "Video"
                visible: callService.videoAvailable
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Label { 
                            text: "Camera"
                            color: Theme.textPrimary
                        }
                        ComboBox {
                            id: cameraDevCombo
                            model: hardwareService.videoDevices.length > 0 ? hardwareService.videoDevices : ["No devices available"]
                            Layout.fillWidth: true
                            enabled: hardwareService.videoDevices.length > 0
                        }
                    }
                    RowLayout {
                        Button {
                            text: "Preview Video"
                            onClicked: hardwareService.startVideoPreview(cameraDevCombo.currentIndex)
                        }
                        Button {
                            text: "Stop Preview"
                            onClicked: hardwareService.stopVideoPreview()
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Apply"
                    onClicked: settingsWindow.applySettings()
                }
                Button {
                    text: "Save"
                    onClicked: {
                        settingsWindow.applySettings()
                        settingsWindow.close()
                    }
                }
                Button { text: "Cancel"; onClicked: settingsWindow.close() }
            }
        }
    }
}
