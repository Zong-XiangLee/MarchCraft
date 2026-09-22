import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal requestAudioImport()
    signal requestTiming()
    signal requestMidiImport()
    signal requestMusicXmlImport()
    property var previewSegments: []
    property var mappingRows: []
    property var mappingMeasures: []
    signal revealMeasure(int index)
    function openMenu() { musicMenu.popup() }
    function refreshPreview() {
        const multiplier = movementMode.currentIndex === 0 ? 1.0
                         : movementMode.currentIndex === 1 ? 0.5
                         : movementMode.currentIndex === 2 ? 2.0 : 0.0
        previewSegments = drillProject.previewSetGeneration(
            drillProject.musicSelectionStart, drillProject.musicSelectionEnd,
            generationMode.currentIndex === 0 ? "subdivide" : "oneMove",
            Number(subdivision.currentText), multiplier)
    }

    Menu {
        id: musicMenu
        MenuItem { text: "Import MIDI…"; onTriggered: root.requestMidiImport() }
        MenuItem { text: "Import MusicXML…"; onTriggered: root.requestMusicXmlImport() }
        MenuItem { text: "Attach rehearsal audio…"; onTriggered: root.requestAudioImport() }
        MenuItem { text: "Timing and synchronization…"; onTriggered: root.requestTiming() }
        MenuSeparator {}
        MenuItem { text: "Track mixer…"; enabled: drillProject.musicTrackCount > 0; onTriggered: trackDialog.open() }
        MenuItem { text: "Map pages to music…"; enabled: drillProject.musicLoaded; onTriggered: { transport.editSet(drillProject.currentSetIndex); mappingDialog.open() } }
        MenuItem { text: "Generate pages from selection…"; enabled: drillProject.musicLoaded; onTriggered: { transport.editSet(drillProject.currentSetIndex); generationOptions.open() } }
        MenuItem { text: "Group selected measures…"; enabled: drillProject.musicLoaded; onTriggered: groupDialog.open() }
        MenuItem { text: "Music sections and parts…"; enabled: drillProject.musicLoaded; onTriggered: groupsDialog.open() }
        MenuItem { text: "Import details…"; enabled: drillProject.musicLoaded; onTriggered: diagnosticsDialog.open() }
        MenuItem { text: "Audio offset…"; onTriggered: offsetDialog.open() }
    }
    Dialog {
        id: diagnosticsDialog; title: "Music import details"; modal: true; width: 480
        anchors.centerIn: Overlay.overlay; standardButtons: Dialog.Close
        contentItem: Label { width: 440; wrapMode: Text.Wrap; text: drillProject.musicDiagnostics || "No import diagnostics." }
    }
    Dialog {
        id: generationOptions; title: "Pages from selected measures"; modal: true
        width: 440; anchors.centerIn: Overlay.overlay; standardButtons: Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Measures " + (Math.min(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1)
                         + "–" + (Math.max(drillProject.musicSelectionStart, drillProject.musicSelectionEnd) + 1) }
            ComboBox { id: generationMode; model: ["Subdivide", "One move"]; Layout.fillWidth: true }
            ComboBox { id: subdivision; model: ["8", "16", "32"]; currentIndex: 1; enabled: generationMode.currentIndex === 0; Layout.fillWidth: true }
            ComboBox { id: movementMode; model: ["Full time", "Half time", "Double time", "Hold"]; Layout.fillWidth: true }
            Button { text: "Preview pages…"; onClicked: { root.refreshPreview(); generationOptions.close(); generationDialog.open() } }
        }
    }
    Dialog {
        id: offsetDialog; title: "Rehearsal audio offset"; modal: true; width: 320
        anchors.centerIn: Overlay.overlay; standardButtons: Dialog.Close
        SpinBox {
            from: -60000; to: 60000; stepSize: 10; editable: true
            value: Math.round(drillProject.audioOffsetMs)
            onValueModified: drillProject.audioOffsetMs = value
            textFromValue: function(value) { return (value / 1000).toFixed(2) + " s" }
            valueFromText: function(text) { return Math.round(parseFloat(text) * 1000) }
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
        title: "Music sections and parts"
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
                    Button { text: "Show"; onClicked: { drillProject.setMusicSelection(modelData.startMeasure, modelData.endMeasure); root.revealMeasure(modelData.startMeasure); groupsDialog.close() } }
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
            AppButton {
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
                        AppToolButton { text: "M"; checkable: true; checked: info.muted; ToolTip.text: "Mute"; ToolTip.visible: hovered; onClicked: drillProject.setMusicTrackMuted(index, checked) }
                        AppToolButton { text: "S"; checkable: true; checked: info.solo; ToolTip.text: "Solo"; ToolTip.visible: hovered; onClicked: drillProject.setMusicTrackSolo(index, checked) }
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
            AppButton {
                text: "Apply mapping"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: if (drillProject.applySetMapping(root.mappingMeasures)) mappingDialog.close()
            }
        }
    }
}
