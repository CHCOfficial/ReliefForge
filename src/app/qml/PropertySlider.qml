import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property string label
    property string suffix: ""
    property real from: 0
    property real to: 1
    property real value: 0
    property int decimals: 2
    signal edited(real value)
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: root.label
            color: "#c7ccd6"
            font.pixelSize: 12
            Layout.fillWidth: true
        }
        TextField {
            text: Number(root.value).toFixed(root.decimals)
            color: "#f0f2f6"
            horizontalAlignment: Text.AlignRight
            selectByMouse: true
            validator: DoubleValidator { bottom: root.from; top: root.to }
            implicitWidth: 74
            implicitHeight: 28
            background: Rectangle {
                color: "#20232a"
                border.color: parent.activeFocus ? "#6da9ff" : "#343944"
                radius: 5
            }
            onEditingFinished: {
                const parsed = Number(text)
                if (!isNaN(parsed)) root.edited(Math.max(root.from, Math.min(root.to, parsed)))
            }
        }
        Label { text: root.suffix; color: "#7e8592"; font.pixelSize: 11 }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        value: root.value
        onMoved: root.edited(value)
    }
}

