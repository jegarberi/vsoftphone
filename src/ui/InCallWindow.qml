import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

ApplicationWindow {
    id: inCallWindow
    onClosing: {
        if (callState !== "DISCONNECTED")
            callService.hangupCall(callId)
        destroy()
    }
    width: 480
    height: 720
    title: "Call - " + remoteUri
    visible: true
    color: Theme.background
    
    property int callId
    property string remoteUri
    property string callState
    property bool isIncoming: false
    property bool hasVideo: false
    property bool isHeld: false
    property bool isInConference: false
    property bool isMuted: false
    property int callDuration: 0
    property int lastStatus: 0
    property string lastReason: ""
    
    // Call duration timer
    Timer {
        id: durationTimer
        interval: 1000
        running: callState === "CONFIRMED"
        repeat: true
        onTriggered: callDuration++
    }

    Timer {
        id: closeTimer
        interval: 3000
        onTriggered: inCallWindow.close()
    }

    function formatDuration(seconds) {
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = seconds % 60
        if (h > 0) {
            return "%1:%2:%3".arg(h).arg(m < 10 ? "0" + m : m).arg(s < 10 ? "0" + s : s)
        }
        return "%1:%2".arg(m).arg(s < 10 ? "0" + s : s)
    }
    
    Rectangle {
        anchors.fill: parent
        color: Theme.background
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            
            // Video/Avatar Area
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: hasVideo ? "#000000" : Theme.backgroundSecondary
                
                // Gradient overlay for better text visibility
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#00000000" }
                        GradientStop { position: 0.7; color: "#00000000" }
                        GradientStop { position: 1.0; color: "#80000000" }
                    }
                }
                
                // Avatar for audio-only calls
                ColumnLayout {
                    visible: !hasVideo
                    anchors.centerIn: parent
                    spacing: Theme.spacing6
                    
                    AppAvatar {
                        name: remoteUri
                        size: 120
                        showStatus: false
                        Layout.alignment: Qt.AlignHCenter
                    }
                    
                    // Animated waveform (placeholder)
                    Row {
                        spacing: 4
                        Layout.alignment: Qt.AlignHCenter
                        
                        Repeater {
                            model: 5
                            Rectangle {
                                width: 4
                                height: 20 + Math.random() * 30
                                radius: 2
                                color: Theme.accentGreen
                                opacity: callState === "CONFIRMED" ? 0.8 : 0.3
                                
                                SequentialAnimation on height {
                                    running: callState === "CONFIRMED"
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 50; duration: 300 + index * 100 }
                                    NumberAnimation { to: 20; duration: 300 + index * 100 }
                                }
                            }
                        }
                    }
                }
                
                // Video placeholder
                Text {
                    visible: hasVideo
                    anchors.centerIn: parent
                    text: "Remote Video"
                    color: "white"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                }
                
                // Local video preview (floating)
                Rectangle {
                    visible: hasVideo
                    width: 120
                    height: 160
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacing4
                    radius: Theme.radiusLarge
                    color: Theme.backgroundTertiary
                    border.width: 2
                    border.color: Theme.border
                    
                    Text {
                        anchors.centerIn: parent
                        text: "You"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }
                
                // Call Info Overlay
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.spacing6
                    spacing: Theme.spacing2
                    
                    Text {
                        text: remoteUri
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.Bold
                        color: "white"
                        Layout.alignment: Qt.AlignHCenter
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: parent.width - Theme.spacing8
                    }
                    
                    Text {
                        text: {
                            if (callState === "CALLING") return "Calling..."
                            if (callState === "INCOMING" || (isIncoming && callState === "EARLY")) return "Incoming Call"
                            if (callState === "CONFIRMED") return formatDuration(callDuration)
                            if (callState === "DISCONNECTED" && lastStatus >= 400) return "Error: " + lastStatus + " " + lastReason
                            return callState
                        }
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSubtitle
                        color: callState === "CONFIRMED" ? Theme.accentGreen : "white"
                        Layout.alignment: Qt.AlignHCenter
                    }
                    
                    // Connection quality indicator
                    Row {
                        visible: callState === "CONFIRMED"
                        spacing: 2
                        Layout.alignment: Qt.AlignHCenter
                        
                        Repeater {
                            model: 4
                            Rectangle {
                                width: 4
                                height: 8 + index * 4
                                radius: 2
                                color: index < 3 ? Theme.accentGreen : Theme.gray500
                            }
                        }
                    }
                }
            }
            
            // Controls Area
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                color: Theme.backgroundSecondary
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing6
                    spacing: Theme.spacing6
                    
                    // Primary Controls
                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacing6
                        
                        // Answer (incoming, not yet answered)
                        ControlButton {
                            icon: "📞"
                            label: "Answer"
                            active: true
                            visible: isIncoming && (callState === "INCOMING" || callState === "EARLY")
                            onClicked: callService.answerCall(callId)
                        }

                        // Mute
                        ControlButton {
                            icon: isMuted ? "🔇" : "🎤"
                            label: "Mute"
                            active: isMuted
                            onClicked: {
                                callService.toggleMute(callId)
                                isMuted = !isMuted
                            }
                        }
                        
                        // Hold/Resume
                        ControlButton {
                            icon: isHeld ? "▶️" : "⏸️"
                            label: isHeld ? "Resume" : "Hold"
                            active: isHeld
                            enabled: callState === "CONFIRMED"
                            onClicked: {
                                if (isHeld)
                                    callService.resumeCall(callId)
                                else
                                    callService.holdCall(callId)
                            }
                        }
                        
                        // Video Toggle
                        ControlButton {
                            icon: "📹"
                            label: "Video"
                            active: hasVideo
                            enabled: false // Implementation pending
                        }
                        
                        // Keypad
                        ControlButton {
                            icon: "#️⃣"
                            label: "Keypad"
                            active: dtmfPanel.visible
                            enabled: callState === "CONFIRMED"
                            onClicked: dtmfPanel.visible = !dtmfPanel.visible
                        }
                    }
                    
                    // Secondary Controls
                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Theme.spacing6
                        
                        // Transfer
                        ControlButton {
                            icon: "➡️"
                            label: "Transfer"
                            enabled: callState === "CONFIRMED"
                            onClicked: transferDialog.open()
                        }
                        
                        // Conference
                        ControlButton {
                            icon: "👥"
                            label: "Conference"
                            active: isInConference
                            enabled: callState === "CONFIRMED"
                            onClicked: {
                                if (isInConference)
                                    callService.removeFromConference(callId)
                                else
                                    callService.addToConference(callId)
                            }
                        }
                        
                        // Add Call
                        ControlButton {
                            icon: "➕"
                            label: "Add Call"
                            enabled: callState === "CONFIRMED"
                        }
                        
                        // More Options
                        ControlButton {
                            icon: "⋯"
                            label: "More"
                        }
                    }
                    
                    // Hangup Button
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 64
                        height: 64
                        radius: Theme.radiusRound
                        color: hangupMouse.pressed ? Qt.darker(Theme.dangerRed, 1.2) : Theme.dangerRed
                        
                        layer.enabled: true
                        layer.effect: DropShadow {
                            horizontalOffset: 0
                            verticalOffset: 4
                            radius: 12
                            samples: 25
                            color: "#60000000"
                            transparentBorder: true
                        }
                        
                        scale: hangupMouse.pressed ? 0.95 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: Theme.durationFast }
                        }
                        
                        Text {
                            text: "📞"
                            font.pixelSize: 28
                            anchors.centerIn: parent
                            rotation: 135
                        }
                        
                        MouseArea {
                            id: hangupMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                callService.hangupCall(callId)
                                inCallWindow.close()
                            }
                        }
                    }
                }
            }
        }
    }
    
    // DTMF Keypad Overlay
    Rectangle {
        id: dtmfPanel
        visible: false
        anchors.fill: parent
        color: "#CC000000"
        
        MouseArea {
            anchors.fill: parent
            onClicked: dtmfPanel.visible = false
        }
        
        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 400
            radius: Theme.radiusXLarge
            color: Theme.backgroundSecondary
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing6
                spacing: Theme.spacing4
                
                Text {
                    text: "Dialpad"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                GridLayout {
                    Layout.alignment: Qt.AlignHCenter
                    columns: 3
                    rowSpacing: Theme.spacing3
                    columnSpacing: Theme.spacing3
                    
                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]
                        
                        Rectangle {
                            width: 60
                            height: 60
                            radius: Theme.radiusRound
                            color: dtmfMouse.pressed ? Theme.backgroundTertiary : Theme.background
                            border.width: 2
                            border.color: Theme.border
                            
                            Text {
                                text: modelData
                                font.family: Theme.fontFamily
                                font.pixelSize: 24
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                anchors.centerIn: parent
                            }
                            
                            MouseArea {
                                id: dtmfMouse
                                anchors.fill: parent
                                onClicked: callService.sendDtmf(callId, modelData)
                            }
                        }
                    }
                }
                
                AppButton {
                    text: "Close"
                    variant: AppButton.Variant.Secondary
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: dtmfPanel.visible = false
                }
            }
        }
    }
    
    // Transfer Dialog
    Dialog {
        id: transferDialog
        title: "Transfer Call"
        modal: true
        anchors.centerIn: parent
        width: 320
        
        background: Rectangle {
            color: Theme.backgroundSecondary
            radius: Theme.radiusXLarge
            border.width: 1
            border.color: Theme.border
        }
        
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing4
            
            AppTextField {
                id: transferTargetField
                Layout.fillWidth: true
                placeholderText: "SIP URI or Number"
            }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing3
                
                AppButton {
                    text: "Cancel"
                    variant: AppButton.Variant.Secondary
                    Layout.fillWidth: true
                    onClicked: transferDialog.close()
                }
                
                AppButton {
                    text: "Transfer"
                    variant: AppButton.Variant.Primary
                    Layout.fillWidth: true
                    onClicked: {
                        if (transferTargetField.text !== "") {
                            var target = transferTargetField.text
                            if (!target.startsWith("sip:"))
                                target = "sip:" + target
                            callService.transferCall(callId, target)
                            transferTargetField.text = ""
                            transferDialog.close()
                        }
                    }
                }
            }
        }
    }
    
    // Call state monitoring
    Connections {
        target: callModel
        function onDataChanged(topLeft, bottomRight, roles) {
            for (var i = 0; i < callModel.rowCount(); i++) {
                if (callModel.data(callModel.index(i, 0), Qt.UserRole + 1) === callId) {
                    callState = callModel.data(callModel.index(i, 0), Qt.UserRole + 3)
                    hasVideo = callModel.data(callModel.index(i, 0), Qt.UserRole + 4)
                    isHeld = callModel.data(callModel.index(i, 0), Qt.UserRole + 5)
                    isInConference = callModel.data(callModel.index(i, 0), Qt.UserRole + 6)
                    lastStatus = callModel.data(callModel.index(i, 0), Qt.UserRole + 7)
                    lastReason = callModel.data(callModel.index(i, 0), Qt.UserRole + 8)

                    if (callState === "DISCONNECTED") {
                        if (lastStatus >= 400) {
                            closeTimer.start()
                        } else {
                            inCallWindow.close()
                        }
                    }
                    break
                }
            }
        }
    }
    
    // Control Button Component
    component ControlButton: Rectangle {
        id: controlBtn
        property string icon: ""
        property string label: ""
        property bool active: false
        signal clicked()
        
        width: 64
        height: 80
        radius: Theme.radiusMedium
        color: active ? Theme.primaryStart : 
               (controlBtnMouse.containsMouse ? Theme.backgroundTertiary : "transparent")
        
        opacity: enabled ? 1.0 : 0.4
        
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacing1
            
            Text {
                text: controlBtn.icon
                font.pixelSize: 28
                Layout.alignment: Qt.AlignHCenter
            }
            
            Text {
                text: controlBtn.label
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: active ? "white" : Theme.textSecondary
                Layout.alignment: Qt.AlignHCenter
            }
        }
        
        MouseArea {
            id: controlBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: if (enabled) controlBtn.clicked()
        }
    }
}
