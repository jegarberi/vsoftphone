import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// Always in view while there is something to scroll, with a raised thumb
T.ScrollBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    visible: control.policy !== T.ScrollBar.AlwaysOff && control.size < 1.0
    minimumSize: orientation === Qt.Horizontal ? height / width : width / height

    contentItem: Bevel {
        implicitWidth: 14
        implicitHeight: 14
        flatRadius: 7
        hovered: control.hovered
        active: Theme.look === "aqua" || (control.pressed && Theme.look !== "win95" && Theme.look !== "win98")
    }

    background: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        color: Theme.look === "" ? "transparent" : Qt.lighter(Theme.background, 1.08)
    }
}
