import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// A framed panel with the desktop's title bar across its top
T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth, implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))
    padding: 12

    background: Bevel { kind: "panel" }

    header: Item {
        visible: control.title !== ""
        implicitHeight: 30
        Bevel {
            id: bar
            kind: "title"
            anchors.fill: parent
            anchors.margins: 3
            anchors.bottomMargin: 0
        }
        Text {
            anchors.fill: bar
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            text: control.title
            font.family: control.font.family
            font.bold: true
            color: bar.ink
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle { color: "#40000000" }
    T.Overlay.modeless: Rectangle { color: "#20000000" }
}
