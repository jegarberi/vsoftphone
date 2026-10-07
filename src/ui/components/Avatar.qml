import QtQuick
import QtQuick.Controls

// A circle with the first letter of a name or SIP address
Rectangle {
    id: control

    property string name: ""
    property int size: 40

    width: size
    height: size
    radius: size / 2
    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)

    function initial(text) {
        var clean = text.replace(/^.*<|>.*$/g, "").replace(/^sips?:/i, "").replace(/"/g, "").trim()
        return clean.length > 0 ? clean.charAt(0).toUpperCase() : "?"
    }

    Label {
        anchors.centerIn: parent
        text: control.initial(control.name)
        color: Theme.accent
        font.pixelSize: control.size * 0.42
        font.bold: true
    }
}
