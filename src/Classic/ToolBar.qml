import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// Drawn as the desktop's title bar
T.ToolBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    verticalPadding: 4

    background: Bevel {
        kind: "title"
        implicitHeight: 40
    }
}
