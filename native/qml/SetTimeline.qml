import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: timelinePanel
    // Explicit dependencies supplied by the application shell.
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
    function showMusic() { workspaceSettingsContext.timelineCollapsed = false }
    function showDragPreview() {
        timeline.dragFrom = 0; timeline.dropSlot = Math.min(3, drillProjectContext.setCount)
        timeline.dragViewportX = timeline.timeX(transportContext.setPositionMs(Math.min(2, drillProjectContext.setCount - 1)))
        timeline.draggedLabel = "Page " + drillProjectContext.setInfo(0).number
    }
    visible: !workspaceSettingsContext.timelineCollapsed
    SplitView.preferredHeight: workspaceSettingsContext.timelineMaximized ? Math.max(270, verticalSplitContext.height - 240) : 290
    SplitView.minimumHeight: 270
    SplitView.maximumHeight: Math.max(270, verticalSplitContext.height - 240)
    padding: 8
    background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider }
    ColumnLayout {
        anchors.fill: parent; spacing: 5
        RowLayout {
            Layout.fillWidth: true; spacing: 4
            AppToolButton { text: "|◀"; ToolTip.text: "First page"; ToolTip.visible: hovered; onClicked: transportContext.firstSet() }
            AppToolButton { text: "◀"; ToolTip.text: "Previous page"; ToolTip.visible: hovered; onClicked: transportContext.previousSet() }
            AppToolButton { text: transportContext.playing ? "❚❚" : "▶"; ToolTip.text: "Play / pause (Ctrl+Space)"; ToolTip.visible: hovered; onClicked: transportContext.playPause() }
            AppToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transportContext.stop() }
            AppToolButton { text: "▶|"; ToolTip.text: "Next page"; ToolTip.visible: hovered; onClicked: transportContext.nextSet() }
            AppToolButton { text: "↻"; checkable: true; checked: drillProjectContext.loopEnabled
                enabled: drillProjectContext.selectedSetStartIndex !== drillProjectContext.selectedSetEndIndex
                ToolTip.text: "Loop selected page range"; ToolTip.visible: hovered; onClicked: transportContext.toggleLoop() }
            Label { text: timeline.formatTime(transportContext.currentMs) + " / " + timeline.formatTime(transportContext.durationMs); color: "#ffcb88"; font.family: "Consolas"; font.pixelSize: 12; Layout.minimumWidth: 100 }
            Item { Layout.fillWidth: true }
            AppToolButton { text: workspaceSettingsContext.timelineMaximized ? "▣" : "□"; ToolTip.text: "Maximize / restore timeline"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.timelineMaximized = !workspaceSettingsContext.timelineMaximized }
            AppToolButton { text: "▁"; ToolTip.text: "Collapse timeline"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.timelineCollapsed = true }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 4
            AppButton { id: pageToolsButton; text: "Page tools…"; onClicked: { transportContext.pause(); timelineActionsPopupContext.open() } }
            AppButton { text: "Music tools…"; onClicked: musicTools.openMenu() }
            Item { Layout.fillWidth: true }
            AppToolButton { text: "−"; ToolTip.text: "Zoom out"; ToolTip.visible: hovered; onClicked: timeline.zoomBy(1 / 1.25, timeline.viewportWidth / 2) }
            AppToolButton { text: "+"; ToolTip.text: "Zoom in"; ToolTip.visible: hovered; onClicked: timeline.zoomBy(1.25, timeline.viewportWidth / 2) }
            AppButton { text: "Fit show"; onClicked: timeline.fitShow() }
            AppButton { text: "Follow"; checkable: true; checked: timeline.followPlayhead; onClicked: { timeline.followPlayhead = !timeline.followPlayhead; if (timeline.followPlayhead) timeline.revealMs(transportContext.currentMs) } }
        }
        TimelineViewport {
            id: timeline
            Layout.fillWidth: true; Layout.fillHeight: true
            project: drillProjectContext; transport: transportContext
            onEditPage: function(index) { transportContext.editSet(index); setDialogContext.editing = true; setDialogContext.open() }
            onPageMenu: function(index, viewX, viewY) {
                setContextMenuContext.setIndex = index
                const p = mapToItem(windowContext.contentItem, viewX, viewY)
                setContextMenuContext.popup(p.x, p.y)
            }
            onReordered: if (drillProjectContext.setLabelsNeedRenumbering()) renumberDialogContext.open()
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Label { text: "Editing: " + drillProjectContext.currentSetName; color: MarchCraftTheme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight; Layout.maximumWidth: 200; Layout.fillWidth: true }
            Label { visible: timelinePanel.width > 800; text: "Click page to jump · Drag ruler to scrub · Ctrl+wheel to zoom"; color: MarchCraftTheme.textMuted; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
            ComboBox {
                model: ["MIDI synth", "Rehearsal audio", "Mute"]; Layout.preferredWidth: 140; implicitHeight: 26
                currentIndex: drillProjectContext.playbackSource === "rehearsal" ? 1 : drillProjectContext.playbackSource === "mute" ? 2 : 0
                onActivated: drillProjectContext.playbackSource = currentIndex === 1 ? "rehearsal" : currentIndex === 2 ? "mute" : "midi"
            }
            Label { visible: timelinePanel.width > 700; text: transportContext.audioStatus; color: MarchCraftTheme.textMuted; font.pixelSize: 10; elide: Text.ElideRight; Layout.maximumWidth: 170 }
        }
    }
    MusicPanel {
        id: musicTools
        onRequestMidiImport: midiDialogContext.open()
        onRequestMusicXmlImport: musicXmlDialogContext.open()
        onRequestAudioImport: audioDialogContext.open()
        onRequestTiming: { transportContext.pause(); timingDialogContext.open() }
        onRevealMeasure: function(index) { timeline.revealMs(transportContext.showMsAtTick(drillProjectContext.musicMeasureInfo(index).startTick)) }
    }
}
