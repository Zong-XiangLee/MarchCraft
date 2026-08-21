import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal requestMidiImport()
    signal requestMusicXmlImport()
    property int selectionAnchor: Math.max(0, drillProject.musicSelectionStart)
    property var previewSegments: []
    property var mappingRows: []
    property var mappingMeasures: []
    property int setRangeRevision: 0
    property int musicRevision: 0
    readonly property bool compact: height < 210
    function revealSetRange() {
        for (let i = 0; i < drillProject.musicMeasureCount; ++i) {
            if (drillProject.musicMeasureInfo(i).inSetRange) {
                measureList.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }
    Connections {
        target: drillProject
        function onMusicChanged() { root.musicRevision++ }
        function onSetRangeChanged() {
            root.setRangeRevision++
            Qt.callLater(root.revealSetRange)
        }
    }

    function refreshPreview() {
        const multiplier = movementMode.currentIndex === 0 ? 1.0
                         : movementMode.currentIndex === 1 ? 0.5
                         : movementMode.currentIndex === 2 ? 2.0 : 0.0
        previewSegments = drillProject.previewSetGeneration(
            drillProject.musicSelectionStart, drillProject.musicSelectionEnd,
            generationMode.currentIndex === 0 ? "subdivide" : "oneMove",
            Number(subdivision.currentText), multiplier)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: drillProject.midiImporting ? "Reading MIDI…"
                    : drillProject.musicLoaded
                      ? drillProject.musicMeasureCount + " measures · " + drillProject.musicTrackCount
                        + " tracks · " + Math.floor(drillProject.musicDurationMs / 60000) + ":"
                        + String(Math.floor((drillProject.musicDurationMs % 60000) / 1000)).padStart(2, "0")
                      : "Attach MIDI or MusicXML to build the musical timeline"
                font.bold: true
            }
            Label { text: transport.audioStatus; color: transport.synthAvailable || drillProject.playbackSource !== "midi" ? "#8fa197" : "#f3c969" }
            Item { Layout.fillWidth: true }
            Button { text: "MIDI…"; onClicked: root.requestMidiImport() }
            Button { text: "MusicXML…"; onClicked: root.requestMusicXmlImport() }
            Button { text: "Tracks…"; enabled: drillProject.musicTrackCount > 0; onClicked: trackDialog.open() }
            Button { text: "Map sets…"; enabled: drillProject.musicLoaded; onClicked: mappingDialog.open() }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.compact && drillProject.musicDiagnostics.length > 0
            text: drillProject.musicDiagnostics
            color: "#d6b66a"; elide: Text.ElideRight; font.pixelSize: 10
        }

        RowLayout {
            Layout.fillWidth: true
            visible: drillProject.musicLoaded
            Label { text: "Measures " + (Math.min(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1)
                          + "–" + (Math.max(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1); color: "#b8c8bf" }
            Button { text: "Group selection…"; onClicked: groupDialog.open() }
            Button { text: "Groups (" + drillProject.musicSections.length + ")"; onClicked: groupsDialog.open() }
            ComboBox { id: generationMode; model: ["Subdivide", "One move"]; Layout.preferredWidth: 116 }
            ComboBox { id: subdivision; model: ["8", "16", "32"]; currentIndex: 1; enabled: generationMode.currentIndex === 0; Layout.preferredWidth: 70 }
            ComboBox { id: movementMode; model: ["Full time", "Half time", "Double time", "Hold"]; Layout.preferredWidth: 118 }
            Button {
                text: "Preview sets…"; highlighted: true
                onClicked: { root.refreshPreview(); generationDialog.open() }
            }
            Label { text: "Audio offset"; color: "#8fa197" }
            SpinBox {
                from: -60000; to: 60000; stepSize: 10; editable: true
                value: Math.round(drillProject.audioOffsetMs)
                onValueModified: drillProject.audioOffsetMs = value
                textFromValue: function(value) { return (value / 1000).toFixed(2) + " s" }
                valueFromText: function(text) { return Math.round(parseFloat(text) * 1000) }
                Layout.preferredWidth: 92
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: !root.compact && drillProject.waveformPeakCount > 0 ? 24 : 0
            visible: height > 0; color: "#0c1217"; radius: 3
            Canvas {
                id: waveform
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d"); ctx.reset(); ctx.strokeStyle = "#5ee0a0"; ctx.lineWidth = 1
                    const count = drillProject.waveformPeakCount
                    if (count < 1) return
                    for (let x = 0; x < width; ++x) {
                        const peak = drillProject.waveformPeak(Math.min(count - 1, Math.floor(x * count / width)))
                        ctx.beginPath(); ctx.moveTo(x, height / 2 - peak * height / 2); ctx.lineTo(x, height / 2 + peak * height / 2); ctx.stroke()
                    }
                }
                Connections { target: drillProject; function onWaveformChanged() { waveform.requestPaint() } }
            }
        }

        ListView {
            id: measureList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 54
            orientation: ListView.Horizontal; clip: true; spacing: 2
            model: drillProject.musicMeasureCount
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    const delta = Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
                                ? event.angleDelta.x : event.angleDelta.y
                    measureList.contentX = Math.max(0, Math.min(
                        Math.max(0, measureList.contentWidth - measureList.width),
                        measureList.contentX - delta))
                    event.accepted = true
                }
            }
            delegate: Rectangle {
                id: measureCard
                required property int index
                property var info: { root.setRangeRevision; root.musicRevision; return drillProject.musicMeasureInfo(index) }
                width: Math.max(92, Math.min(132, measureList.height * 0.65)); height: measureList.height - 10; radius: 4
                color: info.selected ? "#285f49" : info.inSetRange ? "#203c51" : "#172127"
                border.width: info.selected ? 3 : info.setIndex >= 0 ? 2 : 1
                border.color: info.selected ? "#6ee7b7" : info.setIndex >= 0 ? "#f3c969" : "#30414b"
                Rectangle {
                    anchors.fill: parent; anchors.margins: measureCard.border.width
                    radius: 3; color: info.sections && info.sections.length ? info.sections[0].color : "transparent"
                    opacity: info.selected ? 0.10 : 0.20
                }
                Row {
                    z: 2; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    height: 5
                    Repeater {
                        model: info.sections || []
                        Rectangle { required property var modelData; width: measureCard.width / Math.max(1, (info.sections || []).length); height: 5; color: modelData.color }
                    }
                }
                Column {
                    z: 3; anchors.fill: parent; anchors.margins: 7; anchors.topMargin: 9; spacing: 2
                    Row {
                        width: parent.width
                        Label { text: info.number || ""; font.bold: true; color: "#e7f5ed" }
                        Label { text: "  " + (info.numerator || 4) + "/" + (info.denominator || 4); color: "#8fa197"; font.pixelSize: 10 }
                    }
                    Rectangle { width: parent.width; height: Math.max(3, (info.density || 0) * 25); color: "#5ee0a0"; opacity: 0.65; radius: 2 }
                    Label { visible: measureCard.height >= 66; text: (info.counts || 0) + " ct · " + Math.round(info.tempo || 0); color: "#a9bbb1"; font.pixelSize: 9 }
                    Label { visible: info.setIndex >= 0; text: "SET " + (info.setIndex + 1); color: "#f3c969"; font.bold: true; font.pixelSize: 9 }
                    Label {
                        visible: measureCard.height >= 82 && info.sections && info.sections.length > 0
                        text: info.sections && info.sections.length ? info.sections[0].name : ""
                        color: "#f8fafc"; font.bold: true; font.pixelSize: 9
                        width: parent.width; elide: Text.ElideRight
                    }
                }
                Rectangle { visible: info.inSetRange; z: 4; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 5; color: "#60a5fa" }
                Label { visible: info.selected; z: 5; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7; text: "SELECTED"; color: "#a7f3d0"; font.bold: true; font.pixelSize: 8 }
                Rectangle {
                    visible: transport.currentTick >= info.startTick && transport.currentTick < info.endTick
                    x: Math.max(1, Math.min(parent.width - 2,
                        (transport.currentTick - info.startTick) / Math.max(1, info.endTick - info.startTick) * parent.width))
                    width: 2; height: parent.height; color: "#ffffff"; opacity: 0.9
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: function(mouse) {
                        if (!(mouse.modifiers & Qt.ShiftModifier)) root.selectionAnchor = index
                        drillProject.setMusicSelection(root.selectionAnchor, index)
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed) return
                        const p = mapToItem(measureList.contentItem, mouse.x, mouse.y)
                        const target = Math.max(0, Math.min(drillProject.musicMeasureCount - 1, Math.floor(p.x / (parent.width + 2))))
                        drillProject.setMusicSelection(root.selectionAnchor, target)
                    }
                    onDoubleClicked: if (info.setIndex >= 0) drillProject.currentSetIndex = info.setIndex
                }
            }
        }

    }

    Dialog {
        id: groupDialog
        title: "Group selected measures"
        modal: true; width: 410; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        onOpened: {
            groupName.text = ""
            groupType.currentIndex = 0
            groupColor.currentIndex = drillProject.musicSections.length % groupColor.count
        }
        contentItem: GridLayout {
            columns: 2; rowSpacing: 10; columnSpacing: 12
            Label { text: "Measures" }
            Label { text: (Math.min(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1) + "–" + (Math.max(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1); font.bold: true }
            Label { text: "Name" }
            TextField { id: groupName; Layout.fillWidth: true; placeholderText: "Opening, Ballad, Part 1…" }
            Label { text: "Group as" }
            ComboBox { id: groupType; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Movement",value:"movement"},{text:"Part",value:"part"}] }
            Label { text: "Color" }
            ComboBox {
                id: groupColor; Layout.fillWidth: true; textRole: "text"; valueRole: "value"
                model: [{text:"Violet",value:"#8b5cf6"},{text:"Blue",value:"#3b82f6"},{text:"Teal",value:"#14b8a6"},{text:"Amber",value:"#f59e0b"},{text:"Rose",value:"#f43f5e"}]
            }
            RowLayout {
                Layout.columnSpan: 2; Layout.fillWidth: true; Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; onClicked: groupDialog.close() }
                Button {
                    text: "Create group"; highlighted: true; enabled: groupName.text.trim().length > 0
                    onClicked: {
                        drillProject.addMusicSection(groupName.text, groupType.currentValue, groupColor.currentValue,
                                                     drillProject.musicSelectionStart, drillProject.musicSelectionEnd)
                        groupDialog.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: groupsDialog
        title: "Show movements and parts"
        modal: true; width: 520; height: 430; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close
        contentItem: ListView {
            clip: true; spacing: 5; model: drillProject.musicSections
            delegate: Frame {
                required property var modelData
                width: ListView.view.width; height: 58; padding: 8
                background: Rectangle { color: "#121b21"; border.color: modelData.color; radius: 6 }
                RowLayout {
                    anchors.fill: parent
                    Rectangle { width: 8; Layout.fillHeight: true; radius: 3; color: modelData.color }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Label { text: modelData.name; font.bold: true }
                        Label { text: modelData.type.toUpperCase() + " · measures " + modelData.startNumber + "–" + modelData.endNumber; color: "#8fa197"; font.pixelSize: 10 }
                    }
                    Button { text: "Show"; onClicked: { drillProject.setMusicSelection(modelData.startMeasure, modelData.endMeasure); measureList.positionViewAtIndex(modelData.startMeasure, ListView.Beginning); groupsDialog.close() } }
                    ToolButton {
                        id: removeGroupButton
                        text: "×"
                        ToolTip.text: "Remove group"
                        ToolTip.visible: removeGroupButton.hovered
                        onClicked: drillProject.removeMusicSection(modelData.id)
                    }
                }
            }
        }
    }

    Dialog {
        id: generationDialog
        title: "Create drill sets from selected music"
        modal: true; width: 560; height: 430; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent
            Label { text: root.previewSegments.length + " transition" + (root.previewSegments.length === 1 ? "" : "s") + " will be created"; font.bold: true }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: root.previewSegments
                delegate: RowLayout {
                    required property int index
                    width: ListView.view.width
                    property var segment: root.previewSegments[index]
                    Label { Layout.fillWidth: true; text: "Measures " + segment.startMeasure + "–" + segment.endMeasure + "   ·   "
                        + segment.counts + " counts   ·   " + segment.steps + " steps   ·   "
                        + (segment.durationMs / 1000).toFixed(2) + " s" }
                    ComboBox {
                        model: ["Full time", "Half time", "Double time", "Hold"]
                        currentIndex: segment.stepMultiplier === 0.5 ? 1 : segment.stepMultiplier === 2 ? 2 : segment.stepMultiplier === 0 ? 3 : 0
                        onActivated: {
                            segment.stepMultiplier = currentIndex === 0 ? 1.0 : currentIndex === 1 ? 0.5 : currentIndex === 2 ? 2.0 : 0.0
                            segment.steps = segment.counts * segment.stepMultiplier
                            root.previewSegments = root.previewSegments.slice()
                        }
                    }
                }
            }
            Button {
                text: "Create sets"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    drillProject.commitSetGenerationPlan(root.previewSegments)
                    generationDialog.close()
                }
            }
        }
    }

    Dialog {
        id: trackDialog
        title: "MIDI track mixer"
        modal: true; width: 620; height: 560; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close
        ScrollView {
            anchors.fill: parent
            Column {
                width: parent.width
                Repeater {
                    model: drillProject.musicTrackCount
                    RowLayout {
                        required property int index
                        property var info: drillProject.musicTrackInfo(index)
                        width: parent.width
                        Label { text: info.name + "  (" + info.noteCount + " notes)"; Layout.fillWidth: true; elide: Text.ElideRight }
                        ToolButton { text: "M"; checkable: true; checked: info.muted; ToolTip.text: "Mute"; ToolTip.visible: hovered; onClicked: drillProject.setMusicTrackMuted(index, checked) }
                        ToolButton { text: "S"; checkable: true; checked: info.solo; ToolTip.text: "Solo"; ToolTip.visible: hovered; onClicked: drillProject.setMusicTrackSolo(index, checked) }
                        Slider { from: 0; to: 1; value: info.volume; Layout.preferredWidth: 150; onMoved: drillProject.setMusicTrackVolume(index,value) }
                    }
                }
            }
        }
    }

    Dialog {
        id: mappingDialog
        title: "Review set-to-measure mapping"
        modal: true; width: 500; height: 520; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Cancel
        onOpened: {
            root.mappingRows = drillProject.previewSetMapping()
            root.mappingMeasures = []
            for (let i = 0; i < root.mappingRows.length; ++i) root.mappingMeasures.push(root.mappingRows[i].measureIndex)
        }
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Formations are preserved. Choose chronological measure starts, then apply once."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                Column {
                    width: parent.width
                    Repeater {
                        model: root.mappingRows.length
                        RowLayout {
                            required property int index
                            width: parent.width
                            Label { text: root.mappingRows[index].setName; Layout.fillWidth: true }
                            SpinBox {
                                from: 1; to: Math.max(1, drillProject.musicMeasureCount)
                                value: root.mappingMeasures[index] + 1; editable: true
                                onValueModified: root.mappingMeasures[index] = value - 1
                            }
                        }
                    }
                }
            }
            Button {
                text: "Apply mapping"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: if (drillProject.applySetMapping(root.mappingMeasures)) mappingDialog.close()
            }
        }
    }
}
