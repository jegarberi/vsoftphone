import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    function dial(number) {
        display.text = number
        display.forceActiveFocus()
    }

    // Keys typed anywhere in the window end up here (see main.qml)
    function type(character) {
        display.text += character
    }
    function backspace() {
        display.text = display.text.slice(0, -1)
    }

    // A number, user@host or an IP address; the service works out which
    function call(video) {
        var target = display.text.trim()
        if (target !== "")
            callService.makeCall(accountModel.defaultAccountId, target, video)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        TextField {
            id: display
            objectName: "dialDisplay"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 22
            placeholderText: "Number, user@host or IP"
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            onAccepted: root.call(false)
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: 4
            columnSpacing: 4

            Repeater {
                model: [
                    {digit: "1", letters: ""},     {digit: "2", letters: "ABC"}, {digit: "3", letters: "DEF"},
                    {digit: "4", letters: "GHI"},  {digit: "5", letters: "JKL"}, {digit: "6", letters: "MNO"},
                    {digit: "7", letters: "PQRS"}, {digit: "8", letters: "TUV"}, {digit: "9", letters: "WXYZ"},
                    {digit: "*", letters: ""},     {digit: "0", letters: "+"},   {digit: "#", letters: ""}
                ]

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 44
                    flat: true
                    // No keyboard focus, so Enter after a click still calls
                    focusPolicy: Qt.NoFocus
                    onClicked: root.type(modelData.digit)

                    // The digit and its letters, centred on the button both ways
                    contentItem: Item {
                        Column {
                            anchors.centerIn: parent
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.digit
                                font.pixelSize: 22
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                // No empty line under 1, * and #, which would
                                // push the digit off centre
                                visible: modelData.letters !== ""
                                text: modelData.letters
                                font.pixelSize: 9
                                color: Theme.muted
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                flat: true
                text: "⌫"
                font.pixelSize: 18
                enabled: display.text !== ""
                onClicked: display.text = display.text.slice(0, -1)
                onPressAndHold: display.text = ""
            }

            PillButton {
                Layout.fillWidth: true
                kind: "accent"
                text: "Call"
                enabled: display.text.trim() !== ""
                onClicked: root.call(false)
            }

            Button {
                flat: true
                text: "Video"
                visible: callService.videoAvailable
                enabled: display.text.trim() !== ""
                onClicked: root.call(true)
            }
        }
    }
}
