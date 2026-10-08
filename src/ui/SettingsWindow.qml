import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as M
import QtQuick.Layouts

ApplicationWindow {
    id: settingsWindow
    width: 420
    height: 580
    minimumWidth: 340
    minimumHeight: 400
    title: "Settings"
    visible: true
    color: Theme.background

    M.Material.theme: Theme.dark ? M.Material.Dark : M.Material.Light
    M.Material.background: Theme.background
    M.Material.foreground: Theme.foreground
    M.Material.accent: Theme.accent
    M.Material.primary: Theme.accent

    // Apply keeps the current values and stays open; Save also closes
    function applySettings() {
        Theme.uiScale = scaleSlider.value
        Theme.closeToTray = trayCheck.checked
        Theme.saveSettings()

        hardwareService.setAudioDevice(micDevCombo.currentIndex, speakerDevCombo.currentIndex)
        hardwareService.echoCancellation = ecCheckBox.checked
        hardwareService.setVideoDevice(cameraDevCombo.currentIndex)
        hardwareService.saveSettings()
        for (var i = 0; i < codecRepeater.count; i++)
            hardwareService.setCodecEnabled(codecRepeater.itemAt(i).text, codecRepeater.itemAt(i).checked)
        // The volume sliders act at once; from here on Cancel keeps them
        initialSpeakerVolume = hardwareService.speakerVolume
        initialMicVolume = hardwareService.micVolume

        // Read every control before writing anything: a write makes the
        // service announce a change, which sets the controls bound to it
        // back to the saved values, and edits not yet written would be lost
        var values = {
            doNotDisturb: dndCheck.checked,
            autoAnswer: autoAnswerCheck.checked,
            tlsVerify: tlsVerifyCheck.checked,
            userAgent: userAgentField.text,
            logLevel: logLevelCombo.currentIndex,
            acceptIpCalls: acceptIpCheck.checked,
            stunEnabled: stunCheck.checked,
            stunServer: stunField.text,
            turnEnabled: turnCheck.checked,
            turnServer: turnField.text,
            turnUser: turnUserField.text,
            turnPassword: turnPassField.text,
            rport: rportCheck.checked,
            mediaRport: mediaRportCheck.checked,
            forceRelay: forceRelayCheck.checked,
            compactSip: compactSipCheck.checked,
        }
        presenceService.status = presenceCombo.currentText
        for (var key in values)
            sipService[key] = values[key]
        sipService.applyNetworkSettings()
    }

    // The theme and the sound levels are previewed live; closing without
    // Apply goes back to what was saved
    onClosing: {
        Theme.loadSettings()
        hardwareService.stopVideoPreview()
        hardwareService.stopAudioTest()
        hardwareService.restoreSettings()
        hardwareService.speakerVolume = initialSpeakerVolume
        hardwareService.micVolume = initialMicVolume
    }

    // Volumes are the sound device's own levels: nothing is stored for them
    property real initialSpeakerVolume: 1
    property real initialMicVolume: 1
    Component.onCompleted: {
        initialSpeakerVolume = hardwareService.speakerVolume
        initialMicVolume = hardwareService.micVolume
    }

    component Heading: Label {
        Layout.fillWidth: true
        Layout.topMargin: 14
        color: Theme.accent
        font.pixelSize: 11
        font.bold: true
    }

    component Hint: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.muted
        font.pixelSize: 11
    }

    footer: Pane {
        M.Material.background: Theme.surface
        padding: 8
        RowLayout {
            anchors.fill: parent
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                flat: true
                text: "Cancel"
                onClicked: settingsWindow.close()
            }
            PillButton {
                text: "Apply"
                onClicked: settingsWindow.applySettings()
            }
            PillButton {
                kind: "accent"
                text: "Save"
                onClicked: {
                    settingsWindow.applySettings()
                    settingsWindow.close()
                }
            }
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            x: 16
            width: scroll.availableWidth - 32
            spacing: 4

            // --- Appearance -------------------------------------------------
            Heading { text: "APPEARANCE"; Layout.topMargin: 12 }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Theme"; Layout.fillWidth: true }
                ComboBox {
                    id: themeCombo
                    model: Theme.names
                    currentIndex: Math.max(0, Theme.names.indexOf(Theme.current))
                    onActivated: Theme.current = currentText
                }
            }
            Hint {
                visible: (Theme.look !== "") !== Theme.classic
                text: "Colours change at once. The shape of the controls follows the next time the app starts."
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Zoom " + scaleSlider.value.toFixed(2) + "×" }
                Slider {
                    id: scaleSlider
                    Layout.fillWidth: true
                    from: 0.75
                    to: 2.0
                    stepSize: 0.05
                    value: Theme.uiScale
                }
            }
            Hint { text: "Zoom takes effect the next time the app starts." }
            CheckBox {
                id: trayCheck
                Layout.fillWidth: true
                text: "Keep running in the tray when the window is closed"
                checked: Theme.closeToTray
            }

            // --- Calls ------------------------------------------------------
            Heading { text: "CALLS" }
            CheckBox {
                id: dndCheck
                Layout.fillWidth: true
                text: "Do Not Disturb"
                checked: sipService.doNotDisturb
            }
            CheckBox {
                id: autoAnswerCheck
                Layout.fillWidth: true
                text: "Auto answer (when no other call is connected)"
                checked: sipService.autoAnswer
            }
            CheckBox {
                id: acceptIpCheck
                Layout.fillWidth: true
                text: "Accept direct IP calls"
                checked: sipService.acceptIpCalls
            }
            Hint { text: "With this on, anyone who can reach this machine can make it ring." }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Presence"; Layout.fillWidth: true }
                ComboBox {
                    id: presenceCombo
                    model: ["Online", "Away", "Busy", "Offline"]
                    currentIndex: Math.max(0, model.indexOf(presenceService.status))
                }
            }
            Button {
                flat: true
                text: "Open recordings folder"
                onClicked: Qt.openUrlExternally(callService.recordingsUrl())
            }

            // --- Audio ------------------------------------------------------
            Heading { text: "AUDIO" }
            Label { text: "Microphone" }
            ComboBox {
                id: micDevCombo
                Layout.fillWidth: true
                model: hardwareService.audioCaptureDevices
                currentIndex: hardwareService.captureIndex
            }
            Label { text: "Speaker" }
            ComboBox {
                id: speakerDevCombo
                Layout.fillWidth: true
                model: hardwareService.audioPlaybackDevices
                currentIndex: hardwareService.playbackIndex
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Speaker level" }
                Slider {
                    id: speakerVolSlider
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    value: hardwareService.speakerVolume
                    onMoved: hardwareService.speakerVolume = value
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Microphone level" }
                Slider {
                    id: micVolSlider
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    value: hardwareService.micVolume
                    onMoved: hardwareService.micVolume = value
                }
            }
            CheckBox {
                id: ecCheckBox
                Layout.fillWidth: true
                text: "Echo cancellation"
                checked: hardwareService.echoCancellation
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    flat: true
                    text: "Test"
                    onClicked: hardwareService.startAudioTest(micDevCombo.currentIndex, speakerDevCombo.currentIndex)
                }
                Button {
                    flat: true
                    text: "Stop test"
                    onClicked: hardwareService.stopAudioTest()
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    text: "Rescan devices"
                    onClicked: hardwareService.refreshDevices()
                }
            }

            // --- Codecs -----------------------------------------------------
            Heading { text: "AUDIO CODECS" }
            Flow {
                Layout.fillWidth: true
                spacing: 0
                Repeater {
                    id: codecRepeater
                    model: hardwareService.audioCodecs
                    CheckBox {
                        text: modelData
                        checked: hardwareService.isCodecEnabled(modelData)
                    }
                }
            }

            // --- Video ------------------------------------------------------
            Heading { text: "VIDEO" }
            Hint {
                visible: !callService.videoAvailable
                text: "This build has no video support."
            }
            Label { text: "Camera"; visible: callService.videoAvailable }
            ComboBox {
                id: cameraDevCombo
                Layout.fillWidth: true
                visible: callService.videoAvailable
                enabled: count > 0
                model: hardwareService.videoDevices
                currentIndex: hardwareService.videoIndex
                // A running preview follows the selection
                onActivated: if (hardwareService.videoPreviewActive) hardwareService.startVideoPreview(currentIndex)
            }
            RowLayout {
                Layout.fillWidth: true
                visible: callService.videoAvailable
                spacing: 10
                PillButton {
                    text: hardwareService.videoPreviewActive ? "Stop preview" : "Preview"
                    kind: hardwareService.videoPreviewActive ? "accent" : "tonal"
                    enabled: cameraDevCombo.count > 0
                    onClicked: hardwareService.videoPreviewActive
                        ? hardwareService.stopVideoPreview()
                        : hardwareService.startVideoPreview(cameraDevCombo.currentIndex)
                }
                Hint { text: "The preview opens in its own window." }
            }
            Hint {
                visible: callService.videoAvailable && cameraDevCombo.count === 0
                text: "No camera found."
            }

            // --- Network ----------------------------------------------------
            Heading { text: "NETWORK" }
            Hint { text: "STUN and TURN here are used by accounts that name no server of their own." }
            RowLayout {
                Layout.fillWidth: true
                CheckBox {
                    id: stunCheck
                    Layout.fillWidth: true
                    text: "STUN"
                    checked: sipService.stunEnabled
                }
                TextField {
                    id: stunField
                    Layout.fillWidth: true
                    enabled: stunCheck.checked
                    placeholderText: "host:port"
                    text: sipService.stunServer
                }
            }
            RowLayout {
                Layout.fillWidth: true
                CheckBox {
                    id: turnCheck
                    Layout.fillWidth: true
                    text: "TURN"
                    checked: sipService.turnEnabled
                }
                TextField {
                    id: turnField
                    Layout.fillWidth: true
                    enabled: turnCheck.checked
                    placeholderText: "host:port"
                    text: sipService.turnServer
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: turnCheck.checked
                TextField {
                    id: turnUserField
                    Layout.fillWidth: true
                    placeholderText: "TURN username"
                    text: sipService.turnUser
                }
                TextField {
                    id: turnPassField
                    Layout.fillWidth: true
                    placeholderText: "TURN password"
                    echoMode: TextInput.Password
                    text: sipService.turnPassword
                }
            }
            CheckBox {
                id: forceRelayCheck
                Layout.fillWidth: true
                text: "Always send media through the TURN relay"
                checked: sipService.forceRelay
            }
            Hint {
                text: "For networks where audio only goes one way, such as behind a provider's shared address (CGNAT). Needs a TURN server, here or in the account."
            }
            CheckBox {
                id: rportCheck
                Layout.fillWidth: true
                text: "rport: ask servers to reply to our source port"
                checked: sipService.rport
            }
            CheckBox {
                id: mediaRportCheck
                objectName: "mediaRportCheck" // for the test
                Layout.fillWidth: true
                text: "rport for media: advertise the address the server saw"
                checked: sipService.mediaRport
            }
            CheckBox {
                id: compactSipCheck
                objectName: "compactSipCheck"
                Layout.fillWidth: true
                text: "Smaller SIP messages"
                checked: sipService.compactSip
            }
            Hint {
                text: "For calls that never connect because the INVITE is too big for one packet and gets fragmented: short header names and no session timer. Using TCP for the account, or turning off codecs you do not need, helps more."
            }
            CheckBox {
                id: tlsVerifyCheck
                Layout.fillWidth: true
                text: "Verify TLS server certificates"
                checked: sipService.tlsVerify
            }
            TextField {
                id: userAgentField
                Layout.fillWidth: true
                placeholderText: "User agent"
                text: sipService.userAgent
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Log" }
                ComboBox {
                    id: logLevelCombo
                    Layout.fillWidth: true
                    model: ["Errors only", "Normal", "Detailed", "Everything, with SIP messages"]
                    currentIndex: sipService.logLevel
                }
            }
            Hint {
                text: "The user agent is the name this phone gives to servers; leave it empty for the default. The log goes to the terminal the app was started from (logcat on Android); with SIP messages it shows login hashes. These three take effect the next time the app starts."
                Layout.bottomMargin: 12
            }
        }
    }
}
