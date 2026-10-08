import QtQuick

// The face of a control as the theme's desktop drew it: a 3D bevel for
// Windows 3.11, 95 and 98, gradients for Windows XP, 7 and Mac OS X, and a
// plain flat shape for the modern themes.
Item {
    id: bevel

    // "button" (raised), "field" (sunken, white), "panel" (a dialog's frame)
    // or "title" (a title bar)
    property string kind: "button"
    property bool down: false
    property bool hovered: false
    // The default button, a checked tab, a field with the keyboard focus
    property bool active: false
    // A colour of its own (the green answer button); transparent for none
    property color tint: "transparent"
    // Corner radius in the modern themes
    property real flatRadius: 4

    readonly property string look: Theme.look
    readonly property bool tinted: tint.a > 0
    // The colour for text drawn on this face
    readonly property color ink: tinted ? "white"
        : look === "" || kind === "panel" ? Theme.foreground
        : kind === "title" ? (look === "win7" || look === "aqua" ? "black" : "white")
        : "black"

    opacity: enabled ? 1 : 0.5

    // c: gradient colours at 0, p[0], p[1] and 1; edges: the 3D lines as
    // [outer top-left, inner top-left, inner bottom-right, outer bottom-right]
    function flat(color, border, radius) {
        return {c: [color, color, color, color], p: [0.5, 0.5], border: border || "transparent",
                bw: border ? 1 : 0, radius: radius || 0, horizontal: false, edges: null}
    }
    function shaded(colors, border, radius, stops) {
        return {c: colors, p: stops || [0.49, 0.5], border: border, bw: 1, radius: radius,
                horizontal: false, edges: null}
    }
    function raised(face, edges, pressed) {
        var s = flat(face)
        s.edges = pressed ? [edges[3], edges[2], edges[1], edges[0]] : edges
        return s
    }
    function fade(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    readonly property var spec: {
        var gel = tinted ? [Qt.lighter(tint, 1.5), Qt.lighter(tint, 1.15), tint, Qt.lighter(tint, 1.25)] : null
        var s

        if (look === "") {
            if (kind === "title")
                return flat(Theme.surface)
            if (kind === "panel")
                return flat(Theme.surface, Theme.muted, 6)
            if (kind === "field")
                return flat(Theme.surface, active ? Theme.accent : Theme.muted, flatRadius)
            var base = tinted ? tint : fade(Theme.foreground, 0.09)
            return flat(down ? Qt.darker(base, 1.25) : hovered ? Qt.lighter(base, 1.08) : base, null, flatRadius)
        }

        if (look === "win3") {
            if (kind === "title")
                return flat("#000080")
            if (kind === "field")
                return flat("white", "black")
            if (kind === "panel")
                return flat(Theme.background, "black")
            s = raised(tinted ? tint : "#C0C0C0", ["white", "white", "#808080", "#808080"], down)
            s.border = "black"
            s.bw = active ? 2 : 1
            s.radius = 2
            return s
        }

        if (look === "win95" || look === "win98") {
            var face = look === "win95" ? "#C0C0C0" : "#D4D0C8"
            var dark = look === "win95" ? "black" : "#404040"
            if (kind === "title") {
                s = flat("#000080")
                if (look === "win98") {
                    s.c = ["#0A246A", "#3A6EA5", "#6A9FD0", "#A6CAF0"]
                    s.p = [0.33, 0.66]
                    s.horizontal = true
                }
                return s
            }
            if (kind === "field")
                return raised("white", ["#808080", dark, face, "white"], false)
            if (kind === "panel")
                return raised(face, [face, "white", "#808080", dark], false)
            s = raised(tinted ? tint : face, ["white", face, "#808080", dark], down)
            if (active) { // the default button's black outline
                s.border = "black"
                s.bw = 1
            }
            return s
        }

        if (look === "xp") {
            if (kind === "title")
                return shaded(["#0997FF", "#0053EE", "#0058EE", "#0831D9"], "transparent", 0, [0.12, 0.85])
            if (kind === "field")
                return flat("white", active ? "#316AC5" : "#7F9DB9")
            if (kind === "panel") {
                s = flat(Theme.background, "#0054E3")
                s.bw = 3
                s.radius = 4
                return s
            }
            if (tinted)
                return shaded(down ? [gel[2], gel[2], gel[1], gel[1]] : gel, Qt.darker(tint, 1.6), 3)
            return shaded(down ? ["#D6D0C5", "#DDD9CE", "#E3E0D6", "#ECEBE5"]
                               : ["#FFFFFF", "#F4F3EE", "#ECEBE4", "#D6D0C5"],
                          hovered || active ? "#E5971D" : "#003C74", 3, [0.4, 0.85])
        }

        if (look === "win7") {
            if (kind === "title")
                return shaded(["#DCEAF8", "#C9DDF2", "#BDD3EC", "#B4CCE8"], "#8DA2B8", 0)
            if (kind === "field")
                return flat("white", active || hovered ? "#5794BF" : "#ABADB3", 1)
            if (kind === "panel")
                return flat(Theme.background, "#6D8AAE", 5)
            if (tinted)
                return shaded(down ? [gel[2], gel[2], gel[1], gel[1]] : gel, Qt.darker(tint, 1.6), 3)
            if (down)
                return shaded(["#E5F4FC", "#C4E5F6", "#98D1EF", "#68B3DB"], "#2C628B", 3)
            if (hovered || active)
                return shaded(["#EAF6FD", "#D9F0FC", "#BEE6FD", "#A7D9F5"], "#3C7FB1", 3)
            return shaded(["#F2F2F2", "#EBEBEB", "#DDDDDD", "#CFCFCF"], "#707070", 3)
        }

        // Mac OS X
        if (kind === "title")
            return shaded(["#EDEDED", "#DCDCDC", "#D4D4D4", "#C2C2C2"], "#8E8E8E", 0)
        if (kind === "field") {
            s = flat("white", active ? "#6FA8F0" : "#9D9D9D", 2)
            s.bw = active ? 2 : 1
            return s
        }
        if (kind === "panel")
            return flat(Theme.background, "#A0A0A0", 6)
        var round = Math.min(height / 2, 13)
        if (tinted)
            return shaded(down ? [gel[2], gel[2], gel[1], gel[1]] : gel, Qt.darker(tint, 1.6), round)
        if (down || active)
            return shaded(down ? ["#9CC6F5", "#5BA1EE", "#2F86EA", "#7DBDF7"]
                               : ["#C6E0FB", "#7DB5F3", "#4A98EF", "#A5D5FB"], "#2A5DB0", round)
        return shaded(["#FFFFFF", "#F0F0F0", "#E2E2E2", "#F6F6F6"], "#8E8E8E", round)
    }

    Rectangle {
        anchors.fill: parent
        radius: bevel.spec.radius
        border.color: bevel.spec.border
        border.width: bevel.spec.bw
        gradient: Gradient {
            orientation: bevel.spec.horizontal ? Gradient.Horizontal : Gradient.Vertical
            GradientStop { position: 0; color: bevel.spec.c[0] }
            GradientStop { position: bevel.spec.p[0]; color: bevel.spec.c[1] }
            GradientStop { position: bevel.spec.p[1]; color: bevel.spec.c[2] }
            GradientStop { position: 1; color: bevel.spec.c[3] }
        }
    }

    // One ring of the 3D edge: lit from the top left
    component Ring: Item {
        property color light
        property color shade
        Rectangle { width: parent.width; height: 1; color: parent.light }
        Rectangle { width: 1; height: parent.height; color: parent.light }
        Rectangle { y: parent.height - 1; width: parent.width; height: 1; color: parent.shade }
        Rectangle { x: parent.width - 1; width: 1; height: parent.height; color: parent.shade }
    }
    Ring {
        visible: bevel.spec.edges !== null
        anchors.fill: parent
        anchors.margins: bevel.spec.bw
        light: bevel.spec.edges ? bevel.spec.edges[0] : "transparent"
        shade: bevel.spec.edges ? bevel.spec.edges[3] : "transparent"
    }
    Ring {
        visible: bevel.spec.edges !== null
        anchors.fill: parent
        anchors.margins: bevel.spec.bw + 1
        light: bevel.spec.edges ? bevel.spec.edges[1] : "transparent"
        shade: bevel.spec.edges ? bevel.spec.edges[2] : "transparent"
    }
}
