import QtQuick
import QtQuick.Controls

TextField {
    id: control
    
    enum ValidationState {
        None,
        Valid,
        Invalid
    }
    
    property int validationState: AppTextField.ValidationState.None
    property string helperText: ""
    property string leadingIcon: ""
    
    implicitWidth: 200
    implicitHeight: Theme.minTouchTarget
    
    padding: Theme.spacing4
    leftPadding: leadingIcon !== "" ? Theme.spacing12 : Theme.spacing4
    
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    color: Theme.textPrimary
    
    placeholderTextColor: Theme.textSecondary
    
    background: Rectangle {
        radius: Theme.radiusMedium
        color: Theme.backgroundSecondary
        border.width: 2
        border.color: {
            if (control.validationState === AppTextField.ValidationState.Invalid) {
                return Theme.dangerRed
            } else if (control.validationState === AppTextField.ValidationState.Valid) {
                return Theme.accentGreen
            } else if (control.activeFocus) {
                return Theme.primaryStart
            } else {
                return Theme.border
            }
        }
        
        Behavior on border.color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }
    
    // Leading Icon
    Item {
        visible: control.leadingIcon !== ""
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing3
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.fontSizeTitle
        height: Theme.fontSizeTitle
        
        Text {
            text: control.leadingIcon
            font.family: "Material Icons"
            font.pixelSize: Theme.fontSizeTitle
            color: Theme.textSecondary
            anchors.centerIn: parent
        }
    }
    
    // Helper Text
    Text {
        visible: control.helperText !== ""
        text: control.helperText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCaption
        color: {
            if (control.validationState === AppTextField.ValidationState.Invalid) {
                return Theme.dangerRed
            } else if (control.validationState === AppTextField.ValidationState.Valid) {
                return Theme.accentGreen
            } else {
                return Theme.textSecondary
            }
        }
        anchors.top: control.bottom
        anchors.topMargin: Theme.spacing1
        anchors.left: control.left
        anchors.leftMargin: Theme.spacing4
    }
}
