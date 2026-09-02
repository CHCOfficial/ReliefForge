import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick3D

ApplicationWindow {
    id: window
    width: 1500
    height: 920
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "ReliefForge — Image to Fabrication"
    color: "#17191e"

    readonly property color panelColor: "#1c1f25"
    readonly property color dividerColor: "#30343d"

    FileDialog {
        id: imageDialog
        title: "Open source image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.webp)", "All files (*)"]
        onAccepted: appController.openImage(selectedFile)
    }
    FileDialog {
        id: projectDialog
        title: "Open ReliefForge project"
        nameFilters: ["ReliefForge projects (*.reliefstudio)"]
        onAccepted: appController.openProject(selectedFile)
    }
    FileDialog {
        id: saveDialog
        property string format: "stl"
        readonly property string extension: format === "smooth-stl" ? "stl" : format
        readonly property string displayFormat: format === "smooth-stl"
            ? "Smooth High-Resolution STL"
            : extension.toUpperCase()
        fileMode: FileDialog.SaveFile
        title: "Export " + displayFormat
        defaultSuffix: extension
        nameFilters: [displayFormat + " (*." + extension + ")"]
        onAccepted: {
            if (format === "stl") appController.exportStl(selectedFile)
            else if (format === "smooth-stl") appController.exportSmoothStl(selectedFile)
            else if (format === "step") appController.exportStep(selectedFile)
            else if (format === "svg") appController.exportSvg(selectedFile)
            else if (format === "dxf") appController.exportDxf(selectedFile)
            else if (format === "reliefstudio") appController.saveProject(selectedFile)
        }
    }

    header: ToolBar {
        implicitHeight: 58
        background: Rectangle { color: "#1d2026"; border.color: window.dividerColor }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 6
            Label {
                text: "RELIEFFORGE"
                color: "#f0f2f6"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.letterSpacing: 1.5
                Layout.rightMargin: 16
            }
            ToolButton { text: "New"; onClicked: imageDialog.open() }
            ToolButton { text: "Open Image"; onClicked: imageDialog.open() }
            ToolButton { text: "Examples"; onClicked: examplesDialog.open() }
            ToolButton { text: "Open Project"; onClicked: projectDialog.open() }
            ToolButton {
                text: "Save Project"
                enabled: appController.hasImage
                onClicked: { saveDialog.format = "reliefstudio"; saveDialog.open() }
            }
            ToolSeparator {}
            ToolButton { text: "↶"; enabled: false; ToolTip.text: "Undo"; ToolTip.visible: hovered }
            ToolButton { text: "↷"; enabled: false; ToolTip.text: "Redo"; ToolTip.visible: hovered }
            Item { Layout.fillWidth: true }
            ToolButton { text: "About"; onClicked: aboutDialog.open() }
            Button {
                text: "Export"
                enabled: appController.hasImage && !appController.busy
                highlighted: true
                onClicked: exportMenu.open()
                Menu {
                    id: exportMenu
                    width: 310
                    x: parent.width - width
                    y: parent.height
                    MenuItem {
                        text: "3D Print · Smooth High-Res STL…"
                        onTriggered: { saveDialog.format = "smooth-stl"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "3D Print · Original Geometry STL…"
                        onTriggered: { saveDialog.format = "stl"; saveDialog.open() }
                    }
                    MenuSeparator {}
                    MenuItem { text: "CAD STEP…"; onTriggered: { saveDialog.format = "step"; saveDialog.open() } }
                    MenuSeparator {}
                    MenuItem { text: "SVG Contours…"; onTriggered: { saveDialog.format = "svg"; saveDialog.open() } }
                    MenuItem { text: "DXF Contours…"; onTriggered: { saveDialog.format = "dxf"; saveDialog.open() } }
                }
            }
        }
    }

    Dialog {
        id: examplesDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 980)
        height: Math.min(window.height - 64, 680)
        modal: true
        title: "Example library"
        standardButtons: Dialog.Close
        background: Rectangle { color: "#1c2028"; radius: 12; border.color: "#39414f" }
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                Layout.fillWidth: true
                text: "12 built-in height maps · from simple shapes to intricate surfaces"
                color: "#e0e7f2"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: "Choose a starting point, then make it your own. Loading an example resets the relief settings and view. Save your current project first if you want to keep it."
                color: "#a1aabd"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            ScrollView {
                id: exampleScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                GridLayout {
                    width: exampleScroll.availableWidth
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: appController.exampleCatalog
                        Button {
                            id: exampleCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 166
                            padding: 10
                            hoverEnabled: true
                            Accessible.name: modelData.name
                            Accessible.description: modelData.description
                            onClicked: {
                                if (appController.loadExample(modelData.id)) {
                                    camera.position = Qt.vector3d(0, -155, 135)
                                    camera.eulerRotation.x = 43
                                    reliefModel.eulerRotation = Qt.vector3d(0, 0, 0)
                                    examplesDialog.close()
                                }
                            }
                            background: Rectangle {
                                radius: 8
                                color: exampleCard.hovered ? "#303a4d" : "#252b36"
                                border.width: exampleCard.activeFocus || appController.activeExampleId === exampleCard.modelData.id ? 2 : 1
                                border.color: exampleCard.activeFocus || appController.activeExampleId === exampleCard.modelData.id ? "#8baeff" : "#3b4351"
                            }
                            contentItem: ColumnLayout {
                                spacing: 4
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 68
                                    color: "#11161e"
                                    radius: 5
                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 3
                                        source: exampleCard.modelData.imageUrl
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: exampleCard.modelData.level.toUpperCase()
                                    color: "#9db8ef"
                                    font.pixelSize: 9
                                    font.letterSpacing: 1
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: exampleCard.modelData.name
                                    color: "#f0f3f9"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: exampleCard.modelData.description
                                    color: "#b4bed0"
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: aboutDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(window.width - 60, 700)
        height: Math.min(window.height - 60, 620)
        modal: true
        title: "ReliefForge " + Qt.application.version
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "Copyright © 2026 CHCOfficial · Free software under GNU GPLv3"
                wrapMode: Text.WordWrap
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                text: "You may redistribute and modify this program under GPLv3. It comes without warranty. Donations are optional; all features are free."
                wrapMode: Text.WordWrap
            }
            TabBar {
                id: legalTabs
                Layout.fillWidth: true
                TabButton { text: "Licence" }
                TabButton { text: "Credits" }
                TabButton { text: "Third parties" }
                TabButton { text: "Source code" }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: legalTabs.currentIndex === 0 ? TextEdit.PlainText : TextEdit.MarkdownText
                    onLinkActivated: function(link) {
                        if (link === "LICENSE" || link === "COPYING") legalTabs.currentIndex = 0
                        else if (link.startsWith("https://")) Qt.openUrlExternally(link)
                    }
                    text: appController.legalDocument(
                        ["COPYING", "CREDITS.md", "THIRD_PARTY_NOTICES.md", "CORRESPONDING_SOURCE.md"][legalTabs.currentIndex])
                    font.pixelSize: 12
                }
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        ScrollView {
            SplitView.preferredWidth: 292
            SplitView.minimumWidth: 250
            SplitView.maximumWidth: 390
            background: Rectangle { color: window.panelColor }
            contentWidth: availableWidth
            ColumnLayout {
                x: 10
                width: parent.width - 20
                spacing: 0

                InspectorSection {
                    title: "SOURCE IMAGE"
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 122
                        color: "#14171b"
                        radius: 7
                        Image {
                            anchors.fill: parent
                            anchors.margins: 6
                            source: appController.sourceUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: !appController.hasImage
                            text: "No image loaded"
                            color: "#666d78"
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: appController.sourceName
                        visible: appController.hasImage
                        color: "#d3dbea"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    Label { text: appController.sourceInfo; color: "#858c98"; font.pixelSize: 11 }
                    Button { text: "Replace image"; Layout.fillWidth: true; onClicked: imageDialog.open() }
                }

                InspectorSection {
                    title: "IMAGE PROCESSING"
                    PropertySlider {
                        label: "Contrast"; from: 0.2; to: 2.5; value: appController.contrast
                        onEdited: value => appController.contrast = value
                    }
                    PropertySlider {
                        label: "Gamma"; from: 0.2; to: 3; value: appController.gamma
                        onEdited: value => appController.gamma = value
                    }
                    PropertySlider {
                        label: "Denoise / blur"; from: 0; to: 8; value: appController.blurRadius
                        suffix: "px"; onEdited: value => appController.blurRadius = value
                    }
                    Switch {
                        text: "Invert height"
                        checked: appController.inverted
                        onToggled: appController.inverted = checked
                    }
                }

                InspectorSection {
                    title: "HEIGHT CURVE"
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 108
                        color: "#15181d"
                        border.color: "#333844"
                        radius: 6
                        Canvas {
                            id: heightCurveGraph
                            anchors.fill: parent
                            anchors.margins: 10
                            readonly property var samples: appController.heightCurveSamples
                            onSamplesChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                            Component.onCompleted: requestPaint()
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset(); ctx.strokeStyle = "#2c323b"; ctx.lineWidth = 1
                                for (let i = 1; i < 4; ++i) {
                                    ctx.beginPath(); ctx.moveTo(width*i/4, 0); ctx.lineTo(width*i/4, height); ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(0, height*i/4); ctx.lineTo(width, height*i/4); ctx.stroke()
                                }
                                ctx.strokeStyle = "#6ca8ff"; ctx.lineWidth = 2
                                ctx.beginPath()
                                for (let i = 0; i < samples.length; ++i) {
                                    const x = samples[i].x * width
                                    const y = (1 - samples[i].y) * height
                                    if (i === 0) ctx.moveTo(x, y)
                                    else ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                            }
                        }
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Linear", "Soft", "Strong", "Bas Relief", "High Relief", "Custom"]
                        currentIndex: appController.curvePreset
                        onActivated: index => appController.curvePreset = index
                    }
                }
            }
        }

        Item {
            id: viewportPane
            SplitView.fillWidth: true
            SplitView.minimumWidth: 500

            EmptyState {
                anchors.fill: parent
                visible: !appController.hasImage
                onImportRequested: imageDialog.open()
                onDropped: fileUrl => appController.openImage(fileUrl)
            }

            View3D {
                id: viewport
                anchors.fill: parent
                visible: appController.hasImage
                camera: camera
                environment: SceneEnvironment {
                    clearColor: "#111419"
                    backgroundMode: SceneEnvironment.Color
                    antialiasingMode: SceneEnvironment.MSAA
                    antialiasingQuality: SceneEnvironment.High
                    tonemapMode: SceneEnvironment.TonemapModeLinear
                }
                PerspectiveCamera {
                    id: camera
                    position: Qt.vector3d(0, -155, 135)
                    eulerRotation.x: 43
                    clipFar: 2000
                }
                DirectionalLight {
                    eulerRotation: Qt.vector3d(-48, -28, -30)
                    brightness: 1.25
                    castsShadow: true
                    shadowFactor: 35
                }
                DirectionalLight {
                    eulerRotation: Qt.vector3d(-15, 145, 10)
                    brightness: 0.35
                    color: "#b8ccff"
                }
                Model {
                    id: reliefModel
                    geometry: appController.geometry
                    materials: PrincipledMaterial {
                        readonly property var preset: appController.previewMaterialProperties
                        baseColor: preset.baseColor
                        roughness: preset.roughness
                        metalness: preset.metalness
                        specularAmount: preset.specularAmount
                        clearcoatAmount: preset.clearcoatAmount
                        clearcoatRoughnessAmount: 0.12
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    property point previous
                    onPressed: mouse => previous = Qt.point(mouse.x, mouse.y)
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        reliefModel.eulerRotation.y += (mouse.x - previous.x) * 0.35
                        reliefModel.eulerRotation.x += (mouse.y - previous.y) * 0.35
                        previous = Qt.point(mouse.x, mouse.y)
                    }
                    onWheel: wheel => {
                        camera.position.z = Math.max(55, Math.min(400, camera.position.z - wheel.angleDelta.y * 0.12))
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                enabled: appController.hasImage
                onDropped: drop => {
                    if (drop.hasUrls && drop.urls.length > 0) {
                        appController.openImage(drop.urls[0])
                        drop.acceptProposedAction()
                    }
                }
            }

            Rectangle {
                visible: appController.hasImage
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 12
                width: viewToolbarRow.implicitWidth + 8
                height: viewToolbarRow.implicitHeight + 8
                color: "#cc22262d"
                radius: 7
                Row {
                    id: viewToolbarRow
                    anchors.centerIn: parent
                    spacing: 4
                    ToolButton {
                        text: "Perspective"
                        onClicked: { camera.position = Qt.vector3d(0, -155, 135); camera.eulerRotation.x = 43; reliefModel.eulerRotation = Qt.vector3d(0, 0, 0) }
                    }
                    ToolButton {
                        text: "Front"
                        onClicked: { camera.position = Qt.vector3d(0, -190, 15); camera.eulerRotation.x = 90; reliefModel.eulerRotation = Qt.vector3d(0, 0, 0) }
                    }
                    ToolButton {
                        text: "Top"
                        onClicked: { camera.position = Qt.vector3d(0, 0, 190); camera.eulerRotation.x = 0; reliefModel.eulerRotation = Qt.vector3d(0, 0, 0) }
                    }
                    ToolButton {
                        text: "Reset"
                        onClicked: { camera.position = Qt.vector3d(0, -155, 135); camera.eulerRotation.x = 43; reliefModel.eulerRotation = Qt.vector3d(0, 0, 0) }
                    }
                }
            }
            BusyIndicator {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                running: appController.busy
                visible: running
            }
        }

        ScrollView {
            SplitView.preferredWidth: 294
            SplitView.minimumWidth: 250
            SplitView.maximumWidth: 390
            background: Rectangle { color: window.panelColor }
            contentWidth: availableWidth
            ColumnLayout {
                x: 10
                width: parent.width - 20
                spacing: 0
                InspectorSection {
                    title: "DIMENSIONS"
                    PropertySlider {
                        label: "Width"; suffix: "mm"; from: 10; to: 500; value: appController.widthMm
                        onEdited: value => appController.widthMm = value
                    }
                    RowLayout {
                        Label { text: "Height"; color: "#c7ccd6"; font.pixelSize: 12; Layout.fillWidth: true }
                        Label { text: appController.heightMm.toFixed(2) + " mm"; color: "#f0f2f6" }
                    }
                    PropertySlider {
                        label: "Relief depth"; suffix: "mm"; from: 0.1; to: 25; value: appController.reliefDepthMm
                        onEdited: value => appController.reliefDepthMm = value
                    }
                    PropertySlider {
                        label: "Base thickness"; suffix: "mm"; from: 0.2; to: 20; value: appController.baseThicknessMm
                        onEdited: value => appController.baseThicknessMm = value
                    }
                }
                InspectorSection {
                    title: "RELIEF STYLE"
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Standard Relief", "Inverted Relief", "Bas Relief", "High Relief", "Lithophane", "Emboss", "Deboss", "Engraving", "Contour Relief", "Edge Relief"]
                        currentIndex: appController.reliefStyle
                        onActivated: index => appController.reliefStyle = index
                    }
                }
                InspectorSection {
                    title: "GEOMETRY"
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["Draft", "Medium", "High", "Ultra", "Source Resolution", "Custom"]
                        currentIndex: appController.resolutionPreset
                        onActivated: index => appController.resolutionPreset = index
                    }
                    RowLayout {
                        Label { text: "Vertices"; color: "#8f96a1"; Layout.fillWidth: true }
                        Label { text: appController.vertexCountText; color: "#e3e6eb" }
                    }
                    RowLayout {
                        Label { text: "Triangles"; color: "#8f96a1"; Layout.fillWidth: true }
                        Label { text: appController.triangleCountText; color: "#e3e6eb" }
                    }
                    RowLayout {
                        Label { text: "Mesh status"; color: "#8f96a1"; Layout.fillWidth: true }
                        Label {
                            text: !appController.hasImage || appController.busy ? "—" : appController.meshValid ? "VALID" : "CHECK"
                            color: appController.meshValid ? "#66d59a" : "#e6bd79"
                            font.weight: Font.DemiBold
                        }
                    }
                }
                InspectorSection {
                    title: "PREVIEW MATERIAL"
                    ComboBox {
                        Layout.fillWidth: true
                        model: appController.previewMaterialNames
                        currentIndex: appController.previewMaterial
                        onActivated: index => appController.previewMaterial = index
                    }
                    Switch {
                        text: "Smooth print preview"
                        checked: appController.geometry.smoothShading
                        onCheckedChanged: appController.geometry.smoothShading = checked
                    }
                    Label {
                        Layout.fillWidth: true
                        text: appController.geometry.smoothShading
                            ? "Actual high-res geometry · matches Smooth High-Res STL"
                            : "Underlying mesh · matches Original Geometry STL"
                        color: "#747c89"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 30
        color: "#191c21"
        border.color: window.dividerColor
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: appController.statusText
            color: "#848b97"
            font.pixelSize: 11
        }
    }

    Connections {
        target: appController
        function onImageSubmitted() {
            creatorToast.close()
            creatorToast.open()
            creatorToastTimer.restart()
        }
    }

    Timer {
        id: creatorToastTimer
        interval: 12000
        onTriggered: creatorToast.close()
    }

    Popup {
        id: creatorToast
        parent: Overlay.overlay
        x: Math.max(18, window.width - width - 22)
        y: Math.max(18, window.height - height - 54)
        width: Math.min(410, window.width - 36)
        padding: 16
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 130; easing.type: Easing.InCubic }
        }

        background: Rectangle {
            color: "#f7232730"
            border.color: "#4b79bdf7"
            border.width: 1
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 9

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "RELIEFFORGE IS FREE"
                    color: "#f3f5f8"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.8
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "×"
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: creatorToast.close()
                }
            }

            Label {
                Layout.fillWidth: true
                text: "If ReliefForge saves you time, you can support the creator or follow the work."
                color: "#c6cbd5"
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }

            Button {
                text: "☕  Buy Me a Coffee"
                highlighted: true
                Layout.fillWidth: true
                onClicked: Qt.openUrlExternally("https://buymeacoffee.com/CHCOfficial")
            }

            Label {
                Layout.fillWidth: true
                textFormat: Text.RichText
                text: "<a href='https://github.com/CHCOfficialGraphics'>Code</a>  ·  "
                    + "<a href='https://www.deviantart.com/chcofficialAudio'>Graphics</a>  ·  "
                    + "<a href='https://suno.com/@artfulexpchc'>Audio</a>"
                color: "#8ebcff"
                linkColor: "#8ebcff"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }
    }

    Popup {
        id: errorPopup
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 80, 540)
        modal: true
        visible: appController.errorText.length > 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: appController.clearError()
        background: Rectangle { color: "#252930"; border.color: "#49505d"; radius: 9 }
        contentItem: ColumnLayout {
            spacing: 14
            Label { text: "ReliefForge could not complete that action"; color: "#f2f3f6"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Label { text: appController.errorText; color: "#b9bec8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Button { text: "Dismiss"; Layout.alignment: Qt.AlignRight; onClicked: errorPopup.close() }
        }
    }
}
