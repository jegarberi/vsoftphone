import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: root
    
    function dial(number) {
        display.text = number
    }
    
    Rectangle {
        anchors.fill: parent
        color: Theme.background
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacing8
            spacing: Theme.spacing6
            
            // Number Display
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                radius: Theme.radiusLarge
                color: Theme.backgroundSecondary
                border.width: 2
                border.color: display.activeFocus ? Theme.primaryStart : Theme.border
                
                Behavior on border.color {
                    ColorAnimation { duration: Theme.durationFast }
                }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing4
                    spacing: Theme.spacing2
                    
                    TextField {
                        id: display
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        font.family: Theme.fontFamily
                        font.pixelSize: 32
                        font.weight: Font.Medium
                        placeholderText: "Enter number..."
                        horizontalAlignment: TextInput.AlignRight
                        color: Theme.textPrimary
                        background: Item {}
                        selectByMouse: true
                    }
                    
                    AppButton {
                        visible: display.text.length > 0
                        text: "⌫"
                        variant: AppButton.Variant.Ghost
                        onClicked: display.text = display.text.slice(0, -1)
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
            
            // Dialpad Grid
            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                columns: 3
                rowSpacing: Theme.spacing4
                columnSpacing: Theme.spacing4
                
                Repeater {
                    model: [
                        {num: "1", letters: ""},
                        {num: "2", letters: "ABC"},
                        {num: "3", letters: "DEF"},
                        {num: "4", letters: "GHI"},
                        {num: "5", letters: "JKL"},
                        {num: "6", letters: "MNO"},
                        {num: "7", letters: "PQRS"},
                        {num: "8", letters: "TUV"},
                        {num: "9", letters: "WXYZ"},
                        {num: "*", letters: ""},
                        {num: "0", letters: "+"},
                        {num: "#", letters: ""}
                    ]
                    
                    delegate: Rectangle {
                        width: 72
                        height: 72
                        radius: Theme.radiusRound
                        color: dialpadButtonMouse.pressed ? Theme.backgroundTertiary : 
                               dialpadButtonMouse.containsMouse ? Theme.backgroundSecondary : 
                               Theme.backgroundSecondary
                        border.width: 2
                        border.color: Theme.border
                        
                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast }
                        }
                        
                        // Press animation
                        scale: dialpadButtonMouse.pressed ? 0.95 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: Theme.durationFast }
                        }
                        
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 0
                            
                            Text {
                                text: modelData.num
                                font.family: Theme.fontFamily
                                font.pixelSize: 28
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                Layout.alignment: Qt.AlignHCenter
                            }
                            
                            Text {
                                visible: modelData.letters !== ""
                                text: modelData.letters
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        
                        MouseArea {
                            id: dialpadButtonMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: display.text += modelData.num
                        }
                    }
                }
            }
            
            Item { Layout.preferredHeight: Theme.spacing6 }
            
            // Call Actions
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.spacing6
                
                // Video Call Button
                Rectangle {
                    width: 56
                    height: 56
                    radius: Theme.radiusRound
                    color: videoCallMouse.pressed ? Qt.darker(Theme.backgroundSecondary, 1.1) : 
                           videoCallMouse.containsMouse ? Theme.backgroundTertiary : 
                           Theme.backgroundSecondary
                    border.width: 2
                    border.color: Theme.border
                    
                    Text {
                        text: "📹"
                        font.pixelSize: 24
                        anchors.centerIn: parent
                    }
                    
                    MouseArea {
                        id: videoCallMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: display.text !== ""
                        onClicked: {
                            if (display.text !== "") {
                                callService.makeCall(accountModel.defaultAccountId, "sip:" + display.text, true)
                            }
                        }
                    }
                }
                
                // Voice Call Button (Primary)
                Rectangle {
                    width: 72
                    height: 72
                    radius: Theme.radiusRound
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: callButtonMouse.pressed ? Qt.darker(Theme.accentGreen, 1.1) : Theme.accentGreen }
                        GradientStop { position: 1.0; color: callButtonMouse.pressed ? Qt.darker(Theme.accentGreen, 1.2) : Qt.darker(Theme.accentGreen, 1.1) }
                    }
                    
                    // Subtle shadow
                    layer.enabled: true
                    layer.effect: DropShadow {
                        horizontalOffset: 0
                        verticalOffset: 4
                        radius: 12
                        samples: 25
                        color: "#40000000"
                        transparentBorder: true
                    }
                    
                    scale: callButtonMouse.pressed ? 0.95 : 1.0
                    Behavior on scale {
                        NumberAnimation { duration: Theme.durationFast }
                    }
                    
                    Text {
                        text: "📞"
                        font.pixelSize: 32
                        anchors.centerIn: parent
                    }
                    
                    MouseArea {
                        id: callButtonMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: display.text !== ""
                        onClicked: {
                            if (display.text !== "") {
                                callService.makeCall(accountModel.defaultAccountId, "sip:" + display.text)
                            }
                        }
                    }
                }
                
                // Account Selector
                Rectangle {
                    width: 56
                    height: 56
                    radius: Theme.radiusRound
                    color: accountSelectorMouse.pressed ? Qt.darker(Theme.backgroundSecondary, 1.1) : 
                           accountSelectorMouse.containsMouse ? Theme.backgroundTertiary : 
                           Theme.backgroundSecondary
                    border.width: 2
                    border.color: Theme.border
                    
                    Text {
                        text: "👤"
                        font.pixelSize: 24
                        anchors.centerIn: parent
                    }
                    
                    MouseArea {
                        id: accountSelectorMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: accountModel.selectNextAccount()
                    }

                    ToolTip.visible: accountSelectorMouse.containsMouse
                    ToolTip.text: accountModel.defaultAccountId || "No active account"
                }
            }
            
            Item { Layout.preferredHeight: Theme.spacing4 }
        }
    }
}
