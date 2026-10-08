import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// Horizontal only: a sunken groove with a raised thumb
T.Slider {
    id: control

    implicitWidth: 160
    implicitHeight: 28
    padding: 4

    handle: Bevel {
        readonly property bool round: Theme.look === "aqua" || Theme.look === ""
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: round ? 16 : 11
        implicitHeight: round ? 16 : 20
        flatRadius: 8
        tint: Theme.look === "" ? Theme.accent : "transparent"
        hovered: control.hovered
        active: control.pressed
    }

    background: Bevel {
        kind: "field"
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 5
    }
}
