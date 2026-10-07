import QtQuick
import QtQuick.Controls

// A button with a plain filled background, drawn the same by every renderer.
// The style's own buttons take their colour from the window and can vanish
// into it.
Button {
    id: control

    // "tonal" (quiet), "accent", "success" or "danger"
    property string kind: "tonal"

    readonly property color fill: kind === "accent" ? Theme.accent
        : kind === "success" ? Theme.success
        : kind === "danger" ? Theme.danger
        : Qt.rgba(Theme.foreground.r, Theme.foreground.g, Theme.foreground.b, 0.09)
    // Dark themes have light accents, which need dark text on them
    readonly property color ink: kind === "tonal" ? Theme.foreground
        : kind === "accent" && Theme.dark ? "#101214"
        : "white"

    implicitHeight: 36
    // The style leaves a gap above and below its own background; ours fills
    topInset: 0
    bottomInset: 0
    leftPadding: 14
    rightPadding: 14
    font.capitalization: Font.MixedCase

    background: Rectangle {
        radius: height / 2
        color: control.fill
        opacity: !control.enabled ? 0.4 : control.down ? 0.7 : control.hovered ? 0.88 : 1
    }

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.ink
        opacity: control.enabled ? 1 : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
