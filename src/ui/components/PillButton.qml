import QtQuick
import QtQuick.Controls

// A button with a filled background of its own, drawn the same by every
// renderer: a pill in the modern themes, the desktop's button in the classic
// ones. The Material style's own buttons take their colour from the window
// and can vanish into it.
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

    background: Bevel {
        id: face
        flatRadius: height / 2
        // Quiet buttons are the desktop's plain grey ones
        tint: Theme.look !== "" && control.kind === "tonal" ? "transparent" : control.fill
        down: control.down
        hovered: control.hovered
        active: Theme.look !== "" && control.kind === "accent"
    }

    contentItem: Label {
        text: control.text
        font: control.font
        color: Theme.look === "" ? control.ink : face.ink
        opacity: control.enabled ? 1 : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
