import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MicrosipClone 1.0

Item {
    id: root

    HistoryModel {
        id: historyModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: historyModel

            delegate: ItemDelegate {
                width: ListView.view.width
                text: model.remoteParty
                
                contentItem: RowLayout {
                    Image {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        // source: "icon_path" // Todo add icons based on direction
                        // Placeholder
                        Rectangle {
                            anchors.fill: parent
                            color: model.direction === "Missed" ? "red" : (model.direction === "Incoming" ? "blue" : "green")
                        }
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            text: model.remoteParty
                            font.bold: true
                        }
                        Label {
                            text: model.timestamp
                            font.pixelSize: 10
                            color: "gray"
                        }
                    }
                    
                    Label {
                        text: model.duration > 0 ? new Date(model.duration * 1000).toISOString().substr(11, 8) : ""
                    }
                    
                    Button {
                        text: "Call"
                        onClicked: console.log("Call " + model.remoteParty)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Clear History"
                onClicked: historyModel.clearHistory()
            }
        }
    }
}
