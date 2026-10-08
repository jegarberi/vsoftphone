import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import "qrc:/ui/components"

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)
    padding: 6
    leftPadding: 8
    rightPadding: 8

    color: face.ink
    selectionColor: Theme.accent
    selectedTextColor: "white"
    placeholderTextColor: Theme.look === "" ? Theme.muted : "#808080"
    verticalAlignment: TextInput.AlignVCenter

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText
                 && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Bevel {
        id: face
        kind: "field"
        implicitWidth: 160
        implicitHeight: 28
        active: control.activeFocus
        hovered: control.hovered
    }
}
