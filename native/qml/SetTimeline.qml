import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Frame {
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
    property alias exposedSetStrip: setStrip
    property alias exposedTimelineActionsButton: timelineActionsButton
    property alias exposedTimelineTabs: timelineTabs

    id: timelinePanel
    visible: !workspaceSettingsContext.timelineCollapsed
    SplitView.preferredHeight: workspaceSettingsContext.timelineMaximized ? Math.max(150, verticalSplitContext.height - 240) : 260
    SplitView.minimumHeight: 150
    SplitView.maximumHeight: Math.max(150, verticalSplitContext.height - 240)
    padding: 8
    background: Rectangle { color: MarchCraftTheme.panel; radius: 0; border.color: MarchCraftTheme.divider }
    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            Layout.fillWidth: true
            AppToolButton { text: "|◀"; ToolTip.text: "First set"; ToolTip.visible: hovered; onClicked: transportContext.firstSet() }
            AppToolButton { text: "◀"; ToolTip.text: "Previous set"; ToolTip.visible: hovered; onClicked: transportContext.previousSet() }
            AppToolButton { text: transportContext.playing ? "❚❚" : "▶"; font.pixelSize: 17; ToolTip.text: "Play or pause"; ToolTip.visible: hovered; onClicked: transportContext.playPause() }
            AppToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transportContext.stop() }
            AppToolButton { text: "▶|"; ToolTip.text: "Next set"; ToolTip.visible: hovered; onClicked: transportContext.nextSet() }
            AppButton { text: "Play selection"; visible: verticalSplitContext.width > 720; enabled: drillProjectContext.setCount > 1; onClicked: transportContext.playFromSelection() }
            AppToolButton { text: "↻"; checkable: true; checked: drillProjectContext.loopEnabled; enabled: drillProjectContext.selectedSetStartIndex !== drillProjectContext.selectedSetEndIndex; ToolTip.text: "Loop selected range"; ToolTip.visible: hovered; onClicked: transportContext.toggleLoop() }
            Label { text: drillProjectContext.currentSetName; visible: verticalSplitContext.width > 860; font.bold: true }
            Slider {
                property bool resumeAfterSeek: false
                Layout.fillWidth: true
                from: 0; to: 1; value: transportContext.normalizedPosition
                onPressedChanged: {
                    if (pressed) {
                        resumeAfterSeek = transportContext.playing
                        if (resumeAfterSeek) transportContext.pause()
                    } else if (resumeAfterSeek) transportContext.playPause()
                }
                onMoved: transportContext.seekNormalized(value)
            }
            Label { text: Math.floor(transportContext.currentMs / 60000) + ":" + String(Math.floor((transportContext.currentMs % 60000) / 1000)).padStart(2,"0"); color: "#a5afbc" }
            ComboBox {
                visible: verticalSplitContext.width > 790
                model: ["MIDI Synth", "Rehearsal Audio", "Mute"]
                Layout.preferredWidth: 140
                currentIndex: drillProjectContext.playbackSource === "rehearsal" ? 1 : drillProjectContext.playbackSource === "mute" ? 2 : 0
                onActivated: drillProjectContext.playbackSource = currentIndex === 1 ? "rehearsal" : currentIndex === 2 ? "mute" : "midi"
            }
            AppToolButton { text: "▁"; ToolTip.text: "Collapse timeline"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.timelineCollapsed = true }
            AppToolButton { text: workspaceSettingsContext.timelineMaximized ? "▣" : "□"; ToolTip.text: workspaceSettingsContext.timelineMaximized ? "Restore timeline" : "Maximize timeline"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.timelineMaximized = !workspaceSettingsContext.timelineMaximized }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "FORMATION VERSION"; visible: verticalSplitContext.width > 760; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 0.8 }
            ComboBox {
                id: variantSelector
                Layout.preferredWidth: verticalSplitContext.width > 760 ? 210 : 170
                model: drillProjectContext.currentVariantCount
                currentIndex: Math.max(0, drillProjectContext.currentVariantIndex)
                displayText: {
                    const v = drillProjectContext.variantInfo(currentIndex)
                    return v.label ? "Variant " + v.label + " · " + v.name : "No variant"
                }
                delegate: ItemDelegate {
                    required property int index
                    width: variantSelector.width
                    property var variant: drillProjectContext.variantInfo(index)
                    text: "Variant " + variant.label + " · " + variant.name +
                          (variant.caption ? " — " + variant.caption : "")
                }
                onActivated: drillProjectContext.activateVariant(currentIndex)
            }
            AppButton { text: "+ Variant"; visible: verticalSplitContext.width > 1050; onClicked: variantDialogContext.open() }
            AppButton {
                visible: verticalSplitContext.width > 1050
                text: drillProjectContext.currentVariantCount > 1 ? "Archive variant" : "Archive set"
                enabled: drillProjectContext.setCount > 1 || drillProjectContext.currentVariantCount > 1
                onClicked: drillProjectContext.archiveCurrentVariant()
            }
            AppButton { text: "+ Set"; visible: verticalSplitContext.width > 1050; onClicked: { setDialogContext.editing = false; setDialogContext.open() } }
            AppButton { text: "Edit set"; visible: verticalSplitContext.width > 1050; enabled: drillProjectContext.setCount > 0; onClicked: { setDialogContext.editing = true; setDialogContext.open() } }
            AppButton { text: "+ Batch"; visible: verticalSplitContext.width > 1050; onClicked: batchSetDialogContext.open() }
            AppButton { id: timelineActionsButton; text: "Set actions…"; visible: verticalSplitContext.width <= 1050; onClicked: timelineActionsPopupContext.open() }
            Item { Layout.fillWidth: true }
            AppButton {
                visible: verticalSplitContext.width > 1050
                text: "Archive (" + (drillProjectContext.archivedSetCount + drillProjectContext.currentArchivedVariantCount) + ")"
                onClicked: archiveDialogContext.open()
            }
        }
        TabBar {
            id: timelineTabs
            Layout.fillWidth: true
            Component.onCompleted: if (drillProjectContext.musicLoaded) currentIndex = 1
            TabButton { text: "SETS" }
            TabButton { text: "MUSIC" }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: timelineTabs.currentIndex
            Item {
        id: setPane
        ListView {
            id: setStrip
            property int dragFrom: -1
            property int dropSlot: -1
            property real dragViewportX: 0
            property string draggedLabel: ""
            readonly property real cardPitch: 118
            function updateDrop(viewX) {
                dragViewportX = Math.max(0, Math.min(width, viewX))
                dropSlot = Math.max(0, Math.min(drillProjectContext.setCount, Math.round((contentX + dragViewportX) / cardPitch)))
            }
            function targetIndex() {
                if (dragFrom < 0 || dropSlot < 0) return dragFrom
                let target = dropSlot
                if (target > dragFrom) target--
                return Math.max(0, Math.min(drillProjectContext.setCount - 1, target))
            }
            anchors.fill: parent
            orientation: ListView.Horizontal; spacing: 6; clip: true
            model: drillProjectContext.setCount
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
            WheelHandler { onWheel: function(event) { setStrip.contentX=Math.max(0,Math.min(setStrip.contentWidth-setStrip.width,setStrip.contentX-event.angleDelta.y)); event.accepted=true } }
            Connections { target: drillProjectContext; function onCurrentSetChanged(){ setStrip.positionViewAtIndex(drillProjectContext.currentSetIndex,ListView.Contain) } }
            delegate: AppButton {
                        id: setCard
                        required property int index
                        property var info: drillProjectContext.setInfo(index)
                        Connections { target: drillProjectContext; function onSetsChanged() { setCard.info = drillProjectContext.setInfo(setCard.index) } }
                        width: 112; height: 78
                        property real previewShift: setStrip.dragFrom < 0 || index === setStrip.dragFrom ? 0
                            : setStrip.dropSlot > setStrip.dragFrom + 1 && index > setStrip.dragFrom && index < setStrip.dropSlot ? -setStrip.cardPitch
                            : setStrip.dropSlot <= setStrip.dragFrom && index >= setStrip.dropSlot && index < setStrip.dragFrom ? setStrip.cardPitch : 0
                        transform: Translate { x: setCard.previewShift }
                        Behavior on previewShift { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
                        opacity: setStrip.dragFrom === index ? 0.25 : 1
                        checkable: true
                        checked: index >= Math.min(drillProjectContext.selectedSetStartIndex, drillProjectContext.selectedSetEndIndex)
                              && index <= Math.max(drillProjectContext.selectedSetStartIndex, drillProjectContext.selectedSetEndIndex)
                        text: (info.subset ? "SUBSET · " : "") + info.number +
                              (info.variantCount > 1 ? " [" + info.variantLabel + "]" : "") + " · " + info.name +
                              (info.caption ? "\n" + info.caption : "") +
                              "\n" + info.measure + " · " + (info.opening ? (info.openingBehavior === "hold" ? info.counts + " ct HOLD" : "MOVE NOW") : info.counts + " ct")
                        onClicked: {
                            drillProjectContext.selectSetRange(index, (Qt.application.keyboardModifiers & Qt.ShiftModifier) !== 0)
                        }
                        onDoubleClicked: {
                            drillProjectContext.currentSetIndex = index
                            setDialogContext.editing = true
                            setDialogContext.open()
                        }
                        MouseArea {
                            anchors.fill: parent; acceptedButtons: Qt.RightButton; propagateComposedEvents: true
                            onClicked: function(mouse) { setContextMenuContext.setIndex=index; drillProjectContext.currentSetIndex=index; const p=mapToItem(windowContext.contentItem,mouse.x,mouse.y); setContextMenuContext.popup(p.x,p.y) }
                        }
                        DragHandler {
                            id: setDrag
                            enabled: !windowContext.playing; target: null; xAxis.enabled: true; yAxis.enabled: false
                            onCentroidChanged: if(active) { const p=parent.mapToItem(setStrip,centroid.position.x,centroid.position.y); setStrip.updateDrop(p.x) }
                            onActiveChanged: {
                                if(active) {
                                    setStrip.dragFrom=index; setStrip.dropSlot=index; setStrip.draggedLabel=info.number + " · " + info.name
                                    const p=parent.mapToItem(setStrip,centroid.position.x,centroid.position.y); setStrip.updateDrop(p.x)
                                } else if(setStrip.dragFrom >= 0) {
                                    const from=setStrip.dragFrom, target=setStrip.targetIndex()
                                    setStrip.dragFrom=-1; setStrip.dropSlot=-1; setStrip.draggedLabel=""
                                    if(target!==from){drillProjectContext.moveSet(from,target);if(drillProjectContext.setLabelsNeedRenumbering())renumberDialogContext.open()}
                                }
                            }
                        }
                        Timer { interval: 40; repeat: true; running: setDrag.active; onTriggered: { if(setStrip.dragViewportX<36)setStrip.contentX=Math.max(0,setStrip.contentX-18); else if(setStrip.dragViewportX>setStrip.width-36)setStrip.contentX=Math.min(Math.max(0,setStrip.contentWidth-setStrip.width),setStrip.contentX+18); setStrip.updateDrop(setStrip.dragViewportX) } }
                    }
            }
            Rectangle {
                visible: setStrip.dragFrom >= 0 && setStrip.dropSlot >= 0
                z: 20; width: 4; radius: 2; color: "#ffffff"
                height: setPane.height - 22; y: 4
                x: setStrip.dropSlot * setStrip.cardPitch - setStrip.contentX - width / 2
                border.color: "#99ffffff"
            }
            Rectangle {
                visible: setStrip.dragFrom >= 0; z: 19
                width: 112; height: 78; radius: 4
                x: Math.max(0,Math.min(setPane.width-width,setStrip.dragViewportX-width/2)); y: 4
                color: "#dd33444f"; border.color: "#ffffff"; border.width: 2
                Label { anchors.centerIn: parent; width: parent.width-10; text: setStrip.draggedLabel; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.bold: true }
            }
            Label {
                visible: setStrip.dragFrom >= 0; z: 21; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                text: "Drop at position " + (setStrip.targetIndex() + 1)
                color: "#ffffff"; font.bold: true; padding: 4
                background: Rectangle { color: "#cc111827"; radius: 4 }
            }
            }
            MusicPanel {
                onRequestMidiImport: midiDialogContext.open()
                onRequestMusicXmlImport: musicXmlDialogContext.open()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: drillProjectContext.audioSource ? "♫ " + drillProjectContext.audioSource.split(/[\\/]/).pop() : "No audio attached"; color: "#a5afbc"; elide: Text.ElideMiddle; Layout.fillWidth: true }
            AppButton { text: "Audio…"; flat: true; onClicked: audioDialogContext.open() }
            AppButton { text: "Timing…"; flat: true; onClicked: timingDialogContext.open() }
            AppButton { text: "MusicXML…"; flat: true; onClicked: musicXmlDialogContext.open() }
        }
    }
}
