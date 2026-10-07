import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import MicrosipClone 1.0

Item {
    id: root

    signal dialRequested(string number)

    function formatDuration(seconds) {
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    HistoryModel {
        id: historyModel
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: historyModel

            // Click a row to put the number on the dial pad
            delegate: ItemDelegate {
                width: ListView.view.width
                onClicked: root.dialRequested(model.remoteParty)

                contentItem: RowLayout {
                    spacing: 10

                    Label {
                        text: model.direction === "Outgoing" ? "↗"
                            : model.direction === "Missed" ? "✕" : "↙"
                        color: model.direction === "Missed" ? Theme.danger : Theme.muted
                        font.pixelSize: 16
                        Layout.preferredWidth: 16
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: model.remoteParty
                            color: model.direction === "Missed" ? Theme.danger : Theme.foreground
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: model.direction + "  ·  " + model.timestamp
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        text: model.duration > 0 ? root.formatDuration(model.duration) : ""
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: "No calls yet"
                color: Theme.muted
            }
        }

        PillButton {
            Layout.fillWidth: true
            Layout.margins: 8
            text: "Clear history"
            enabled: list.count > 0
            onClicked: historyModel.clearHistory()
        }
    }
}
