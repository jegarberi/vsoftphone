import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: control
    
    property string name: ""
    property string imageSource: ""
    property bool showStatus: false
    property string status: "offline" // online, away, busy, offline
    property int size: 40
    
    width: size
    height: size
    
    Rectangle {
        id: avatarBackground
        anchors.fill: parent
        radius: Theme.radiusRound
        color: Theme.backgroundTertiary
        
        // Image or Initials
        Image {
            visible: control.imageSource !== ""
            anchors.fill: parent
            source: control.imageSource
            fillMode: Image.PreserveAspectCrop
            layer.enabled: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: control.size
                    height: control.size
                    radius: Theme.radiusRound
                }
            }
        }
        
        Text {
            visible: control.imageSource === "" && control.name !== ""
            text: {
                var parts = control.name.split(" ")
                if (parts.length >= 2) {
                    return parts[0].charAt(0).toUpperCase() + parts[1].charAt(0).toUpperCase()
                } else if (parts.length === 1) {
                    return parts[0].charAt(0).toUpperCase()
                }
                return "?"
            }
            font.family: Theme.fontFamily
            font.pixelSize: control.size * 0.4
            font.weight: Font.Medium
            color: Theme.textPrimary
            anchors.centerIn: parent
        }
    }
    
    // Status Indicator
    Rectangle {
        visible: control.showStatus
        width: control.size * 0.3
        height: control.size * 0.3
        radius: Theme.radiusRound
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: -2
        anchors.bottomMargin: -2
        
        color: {
            switch(control.status) {
                case "online": return Theme.accentGreen
                case "away": return Theme.warningOrange
                case "busy": return Theme.dangerRed
                default: return Theme.gray500
            }
        }
        
        border.width: 2
        border.color: Theme.background
    }
}
