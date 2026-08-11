import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

ApplicationWindow {
    id: window
    width: 1520
    height: 940
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    title: (drillProject.dirty ? "• " : "") + drillProject.showName + " — MarchCraft"
    color: "#090d11"

    property int activePerformer: -1
    property bool threeD: false
    property bool playing: transport.playing
    property string qa3DView: ""
    property bool qaSetDragPreview: false

    Settings {
        id: workspaceSettings
        property var horizontalSplitState
        property var verticalSplitState
        property bool rosterCollapsed: false
        property bool inspectorCollapsed: false
        property bool timelineCollapsed: false
        property bool timelineMaximized: false
    }

    onQa3DViewChanged: {
        if (qa3DView.length > 0) {
            window.threeD = true
            Qt.callLater(function() { threeDView.setCameraPreset(qa3DView) })
        }
    }
    onQaSetDragPreviewChanged: if (qaSetDragPreview) {
        Qt.callLater(function() {
            setStrip.dragFrom = 0
            setStrip.dropSlot = Math.min(3, drillProject.setCount)
            setStrip.dragViewportX = Math.min(setStrip.width - 56, setStrip.cardPitch * 2.5)
            const first = drillProject.setInfo(0)
            setStrip.draggedLabel = first.number + " · " + first.name
        })
    }
    property bool freehandDrawing: false
    property string freehandMovementMode: "rehearsalSafe"
    property string freehandRecognitionMode: "auto"
    property bool freehandCreateGroup: false
    function stopPlayback() { transport.stop() }
    function playCurrentTransition() { transport.playCurrentTransition() }
    function playWholeShow() { transport.playFromSelection() }

    palette {
        window: "#0f151b"
        windowText: "#e5ece8"
        base: "#121a21"
        alternateBase: "#172129"
        text: "#e5ece8"
        button: "#1b2730"
        buttonText: "#e5ece8"
        highlight: "#0f8b68"
        highlightedText: "#ffffff"
        mid: "#34444e"
    }

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action { text: "New"; shortcut: StandardKey.New; onTriggered: drillProject.newProject() }
            Action { text: "Open…"; shortcut: StandardKey.Open; onTriggered: openDialog.open() }
            Action { text: "Save"; shortcut: StandardKey.Save; onTriggered: drillProject.projectPath ? drillProject.saveProject() : saveDialog.open() }
            Action { text: "Save As…"; shortcut: StandardKey.SaveAs; onTriggered: saveDialog.open() }
            MenuSeparator {}
            Action { text: "Import coordinate JSON…"; onTriggered: importCoordinateDialog.open() }
            Action { text: "Import MIDI…"; onTriggered: midiDialog.open() }
            Action { text: "Import MusicXML…"; onTriggered: musicXmlDialog.open() }
            Action { text: "Attach audio…"; onTriggered: audioDialog.open() }
            MenuSeparator {}
            Action { text: "Export analytics CSV…"; onTriggered: csvDialog.open() }
            Action { text: "Export coordinate sheets PDF…"; onTriggered: pdfDialog.open() }
            MenuSeparator {}
            Action { text: "Exit"; shortcut: StandardKey.Quit; onTriggered: Qt.quit() }
        }
        Menu {
            title: "&Edit"
            Action { text: "Undo"; shortcut: StandardKey.Undo; enabled: drillProject.canUndo; onTriggered: drillProject.undo() }
            Action { text: "Redo"; shortcut: StandardKey.Redo; enabled: drillProject.canRedo; onTriggered: drillProject.redo() }
            MenuSeparator {}
            Action { text: "Select all"; shortcut: StandardKey.SelectAll; onTriggered: drillProject.selectAll() }
            Action { text: "Clear selection"; shortcut: "Escape"; onTriggered: drillProject.clearSelection() }
            Action { text: "Delete selected"; shortcut: StandardKey.Delete; onTriggered: drillProject.removeSelectedPerformers() }
            MenuSeparator {}
            Action { text: "Configure…"; onTriggered: settingsDialog.open() }
        }
        Menu {
            title: "&Formation"
            Action { text: "Formation builder…"; onTriggered: formationDialog.open() }
            Action { text: "Snap to 1-step grid"; onTriggered: drillProject.snapSelected(1.0) }
            Action { text: "Mirror side-to-side"; onTriggered: drillProject.mirrorSelected(true) }
            Action { text: "Mirror front-to-back"; onTriggered: drillProject.mirrorSelected(false) }
            Action { text: "Auto-label selected"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.autoLabel("P") }
            MenuSeparator {}
            Action { text: "Face front"; onTriggered: drillProject.faceSelected(0) }
            Action { text: "Face back"; onTriggered: drillProject.faceSelected(180) }
            Action { text: "Face side 1"; onTriggered: drillProject.faceSelected(270) }
            Action { text: "Face side 2"; onTriggered: drillProject.faceSelected(90) }
        }
        Menu {
            title: "&View"
            Action { text: "2D drill editor"; checkable: true; checked: !window.threeD; onTriggered: window.threeD = false }
            Action { text: "3D preview"; checkable: true; checked: window.threeD; onTriggered: window.threeD = true }
            MenuSeparator {}
            Action { text: "Show paths"; checkable: true; checked: drillProject.showTransitionPaths; onToggled: drillProject.showTransitionPaths = checked }
            Action { text: "Show shape guides"; checkable: true; checked: drillProject.showShapeGuides; onToggled: drillProject.showShapeGuides = checked }
            Action { text: "Show field grid"; checkable: true; checked: drillProject.showFieldGrid; onToggled: drillProject.showFieldGrid = checked }
            Action { text: "Show labels"; checkable: true; checked: fieldView.showLabels; onToggled: fieldView.showLabels = checked }
            MenuSeparator {}
            Action { text: "Roster panel"; checkable: true; checked: !workspaceSettings.rosterCollapsed; onToggled: workspaceSettings.rosterCollapsed = !checked }
            Action { text: "Inspector panel"; checkable: true; checked: !workspaceSettings.inspectorCollapsed; onToggled: workspaceSettings.inspectorCollapsed = !checked }
            Action { text: "Timeline panel"; checkable: true; checked: !workspaceSettings.timelineCollapsed; onToggled: workspaceSettings.timelineCollapsed = !checked }
        }
        Menu {
            title: "&Help"
            Action { text: "Load demo show"; onTriggered: drillProject.loadDemo() }
            Action { text: "About MarchCraft"; onTriggered: aboutDialog.open() }
        }
    }

    header: ToolBar {
        height: 54
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 6

            Label {
                text: "MARCHCRAFT"
                font.bold: true
                font.pixelSize: 15
                color: "#4ade80"
                Layout.rightMargin: 10
            }
            ToolButton { text: "↶"; font.pixelSize: 20; enabled: drillProject.canUndo; ToolTip.text: "Undo"; ToolTip.visible: hovered; onClicked: drillProject.undo() }
            ToolButton { text: "↷"; font.pixelSize: 20; enabled: drillProject.canRedo; ToolTip.text: "Redo"; ToolTip.visible: hovered; onClicked: drillProject.redo() }
            ToolSeparator {}
            Button { text: "+ Performer"; onClicked: { performerDialog.editing = false; performerDialog.open() } }
            Button { text: "+ Batch"; onClicked: batchDialog.open() }
            Button { text: "Shape"; enabled: drillProject.selectedCount > 0; onClicked: formationDialog.open() }
            Button { text: window.freehandDrawing ? "Draw on field…" : "Freehand"; enabled: drillProject.selectedCount > 0; highlighted: window.freehandDrawing; onClicked: freehandDialog.open() }
            ToolButton { text: "Snap"; enabled: drillProject.selectedCount > 0; ToolTip.text: "Snap selection to one-step grid"; ToolTip.visible: hovered; onClicked: drillProject.snapSelected(1) }
            ToolButton { text: "Mirror"; enabled: drillProject.selectedCount > 0; ToolTip.text: "Mirror selection side-to-side"; ToolTip.visible: hovered; onClicked: drillProject.mirrorSelected(true) }
            ToolButton {
                text: "Paths"; checkable: true; checked: drillProject.showTransitionPaths
                ToolTip.text: checked ? "Hide transition paths" : "Show transition paths"; ToolTip.visible: hovered
                onToggled: drillProject.showTransitionPaths = checked
            }
            ToolButton {
                text: "Guides"; checkable: true; checked: drillProject.showShapeGuides
                ToolTip.text: checked ? "Hide shape guides" : "Show shape guides"; ToolTip.visible: hovered
                onToggled: drillProject.showShapeGuides = checked
            }
            ToolButton { text: "Grid"; checkable: true; checked: drillProject.showFieldGrid; onToggled: drillProject.showFieldGrid = checked }
            ToolSeparator {}
            ToolButton { text: "2D"; checkable: true; checked: !window.threeD; onClicked: window.threeD = false }
            ToolButton { text: "3D"; checkable: true; checked: window.threeD; onClicked: window.threeD = true }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: fieldPreset
                model: [{text: "High School", value: "hs"}, {text: "College", value: "college"},
                        {text: "Professional", value: "nfl"}, {text: "Indoor", value: "indoor"}]
                textRole: "text"
                valueRole: "value"
                Component.onCompleted: currentIndex = indexOfValue(drillProject.fieldPreset)
                onActivated: drillProject.fieldPreset = currentValue
            }
            Label { text: drillProject.dirty ? "Unsaved" : "Saved"; color: drillProject.dirty ? "#fbbf24" : "#86efac" }
        }
    }

    SplitView {
        id: horizontalSplit
        anchors.fill: parent
        anchors.margins: 8
        orientation: Qt.Horizontal
        Component.onCompleted: if (workspaceSettings.horizontalSplitState) restoreState(workspaceSettings.horizontalSplitState)
        onResizingChanged: if (!resizing) workspaceSettings.horizontalSplitState = saveState()
        handle: Rectangle {
            implicitWidth: 8; color: SplitHandle.pressed ? "#4ade80" : SplitHandle.hovered ? "#47616f" : "#26343d"
            TapHandler { onDoubleTapped: {
                rosterPanel.SplitView.preferredWidth = 240
                inspectorPanel.SplitView.preferredWidth = 286
                workspaceSettings.horizontalSplitState = undefined
            } }
        }

        Frame {
            id: rosterPanel
            visible: !workspaceSettings.rosterCollapsed
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 460
            padding: 0
            background: Rectangle { color: "#10171d"; radius: 9; border.color: "#26343d" }
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 10
                    Label { text: "ROSTER"; font.bold: true; color: "#9fb1a7" }
                    Item { Layout.fillWidth: true }
                    Label { text: drillProject.performerCount; color: "#71847a" }
                    ToolButton { text: "‹"; ToolTip.text: "Collapse roster"; ToolTip.visible: hovered; onClicked: workspaceSettings.rosterCollapsed = true }
                }
                TextField {
                    id: rosterSearch
                    Layout.fillWidth: true
                    Layout.margins: 8
                    placeholderText: "Search labels or instruments"
                    leftPadding: 10
                }
                ListView {
                    id: roster
                    property int selectionAnchor: -1
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: drillProject
                    currentIndex: window.activePerformer
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: rosterDelegate
                        required property int index
                        required property string label
                        required property string performerName
                        required property string instrument
                        required property string section
                        required property color performerColor
                        required property bool isSelected
                        required property bool hasWarning
                        required property bool performerVisible
                        required property bool performerLocked
                        required property real totalDistance
                        width: ListView.view.width
                        height: visible ? 58 : 0
                        visible: rosterSearch.text.length === 0 ||
                                 (label + " " + performerName + " " + instrument + " " + section).toLowerCase().includes(rosterSearch.text.toLowerCase())
                        highlighted: isSelected
                        contentItem: RowLayout {
                            spacing: 8
                            Rectangle { width: 8; height: 34; radius: 4; color: rosterDelegate.performerColor }
                            ColumnLayout {
                                spacing: 0; Layout.fillWidth: true
                                Label { text: rosterDelegate.label; font.bold: true }
                                Label { text: rosterDelegate.instrument; color: "#8fa197"; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            Label { visible: rosterDelegate.hasWarning; text: "⚠"; color: "#fb7185" }
                            Label { visible: rosterDelegate.performerLocked; text: "🔒" }
                            Label { visible: !rosterDelegate.performerVisible; text: "◌"; color: "#8fa197" }
                            Label { text: rosterDelegate.totalDistance.toFixed(1); color: "#9fb1a7"; font.pixelSize: 11 }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    const group = drillProject.performerGroupInfo(index)
                                    if (Object.keys(group).length > 0) drillProject.selectGroupForPerformer(index)
                                    else if (!rosterDelegate.isSelected) drillProject.selectPerformerMode(index, 0)
                                    window.activePerformer = index; inspector.refresh()
                                    const p = mapToItem(window.contentItem, mouse.x, mouse.y)
                                    fieldContextMenu.popup(p.x, p.y); return
                                }
                                const ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                                const shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
                                if (shift && roster.selectionAnchor >= 0)
                                    drillProject.selectPerformerRange(roster.selectionAnchor, index, ctrl)
                                else {
                                    drillProject.selectPerformerMode(index, ctrl ? 1 : 0)
                                    roster.selectionAnchor = index
                                }
                                window.activePerformer = index
                                inspector.refresh()
                            }
                        }
                    }
                }
            }
        }

        SplitView {
            id: verticalSplit
            SplitView.fillWidth: true
            SplitView.minimumWidth: 520
            orientation: Qt.Vertical
            Component.onCompleted: if (workspaceSettings.verticalSplitState) restoreState(workspaceSettings.verticalSplitState)
            onResizingChanged: if (!resizing) workspaceSettings.verticalSplitState = saveState()
            handle: Rectangle {
                implicitHeight: 8; color: SplitHandle.pressed ? "#4ade80" : SplitHandle.hovered ? "#47616f" : "#26343d"
                TapHandler { onDoubleTapped: {
                    timelinePanel.SplitView.preferredHeight = 300
                    workspaceSettings.timelineMaximized = false
                    workspaceSettings.verticalSplitState = undefined
                } }
            }

            StackLayout {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 240
                currentIndex: window.threeD ? 1 : 0
                FieldView {
                    id: fieldView
                    drawMode: window.freehandDrawing
                    showPaths: drillProject.showTransitionPaths
                    showShapeGuides: drillProject.showShapeGuides
                    onPerformerActivated: function(row) { window.activePerformer = row; inspector.refresh() }
                    onContextMenuRequested: function(screenX, screenY, performerRow) {
                        window.activePerformer = performerRow
                        if (performerRow >= 0) inspector.refresh()
                        fieldContextMenu.popup(screenX, screenY)
                    }
                    onFreehandCompleted: function(points) {
                        drillProject.requestFreehandPreview(points, window.freehandMovementMode,
                                                            window.freehandCreateGroup, window.freehandRecognitionMode)
                        window.freehandDrawing = false
                        freehandPreviewDialog.applied = false
                        freehandPreviewDialog.open()
                    }
                }
                ThreeDView { id: threeDView }
            }

            Frame {
                id: timelinePanel
                visible: !workspaceSettings.timelineCollapsed
                SplitView.preferredHeight: workspaceSettings.timelineMaximized ? Math.max(150, verticalSplit.height - 240) : 300
                SplitView.minimumHeight: 150
                SplitView.maximumHeight: Math.max(150, verticalSplit.height - 240)
                padding: 8
                background: Rectangle { color: "#10171d"; radius: 9; border.color: "#26343d" }
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton { text: "|◀"; ToolTip.text: "First set"; ToolTip.visible: hovered; onClicked: transport.firstSet() }
                        ToolButton { text: "◀"; ToolTip.text: "Previous set"; ToolTip.visible: hovered; onClicked: transport.previousSet() }
                        ToolButton { text: transport.playing ? "❚❚" : "▶"; font.pixelSize: 17; ToolTip.text: "Play or pause"; ToolTip.visible: hovered; onClicked: transport.playPause() }
                        ToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transport.stop() }
                        ToolButton { text: "▶|"; ToolTip.text: "Next set"; ToolTip.visible: hovered; onClicked: transport.nextSet() }
                        Button { text: "Play selection"; visible: verticalSplit.width > 720; enabled: drillProject.setCount > 1; onClicked: transport.playFromSelection() }
                        ToolButton { text: "↻"; checkable: true; checked: drillProject.loopEnabled; enabled: drillProject.selectedSetStartIndex !== drillProject.selectedSetEndIndex; ToolTip.text: "Loop selected range"; ToolTip.visible: hovered; onClicked: transport.toggleLoop() }
                        Label { text: drillProject.currentSetName; visible: verticalSplit.width > 860; font.bold: true }
                        Slider {
                            property bool resumeAfterSeek: false
                            Layout.fillWidth: true
                            from: 0; to: 1; value: transport.normalizedPosition
                            onPressedChanged: {
                                if (pressed) {
                                    resumeAfterSeek = transport.playing
                                    if (resumeAfterSeek) transport.pause()
                                } else if (resumeAfterSeek) transport.playPause()
                            }
                            onMoved: transport.seekNormalized(value)
                        }
                        Label { text: Math.floor(transport.currentMs / 60000) + ":" + String(Math.floor((transport.currentMs % 60000) / 1000)).padStart(2,"0"); color: "#8fa197" }
                        ComboBox {
                            visible: verticalSplit.width > 790
                            model: ["MIDI Synth", "Rehearsal Audio", "Mute"]
                            Layout.preferredWidth: 140
                            currentIndex: drillProject.playbackSource === "rehearsal" ? 1 : drillProject.playbackSource === "mute" ? 2 : 0
                            onActivated: drillProject.playbackSource = currentIndex === 1 ? "rehearsal" : currentIndex === 2 ? "mute" : "midi"
                        }
                        ToolButton { text: "▁"; ToolTip.text: "Collapse timeline"; ToolTip.visible: hovered; onClicked: workspaceSettings.timelineCollapsed = true }
                        ToolButton { text: workspaceSettings.timelineMaximized ? "▣" : "□"; ToolTip.text: workspaceSettings.timelineMaximized ? "Restore timeline" : "Maximize timeline"; ToolTip.visible: hovered; onClicked: workspaceSettings.timelineMaximized = !workspaceSettings.timelineMaximized }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "FORMATION VERSION"; color: "#8fa197"; font.bold: true }
                        ComboBox {
                            id: variantSelector
                            Layout.preferredWidth: 210
                            model: drillProject.currentVariantCount
                            currentIndex: Math.max(0, drillProject.currentVariantIndex)
                            displayText: {
                                const v = drillProject.variantInfo(currentIndex)
                                return v.label ? "Variant " + v.label + " · " + v.name : "No variant"
                            }
                            delegate: ItemDelegate {
                                required property int index
                                width: variantSelector.width
                                property var variant: drillProject.variantInfo(index)
                                text: "Variant " + variant.label + " · " + variant.name +
                                      (variant.caption ? " — " + variant.caption : "")
                            }
                            onActivated: drillProject.activateVariant(currentIndex)
                        }
                        Button { text: "+ Variant"; onClicked: variantDialog.open() }
                        Button {
                            text: drillProject.currentVariantCount > 1 ? "Archive variant" : "Archive set"
                            enabled: drillProject.setCount > 1 || drillProject.currentVariantCount > 1
                            onClicked: drillProject.archiveCurrentVariant()
                        }
                        Button { text: "+ Set"; onClicked: { setDialog.editing = false; setDialog.open() } }
                        Button { text: "Edit set"; enabled: drillProject.setCount > 0; onClicked: { setDialog.editing = true; setDialog.open() } }
                        Button { text: "+ Batch"; onClicked: batchSetDialog.open() }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Archive (" + (drillProject.archivedSetCount + drillProject.currentArchivedVariantCount) + ")"
                            onClicked: archiveDialog.open()
                        }
                    }
                    TabBar {
                        id: timelineTabs
                        Layout.fillWidth: true
                        Component.onCompleted: if (drillProject.musicLoaded) currentIndex = 1
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
                            dropSlot = Math.max(0, Math.min(drillProject.setCount, Math.round((contentX + dragViewportX) / cardPitch)))
                        }
                        function targetIndex() {
                            if (dragFrom < 0 || dropSlot < 0) return dragFrom
                            let target = dropSlot
                            if (target > dragFrom) target--
                            return Math.max(0, Math.min(drillProject.setCount - 1, target))
                        }
                        anchors.fill: parent
                        orientation: ListView.Horizontal; spacing: 6; clip: true
                        model: drillProject.setCount
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
                        WheelHandler { onWheel: function(event) { setStrip.contentX=Math.max(0,Math.min(setStrip.contentWidth-setStrip.width,setStrip.contentX-event.angleDelta.y)); event.accepted=true } }
                        Connections { target: drillProject; function onCurrentSetChanged(){ setStrip.positionViewAtIndex(drillProject.currentSetIndex,ListView.Contain) } }
                        delegate: Button {
                                    id: setCard
                                    required property int index
                                    property var info: drillProject.setInfo(index)
                                    Connections { target: drillProject; function onSetsChanged() { setCard.info = drillProject.setInfo(setCard.index) } }
                                    width: 112; height: 78
                                    property real previewShift: setStrip.dragFrom < 0 || index === setStrip.dragFrom ? 0
                                        : setStrip.dropSlot > setStrip.dragFrom + 1 && index > setStrip.dragFrom && index < setStrip.dropSlot ? -setStrip.cardPitch
                                        : setStrip.dropSlot <= setStrip.dragFrom && index >= setStrip.dropSlot && index < setStrip.dragFrom ? setStrip.cardPitch : 0
                                    transform: Translate { x: setCard.previewShift }
                                    Behavior on previewShift { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
                                    opacity: setStrip.dragFrom === index ? 0.25 : 1
                                    checkable: true
                                    checked: index >= Math.min(drillProject.selectedSetStartIndex, drillProject.selectedSetEndIndex)
                                          && index <= Math.max(drillProject.selectedSetStartIndex, drillProject.selectedSetEndIndex)
                                    text: (info.subset ? "SUBSET · " : "") + info.number +
                                          (info.variantCount > 1 ? " [" + info.variantLabel + "]" : "") + " · " + info.name +
                                          (info.caption ? "\n" + info.caption : "") +
                                          "\n" + info.measure + " · " + (info.opening ? (info.openingBehavior === "hold" ? info.counts + " ct HOLD" : "MOVE NOW") : info.counts + " ct")
                                    onClicked: {
                                        drillProject.selectSetRange(index, (Qt.application.keyboardModifiers & Qt.ShiftModifier) !== 0)
                                    }
                                    MouseArea {
                                        anchors.fill: parent; acceptedButtons: Qt.RightButton; propagateComposedEvents: true
                                        onClicked: function(mouse) { setContextMenu.setIndex=index; drillProject.currentSetIndex=index; const p=mapToItem(window.contentItem,mouse.x,mouse.y); setContextMenu.popup(p.x,p.y) }
                                    }
                                    DragHandler {
                                        id: setDrag
                                        enabled: !window.playing; target: null; xAxis.enabled: true; yAxis.enabled: false
                                        onCentroidChanged: if(active) { const p=parent.mapToItem(setStrip,centroid.position.x,centroid.position.y); setStrip.updateDrop(p.x) }
                                        onActiveChanged: {
                                            if(active) {
                                                setStrip.dragFrom=index; setStrip.dropSlot=index; setStrip.draggedLabel=info.number + " · " + info.name
                                                const p=parent.mapToItem(setStrip,centroid.position.x,centroid.position.y); setStrip.updateDrop(p.x)
                                            } else if(setStrip.dragFrom >= 0) {
                                                const from=setStrip.dragFrom, target=setStrip.targetIndex()
                                                setStrip.dragFrom=-1; setStrip.dropSlot=-1; setStrip.draggedLabel=""
                                                if(target!==from){drillProject.moveSet(from,target);renumberDialog.open()}
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
                            onRequestMidiImport: midiDialog.open()
                            onRequestMusicXmlImport: musicXmlDialog.open()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: drillProject.audioSource ? "♫ " + drillProject.audioSource.split(/[\\/]/).pop() : "No audio attached"; color: "#8fa197"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        Button { text: "Audio…"; flat: true; onClicked: audioDialog.open() }
                        Button { text: "Timing…"; flat: true; onClicked: timingDialog.open() }
                        Button { text: "MusicXML…"; flat: true; onClicked: musicXmlDialog.open() }
                    }
                }
            }
        }

        Frame {
            id: inspectorPanel
            visible: !workspaceSettings.inspectorCollapsed
            SplitView.preferredWidth: 286
            SplitView.minimumWidth: 220
            SplitView.maximumWidth: 480
            padding: 0
            background: Rectangle { color: "#10171d"; radius: 9; border.color: "#26343d" }
            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth
                ColumnLayout {
                    id: inspector
                    width: parent.width
                    spacing: 10
                    property var person: ({})
                    property string clinicSeverityFilter: "all"
                    property string clinicTypeFilter: "all"
                    function refresh() { person = drillProject.performerInfo(window.activePerformer) }
                    Connections {
                        target: drillProject
                        function onCurrentSetChanged() { inspector.refresh() }
                        function onSelectionChanged() { inspector.refresh() }
                    }

                    RowLayout { Layout.fillWidth: true; Layout.margins: 8
                        Label { text: "CONTEXT INSPECTOR"; font.bold: true; color: "#9fb1a7"; Layout.fillWidth: true }
                        ToolButton { text: "›"; ToolTip.text: "Collapse inspector"; ToolTip.visible: hovered; onClicked: workspaceSettings.inspectorCollapsed = true }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        visible: drillProject.selectedCount === 1
                        Label { text: inspector.person.label || "No performer selected"; font.pixelSize: 22; font.bold: true }
                        Label { text: inspector.person.instrument || "Select a performer on the field"; color: "#8fa197" }
                        Label { text: inspector.person.coordinate || ""; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#b8c8bf"; font.pixelSize: 11 }
                        GridLayout {
                            columns: 2; Layout.fillWidth: true
                            Label { text: "Incoming"; color: "#8fa197" }
                            Label { text: drillProject.formatDistance(inspector.person.incomingDistance || 0); Layout.alignment: Qt.AlignRight }
                            Label { text: "Steps / count"; color: "#8fa197" }
                            Label { text: Number(inspector.person.stepsPerCount || 0).toFixed(2); Layout.alignment: Qt.AlignRight; color: inspector.person.warning === "critical" ? "#fb7185" : inspector.person.warning === "caution" ? "#fbbf24" : "#e5ece8" }
                            Label { text: "Outgoing"; color: "#8fa197" }
                            Label { text: drillProject.formatDistance(inspector.person.outgoingDistance || 0); Layout.alignment: Qt.AlignRight }
                            Label { text: "Direction change"; color: "#8fa197" }
                            Label { text: Number(inspector.person.directionChange || 0).toFixed(0) + " deg"; Layout.alignment: Qt.AlignRight }
                            Label { text: "Incoming path"; color: "#8fa197" }
                            Label { text: inspector.person.pathType || "direct"; Layout.alignment: Qt.AlignRight }
                        }
                        Button { text: "Edit performer…"; enabled: window.activePerformer >= 0; Layout.fillWidth: true; onClicked: { performerDialog.editing = true; performerDialog.open() } }
                        Button {
                            text: "Uniform color…"
                            enabled: window.activePerformer >= 0
                            Layout.fillWidth: true
                            onClicked: {
                                uniformColorDialog.selectedColor = inspector.person.color || "#38bdf8"
                                uniformColorDialog.open()
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        visible: drillProject.selectedCount === 0
                        Label { text: drillProject.currentSetName; font.pixelSize: 20; font.bold: true }
                        Label { text: drillProject.currentSetIndex > 0 ? drillProject.currentSetCounts + " counts / " + drillProject.effectiveTempoText(drillProject.currentSetIndex) : "Opening formation"; color: "#8fa197" }
                        Label { text: drillProject.clinicIssueCount ? drillProject.clinicIssueCount + " Clinic issue(s) in view" : "Active transition passes the current profile"; color: drillProject.clinicIssueCount ? "#fbbf24" : "#86efac"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Button { text: "Scan whole show"; Layout.fillWidth: true; onClicked: drillProject.scanShow() }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#26343d" }
                    Label { text: "SELECTION"; font.bold: true; color: "#9fb1a7"; Layout.leftMargin: 12; visible: false }
                    GridLayout {
                        visible: false; columns: 3; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        Button { text: "←"; onClicked: drillProject.nudgeSelected(-0.25, 0) }
                        Button { text: "↑"; onClicked: drillProject.nudgeSelected(0, -0.25) }
                        Button { text: "→"; onClicked: drillProject.nudgeSelected(0.25, 0) }
                        Button { text: "Mirror X"; onClicked: drillProject.mirrorSelected(true) }
                        Button { text: "↓"; onClicked: drillProject.nudgeSelected(0, 0.25) }
                        Button { text: "Snap"; onClicked: drillProject.snapSelected(1) }
                    }
                    Button { text: "Auto-label selection"; visible: false; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; onClicked: drillProject.autoLabel("P") }
                    Button { text: "Bulk edit selection…"; visible: drillProject.selectedCount > 0; enabled: visible; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; onClicked: bulkEditDialog.open() }
                    RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; visible: drillProject.selectedCount > 1
                        Label { text: drillProject.selectedCount + " performers"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
                        Button { text: "Optimizeâ€¦"; onClicked: formationDialog.open() }
                    }
                    Label { text: "FORMATION METRICS"; font.bold: true; color: "#9fb1a7"; Layout.leftMargin: 12; visible: drillProject.selectedCount > 1 }
                    GridLayout {
                        columns: 2; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        visible: drillProject.selectedCount > 1
                        property var metrics: drillProject.selectionMetrics
                        Label { text: "Formation"; color: "#8fa197" }
                        Label { text: parent.metrics.shapeType ? parent.metrics.shapeType + " / " + parent.metrics.count : parent.metrics.count + " performers"; font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Average spacing"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(parent.metrics.averageSpacing || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Minimum spacing"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(parent.metrics.minimumSpacing || 0); color: (parent.metrics.collisionCount || 0) > 0 ? drillProject.markerWarningColor : "#e5eee9"; font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Average move"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(parent.metrics.averageMove || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Size"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(parent.metrics.width || 0) + " x " + drillProject.formatDistance(parent.metrics.height || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
                    }
                    Label { text: "TRANSITION PATH"; visible: drillProject.selectedCount > 0; font.bold: true; color: "#9fb1a7"; Layout.leftMargin: 12 }
                    RowLayout {
                        visible: drillProject.selectedCount > 0
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        Button { text: "Direct"; enabled: drillProject.selectedCount > 0 && drillProject.currentSetIndex > 0; onClicked: drillProject.setSelectedTransitionPath("direct") }
                        Button { text: "Curve"; enabled: drillProject.selectedCount > 0 && drillProject.currentSetIndex > 0; onClicked: drillProject.setSelectedTransitionPath("curved") }
                        Button { text: "Delayed"; enabled: drillProject.selectedCount > 0 && drillProject.currentSetIndex > 0; onClicked: drillProject.setSelectedTransitionPath("delayed") }
                    }
                    RowLayout {
                        visible: drillProject.selectedCount > 0
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        Label { text: drillProject.currentShapeCount + " persistent shape(s)"; color: "#8fa197"; Layout.fillWidth: true }
                        Button { text: "Bake last"; enabled: drillProject.currentShapeCount > 0; onClicked: drillProject.removeShape(drillProject.currentShapeCount - 1, true) }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#26343d" }
                    Label { text: "SHOW ANALYTICS"; font.bold: true; color: "#9fb1a7"; Layout.leftMargin: 12 }
                    GridLayout {
                        columns: 2; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; rowSpacing: 10
                        Label { text: "Average / performer"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(drillProject.averageDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Ensemble total"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(drillProject.totalDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Longest move"; color: "#8fa197" }
                        Label { text: drillProject.formatDistance(drillProject.longestDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                        Label { text: "Current warnings"; color: "#8fa197" }
                        Label { text: drillProject.warningCount; color: drillProject.warningCount ? "#fb7185" : "#86efac"; font.bold: true; Layout.alignment: Qt.AlignRight }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#26343d" }
                    RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        Label { text: "DRILL CLINIC"; font.bold: true; color: "#9fb1a7"; Layout.fillWidth: true }
                        Label { text: drillProject.capabilityProfile.toUpperCase(); color: "#4ade80"; font.pixelSize: 10 }
                    }
                    RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        ComboBox { Layout.fillWidth: true; model: ["all","critical","caution","info"]; onActivated: inspector.clinicSeverityFilter = currentText }
                        ComboBox { Layout.fillWidth: true; model: ["all","stride","collision","equipmentCollision","propCollision","crossing","direction","spacing","boundary","complexPath"]; onActivated: inspector.clinicTypeFilter = currentText }
                    }
                    Label {
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        visible: drillProject.clinicIssueCount === 0
                        text: drillProject.currentSetIndex > 0 ? "No active issues at this capability profile." : "Choose a destination set to analyze its incoming transition."
                        color: "#86efac"
                        wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: drillProject.clinicIssues.filter(function(issue) {
                            return (inspector.clinicSeverityFilter === "all" || issue.severity === inspector.clinicSeverityFilter)
                                && (inspector.clinicTypeFilter === "all" || issue.type === inspector.clinicTypeFilter)
                        })
                        delegate: Frame {
                            required property var modelData
                            property string selectedSuggestionId: modelData.actions && modelData.actions.length ? modelData.actions[0].id : ""
                            Layout.fillWidth: true; Layout.leftMargin: 10; Layout.rightMargin: 10
                            background: Rectangle { color: modelData.severity === "critical" ? "#321820" : modelData.severity === "caution" ? "#302817" : "#15242b"; border.color: modelData.severity === "critical" ? "#fb7185" : modelData.severity === "caution" ? "#fbbf24" : "#38bdf8"; radius: 6 }
                            ColumnLayout {
                                anchors.fill: parent
                                Label { text: modelData.title; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                Label { text: "Set " + modelData.setLabel + (modelData.count > 0 ? " / count " + Number(modelData.count).toFixed(1) : ""); color: "#9fb1a7"; font.pixelSize: 10 }
                                Label { text: modelData.detail; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#d3dfd8"; font.pixelSize: 11 }
                                Label { text: modelData.performers ? "Affected: " + modelData.performers : ""; visible: text.length > 0; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#f1f5f9"; font.pixelSize: 10 }
                                Label { text: modelData.limit > 0 ? "Measured " + Number(modelData.measured).toFixed(2) + " / limit " + Number(modelData.limit).toFixed(2) : "Measured " + Number(modelData.measured).toFixed(2); color: "#9fb1a7"; font.pixelSize: 10 }
                                ComboBox { id: clinicAction; visible: modelData.actions && modelData.actions.length > 0; Layout.fillWidth: true; model: modelData.actions || []; textRole: "label"; onActivated: selectedSuggestionId = modelData.actions[currentIndex].id }
                                RowLayout { Layout.fillWidth: true
                                    Button { text: "Highlight"; onClicked: drillProject.selectClinicIssue(modelData.id) }
                                    Button { text: "Preview fix"; enabled: selectedSuggestionId.length > 0; onClicked: drillProject.previewSuggestion(selectedSuggestionId) }
                                    Button { text: "Apply"; enabled: selectedSuggestionId.length > 0; onClicked: drillProject.acceptSuggestion(selectedSuggestionId) }
                                    ToolButton { text: "x"; ToolTip.text: "Dismiss until this transition changes"; ToolTip.visible: hovered; onClicked: drillProject.dismissIssue(modelData.id) }
                                }
                            }
                        }
                    }
                    RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                        Button { text: "Scan show"; Layout.fillWidth: true; onClicked: drillProject.scanShow() }
                        Button { text: "Suggest next set"; enabled: drillProject.selectedCount > 1; Layout.fillWidth: true; onClicked: nextSetSuggestionDialog.open() }
                    }
                    Item { Layout.preferredHeight: 12 }
                }
            }
        }
    }

    footer: ToolBar {
        height: 28
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10; anchors.rightMargin: 10
            Label { text: drillProject.statusMessage; color: "#9fb1a7"; font.pixelSize: 11; Layout.fillWidth: true }
            Label { text: drillProject.selectedCount + " selected"; color: "#9fb1a7"; font.pixelSize: 11 }
            Label { text: window.threeD ? "3D PREVIEW" : "2D EDITOR"; color: "#4ade80"; font.bold: true; font.pixelSize: 10 }
        }
    }

    FormationDialog { id: formationDialog; anchors.centerIn: Overlay.overlay; onSettingsRequested: settingsDialog.open() }

    Dialog {
        id: freehandDialog; title: "Freehand formation"; modal: true; anchors.centerIn: Overlay.overlay; width: 480
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label { text: "Draw a line, letter, symbol, or organic form directly on the field."; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Label { text: "Stroke cleanup"; font.bold: true }
            ComboBox { id: recognitionMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Auto-recognize lines and circles",value:"auto"},{text:"Smooth handwriting / letters",value:"smooth"},{text:"Straighten line",value:"straighten"},{text:"Preserve exact stroke",value:"preserve"}]; onActivated: window.freehandRecognitionMode=currentValue }
            Label { text: "Movement assignment"; font.bold: true }
            ComboBox { id: movementMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Rehearsal safe",value:"rehearsalSafe"},{text:"Shortest total",value:"shortest"},{text:"Preserve form order",value:"preserveOrder"},{text:"Even effort",value:"evenEffort"},{text:"Feature move",value:"featureMove"},{text:"Roster order",value:"rosterOrder"}]; currentIndex: 0; onActivated: window.freehandMovementMode=currentValue }
            CheckBox { text: "Create as group"; checked: window.freehandCreateGroup; onToggled: window.freehandCreateGroup=checked }
            Label { text: "The stroke is smoothed, spaced by equal arc length, expanded if necessary, and shifted locally to avoid unselected performers."; color: "#8fa197"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Button { text: "Start drawing"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { window.freehandRecognitionMode=recognitionMode.currentValue;window.freehandMovementMode=movementMode.currentValue;window.freehandDrawing=true;freehandDialog.close() } }
        }
    }

    Dialog {
        id: freehandPreviewDialog
        property bool applied: false
        title: "Review freehand assignment"; modal: true; anchors.centerIn: Overlay.overlay; width: 460
        standardButtons: Dialog.NoButton
        onClosed: if (!applied) drillProject.cancelFormationPreview()
        contentItem: ColumnLayout {
            BusyIndicator { running: drillProject.formationPreviewBusy; visible: running; Layout.alignment: Qt.AlignHCenter }
            Label { text: drillProject.formationPreviewBusy ? "Finding a safe one-to-one assignment..." : "Review the ghost destinations and proposed paths on the field."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#a9bbb1" }
            GridLayout { columns: 2; Layout.fillWidth: true; visible: drillProject.formationPreviewActive
                property var metrics: drillProject.formationPreviewMetrics
                Label { text: "Average move"; color: "#8fa197" }
                Label { text: drillProject.formatDistance(parent.metrics.averageMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum move"; color: "#8fa197" }
                Label { text: drillProject.formatDistance(parent.metrics.maximumMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum steps / count"; color: "#8fa197" }
                Label { text: Number(parent.metrics.maximumStepsPerCount || 0).toFixed(2); Layout.alignment: Qt.AlignRight }
            }
            RowLayout { Layout.alignment: Qt.AlignRight
                Button { text: "Cancel"; onClicked: freehandPreviewDialog.close() }
                Button { text: "Apply"; highlighted: true; enabled: drillProject.formationPreviewActive; onClicked: { freehandPreviewDialog.applied = drillProject.commitFormationPreview(); freehandPreviewDialog.close() } }
            }
        }
    }

    Dialog {
        id: nextSetSuggestionDialog
        property var candidates: []
        title: "Drill Clinic / next-set ideas"; modal: true; anchors.centerIn: Overlay.overlay; width: 520
        standardButtons: Dialog.Close
        onOpened: candidates = drillProject.suggestNextSet()
        onClosed: drillProject.cancelFormationPreview()
        contentItem: ColumnLayout {
            Label { text: "Three safe starting points, ranked for spacing and reachable movement. Preview before accepting."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#a9bbb1" }
            Repeater {
                model: nextSetSuggestionDialog.candidates
                delegate: Frame {
                    required property var modelData
                    Layout.fillWidth: true
                    RowLayout { anchors.fill: parent
                        ColumnLayout { Layout.fillWidth: true
                            Label { text: modelData.label; font.bold: true }
                            Label { text: "Clinic score " + Number(modelData.score).toFixed(1); color: "#8fa197" }
                        }
                        Button { text: drillProject.formationPreviewBusy ? "Optimizing..." : "Preview"; enabled: !drillProject.formationPreviewBusy; onClicked: drillProject.requestFormationPreview(modelData.type, modelData.options, "rehearsalSafe") }
                    }
                }
            }
            RowLayout { Layout.alignment: Qt.AlignRight
                Button { text: "Cancel preview"; enabled: drillProject.formationPreviewActive; onClicked: drillProject.cancelFormationPreview() }
                Button { text: "Apply preview"; highlighted: true; enabled: drillProject.formationPreviewActive; onClicked: { drillProject.commitFormationPreview(); nextSetSuggestionDialog.close() } }
            }
        }
    }

    Menu {
        id: fieldContextMenu
        MenuItem { text: "Group selected"; enabled: drillProject.selectedCount >= 2 && !drillProject.selectionIsExactGroup; onTriggered: drillProject.groupSelected() }
        MenuItem { text: "Select group"; enabled: window.activePerformer >= 0 && Object.keys(drillProject.performerGroupInfo(window.activePerformer)).length > 0; onTriggered: drillProject.selectGroupForPerformer(window.activePerformer) }
        MenuSeparator {}
        MenuItem { text: "Remove selected from group"; enabled: drillProject.selectedGroupedCount > 0; onTriggered: drillProject.removeSelectedFromGroup() }
        MenuItem { text: "Ungroup"; enabled: drillProject.selectedGroupedCount > 0; onTriggered: drillProject.ungroupSelected() }
        MenuSeparator {}
        MenuItem { text: "Snap to 1-step grid"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.snapSelected(1) }
        MenuItem { text: "Mirror side-to-side"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(true) }
        MenuItem { text: "Auto-label selected"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.autoLabel("P") }
    }

    Menu {
        id: setContextMenu; property int setIndex: -1
        MenuItem { text: "Edit set"; onTriggered: { drillProject.currentSetIndex=setContextMenu.setIndex; setDialog.editing=true; setDialog.open() } }
        MenuItem { text: "Copy set"; onTriggered: { drillProject.duplicateSetAt(setContextMenu.setIndex); renumberDialog.open() } }
        MenuItem { text: "Add before"; onTriggered: { drillProject.insertSetAt(setContextMenu.setIndex); renumberDialog.open() } }
        MenuItem { text: "Add after"; onTriggered: { drillProject.insertSetAt(setContextMenu.setIndex+1); renumberDialog.open() } }
        MenuItem { text: "Create variant"; onTriggered: { drillProject.currentSetIndex=setContextMenu.setIndex; variantDialog.open() } }
        MenuSeparator {}
        MenuItem { text: "Delete set"; enabled: drillProject.setCount>1; onTriggered: { drillProject.archiveSetAt(setContextMenu.setIndex); renumberDialog.open() } }
    }

    Dialog {
        id: renumberDialog; title: "Renumber set labels?"; modal: true; anchors.centerIn: Overlay.overlay; width: 460
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            Label { text: "Renumber full sets 1, 2, 3… and subsets 1A, 1B…?"; wrapMode: Text.Wrap }
            RowLayout { Layout.alignment: Qt.AlignRight
                Button { text: "Keep labels"; onClicked: renumberDialog.close() }
                Button { text: "Renumber"; highlighted: true; onClicked: { drillProject.renumberSets(); renumberDialog.close() } }
            }
        }
    }

    Dialog {
        id: settingsDialog
        title: "Configure MarchCraft"
        modal: true; anchors.centerIn: Overlay.overlay; width: 620; height: 560
        standardButtons: Dialog.Close
        onOpened: placementMode.currentIndex = placementMode.indexOfValue(drillProject.shapePlacementMode)
        contentItem: ColumnLayout {
            TabBar { id: configureTabs; Layout.fillWidth: true; TabButton { text: "Performers" } TabButton { text: "Field & Grid" } TabButton { text: "Overlays" } TabButton { text: "Formations" } TabButton { text: "Drill Clinic" } }
            StackLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: configureTabs.currentIndex
                GridLayout {
                    columns: 2
                    Label { text: "Marker shape" }
                    ComboBox { Layout.fillWidth: true; model: ["circle","square","diamond"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProject.markerGeometry)); onActivated: drillProject.markerGeometry=currentText }
                    Label { text: "Fill" }
                    TextField { Layout.fillWidth: true; text: drillProject.markerFillColor; placeholderText: "section, black, or #RRGGBB"; onEditingFinished: drillProject.markerFillColor=text }
                    Label { text: "Size" }
                    Slider { Layout.fillWidth: true; from: 8; to: 28; stepSize: 1; value: drillProject.performerMarkerSize; onMoved: drillProject.performerMarkerSize=Math.round(value) }
                    Label { text: "Outline color" }
                    TextField { Layout.fillWidth: true; text: drillProject.markerOutlineColor; onEditingFinished: drillProject.markerOutlineColor=text }
                    Label { text: "Outline width" }
                    SpinBox { from: 0; to: 5; value: drillProject.markerOutlineWidth; onValueModified: drillProject.markerOutlineWidth=value }
                    Label { text: "Labels" }
                    ComboBox { Layout.fillWidth: true; model: ["off","selected","adaptive","always"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProject.markerLabelMode)); onActivated: drillProject.markerLabelMode=currentText }
                    Label { text: "Label color" }
                    TextField { Layout.fillWidth: true; text: drillProject.markerLabelColor; onEditingFinished: drillProject.markerLabelColor=text }
                    Label { text: "Warning color" }
                    TextField { Layout.fillWidth: true; text: drillProject.markerWarningColor; onEditingFinished: drillProject.markerWarningColor=text }
                    CheckBox { text: "Show facing indicator"; checked: drillProject.markerFacingVisible; onToggled: drillProject.markerFacingVisible=checked }
                    TextField { Layout.fillWidth: true; text: drillProject.markerFacingColor; onEditingFinished: drillProject.markerFacingColor=text }
                }
                GridLayout {
                    columns: 2
                    Label { text: "Field preset" }
                    ComboBox { Layout.fillWidth: true; model: ["hs","college","nfl","indoor"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProject.fieldPreset)); onActivated: drillProject.fieldPreset=currentText }
                    CheckBox { text: "Show field grid"; checked: drillProject.showFieldGrid; onToggled: drillProject.showFieldGrid=checked }
                    Item {}
                    Label { text: "Grid interval" }
                    ComboBox { Layout.fillWidth: true; model: ["4","2","1","0.5","0.25"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProject.fieldGridInterval.toString())); onActivated: drillProject.fieldGridInterval=Number(currentText) }
                    Label { text: "Grid color" }
                    TextField { Layout.fillWidth: true; text: drillProject.fieldGridColor; onEditingFinished: drillProject.fieldGridColor=text }
                    Label { text: "Grid opacity" }
                    Slider { Layout.fillWidth: true; from: .02; to: .8; value: drillProject.fieldGridOpacity; onMoved: drillProject.fieldGridOpacity=value }
                    Label { text: "Measurement display" }
                    ComboBox { Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Marching steps",value:"steps"},{text:"Yards",value:"yards"}]; Component.onCompleted: currentIndex=Math.max(0,indexOfValue(drillProject.measurementUnit)); onActivated: drillProject.measurementUnit=currentValue }
                    CheckBox { text: "Enable snapping"; checked: fieldView.snapEnabled; onToggled: fieldView.snapEnabled=checked }
                    ComboBox { Layout.fillWidth: true; model: ["4","2","1","0.5","0.25"]; Component.onCompleted: currentIndex=2; onActivated: fieldView.gridSize=Number(currentText) }
                }
                ColumnLayout {
                    CheckBox { text: "Show transition paths"; checked: drillProject.showTransitionPaths; onToggled: drillProject.showTransitionPaths=checked }
                    CheckBox { text: "Show shape guides"; checked: drillProject.showShapeGuides; onToggled: drillProject.showShapeGuides=checked }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    Label { text: "Formation placement"; font.bold: true }
                    ComboBox { id: placementMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Selection centered",value:"selection"},{text:"Nearest open space",value:"openSpace"},{text:"Field centered",value:"fieldCenter"}]; onActivated: drillProject.shapePlacementMode=currentValue }
                    Label { text: "Default spacing: four marching steps"; color: "#8fa197" }
                    Item { Layout.fillHeight: true }
                }
                GridLayout {
                    columns: 2
                    Label { text: "Capability profile" }
                    ComboBox { id: clinicProfile; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Beginner",value:"beginner"},{text:"Intermediate",value:"intermediate"},{text:"Advanced",value:"advanced"},{text:"Custom",value:"custom"}]; Component.onCompleted: currentIndex=Math.max(0,indexOfValue(drillProject.capabilityProfile)); onActivated: drillProject.capabilityProfile=currentValue }
                    Label { text: "Maximum steps / count" }
                    SpinBox { from: 25; to: 400; stepSize: 5; value: Math.round(drillProject.maximumStepsPerCount*100); editable: true; textFromValue: function(v){return (v/100).toFixed(2)}; valueFromText: function(t){return Math.round(Number(t)*100)}; onValueModified: drillProject.maximumStepsPerCount=value/100 }
                    Label { text: "Collision clearance (steps)" }
                    SpinBox { from: 25; to: 800; stepSize: 5; value: Math.round(drillProject.collisionClearance*100); editable: true; textFromValue: function(v){return (v/100).toFixed(2)}; valueFromText: function(t){return Math.round(Number(t)*100)}; onValueModified: drillProject.collisionClearance=value/100 }
                    Label { text: "Direction-change warning" }
                    SpinBox { from: 15; to: 180; stepSize: 5; value: Math.round(drillProject.directionChangeDegrees); editable: true; onValueModified: drillProject.directionChangeDegrees=value }
                    Label { text: "Caution threshold" }
                    Label { text: "85% of the configured limit"; color: "#8fa197" }
                    Item { Layout.columnSpan: 2; Layout.fillHeight: true }
                }
            }
        }
    }

    Dialog {
        id: performerDialog
        property bool editing: false
        title: editing ? "Edit performer" : "Add performer"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420
        onOpened: {
            const p = editing ? drillProject.performerInfo(window.activePerformer) : ({})
            performerLabel.text = p.label || ""
            performerName.text = p.name || ""
            performerInstrument.currentIndex = Math.max(0, performerInstrument.find(p.instrument || "Trumpet"))
            performerSection.text = p.section || "Winds"
            performerNotes.text = p.notes || ""
            performerBody.currentIndex = Math.max(0, performerBody.indexOfValue(p.bodyRigId || "performer.body.standard"))
            performerSkin.currentIndex = Math.max(0, performerSkin.indexOfValue(p.skinPaletteId || "skin.medium"))
            performerAsset.currentIndex = Math.max(0, performerAsset.indexOfValue(p.instrumentAssetId || "instrument.generic"))
            performerHeight.value = Math.round((p.heightMeters || 1.75) * 100)
        }
        contentItem: ColumnLayout {
            Label { text: "Label" }
            TextField { id: performerLabel; Layout.fillWidth: true; placeholderText: "T01" }
            Label { text: "Name" }
            TextField { id: performerName; Layout.fillWidth: true; placeholderText: "Optional student name" }
            Label { text: "Instrument / role" }
            ComboBox {
                id: performerInstrument; Layout.fillWidth: true; editable: true
                model: ["Trumpet", "Mellophone", "Trombone", "Baritone", "Tuba", "Flute", "Clarinet", "Alto Sax", "Tenor Sax", "Percussion", "Guard", "Drum Major", "Prop", "Unassigned"]
            }
            Label { text: "Section" }
            TextField { id: performerSection; Layout.fillWidth: true; placeholderText: "Brass" }
            Label { text: "3D body rig" }
            ComboBox { id: performerBody; Layout.fillWidth: true; model: assetCatalog.bodyRigs; textRole: "label"; valueRole: "id" }
            Label { text: "3D instrument / equipment" }
            ComboBox { id: performerAsset; Layout.fillWidth: true; model: assetCatalog.instruments; textRole: "label"; valueRole: "id" }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Label { text: "Skin palette" }
                    ComboBox { id: performerSkin; model: [{text:"Light",value:"skin.light"},{text:"Medium",value:"skin.medium"},{text:"Deep",value:"skin.deep"}]; textRole: "text"; valueRole: "value" }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "Height (cm)" }
                    SpinBox { id: performerHeight; from: 110; to: 225; value: 175; editable: true; Layout.fillWidth: true }
                }
            }
            Label { text: "Notes" }
            TextArea { id: performerNotes; Layout.fillWidth: true; Layout.preferredHeight: 64 }
            Button {
                text: performerDialog.editing ? "Save changes" : "Add to field"
                highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    let targetRow = window.activePerformer
                    if (performerDialog.editing) {
                        drillProject.updatePerformer(window.activePerformer, performerLabel.text, performerName.text,
                                                     performerInstrument.editText, performerSection.text, performerNotes.text)
                    } else {
                        drillProject.addPerformer(performerLabel.text, performerInstrument.editText, performerSection.text)
                        targetRow = drillProject.performerCount - 1
                    }
                    drillProject.setPerformerAppearance(targetRow, performerBody.currentValue,
                                                        "uniform.marchcraft.default", performerSkin.currentValue,
                                                        performerAsset.currentValue, performerHeight.value / 100)
                    performerDialog.close(); inspector.refresh()
                }
            }
        }
    }

    Dialog {
        id: batchDialog
        title: "Batch add performers"
        modal: true; anchors.centerIn: Overlay.overlay; width: 400
        contentItem: ColumnLayout {
            Label { text: "Label prefix" }
            TextField { id: batchPrefix; text: "T"; Layout.fillWidth: true }
            Label { text: "Number of performers" }
            SpinBox { id: batchCount; from: 1; to: 500; value: 12; editable: true; Layout.fillWidth: true }
            Label { text: "Instrument" }
            TextField { id: batchInstrument; text: "Trumpet"; Layout.fillWidth: true }
            Label { text: "Section" }
            TextField { id: batchSection; text: "Brass"; Layout.fillWidth: true }
            Button { text: "Create performers"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { drillProject.batchAddPerformers(batchPrefix.text, batchCount.value, batchInstrument.text, batchSection.text); batchDialog.close() } }
        }
    }

    Dialog {
        id: bulkEditDialog
        title: "Bulk edit selected performers"
        modal: true; anchors.centerIn: Overlay.overlay; width: 430
        contentItem: ColumnLayout {
            Label { text: "Leave text fields blank to preserve mixed values."; color: "#8fa197" }
            TextField { id: bulkInstrument; placeholderText: "Instrument (unchanged)"; Layout.fillWidth: true }
            TextField { id: bulkSection; placeholderText: "Section (unchanged)"; Layout.fillWidth: true }
            TextField { id: bulkColor; placeholderText: "Color, e.g. #38bdf8 (unchanged)"; Layout.fillWidth: true }
            RowLayout {
                Label { text: "Facing" }
                SpinBox { id: bulkFacing; from: 0; to: 359; value: 0; editable: true; Layout.fillWidth: true }
            }
            CheckBox { id: bulkVisibilityEnabled; text: "Change visibility" }
            CheckBox { id: bulkVisible; text: "Visible"; checked: true; enabled: bulkVisibilityEnabled.checked }
            CheckBox { id: bulkLockEnabled; text: "Change locking" }
            CheckBox { id: bulkLocked; text: "Locked"; enabled: bulkLockEnabled.checked }
            Button {
                text: "Apply to " + drillProject.selectedCount + " performers"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    drillProject.updateSelectedPerformers(bulkInstrument.text, bulkSection.text, bulkColor.text,
                                                          bulkFacing.value,
                                                          bulkVisibilityEnabled.checked ? (bulkVisible.checked ? 1 : 0) : -1,
                                                          bulkLockEnabled.checked ? (bulkLocked.checked ? 1 : 0) : -1)
                    bulkEditDialog.close()
                }
            }
        }
    }

    Dialog {
        id: setDialog
        property bool editing: false
        title: editing ? "Edit set" : "Add set"
        modal: true; anchors.centerIn: Overlay.overlay; width: 380
        onOpened: {
            const s = editing ? drillProject.setInfo(drillProject.currentSetIndex) : ({})
            setNumber.text = s.number || String(drillProject.setCount + 1)
            setName.text = s.name || "Set " + (drillProject.setCount + 1)
            setCaption.text = s.caption || ""
            setMeasure.text = s.measure || ""
            setCounts.value = s.opening ? drillProject.openingCounts : (s.counts || 8)
            openingBehavior.currentIndex = openingBehavior.indexOfValue(s.openingBehavior || drillProject.openingBehavior)
            setSubset.checked = s.subset || false
        }
        contentItem: ColumnLayout {
            Label { text: "Set number" }
            TextField { id: setNumber; Layout.fillWidth: true; placeholderText: "1A" }
            Label { text: "Set name" }
            TextField { id: setName; Layout.fillWidth: true }
            Label { text: "Caption" }
            TextField { id: setCaption; Layout.fillWidth: true; placeholderText: "Optional description" }
            Label { text: "Measures" }
            TextField { id: setMeasure; Layout.fillWidth: true; placeholderText: "17–20" }
            Label { text: "Opening behavior"; visible: setDialog.editing && drillProject.currentSetIndex === 0 }
            ComboBox { id: openingBehavior; visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Hold at Set 1",value:"hold"},{text:"Start moving immediately",value:"move"}] }
            Label { text: setDialog.editing && drillProject.currentSetIndex === 0 ? "Opening hold counts" : "Incoming transition counts"; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            SpinBox { id: setCounts; from: 1; to: 2048; editable: true; Layout.fillWidth: true; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            Label { visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#8fa197"; text: openingBehavior.currentValue === "hold" ? "The entire ensemble remains at Set 1 for these counts before the show clock and music begin moving." : "Playback begins the Set 1 to Set 2 transition immediately, with no opening standstill." }
            CheckBox { id: setSubset; text: "This is a subset" }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    visible: setDialog.editing
                    text: drillProject.currentVariantCount > 1 ? "Archive variant" : "Archive set"
                    enabled: drillProject.setCount > 1 || drillProject.currentVariantCount > 1
                    onClicked: { drillProject.archiveCurrentVariant(); setDialog.close() }
                }
                Button {
                    visible: setDialog.editing && drillProject.currentVariantCount > 1
                    text: "Archive entire set"
                    enabled: drillProject.setCount > 1
                    onClicked: { drillProject.archiveCurrentSet(); setDialog.close() }
                }
                Item { Layout.fillWidth: true }
                Button { text: setDialog.editing ? "Save" : "Add after current"; highlighted: true; onClicked: { if (setDialog.editing) { drillProject.updateCurrentSet(setNumber.text, setName.text, setCaption.text, setMeasure.text, setCounts.value, setSubset.checked); if (drillProject.currentSetIndex === 0) drillProject.setOpeningBehavior(openingBehavior.currentValue, setCounts.value) } else drillProject.addSet(setName.text, setCounts.value, setSubset.checked); setDialog.close() } }
            }
        }
    }

    Dialog {
        id: variantDialog
        title: "Create formation variant"
        modal: true; anchors.centerIn: Overlay.overlay; width: 420
        onOpened: {
            const current = drillProject.setInfo(drillProject.currentSetIndex)
            variantName.text = (current.name || "Set") + " alternate"
            variantCaption.text = ""
        }
        contentItem: ColumnLayout {
            Label { text: "Variant name" }
            TextField { id: variantName; Layout.fillWidth: true }
            Label { text: "Caption" }
            TextField { id: variantCaption; Layout.fillWidth: true; placeholderText: "What is different in this version?" }
            Label {
                text: "The new variant shares this set's measure, counts, and timing."
                color: "#8fa197"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            Button {
                text: "Create and activate"
                highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    drillProject.createVariant(variantName.text, variantCaption.text)
                    variantDialog.close()
                }
            }
        }
    }

    Dialog {
        id: timingDialog
        title: "Musical timing region"
        modal: true; anchors.centerIn: Overlay.overlay; width: 460
        property double startTick: 0
        property double endTick: 15360
        onOpened: {
            const destination = Math.max(1, drillProject.currentSetIndex)
            startTick = drillProject.setInfo(destination - 1).startTick || 0
            endTick = drillProject.setInfo(destination).startTick || (startTick + 15360)
        }
        contentItem: ColumnLayout {
            Label { text: "Applies to the selected transition"; color: "#8fa197" }
            GridLayout {
                columns: 2; Layout.fillWidth: true
                Label { text: "Meter" }
                RowLayout {
                    SpinBox { id: meterBeats; from: 1; to: 32; value: 4; editable: true }
                    Label { text: "/" }
                    ComboBox { id: meterDenominator; model: [1, 2, 4, 8, 16]; currentIndex: 2 }
                }
                Label { text: "Marching pulse" }
                ComboBox {
                    id: marchingPulse; Layout.fillWidth: true
                    textRole: "text"; valueRole: "ticks"
                    model: [{text: "Quarter note", ticks: 960}, {text: "Eighth note", ticks: 480},
                            {text: "Dotted quarter", ticks: 1440}, {text: "Half note", ticks: 1920}]
                }
                Label { text: "Tempo name" }
                TextField { id: tempoName; text: "Tempo"; Layout.fillWidth: true }
                Label { text: "Starting BPM" }
                SpinBox { id: tempoStart; from: 20; to: 400; value: Math.round(drillProject.bpm); editable: true; Layout.fillWidth: true }
                Label { text: "Ending BPM" }
                SpinBox { id: tempoEnd; from: 20; to: 400; value: Math.round(drillProject.bpm); editable: true; Layout.fillWidth: true }
            }
            Label {
                text: tempoStart.value === tempoEnd.value ? "Fixed tempo" : (tempoEnd.value > tempoStart.value ? "Accelerando" : "Ritardando")
                color: "#8fa197"
            }
            Button {
                text: "Apply meter, pulse, and tempo"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    drillProject.setMeterRegion(timingDialog.startTick, timingDialog.endTick,
                                                meterBeats.value, meterDenominator.currentValue,
                                                marchingPulse.currentValue, String(meterBeats.value))
                    drillProject.setTempoRegion(timingDialog.startTick, timingDialog.endTick,
                                                tempoStart.value, tempoEnd.value, tempoName.text)
                    timingDialog.close()
                }
            }
        }
    }

    Dialog {
        id: archiveDialog
        title: "Set archive"
        modal: true; anchors.centerIn: Overlay.overlay; width: 660; height: 520
        contentItem: ColumnLayout {
            Label { text: "ARCHIVED SETS"; color: "#9fb1a7"; font.bold: true }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: 190; clip: true
                model: drillProject.archivedSetCount
                spacing: 4
                delegate: Frame {
                    required property int index
                    width: ListView.view.width; height: 54
                    property var info: drillProject.archivedSetInfo(index)
                    RowLayout {
                        anchors.fill: parent
                        Label { text: info.number + " · " + info.name; font.bold: true }
                        Label { text: info.caption || info.measure; color: "#8fa197"; Layout.fillWidth: true; elide: Text.ElideRight }
                        Button { text: "Restore"; onClicked: drillProject.restoreArchivedSet(index) }
                        Button {
                            text: "Delete permanently"
                            onClicked: {
                                permanentDeleteDialog.objectKind = "set"
                                permanentDeleteDialog.objectIndex = index
                                permanentDeleteDialog.open()
                            }
                        }
                    }
                }
            }
            Label { text: "ARCHIVED VARIANTS IN CURRENT SET"; color: "#9fb1a7"; font.bold: true }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: drillProject.currentArchivedVariantCount
                spacing: 4
                delegate: Frame {
                    required property int index
                    width: ListView.view.width; height: 54
                    property var info: drillProject.archivedVariantInfo(index)
                    RowLayout {
                        anchors.fill: parent
                        Label { text: "Variant " + info.label + " · " + info.name; font.bold: true }
                        Label { text: info.caption; color: "#8fa197"; Layout.fillWidth: true; elide: Text.ElideRight }
                        Button { text: "Restore"; onClicked: drillProject.restoreArchivedVariant(index) }
                        Button {
                            text: "Delete permanently"
                            onClicked: {
                                permanentDeleteDialog.objectKind = "variant"
                                permanentDeleteDialog.objectIndex = index
                                permanentDeleteDialog.open()
                            }
                        }
                    }
                }
            }
            Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: archiveDialog.close() }
        }
    }

    Dialog {
        id: permanentDeleteDialog
        property string objectKind: "set"
        property int objectIndex: -1
        title: "Permanently delete archived " + objectKind + "?"
        modal: true; anchors.centerIn: Overlay.overlay; width: 480
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            width: 430; wrapMode: Text.Wrap
            text: "This removes the archived " + permanentDeleteDialog.objectKind +
                  " and its formations from this project. This action requires confirmation."
        }
        onAccepted: {
            if (objectKind === "set") drillProject.purgeArchivedSet(objectIndex)
            else drillProject.purgeArchivedVariant(objectIndex)
        }
    }

    Dialog {
        id: batchSetDialog
        title: "Batch add sets"
        modal: true; anchors.centerIn: Overlay.overlay; width: 360
        contentItem: ColumnLayout {
            Label { text: "Number of sets" }
            SpinBox { id: batchSetCount; from: 1; to: 100; value: 8; editable: true; Layout.fillWidth: true }
            Label { text: "Counts per set" }
            SpinBox { id: batchSetCounts; from: 1; to: 256; value: 8; editable: true; Layout.fillWidth: true }
            Button { text: "Create sets"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { drillProject.batchAddSets(batchSetCount.value, batchSetCounts.value); batchSetDialog.close() } }
        }
    }

    Dialog {
        id: aboutDialog
        title: "About MarchCraft"
        modal: true; anchors.centerIn: Overlay.overlay; width: 480
        standardButtons: Dialog.Ok
        contentItem: Label {
            width: 430; wrapMode: Text.Wrap
            text: "MarchCraft production editor preview\n\nA C++20 and Qt 6 drill-writing application. The data model, editor, analytics, 3D preview, and exports are native—there is no HTML or embedded browser.\n\nOpenMarch was used as a public feature reference; this is an independent implementation."
        }
    }

    FileDialog { id: openDialog; title: "Open MarchCraft project"; nameFilters: ["MarchCraft projects (*.marchcraft *.drill)"]; onAccepted: drillProject.loadProject(selectedFile) }
    FileDialog { id: saveDialog; title: "Save MarchCraft project"; fileMode: FileDialog.SaveFile; nameFilters: ["MarchCraft database (*.marchcraft)"]; defaultSuffix: "marchcraft"; onAccepted: drillProject.saveProject(selectedFile) }
    FileDialog { id: importCoordinateDialog; title: "Import coordinate data"; nameFilters: ["Coordinate JSON (*.json)"]; onAccepted: drillProject.importCoordinateJson(selectedFile) }
    FileDialog { id: midiDialog; title: "Import MIDI score"; nameFilters: ["MIDI (*.mid *.midi)"]; onAccepted: { timelineTabs.currentIndex = 1; drillProject.importMidiAsync(selectedFile) } }
    FileDialog { id: musicXmlDialog; title: "Import MusicXML score"; nameFilters: ["MusicXML (*.musicxml *.xml)"]; onAccepted: { timelineTabs.currentIndex = 1; drillProject.importMusicXml(selectedFile) } }
    FileDialog { id: audioDialog; title: "Attach rehearsal audio"; nameFilters: ["Audio (*.wav *.mp3 *.m4a *.flac)"]; onAccepted: drillProject.attachAudio(selectedFile) }
    FileDialog { id: csvDialog; title: "Export analytics"; fileMode: FileDialog.SaveFile; nameFilters: ["CSV (*.csv)"]; defaultSuffix: "csv"; onAccepted: drillProject.exportCsv(selectedFile) }
    FileDialog { id: pdfDialog; title: "Export coordinate sheets"; fileMode: FileDialog.SaveFile; nameFilters: ["PDF (*.pdf)"]; defaultSuffix: "pdf"; onAccepted: drillProject.exportCoordinatePdf(selectedFile) }
    ColorDialog {
        id: uniformColorDialog
        title: "Choose uniform color"
        onAccepted: {
            drillProject.setPerformerColor(window.activePerformer, selectedColor.toString())
            inspector.refresh()
        }
    }

    Shortcut { sequence: "Left"; onActivated: drillProject.nudgeSelected(-0.25, 0) }
    Shortcut { sequence: "Right"; onActivated: drillProject.nudgeSelected(0.25, 0) }
    Shortcut { sequence: "Up"; onActivated: drillProject.nudgeSelected(0, -0.25) }
    Shortcut { sequence: "Down"; onActivated: drillProject.nudgeSelected(0, 0.25) }
}
