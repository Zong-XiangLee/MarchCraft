import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal requestAudioImport()
    signal requestTiming()
    signal requestMidiImport()
    signal requestMusicXmlImport()
    signal revealTick(real tick)
    property var previewSegments: []
    property var mappingRows: []
    property var mappingMeasures: []
    property int markerEditIndex: -1
    property var markerEditData: ({})
    signal revealMeasure(int index)
    function openMenu() { musicMenu.popup() }
    function openSetPlan() { setPlanOptions.open() }
    function openSetPlanReview() { setPlanReview.open() }
    function openMarkers() { markerLibrary.open() }
    function openTracks() { trackDialog.open() }
    function openSections() { groupsDialog.open() }
    function openGroupSelection() { groupDialog.open() }
    function openDiagnostics() { diagnosticsDialog.open() }
    function openOffset() { offsetDialog.open() }
    function openGeneration() { transport.editSet(drillProject.currentSetIndex); generationOptions.open() }
    function openMapping() { transport.editSet(drillProject.currentSetIndex); mappingDialog.open() }
    function preferredCountValues() {
        const pieces = preferredCounts.text.split(/[, ]+/)
        const result = []
        for (let i = 0; i < pieces.length; ++i) {
            const value = Number(pieces[i])
            if (value > 0 && result.indexOf(value) < 0)
                result.push(value)
        }
        return result
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

    Menu {
        id: musicMenu
        MenuItem { text: "Import MIDI…"; onTriggered: root.requestMidiImport() }
        MenuItem { text: "Import MusicXML…"; onTriggered: root.requestMusicXmlImport() }
        MenuItem { text: "Attach rehearsal audio…"; onTriggered: root.requestAudioImport() }
        MenuItem { text: "Timing and synchronization…"; onTriggered: root.requestTiming() }
        MenuSeparator {}
        MenuItem { text: "Analyze music for set planning…"; enabled: drillProject.musicLoaded || drillProject.timelineMarkerCount > 0; onTriggered: root.openSetPlan() }
        MenuItem { text: "Timeline markers…"; onTriggered: root.openMarkers() }
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

    Dialog {
        id: setPlanOptions
        title: "Analyze music for set planning"
        modal: true
        width: 500
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 10
            Label {
                text: "MarchCraft will suggest editable set boundaries from phrasing, texture changes, tempo and meter events, score markers, existing sets, and strong MIDI attacks. Nothing changes until Apply."
                wrapMode: Text.Wrap
                color: MarchCraftTheme.textSecondary
                Layout.fillWidth: true
            }
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Label { text: "Density" }
                ComboBox {
                    id: planDensity
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "Sparse · major phrases", value: "sparse" },
                        { text: "Balanced · rehearsal ready", value: "balanced" },
                        { text: "Detailed · more options", value: "detailed" }
                    ]
                    currentIndex: 1
                }
                Label { text: "Prioritize" }
                ComboBox {
                    id: planPriority
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "Balanced evidence", value: "balanced" },
                        { text: "Musical impacts", value: "impacts" },
                        { text: "Phrase structure", value: "phrases" },
                        { text: "Regular count structure", value: "regular" },
                        { text: "Authored markers", value: "markers" }
                    ]
                }
                Label { text: "Preferred counts" }
                TextField {
                    id: preferredCounts
                    text: "8, 12, 16, 24, 32"
                    placeholderText: "8, 16, 24, 32"
                    Layout.fillWidth: true
                }
                Label { text: "Set budget" }
                SpinBox {
                    id: planMaximumSets
                    from: 16
                    to: 512
                    value: 112
                    editable: true
                    Layout.fillWidth: true
                    ToolTip.text: "Maximum total pages after applying the plan"
                    ToolTip.visible: hovered
                }
            }
            Label {
                text: "The 112-page default follows a full-size reference production (1,178 counts, about 10.6 counts per page). Regular eight-count grid points are guides unless you explicitly prioritize them."
                wrapMode: Text.Wrap
                color: MarchCraftTheme.textMuted
                font.pixelSize: 10
                Layout.fillWidth: true
            }
            Label {
                visible: !drillProject.musicLoaded
                text: "No score is loaded; analysis will use your authored timeline markers."
                color: MarchCraftTheme.warning
                font.pixelSize: 10
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton {
                    text: "Analyze and preview"
                    highlighted: true
                    enabled: drillProject.musicLoaded || drillProject.timelineMarkerCount > 0
                    onClicked: {
                        if (drillProject.analyzeMusicForSetPlan(planDensity.currentValue,
                                                                planPriority.currentValue,
                                                                root.preferredCountValues(),
                                                                planMaximumSets.value)) {
                            setPlanOptions.close()
                            setPlanReview.open()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: setPlanReview
        title: "Review set-planning suggestions"
        modal: true
        width: 760
        height: 600
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        onClosed: if (!drillProject.setPlanPreviewActive) candidateList.positionViewAtBeginning()

        contentItem: ColumnLayout {
            spacing: 7
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: drillProject.setPlanAcceptedNewSetCount + " new sets selected · "
                        + drillProject.setPlanCandidateCount + " evidence items · translucent markers preview on the timeline"
                    color: MarchCraftTheme.textSecondary
                    Layout.fillWidth: true
                }
                AppButton {
                    text: "+ At playhead"
                    onClicked: drillProject.addSetPlanCandidate(transport.tickAtShowMs(transport.currentMs))
                }
                AppButton { text: "Options…"; onClicked: { setPlanReview.close(); setPlanOptions.open() } }
            }
            Rectangle {
                Layout.fillWidth: true
                height: 30
                color: MarchCraftTheme.panelHeader
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Label { text: "USE"; font.bold: true; Layout.preferredWidth: 42 }
                    Label { text: "SUGGESTION / EVIDENCE"; font.bold: true; Layout.fillWidth: true }
                    Label { text: "LOCATION"; font.bold: true; Layout.preferredWidth: 120 }
                    Label { text: "COUNTS"; font.bold: true; Layout.preferredWidth: 64 }
                    Item { Layout.preferredWidth: 34 }
                }
            }
            ListView {
                id: candidateList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                model: drillProject.setPlanCandidates
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 64
                    radius: 4
                    color: modelData.accepted ? "#27233a" : MarchCraftTheme.surface
                    border.color: modelData.accepted ? (modelData.color || "#d990ea") : MarchCraftTheme.divider
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        CheckBox {
                            checked: modelData.accepted
                            enabled: modelData.action !== "review"
                            Layout.preferredWidth: 42
                            onClicked: drillProject.setSetPlanCandidateAccepted(index, checked)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: modelData.title + (modelData.action === "align" ? " · align existing set"
                                    : modelData.action === "review" ? " · review only" : " · add set")
                                    + " · " + Math.round(Number(modelData.confidence || 0) * 100) + "%"
                                color: MarchCraftTheme.textPrimary
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.reason
                                color: MarchCraftTheme.textMuted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            text: "m" + (modelData.measure || "—") + " b" + (modelData.beat || "—")
                                + "\n" + (Number(modelData.timeMs || 0) / 1000).toFixed(2) + " s"
                            color: MarchCraftTheme.textSecondary
                            font.pixelSize: 10
                            Layout.preferredWidth: 120
                        }
                        SpinBox {
                            from: 1
                            to: 2048
                            editable: true
                            enabled: modelData.action !== "review" && modelData.countsFromPrevious > 0
                            value: Math.max(1, modelData.countsFromPrevious || 1)
                            Layout.preferredWidth: 78
                            onValueModified: drillProject.setSetPlanCandidateCounts(index, value)
                        }
                        AppToolButton {
                            text: "×"
                            ToolTip.text: "Remove suggestion"
                            ToolTip.visible: hovered
                            onClicked: drillProject.removeSetPlanCandidate(index)
                        }
                    }
                }
            }
            Label {
                text: "Drag a purple preview marker on the timeline to retime it. Apply is a single undoable transaction; Cancel discards the preview."
                wrapMode: Text.Wrap
                color: MarchCraftTheme.textMuted
                font.pixelSize: 10
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    text: "Cancel preview"
                    onClicked: { drillProject.cancelSetPlanPreview(); setPlanReview.close() }
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: "Apply selected suggestions"
                    highlighted: true
                    enabled: drillProject.setPlanPreviewActive
                    onClicked: if (drillProject.applySetPlan()) setPlanReview.close()
                }
            }
        }
    }

    Dialog {
        id: markerLibrary
        title: "Timeline markers"
        modal: true
        width: 680
        height: 520
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            spacing: 7
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Named rehearsal, impact, phrase, tempo, and annotation markers"
                    color: MarchCraftTheme.textSecondary
                    Layout.fillWidth: true
                }
                AppButton {
                    text: "+ At playhead"
                    onClicked: drillProject.addTimelineMarker(
                        transport.tickAtShowMs(transport.currentMs),
                        "Marker " + (drillProject.timelineMarkerCount + 1), "rehearsal", "#38bdf8", "")
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: drillProject.timelineMarkers
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 58
                    radius: 4
                    color: MarchCraftTheme.surface
                    border.color: modelData.color || MarchCraftTheme.divider
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        Rectangle { width: 8; Layout.fillHeight: true; radius: 3; color: modelData.color }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { text: modelData.name; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label {
                                text: modelData.type + " · count " + modelData.absoluteCount
                                    + (modelData.measure ? " · m" + modelData.measure + " b" + modelData.beat : "")
                                    + " · " + (Number(modelData.timeMs || 0) / 1000).toFixed(2) + " s"
                                color: MarchCraftTheme.textMuted
                                font.pixelSize: 10
                            }
                        }
                        AppButton { text: "Show"; onClicked: { root.revealTick(modelData.tick); transport.seekTick(modelData.tick) } }
                        AppButton {
                            text: "Edit"
                            enabled: !modelData.readOnly
                            onClicked: {
                                root.markerEditIndex = index
                                root.markerEditData = modelData
                                markerName.text = modelData.name
                                markerType.editText = modelData.type
                                markerColor.text = modelData.color
                                markerCount.value = modelData.absoluteCount
                                markerNotes.text = modelData.notes || ""
                                markerEditor.open()
                            }
                        }
                        AppToolButton {
                            text: "×"
                            enabled: !modelData.readOnly
                            ToolTip.text: modelData.readOnly ? "Imported score markers are read-only" : "Remove marker"
                            ToolTip.visible: hovered
                            onClicked: drillProject.removeTimelineMarker(modelData.id)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: markerEditor
        title: "Edit timeline marker"
        modal: true
        width: 460
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        contentItem: GridLayout {
            columns: 2
            rowSpacing: 8
            columnSpacing: 10
            Label { text: "Name" }
            TextField { id: markerName; Layout.fillWidth: true; maximumLength: 80 }
            Label { text: "Type" }
            ComboBox {
                id: markerType
                editable: true
                model: ["rehearsal", "impact", "phrase", "tempo", "annotation"]
                Layout.fillWidth: true
            }
            Label { text: "Absolute count" }
            SpinBox { id: markerCount; from: 0; to: 1000000; editable: true; Layout.fillWidth: true }
            Label { text: "Color" }
            TextField { id: markerColor; placeholderText: "#38bdf8"; Layout.fillWidth: true }
            Label { text: "Notes"; Layout.alignment: Qt.AlignTop }
            TextArea { id: markerNotes; Layout.fillWidth: true; Layout.preferredHeight: 90; wrapMode: TextEdit.Wrap }
            RowLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; onClicked: markerEditor.close() }
                AppButton {
                    text: "Save marker"
                    highlighted: true
                    enabled: markerName.text.trim().length > 0
                    onClicked: {
                        if (drillProject.updateTimelineMarker(root.markerEditData.id,
                                                              drillProject.tickAtAbsoluteCount(markerCount.value),
                                                              markerName.text, markerType.editText,
                                                              markerColor.text, markerNotes.text))
                            markerEditor.close()
                    }
                }
            }
        }
    }
}
