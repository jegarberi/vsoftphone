import QtQuick
import QtQuick.Controls

Button {
    id: control
    
    enum Variant {
        Primary,
        Secondary,
        Danger,
        Ghost
    }
    
    property int variant: AppButton.Variant.Primary
    property bool isLoading: false
    
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             Theme.minTouchTarget)
    
    padding: Theme.spacing4
    leftPadding: Theme.spacing6
    rightPadding: Theme.spacing6
    
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    font.weight: Font.Medium
    
    contentItem: Row {
        spacing: Theme.spacing2
        
        BusyIndicator {
            visible: control.isLoading
            running: control.isLoading
            width: Theme.fontSizeBody
            height: Theme.fontSizeBody
            anchors.verticalCenter: parent.verticalCenter
        }
        
        Text {
            text: control.text
            font: control.font
            opacity: enabled ? 1.0 : 0.5
            color: {
                if (control.variant === AppButton.Variant.Primary) {
                    return "white"
                } else if (control.variant === AppButton.Variant.Danger) {
                    return "white"
                } else if (control.variant === AppButton.Variant.Ghost) {
                    return Theme.textPrimary
                } else {
                    return Theme.textPrimary
                }
            }
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    
    background: Rectangle {
        implicitWidth: 100
        implicitHeight: Theme.minTouchTarget
        radius: Theme.radiusMedium
        
        color: {
            if (!control.enabled) {
                return Theme.gray300
            }
            
            if (control.variant === AppButton.Variant.Primary) {
                return control.down ? Qt.darker(Theme.primaryEnd, 1.1) : 
                       control.hovered ? Qt.lighter(Theme.primaryStart, 1.1) : 
                       Theme.primaryStart
            } else if (control.variant === AppButton.Variant.Danger) {
                return control.down ? Qt.darker(Theme.dangerRed, 1.1) : 
                       control.hovered ? Qt.lighter(Theme.dangerRed, 1.1) : 
                       Theme.dangerRed
            } else if (control.variant === AppButton.Variant.Ghost) {
                return control.down ? Theme.backgroundTertiary : 
                       control.hovered ? Theme.backgroundSecondary : 
                       "transparent"
            } else { // Secondary
                return control.down ? Theme.backgroundTertiary : 
                       control.hovered ? Theme.backgroundSecondary : 
                       Theme.backgroundSecondary
            }
        }
        
        border.color: {
            if (control.variant === AppButton.Variant.Ghost) {
                return "transparent"
            }
            return "transparent"
        }
        
        border.width: 0
        
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
        
        // Press animation
        scale: control.down ? 0.95 : 1.0
        Behavior on scale {
            NumberAnimation { duration: Theme.durationFast }
        }
    }
}
