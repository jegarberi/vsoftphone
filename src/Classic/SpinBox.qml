import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 8
    rightPadding: 24

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: face.ink
        selectionColor: Theme.accent
        selectedTextColor: "white"
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Bevel {
        x: control.width - width - 2
        y: 2
        implicitWidth: 20
        height: (control.height - 4) / 2
        down: control.up.pressed
        hovered: control.up.hovered
        Text { anchors.centerIn: parent; text: "▲"; font.pixelSize: 6; color: parent.ink }
    }

    down.indicator: Bevel {
        x: control.width - width - 2
        y: control.height / 2
        implicitWidth: 20
        height: (control.height - 4) / 2
        down: control.down.pressed
        hovered: control.down.hovered
        Text { anchors.centerIn: parent; text: "▼"; font.pixelSize: 6; color: parent.ink }
    }

    background: Bevel {
        id: face
        kind: "field"
        implicitWidth: 90
        implicitHeight: 28
        active: control.activeFocus
    }
}
