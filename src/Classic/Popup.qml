import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    padding: 12

    background: Bevel { kind: "panel" }

    T.Overlay.modal: Rectangle { color: "#40000000" }
    T.Overlay.modeless: Rectangle { color: "#20000000" }
}
