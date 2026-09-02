import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal importRequested()
    signal dropped(url fileUrl)

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (drop.hasUrls && drop.urls.length > 0) root.dropped(drop.urls[0])
        }
    }
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 560)
        spacing: 14
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "IMAGE  →  RELIEF"
            color: "#f3f5f8"
            font.pixelSize: 30
            font.weight: Font.Light
            font.letterSpacing: 3
        }
        Label {
            Layout.fillWidth: true
            text: "Turn photographs, artwork, logos and height maps into fabrication-ready 3D reliefs."
            color: "#9299a6"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            text: "IMPORT IMAGE"
            highlighted: true
            onClicked: root.importRequested()
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "or drop PNG, JPEG, TIFF, BMP or WebP here"
            color: "#666e7a"
            font.pixelSize: 11
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 22
            spacing: 18
            Repeater {
                model: ["Portrait Relief", "Lithophane", "Coin", "Logo Emboss"]
                delegate: Label {
                    required property string modelData
                    text: modelData
                    color: "#7f8794"
                    font.pixelSize: 11
                }
            }
        }
    }
}

