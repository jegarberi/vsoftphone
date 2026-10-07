import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: settingsWindow
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

                    CheckBox { text: "Single Call Mode" }
                    RowLayout {
                        Label { text: "Ringtone" }
                        TextField { Layout.fillWidth: true; placeholderText: "Path to ringtone..." }
                        Button { text: "..." }
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
                            text: "Ring Device"
                            color: Theme.textPrimary
                        }
                        ComboBox {
                            id: ringDevCombo
                            model: hardwareService.audioPlaybackDevices.length > 0 ? hardwareService.audioPlaybackDevices : ["No devices available"]
                            Layout.fillWidth: true
                            enabled: hardwareService.audioPlaybackDevices.length > 0
                        }
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

                    CheckBox { text: "Microphone Amplification" }
                    CheckBox { text: "Software Level Adjustment" }
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
                title: "Codecs"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 20
                        ColumnLayout {
                            Label { text: "Available" }
                            Frame {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 200
                                padding: 0
                                ListView {
                                    anchors.fill: parent
                                    model: hardwareService.audioCodecs
                                    delegate: ItemDelegate {
                                        width: parent.width
                                        text: modelData
                                    }
                                    clip: true
                                }
                            }
                        }
                        ColumnLayout {
                            Button { text: "->" }
                            Button { text: "<-" }
                        }
                        ColumnLayout {
                            Label { text: "Enabled" }
                            Frame {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 200
                                padding: 0
                                ListView {
                                    anchors.fill: parent
                                    model: ["Opus", "PCMU", "PCMA"]
                                    delegate: ItemDelegate {
                                        width: parent.width
                                        text: modelData
                                    }
                                    clip: true
                                }
                            }
                        }
                    }
                }
            }

            GroupBox {
                title: "Video"
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
                        Label { text: "Video Codec" }
                        ComboBox {
                            id: videoCodecDevCombo
                            model: hardwareService.videoCodecs
                            Layout.fillWidth: true
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

            GroupBox {
                title: "Network"
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    GridLayout {
                        columns: 4
                        Layout.fillWidth: true
                        Label { text: "Source Port" }
                        TextField { id: sourcePortField; text: "5060"; Layout.preferredWidth: 80 }
                        CheckBox { text: "rport"; checked: true }
                        Item { Layout.fillWidth: true }
                        
                        Label { text: "RTP Ports" }
                        TextField { text: "4000"; Layout.preferredWidth: 80 }
                        Label { text: "-" }
                        TextField { text: "4010"; Layout.preferredWidth: 80 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "STUN Server" }
                        TextField { Layout.fillWidth: true; placeholderText: "stun.l.google.com:19302" }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Save"
                    onClicked: {
                        Theme.saveSettings()
                        hardwareService.setAudioDevice(micDevCombo.currentIndex, speakerDevCombo.currentIndex)
                        hardwareService.speakerVolume = speakerVolSlider.value
                        hardwareService.micVolume = micVolSlider.value
                        hardwareService.echoCancellation = ecCheckBox.checked
                        settingsWindow.close()
                    }
                }
                Button { text: "Cancel"; onClicked: settingsWindow.close() }
            }
        }
    }
}
