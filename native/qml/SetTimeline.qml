import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: timelinePanel

    required property var archiveDialogContext
    required property var audioDialogContext
    required property var batchSetDialogContext
    required property var drillProjectContext
    required property var midiDialogContext
    required property var musicXmlDialogContext
    required property var renumberDialogContext
    required property var setContextMenuContext
    required property var setDialogContext
    required property var timelineActionsPopupContext
    required property var timingDialogContext
    required property var transportContext
    required property var variantDialogContext
    required property var verticalSplitContext
    required property var windowContext
    required property var workspaceSettingsContext

    property alias exposedSetStrip: timeline
    property alias exposedTimelineActionsButton: pageToolsButton
    property int inspectorRevision: 0
    readonly property bool compactLayout: height < 285
    property var transitionDetails: ({})
    property var selectedSetDetails: {
        inspectorRevision
        return drillProjectContext.selectedSetIndices.length === 1
            ? drillProjectContext.setInfo(drillProjectContext.selectedSetIndices[0]) : ({})
    }

    function showMusic() {
        workspaceSettingsContext.timelineCollapsed = false
    }

    function openMusicTool(tool) {
        if (tool === "group") musicTools.openGroupSelection()
        else if (tool === "sections") musicTools.openSections()
        else if (tool === "markers") musicTools.openMarkers()
        else if (tool === "timing") { transportContext.pause(); timingDialogContext.open() }
        else if (tool === "offset") musicTools.openOffset()
        else if (tool === "mapping") musicTools.openMapping()
        else if (tool === "generation") musicTools.openGeneration()
        else if (tool === "planning") musicTools.openSetPlan()
        else if (tool === "diagnostics") musicTools.openDiagnostics()
        else if (tool === "tracks") musicTools.openTracks()
        else musicTools.openMenu()
    }

    function showDragPreview() {
        timeline.dragFrom = 0
        timeline.dropSlot = Math.min(3, drillProjectContext.setCount)
        timeline.dragViewportX = timeline.timeX(transportContext.setPositionMs(Math.min(2, drillProjectContext.setCount - 1)))
        timeline.draggedLabel = "Set " + drillProjectContext.setInfo(0).number
    }

    function showQaTimelineMode(mode) {
        workspaceSettingsContext.timelineCollapsed = false
        workspaceSettingsContext.timelineMaximized = mode !== "normal"
        if (mode === "zoom") {
            timeline.fitShow()
            timeline.zoomBy(3.2, timeline.viewportWidth / 2)
        } else if (mode === "selection") {
            if (drillProjectContext.setCount > 2) {
                drillProjectContext.selectTimelineSet(1, 0)
                drillProjectContext.selectTimelineSet(2, 1)
                timeline.fitSelection()
            }
        } else if (mode === "transition") {
            if (drillProjectContext.setCount > 1) {
                drillProjectContext.selectTimelineTransition(1)
                timeline.fitSelection()
            }
        } else if (mode === "resize") {
            if (drillProjectContext.setCount > 1) {
                drillProjectContext.selectTimelineTransition(1)
                timeline.resizingTransition = 1
                const x = timeline.timeX(transportContext.setPositionMs(1))
                timeline.previewResize(1, x)
            }
        } else if (mode === "plan") {
            timeline.fitShow()
        } else {
            timeline.fitShow()
        }
    }

    function refreshInspector() {
        inspectorRevision++
        transitionDetails = drillProjectContext.selectedTransitionIndex > 0
            ? drillProjectContext.transitionInfo(drillProjectContext.selectedTransitionIndex) : ({})
    }

    visible: !workspaceSettingsContext.timelineCollapsed
    SplitView.preferredHeight: workspaceSettingsContext.timelineMaximized
        ? Math.max(380, verticalSplitContext.height - 210) : 190
    SplitView.minimumHeight: 170
    SplitView.maximumHeight: Math.max(380, verticalSplitContext.height - 170)
    padding: 8
    background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider }

    Connections {
        target: drillProjectContext
        function onTimelineSelectionChanged() { timelinePanel.refreshInspector() }
        function onTimingChanged() { timelinePanel.refreshInspector() }
        function onSetsChanged() { timelinePanel.refreshInspector() }
    }

    Component.onCompleted: refreshInspector()

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            AppToolButton { text: "|◀"; ToolTip.text: "First set"; ToolTip.visible: hovered; onClicked: transportContext.firstSet() }
            AppToolButton { text: "◀"; ToolTip.text: "Previous set"; ToolTip.visible: hovered; onClicked: transportContext.previousSet() }
            AppToolButton { text: transportContext.playing ? "❚❚" : "▶"; ToolTip.text: "Play / pause (Ctrl+Space)"; ToolTip.visible: hovered; onClicked: transportContext.playPause() }
            AppToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transportContext.stop() }
            AppToolButton { text: "▶|"; ToolTip.text: "Next set"; ToolTip.visible: hovered; onClicked: transportContext.nextSet() }
            AppToolButton {
                text: "↻"
                checkable: true
                checked: drillProjectContext.loopEnabled
                enabled: drillProjectContext.timelineSelectionKind === "set"
                    && drillProjectContext.selectedSetStartIndex !== drillProjectContext.selectedSetEndIndex
                    && drillProjectContext.selectedSetIndices.length
                        === Math.abs(drillProjectContext.selectedSetEndIndex
                            - drillProjectContext.selectedSetStartIndex) + 1
                ToolTip.text: "Loop selected set range"
                ToolTip.visible: hovered
                onClicked: transportContext.toggleLoop()
            }
            Label {
                text: timeline.formatTime(transportContext.currentMs) + " / " + timeline.formatTime(transportContext.durationMs)
                color: "#ffcb88"
                font.family: "Consolas"
                font.pixelSize: 12
                Layout.minimumWidth: 118
            }
            Label {
                visible: timelinePanel.width > 880
                text: drillProjectContext.timelineSelectionKind === "transition" ? "Transition selected"
                    : drillProjectContext.timelineSelectionKind === "time" ? "Time range selected"
                    : drillProjectContext.timelineSelectionKind === "measure" ? "Measure range selected"
                    : drillProjectContext.timelineSelectionKind === "none" ? "No timeline selection"
                    : (drillProjectContext.selectedSetIndices.length + " set" + (drillProjectContext.selectedSetIndices.length === 1 ? "" : "s") + " selected")
                color: MarchCraftTheme.textSecondary
                font.pixelSize: 10
            }
            Item { Layout.fillWidth: true }
            AppToolButton {
                text: workspaceSettingsContext.timelineMaximized ? "▣" : "□"
                ToolTip.text: "Maximize / restore timeline"
                ToolTip.visible: hovered
                onClicked: workspaceSettingsContext.timelineMaximized = !workspaceSettingsContext.timelineMaximized
            }
            AppToolButton { text: "▁"; ToolTip.text: "Collapse timeline"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.timelineCollapsed = true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            AppButton { id: pageToolsButton; text: "Set tools…"; onClicked: { transportContext.pause(); timelineActionsPopupContext.open() } }
            AppButton {
                text: timeline.snapEnabled ? "Snap: On" : "Snap: Off"
                checkable: true
                checked: timeline.snapEnabled
                ToolTip.text: "Snap scrubbing, ranges, plan markers, and set boundaries to counts and musical landmarks. Hold Alt to bypass."
                ToolTip.visible: hovered
                onClicked: timeline.snapEnabled = !timeline.snapEnabled
            }
            AppButton { text: "Music tools…"; onClicked: musicTools.openMenu() }
            AppButton { text: "Analyze music…"; onClicked: musicTools.openSetPlan() }
            AppButton { text: "Markers…"; onClicked: musicTools.openMarkers() }
            AppButton { text: "Production sheet…"; onClicked: productionSheet.open() }
            Item { Layout.fillWidth: true }
            AppToolButton { text: "−"; ToolTip.text: "Zoom out"; ToolTip.visible: hovered; onClicked: timeline.zoomBy(1 / 1.25, timeline.viewportWidth / 2) }
            AppToolButton { text: "+"; ToolTip.text: "Zoom in"; ToolTip.visible: hovered; onClicked: timeline.zoomBy(1.25, timeline.viewportWidth / 2) }
            AppButton { text: "Fit selection"; onClicked: timeline.fitSelection() }
            AppButton { text: "Fit show"; onClicked: timeline.fitShow() }
            AppToolButton { text: "◎"; ToolTip.text: "Reveal editing set"; ToolTip.visible: hovered; onClicked: timeline.revealCurrentSet() }
            AppButton {
                text: "Follow"
                checkable: true
                checked: timeline.followPlayhead
                onClicked: {
                    timeline.followPlayhead = !timeline.followPlayhead
                    if (timeline.followPlayhead)
                        timeline.revealMs(transportContext.currentMs)
                }
            }
        }

        TimelineViewport {
            id: timeline
            Layout.fillWidth: true
            Layout.fillHeight: true
            project: drillProjectContext
            transport: transportContext
            onEditPage: function(index) {
                transportContext.editSet(index)
                setDialogContext.editing = true
                setDialogContext.open()
            }
            onPageMenu: function(index, viewX, viewY) {
                setContextMenuContext.setIndex = index
                const p = mapToItem(windowContext.contentItem, viewX, viewY)
                setContextMenuContext.popup(p.x, p.y)
            }
            onReordered: if (drillProjectContext.setLabelsNeedRenumbering()) renumberDialogContext.open()
            onTransitionSelected: timelinePanel.refreshInspector()
        }

        Frame {
            visible: !timelinePanel.compactLayout
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 48 : 0
            padding: 5
            background: Rectangle {
                color: MarchCraftTheme.panelHeader
                border.color: drillProjectContext.timelineSelectionKind === "transition"
                    ? MarchCraftTheme.accent : MarchCraftTheme.divider
                radius: 4
            }

            RowLayout {
                anchors.fill: parent
                spacing: 7

                RowLayout {
                    visible: drillProjectContext.timelineSelectionKind === "transition"
                        && drillProjectContext.selectedTransitionIndex > 0
                    spacing: 7
                    Label {
                        text: "SET " + (timelinePanel.transitionDetails.sourceNumber || "?")
                            + "  →  SET " + (timelinePanel.transitionDetails.destinationNumber || "?")
                        color: MarchCraftTheme.textPrimary
                        font.bold: true
                    }
                    Label {
                        visible: timelinePanel.width > 710
                        text: timelinePanel.transitionDetails.durationMs !== undefined
                            ? (Number(timelinePanel.transitionDetails.durationMs) / 1000).toFixed(2) + " s · m"
                                + timelinePanel.transitionDetails.measure + " b" + timelinePanel.transitionDetails.beat
                            : ""
                        color: MarchCraftTheme.textMuted
                        font.pixelSize: 10
                    }
                    Label { text: "Counts"; color: MarchCraftTheme.textSecondary }
                    SpinBox {
                        id: directCounts
                        objectName: "transitionCountsEditor"
                        from: 1
                        to: 2048
                        editable: true
                        value: timelinePanel.transitionDetails.counts || 1
                        onValueModified: drillProjectContext.setTransitionCounts(drillProjectContext.selectedTransitionIndex, value)
                    }
                    AppButton {
                        text: "−8"
                        enabled: (timelinePanel.transitionDetails.counts || 0) > 8
                        onClicked: drillProjectContext.deleteCountsFromTransition(drillProjectContext.selectedTransitionIndex, 8)
                    }
                    AppButton { text: "+8"; onClicked: drillProjectContext.insertCountsBeforeSet(drillProjectContext.selectedTransitionIndex, 8) }
                    Label {
                        visible: timelinePanel.width > 940
                        text: "Travel avg " + Number(timelinePanel.transitionDetails.averageDistance || 0).toFixed(1)
                            + " yd · max " + Number(timelinePanel.transitionDetails.maximumDistance || 0).toFixed(1) + " yd"
                        color: MarchCraftTheme.textSecondary
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    visible: drillProjectContext.timelineSelectionKind === "set"
                    Label {
                        text: drillProjectContext.selectedSetIndices.length === 1
                            ? "SET " + (timelinePanel.selectedSetDetails.number || "") + " · "
                                + (timelinePanel.selectedSetDetails.name || "")
                            : drillProjectContext.selectedSetIndices.length + " sets selected"
                        color: MarchCraftTheme.textPrimary
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: 250
                    }
                    Label {
                        visible: drillProjectContext.selectedSetIndices.length === 1
                        text: "abs " + (timelinePanel.selectedSetDetails.absoluteCount || 0)
                            + (!timelinePanel.selectedSetDetails.opening
                                ? " · incoming " + (timelinePanel.selectedSetDetails.counts || 0) + " ct" : "")
                            + (timelinePanel.selectedSetDetails.subset ? " · subset" : "")
                            + (timelinePanel.selectedSetDetails.endingMeasure ? " · m" + timelinePanel.selectedSetDetails.endingMeasure
                                + " b" + timelinePanel.selectedSetDetails.endingBeat : "")
                            + " · " + (timelinePanel.selectedSetDetails.tempo || "")
                            + (timelinePanel.selectedSetDetails.caption ? " · " + timelinePanel.selectedSetDetails.caption : "")
                            + (timelinePanel.selectedSetDetails.variantCount > 1
                                ? " · variant " + timelinePanel.selectedSetDetails.variantLabel : "")
                        color: MarchCraftTheme.textMuted
                        font.pixelSize: 10
                    }
                    Label {
                        visible: drillProjectContext.selectedSetIndices.length !== 1
                        text: "Shift-click a range · Ctrl-click discontiguous sets"
                        color: MarchCraftTheme.textMuted
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    visible: drillProjectContext.timelineSelectionKind === "time"
                        || drillProjectContext.timelineSelectionKind === "measure"
                    Label {
                        text: drillProjectContext.timelineSelectionKind === "measure"
                            ? "Measures " + (Math.min(drillProjectContext.musicSelectionStart,
                                drillProjectContext.musicSelectionEnd) + 1) + "–"
                                + (Math.max(drillProjectContext.musicSelectionStart,
                                    drillProjectContext.musicSelectionEnd) + 1)
                            : "Time range"
                        color: MarchCraftTheme.textPrimary
                        font.bold: true
                    }
                    Label {
                        text: "Counts " + drillProjectContext.absoluteCountAtTick(drillProjectContext.timelineRangeStartTick)
                            + "–" + drillProjectContext.absoluteCountAtTick(drillProjectContext.timelineRangeEndTick)
                        color: MarchCraftTheme.textSecondary
                    }
                }

                Item { Layout.fillWidth: true }
                AppToolButton {
                    visible: drillProjectContext.timelineSelectionKind !== "none"
                    text: "×"
                    ToolTip.text: "Clear timeline selection"
                    ToolTip.visible: hovered
                    onClicked: drillProjectContext.clearTimelineSelection()
                }
                Label {
                    visible: drillProjectContext.setPlanPreviewActive
                    text: drillProjectContext.setPlanCandidateCount + " analysis candidates in preview"
                    color: "#d990ea"
                    font.pixelSize: 10
                }
                AppButton {
                    visible: drillProjectContext.setPlanPreviewActive
                    text: "Review…"
                    onClicked: musicTools.openSetPlanReview()
                }
                ComboBox {
                    model: ["MIDI synth", "Rehearsal audio", "Mute"]
                    Layout.preferredWidth: 135
                    implicitHeight: 26
                    currentIndex: drillProjectContext.playbackSource === "rehearsal" ? 1
                        : drillProjectContext.playbackSource === "mute" ? 2 : 0
                    onActivated: drillProjectContext.playbackSource = currentIndex === 1 ? "rehearsal"
                        : currentIndex === 2 ? "mute" : "midi"
                }
            }
        }
    }

    MusicPanel {
        id: musicTools
        onRequestMidiImport: midiDialogContext.open()
        onRequestMusicXmlImport: musicXmlDialogContext.open()
        onRequestAudioImport: audioDialogContext.open()
        onRequestTiming: { transportContext.pause(); timingDialogContext.open() }
        onRevealMeasure: function(index) {
            timeline.revealMs(transportContext.showMsAtTick(drillProjectContext.musicMeasureInfo(index).startTick))
        }
        onRevealTick: function(tick) { timeline.revealMs(transportContext.showMsAtTick(tick)) }
    }

    Dialog {
        id: productionSheet
        title: "Production sheet · timing structure"
        modal: true
        width: Math.min(980, timelinePanel.width - 30)
        height: Math.min(620, timelinePanel.height + 260)
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            spacing: 5
            Label {
                text: "Edit incoming counts directly. Changes ripple through later set timestamps and remain one undoable command."
                color: MarchCraftTheme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.fillWidth: true
                height: 28
                color: MarchCraftTheme.panelHeader
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Label { text: "SET"; font.bold: true; Layout.preferredWidth: 54 }
                    Label { text: "FORMATION"; font.bold: true; Layout.fillWidth: true }
                    Label { text: "INCOMING"; font.bold: true; Layout.preferredWidth: 96 }
                    Label { text: "START COUNT"; font.bold: true; Layout.preferredWidth: 90 }
                    Label { text: "TIMESTAMP"; font.bold: true; Layout.preferredWidth: 90 }
                    Label { text: "MUSIC"; font.bold: true; Layout.preferredWidth: 90 }
                    Label { text: "TEMPO"; font.bold: true; Layout.preferredWidth: 90 }
                    Label { text: "TRAVEL"; font.bold: true; Layout.preferredWidth: 110 }
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: drillProjectContext.setCount
                delegate: Rectangle {
                    required property int index
                    property var setData: drillProjectContext.setInfo(index)
                    property var transitionData: index > 0 ? drillProjectContext.transitionInfo(index) : ({})
                    width: ListView.view.width
                    height: 38
                    color: index % 2 ? MarchCraftTheme.surface : MarchCraftTheme.surfaceRaised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Label { text: setData.number + (setData.subset ? " · SUB" : ""); font.bold: true; Layout.preferredWidth: 54 }
                        Label {
                            text: setData.name + (setData.caption ? " · " + setData.caption : "")
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            Layout.preferredWidth: 96
                            from: index === 0 ? 0 : 1
                            to: index === 0 ? 0 : 2048
                            enabled: index > 0
                            editable: true
                            value: index === 0 ? 0 : transitionData.counts
                            onValueModified: if (index > 0) drillProjectContext.setTransitionCounts(index, value)
                        }
                        Label { text: setData.absoluteCount !== undefined ? setData.absoluteCount : "—"; Layout.preferredWidth: 90 }
                        Label { text: timeline.formatTime(transportContext.setPositionMs(index)); Layout.preferredWidth: 90 }
                        Label {
                            text: index > 0 && transitionData.measure ? "m" + transitionData.measure + " b" + transitionData.beat : "—"
                            Layout.preferredWidth: 90
                        }
                        Label { text: setData.tempo || "—"; elide: Text.ElideRight; Layout.preferredWidth: 90 }
                        Label {
                            text: index > 0 ? Number(transitionData.averageDistance || 0).toFixed(1) + " / "
                                + Number(transitionData.maximumDistance || 0).toFixed(1) + " yd" : "—"
                            Layout.preferredWidth: 110
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        z: -1
                        onClicked: timelinePanel.transportContext.navigateToSetWithMode(index, 0)
                    }
                }
            }
        }
    }
}
