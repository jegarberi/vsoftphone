import QtQuick
import QtQuick.Templates as T

// A list row: the selected one is filled with the highlight colour
T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 8
    spacing: 8

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? "white" : Theme.look === "" ? Theme.foreground : "black"
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: 28
        color: control.highlighted ? Theme.accent
             : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                       control.down ? 0.3 : control.hovered ? 0.12 : 0)
    }
}
