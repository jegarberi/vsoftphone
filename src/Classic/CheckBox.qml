import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// A sunken box with a tick
T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    padding: 6
    spacing: 6

    indicator: Bevel {
        kind: "field"
        implicitWidth: 15
        implicitHeight: 15
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        active: control.visualFocus
        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✔"
            font.pixelSize: 11
            color: Theme.look === "" ? Theme.accent : Theme.look === "xp" ? "#21A121" : "black"
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: Theme.foreground
        opacity: control.enabled ? 1 : 0.5
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
}
