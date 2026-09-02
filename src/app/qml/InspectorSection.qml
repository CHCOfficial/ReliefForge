import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property alias title: header.text
    property bool expanded: true
    default property alias content: body.data
    Layout.fillWidth: true
    spacing: 0

    ToolButton {
        id: sectionButton
        Layout.fillWidth: true
        implicitHeight: 38
        contentItem: RowLayout {
            Label {
                text: root.expanded ? "⌄" : "›"
                color: "#818997"
                font.pixelSize: 15
            }
            Label {
                id: header
                color: "#eef0f4"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }
        background: Rectangle { color: sectionButton.hovered ? "#242830" : "transparent"; radius: 6 }
        onClicked: root.expanded = !root.expanded
    }
    ColumnLayout {
        id: body
        visible: root.expanded
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.bottomMargin: 14
        spacing: 12
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: "#2d313a"
    }
}
