import QtQuick
import QtQuick.Templates as T
import "qrc:/ui/components"

// A sunken field with a raised arrow button at its right end
T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 8
    rightPadding: 24

    delegate: ItemDelegate {
        width: ListView.view.width
        text: control.textRole
            ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
            : modelData
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: Bevel {
        x: control.width - width - 2
        y: 2
        width: 20
        height: control.height - 4
        down: control.down
        hovered: control.hovered
        active: Theme.look === "aqua"
        Text {
            anchors.centerIn: parent
            text: "▼"
            font.pixelSize: 8
            color: Theme.look === "aqua" ? "white" : parent.ink
        }
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse
        font: control.font
        color: face.ink
        selectionColor: Theme.accent
        selectedTextColor: "white"
        verticalAlignment: Text.AlignVCenter
    }

    background: Bevel {
        id: face
        kind: "field"
        implicitWidth: 130
        implicitHeight: 28
        active: control.visualFocus || (control.editable && control.activeFocus)
        hovered: control.hovered
    }

    popup: T.Popup {
        y: control.height
        width: control.width
        height: Math.min(contentItem.implicitHeight + 2, control.Window.height - topMargin - bottomMargin)
        topMargin: 6
        bottomMargin: 6
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            T.ScrollBar.vertical: ScrollBar { }
        }

        background: Rectangle {
            color: Theme.look === "" ? Theme.surface : "white"
            border.color: Theme.look === "" ? Theme.muted : "black"
        }
    }
}
