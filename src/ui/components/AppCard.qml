import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Rectangle {
    id: control
    
    property int elevation: 1
    property bool hoverable: false
    
    color: Theme.backgroundSecondary
    radius: Theme.radiusLarge
    
    border.width: 1
    border.color: Theme.borderLight
    
    layer.enabled: elevation > 0
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: elevation
        radius: elevation * 4
        samples: elevation * 8 + 1
        color: Theme.isDarkMode ? "#40000000" : "#20000000"
        transparentBorder: true
    }
    
    // Hover effect
    states: State {
        name: "hovered"
        when: hoverable && mouseArea.containsMouse
        PropertyChanges {
            target: control
            elevation: elevation + 1
        }
    }
    
    transitions: Transition {
        NumberAnimation {
            properties: "elevation"
            duration: Theme.durationNormal
            easing.type: Theme.easingStandard
        }
    }
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: hoverable
        propagateComposedEvents: true
        onPressed: mouse.accepted = false
    }
}
