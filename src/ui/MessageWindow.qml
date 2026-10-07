import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    onClosing: destroy()
    width: 400
    height: 500
    visible: true
    title: "Chat with " + remoteParty

    property string remoteParty: ""

    MessageView {
        anchors.fill: parent
        remoteParty: root.remoteParty
    }
}
