import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 6
    horizontalPadding: 8

    contentItem: Text {
        text: control.text
        font: control.font
        color: face.ink
        opacity: control.enabled ? 1 : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Bevel {
        id: face
        implicitWidth: 28
        implicitHeight: 28
        down: control.down || control.checked || control.highlighted
        hovered: control.hovered
        active: control.highlighted || control.visualFocus || control.checked
    }
}
