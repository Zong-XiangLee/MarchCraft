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
    title: workspaceState.workspaceActive
           ? (drillProject.dirty ? "• " : "") + drillProject.showName + " — MarchCraft"
           : "MarchCraft"
    color: MarchCraftTheme.canvas

    function showQaSurface(surface) {
        if (surface === "preferences") settingsDialog.open()
        else if (surface === "performer") { performerDialog.editing = false; performerDialog.open() }
        else if (surface === "music") timelinePanel.exposedTimelineTabs.currentIndex = 1
    }

    property int activePerformer: -1
    property bool threeD: false
    property bool playing: transport.playing
    property string qa3DView: ""
    property bool qaSetDragPreview: false
    property bool forceClosing: false
    property string savePurpose: "normal"
    property string homeMode: initialHomeMode
    property bool qaShapePalette: false

    WorkspaceLogic {
        id: workspaceState
        workspaceActive: initialWorkspaceActive
        hasCurrentProject: initialWorkspaceActive
        onActionReady: function(action, path) { window.executeWorkspaceAction(action, path) }
        onAwaitingConfirmationChanged: if (awaitingConfirmation) unsavedChangesDialog.open()
    }

    function startNewProject() {
        workspaceState.request("new", "", drillProject.dirty)
    }

    function returnHome() {
        homeMode = "dashboard"
        workspaceState.workspaceActive = false
        Qt.callLater(function() { (homePage.exposedResumeButton.visible ? homePage.exposedResumeButton : homePage.exposedNewProjectHomeButton).forceActiveFocus() })
    }

    function createProject(name, fieldPreset, lightingPreset) {
        drillProject.newProject()
        drillProject.showName = name
        drillProject.fieldPreset = fieldPreset
        drillProject.lightingPreset = lightingPreset
        homeMode = "dashboard"
        workspaceState.enteredProject()
    }

    function requestOpenProject() {
        workspaceState.request("open", "", drillProject.dirty)
    }

    function requestSampleProject() {
        workspaceState.request("sample", "", drillProject.dirty)
    }

    function openProjectPath(path) {
        if (!drillProject.loadProject(path)) return
        workspaceController.recordRecentProject(drillProject.projectPath, drillProject.showName)
        workspaceState.enteredProject()
    }

    function executeWorkspaceAction(action, path) {
        if (action === "new") {
            workspaceState.workspaceActive = false
            homeMode = "new"
        } else if (action === "open") {
            openDialog.open()
        } else if (action === "openPath") {
            openProjectPath(path)
        } else if (action === "sample") {
            drillProject.loadDemo()
            workspaceState.enteredProject()
        } else if (action === "exit") {
            forceClosing = true
            Qt.quit()
        }
    }

    function saveCurrentProject(purpose) {
        savePurpose = purpose || "normal"
        if (!drillProject.projectPath) {
            saveDialog.open()
            return
        }
        if (drillProject.saveProject()) {
            workspaceController.recordRecentProject(drillProject.projectPath, drillProject.showName)
            if (savePurpose === "pending") workspaceState.confirmAfterSave()
        }
    }

    Settings {
        id: workspaceSettings
        property var horizontalSplitState
        property var verticalSplitState
        property bool rosterCollapsed: false
        property bool inspectorCollapsed: false
        property bool timelineCollapsed: false
        property bool timelineMaximized: false
        property var quickShapes: ["line", "rectangle", "circle", "triangle"]
    }

    Component.onCompleted: {
        workspaceController.refreshRecentProjects()
        drillProject.refreshRecoveryCandidates()
        if (!workspaceState.workspaceActive && !qaMode && workspaceController.startupSoundEnabled)
            Qt.callLater(function() { workspaceController.playStartupSound() })
        if (!workspaceState.workspaceActive) homePage.exposedHomeIntro.restart()
    }

    onClosing: function(close) {
        if (!forceClosing && workspaceState.hasCurrentProject && drillProject.dirty) {
            close.accepted = false
            workspaceState.request("exit", "", true)
        }
    }

    onQa3DViewChanged: {
        if (qa3DView.length > 0) {
            window.threeD = true
            Qt.callLater(function() { threeDView.setCameraPreset(qa3DView) })
        }
    }
    onQaSetDragPreviewChanged: if (qaSetDragPreview) {
        Qt.callLater(function() {
            timelinePanel.exposedSetStrip.dragFrom = 0
            timelinePanel.exposedSetStrip.dropSlot = Math.min(3, drillProject.setCount)
            timelinePanel.exposedSetStrip.dragViewportX = Math.min(timelinePanel.exposedSetStrip.width - 56, timelinePanel.exposedSetStrip.cardPitch * 2.5)
            const first = drillProject.setInfo(0)
            timelinePanel.exposedSetStrip.draggedLabel = first.number + " · " + first.name
        })
    }
    property bool freehandDrawing: false
    property string shapeDrawing: ""
    property var quickShapes: workspaceSettings.quickShapes
    property string freehandMovementMode: "rehearsalSafe"
    property string freehandRecognitionMode: "auto"
    property bool freehandCreateGroup: false
    function setShapeTool(kind) {
        freehandDrawing = false
        shapeDrawing = kind
    }
    function toggleQuickShape(kind) {
        var next = quickShapes.slice()
        var index = next.indexOf(kind)
        if (index >= 0) next.splice(index, 1)
        else next.push(kind)
        quickShapes = next
        workspaceSettings.quickShapes = next
    }
    function stopPlayback() { transport.stop() }
    function playCurrentTransition() { transport.playCurrentTransition() }
    function playWholeShow() { transport.playFromSelection() }

    palette {
        window: MarchCraftTheme.surface
        windowText: MarchCraftTheme.textPrimary
        base: "#0f151d"
        alternateBase: MarchCraftTheme.surfaceRaised
        text: MarchCraftTheme.textPrimary
        button: MarchCraftTheme.surfaceRaised
        buttonText: MarchCraftTheme.textPrimary
        highlight: MarchCraftTheme.accent
        highlightedText: "#ffffff"
        mid: MarchCraftTheme.divider
    }
    onQaShapePaletteChanged: if (qaShapePalette) Qt.callLater(function() { shapePalette.open() })

    menuBar: MenuBar {
        visible: workspaceState.workspaceActive
        Menu {
            title: "&File"
            Action { text: "Home"; onTriggered: window.returnHome() }
            MenuSeparator {}
            Action { text: "New project…"; shortcut: StandardKey.New; onTriggered: window.startNewProject() }
            Action { text: "Open…"; shortcut: StandardKey.Open; onTriggered: window.requestOpenProject() }
            Action { text: "Save"; shortcut: StandardKey.Save; enabled: workspaceState.hasCurrentProject; onTriggered: window.saveCurrentProject("normal") }
            Action { text: "Save As…"; shortcut: StandardKey.SaveAs; enabled: workspaceState.hasCurrentProject; onTriggered: { window.savePurpose = "normal"; saveDialog.open() } }
            Action { text: "Restore version…"; enabled: drillProject.projectHistory.length > 0; onTriggered: { drillProject.refreshProjectHistory(); historyDialog.open() } }
            MenuSeparator {}
            Action { text: "Import coordinate JSON…"; onTriggered: importCoordinateDialog.open() }
            Action { text: "Import MIDI…"; onTriggered: midiDialog.open() }
            Action { text: "Import MusicXML…"; onTriggered: musicXmlDialog.open() }
            Action { text: "Attach audio…"; onTriggered: audioDialog.open() }
            MenuSeparator {}
            Action { text: "Export analytics CSV…"; onTriggered: csvDialog.open() }
            Action { text: "Export coordinate sheets PDF…"; onTriggered: pdfDialog.open() }
            MenuSeparator {}
            Action { text: "Exit"; shortcut: StandardKey.Quit; onTriggered: workspaceState.request("exit", "", drillProject.dirty) }
        }
        Menu {
            title: "&Edit"
            Action { text: "Undo"; shortcut: StandardKey.Undo; enabled: drillProject.canUndo; onTriggered: drillProject.undo() }
            Action { text: "Redo"; shortcut: StandardKey.Redo; enabled: drillProject.canRedo; onTriggered: drillProject.redo() }
            MenuSeparator {}
            Action { text: "Select all"; shortcut: StandardKey.SelectAll; onTriggered: drillProject.selectAll() }
            Action { text: "Clear selection"; shortcut: "Escape"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.clearSelection() }
            Action { text: "Delete selected"; shortcut: StandardKey.Delete; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.removeSelectedPerformers() }
            MenuSeparator {}
            Action { text: "Project setup…"; onTriggered: { projectSetupDialog.creationMode = false; projectSetupDialog.open() } }
            Action { text: "Editor preferences…"; onTriggered: settingsDialog.open() }
        }
        Menu {
            title: "&Formation"
            Action { text: "Formation builder…"; enabled: drillProject.selectedCount > 0; onTriggered: formationDialog.open() }
            Action { text: "Snap to 1-step grid"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.snapSelected(1.0) }
            Action { text: "Mirror side-to-side"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(true) }
            Action { text: "Mirror front-to-back"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(false) }
            Action { text: "Auto-label selected"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.autoLabel("P") }
            MenuSeparator {}
            Action { text: "Face front"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(0) }
            Action { text: "Face back"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(180) }
            Action { text: "Face side 1"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(270) }
            Action { text: "Face side 2"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(90) }
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
            Action { text: "Load sample show"; onTriggered: window.requestSampleProject() }
            Action { text: "About MarchCraft"; onTriggered: aboutDialog.open() }
        }
    }

    header: EditorCommandBars {
        id: commandBars
        batchDialogContext: batchDialog
        drillProjectContext: drillProject
        editorActionsPopupContext: editorActionsPopup
        freehandDialogContext: freehandDialog
        performerDialogContext: performerDialog
        shapePaletteContext: shapePalette
        windowContext: window
        workspaceStateContext: workspaceState
    }

    Popup {
        id: shapePalette
        x: Math.min(window.width - width - 16, commandBars.shapeButton.mapToItem(window.contentItem, 0, 0).x)
        y: 92; width: 360; height: 250; padding: 14; modal: false; focus: true
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: MarchCraftTheme.motionMedium }
            NumberAnimation { property: "scale"; from: .97; to: 1; duration: MarchCraftTheme.motionMedium; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: MarchCraftTheme.motionFast } }
        background: Rectangle { color: MarchCraftTheme.surfaceRaised; radius: MarchCraftTheme.radiusLarge; border.color: MarchCraftTheme.dividerStrong }
        contentItem: ColumnLayout {
            spacing: 10
            RowLayout {
                Label { text: "Formation shapes"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 15 }
                Item { Layout.fillWidth: true }
                AppButton { text: "Advanced…"; flat: true; onClicked: { shapePalette.close(); window.shapeDrawing = ""; formationDialog.open() } }
            }
            GridLayout {
                columns: 4; columnSpacing: 6; rowSpacing: 6; Layout.fillWidth: true; Layout.fillHeight: true
                Repeater {
                    model: [{kind:"line",label:"Line"},{kind:"rectangle",label:"Rectangle"},{kind:"circle",label:"Circle"},{kind:"triangle",label:"Triangle"},{kind:"arc",label:"Arc"},{kind:"ellipse",label:"Ellipse"},{kind:"diamond",label:"Diamond"},{kind:"block",label:"Block"}]
                    delegate: AppButton {
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentItem: ColumnLayout {
                            spacing: 5
                            ShapeIcon { kind: modelData.kind; iconColor: parent.parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled; Layout.alignment: Qt.AlignHCenter }
                            Label { text: modelData.label; color: MarchCraftTheme.textPrimary; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                        }
                        highlighted: window.shapeDrawing === modelData.kind
                        onClicked: { window.setShapeTool(modelData.kind); shapePalette.close() }
                    }
                }
            }
        }
    }

    Popup {
        id: editorActionsPopup
        x: window.width - width - 16; y: 52; width: 220; padding: 8; focus: true
        background: Rectangle { color: MarchCraftTheme.surfaceRaised; radius: MarchCraftTheme.radiusLarge; border.color: MarchCraftTheme.dividerStrong }
        contentItem: ColumnLayout {
            spacing: 4
            AppButton { text: "New project…"; flat: true; Layout.fillWidth: true; onClicked: { editorActionsPopup.close(); window.startNewProject() } }
            AppButton { text: "Project setup…"; flat: true; Layout.fillWidth: true; onClicked: { editorActionsPopup.close(); projectSetupDialog.creationMode = false; projectSetupDialog.open() } }
            AppButton { text: "Preferences…"; flat: true; Layout.fillWidth: true; onClicked: { editorActionsPopup.close(); preferencesDialog.open() } }
        }
    }

    Popup {
        id: timelineActionsPopup
        x: Math.min(window.width - width - 16, timelinePanel.exposedTimelineActionsButton.mapToItem(window.contentItem, 0, 0).x)
        y: Math.min(window.height - height - 16, timelinePanel.exposedTimelineActionsButton.mapToItem(window.contentItem, 0, timelinePanel.exposedTimelineActionsButton.height).y)
        width: 220; padding: 8; focus: true
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: MarchCraftTheme.motionMedium } }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: MarchCraftTheme.motionFast } }
        background: Rectangle { color: MarchCraftTheme.surfaceRaised; radius: MarchCraftTheme.radiusLarge; border.color: MarchCraftTheme.dividerStrong }
        contentItem: ColumnLayout {
            spacing: 4
            AppButton { text: "New variant…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); variantDialog.open() } }
            AppButton { text: "New set…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); setDialog.editing = false; setDialog.open() } }
            AppButton { text: "Edit current set…"; flat: true; enabled: drillProject.setCount > 0; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); setDialog.editing = true; setDialog.open() } }
            AppButton { text: "Add sets in batch…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); batchSetDialog.open() } }
            AppButton { text: drillProject.currentVariantCount > 1 ? "Archive variant" : "Archive set"; flat: true; enabled: drillProject.setCount > 1 || drillProject.currentVariantCount > 1; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); drillProject.archiveCurrentVariant() } }
            AppButton { text: "Open archive…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); archiveDialog.open() } }
        }
    }

    WelcomeWorkspace {
        id: homePage
        drillProjectContext: drillProject
        qaModeContext: qaMode
        windowContext: window
        workspaceControllerContext: workspaceController
        workspaceStateContext: workspaceState
    }

    SplitView {
        id: horizontalSplit
        enabled: workspaceState.workspaceActive
        visible: opacity > 0.01
        opacity: workspaceState.workspaceActive ? 1 : 0
        transform: Translate {
            y: workspaceState.workspaceActive ? 0 : 8
            Behavior on y { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
        anchors.fill: parent
        anchors.margins: 6
        orientation: Qt.Horizontal
        Component.onCompleted: if (workspaceSettings.horizontalSplitState) restoreState(workspaceSettings.horizontalSplitState)
        onResizingChanged: if (!resizing) workspaceSettings.horizontalSplitState = saveState()
        handle: Rectangle {
            implicitWidth: 6; color: SplitHandle.pressed ? "#5b8def" : SplitHandle.hovered ? "#3b4b5f" : "#293443"
            TapHandler { onDoubleTapped: {
                rosterPanel.SplitView.preferredWidth = 220
                inspectorPanel.SplitView.preferredWidth = 270
                workspaceSettings.horizontalSplitState = undefined
            } }
        }

    RosterPanel {
        id: rosterPanel
        drillProjectContext: drillProject
        fieldContextMenuContext: fieldContextMenu
        inspectorContext: inspectorPanel.exposedInspector
        performerDialogContext: performerDialog
        windowContext: window
        workspaceSettingsContext: workspaceSettings
    }

        SplitView {
            id: verticalSplit
            SplitView.fillWidth: true
            SplitView.minimumWidth: 520
            orientation: Qt.Vertical
            Component.onCompleted: if (workspaceSettings.verticalSplitState) restoreState(workspaceSettings.verticalSplitState)
            onResizingChanged: if (!resizing) workspaceSettings.verticalSplitState = saveState()
            handle: Rectangle {
                implicitHeight: 8; color: SplitHandle.pressed ? "#5b8def" : SplitHandle.hovered ? "#3b4b5f" : "#293443"
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
                    shapeDrawMode: window.shapeDrawing
                    showPaths: drillProject.showTransitionPaths
                    showShapeGuides: drillProject.showShapeGuides
                    onPerformerActivated: function(row) { window.activePerformer = row; inspectorPanel.exposedInspector.refresh() }
                    onContextMenuRequested: function(screenX, screenY, performerRow) {
                        window.activePerformer = performerRow
                        if (performerRow >= 0) inspectorPanel.exposedInspector.refresh()
                        fieldContextMenu.popup(screenX, screenY)
                    }
                    onFreehandCompleted: function(points) {
                        drillProject.requestFreehandPreview(points, window.freehandMovementMode,
                                                            window.freehandCreateGroup, window.freehandRecognitionMode)
                        window.freehandDrawing = false
                        freehandPreviewDialog.applied = false
                        freehandPreviewDialog.open()
                    }
                    onShapeCompleted: function(kind, start, end) {
                        if (kind === "line") {
                            drillProject.distributeLine(start.x, start.y, end.x, end.y)
                        } else if (kind === "rectangle") {
                            drillProject.distributeRectangle(Math.min(start.x, end.x), Math.min(start.y, end.y),
                                                             Math.abs(end.x - start.x), Math.abs(end.y - start.y))
                        } else if (kind === "triangle") {
                            drillProject.createFormation("triangle", {
                                centerX: (start.x + end.x) / 2,
                                centerY: (start.y + end.y) / 2,
                                width: Math.max(2, Math.max(Math.abs(end.x - start.x), Math.abs(end.y - start.y)))
                            })
                        } else if (kind === "ellipse" || kind === "diamond" || kind === "polygon" || kind === "star" || kind === "spiral" || kind === "block") {
                            const width = Math.max(2, Math.abs(end.x - start.x))
                            const height = Math.max(2, Math.abs(end.y - start.y))
                            const centerX = (start.x + end.x) / 2, centerY = (start.y + end.y) / 2
                            const size = Math.max(width, height)
                            if (kind === "ellipse") drillProject.createFormation("ellipse", { centerX: centerX, centerY: centerY, width: width, height: height })
                            else if (kind === "diamond") drillProject.createFormation("diamond", { centerX: centerX, centerY: centerY, width: size })
                            else if (kind === "polygon") drillProject.createFormation("polygon", { centerX: centerX, centerY: centerY, width: size, sides: 6 })
                            else if (kind === "star") drillProject.createFormation("star", { centerX: centerX, centerY: centerY, width: size, points: 5 })
                            else if (kind === "spiral") drillProject.createFormation("spiral", { centerX: centerX, centerY: centerY, width: size, outerRadius: size / 2, turns: 1.5 })
                            else drillProject.createFormation("block", { centerX: centerX, centerY: centerY, rows: Math.max(1, Math.ceil(Math.sqrt(drillProject.selectedCount))), spacing: Math.max(1, Math.min(width, height) / Math.max(1, Math.ceil(Math.sqrt(drillProject.selectedCount)))) })
                        } else {
                            const radius = Math.hypot(end.x - start.x, end.y - start.y)
                            if (kind === "circle") drillProject.distributeArc(start.x, start.y, radius, 0, 360)
                            else {
                                const heading = Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI
                                drillProject.distributeArc(start.x, start.y, radius, heading - 90, heading + 90)
                            }
                        }
                    }
                    onShapeDrawingCanceled: { window.shapeDrawing = ""; window.freehandDrawing = false }
                }
                ThreeDView { id: threeDView }
            }

    SetTimeline {
        id: timelinePanel
        archiveDialogContext: archiveDialog
        audioDialogContext: audioDialog
        batchSetDialogContext: batchSetDialog
        drillProjectContext: drillProject
        midiDialogContext: midiDialog
        musicXmlDialogContext: musicXmlDialog
        renumberDialogContext: renumberDialog
        setContextMenuContext: setContextMenu
        setDialogContext: setDialog
        timelineActionsPopupContext: timelineActionsPopup
        timingDialogContext: timingDialog
        transportContext: transport
        variantDialogContext: variantDialog
        verticalSplitContext: verticalSplit
        windowContext: window
        workspaceSettingsContext: workspaceSettings
    }
        }

    InspectorPanel {
        id: inspectorPanel
        bulkEditDialogContext: bulkEditDialog
        drillProjectContext: drillProject
        formationDialogContext: formationDialog
        performerDialogContext: performerDialog
        uniformColorDialogContext: uniformColorDialog
        windowContext: window
        workspaceSettingsContext: workspaceSettings
    }
    }

    footer: ToolBar {
        visible: workspaceState.workspaceActive
        height: visible ? 28 : 0
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10; anchors.rightMargin: 10
            Label { text: drillProject.statusMessage; color: "#a5afbc"; font.pixelSize: 11; Layout.fillWidth: true }
            Label { text: drillProject.selectedCount + " selected"; color: "#a5afbc"; font.pixelSize: 11 }
            Label { text: window.threeD ? "3D PREVIEW" : "2D EDITOR"; color: "#5b8def"; font.bold: true; font.pixelSize: 10 }
        }
    }

    FormationDialog {
        id: formationDialog
        anchors.centerIn: Overlay.overlay
        onSettingsRequested: settingsDialog.open()
    }

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
            Label { text: "The stroke is smoothed, spaced by equal arc length, expanded if necessary, and shifted locally to avoid unselected performers."; color: "#a5afbc"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            AppButton { text: "Start drawing"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { window.shapeDrawing="";window.freehandRecognitionMode=recognitionMode.currentValue;window.freehandMovementMode=movementMode.currentValue;window.freehandDrawing=true;freehandDialog.close() } }
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
                Label { text: "Average move"; color: "#a5afbc" }
                Label { text: drillProject.formatDistance(parent.metrics.averageMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum move"; color: "#a5afbc" }
                Label { text: drillProject.formatDistance(parent.metrics.maximumMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum steps / count"; color: "#a5afbc" }
                Label { text: Number(parent.metrics.maximumStepsPerCount || 0).toFixed(2); Layout.alignment: Qt.AlignRight }
            }
            RowLayout { Layout.alignment: Qt.AlignRight
                AppButton { text: "Cancel"; onClicked: freehandPreviewDialog.close() }
                AppButton { text: "Apply"; highlighted: true; enabled: drillProject.formationPreviewActive; onClicked: { freehandPreviewDialog.applied = drillProject.commitFormationPreview(); freehandPreviewDialog.close() } }
            }
        }
    }

    AppToolButton {
        id: rosterRevealButton
        visible: workspaceSettings.rosterCollapsed
        z: 20
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 12
        width: 24
        height: 42
        text: "›"
        font.pixelSize: 17
        ToolTip.text: "Show rosterPanel.exposedRoster (Ctrl+Shift+R)"
        ToolTip.visible: hovered
        onClicked: workspaceSettings.rosterCollapsed = false
        background: Rectangle {
            color: rosterRevealButton.hovered ? "#24343d" : "#151d27"
            radius: 0
            border.color: rosterRevealButton.hovered ? "#526772" : "#293443"
        }
    }

    Menu {
        id: fieldContextMenu
        AppMenuItem { text: "Group"; enabled: drillProject.canGroupSelection; onTriggered: drillProject.groupSelected() }
        MenuSeparator {}
        AppMenuItem { text: "Remove from group"; enabled: drillProject.canRemoveSelectionFromGroup; onTriggered: drillProject.removeSelectedFromGroup() }
        AppMenuItem { text: "Ungroup"; enabled: drillProject.canUngroupSelection; onTriggered: drillProject.ungroupSelected() }
        MenuSeparator {}
        AppMenuItem { text: "Snap to 1-step grid"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.snapSelected(1) }
        AppMenuItem { text: "Mirror side-to-side"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(true) }
        AppMenuItem { text: "Auto-label selected"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.autoLabel("P") }
    }

    Menu {
        id: setContextMenu; property int setIndex: -1
        AppMenuItem { text: "Edit set"; onTriggered: { drillProject.currentSetIndex=setContextMenu.setIndex; setDialog.editing=true; setDialog.open() } }
        AppMenuItem { text: "Copy set"; onTriggered: { drillProject.duplicateSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Add before"; onTriggered: { drillProject.insertSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Add after"; onTriggered: { drillProject.insertSetAt(setContextMenu.setIndex+1); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Create variant"; onTriggered: { drillProject.currentSetIndex=setContextMenu.setIndex; variantDialog.open() } }
        MenuSeparator {}
        AppMenuItem { text: "Delete set"; enabled: drillProject.setCount>1; onTriggered: { drillProject.archiveSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
    }

    Dialog {
        id: unsavedChangesDialog
        title: "Save changes?"
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 470
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            spacing: 16
            Label {
                text: "“" + drillProject.showName + "” has changes that have not been saved."
                color: MarchCraftTheme.textPrimary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: "Save the project before continuing, or discard the current changes."
                color: MarchCraftTheme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: "Cancel"; onClicked: { workspaceState.cancel(); unsavedChangesDialog.close() } }
                Item { Layout.fillWidth: true }
                AppButton { text: "Discard"; onClicked: { unsavedChangesDialog.close(); workspaceState.confirmDiscard() } }
                AppButton { text: "Save"; highlighted: true; onClicked: { unsavedChangesDialog.close(); window.saveCurrentProject("pending") } }
            }
        }
    }

    Dialog {
        id: renumberDialog; title: "Renumber set labels?"; modal: true; anchors.centerIn: Overlay.overlay; width: 460
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            Label { text: "Renumber full sets 1, 2, 3… and subsets 1A, 1B…?"; wrapMode: Text.Wrap }
            RowLayout { Layout.alignment: Qt.AlignRight
                AppButton { text: "Keep labels"; onClicked: renumberDialog.close() }
                AppButton { text: "Renumber"; highlighted: true; onClicked: { drillProject.renumberSets(); renumberDialog.close() } }
            }
        }
    }

    ProjectSettingsDialog {
        id: projectSetupDialog
        drillProjectContext: drillProject
        workspaceStateContext: workspaceState
    }

    EditorPreferences {
        id: settingsDialog
        drillProjectContext: drillProject
        fieldViewContext: fieldView
        windowContext: window
        workspaceControllerContext: workspaceController
    }

    PerformerDialog {
        id: performerDialog
        assetCatalogContext: assetCatalog
        drillProjectContext: drillProject
        inspectorContext: inspectorPanel.exposedInspector
        windowContext: window
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
            AppButton { text: "Create performers"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { drillProject.batchAddPerformers(batchPrefix.text, batchCount.value, batchInstrument.text, batchSection.text); batchDialog.close() } }
        }
    }

    Dialog {
        id: bulkEditDialog
        title: "Bulk edit selected performers"
        modal: true; anchors.centerIn: Overlay.overlay; width: 430
        contentItem: ColumnLayout {
            Label { text: "Leave text fields blank to preserve mixed values."; color: "#a5afbc" }
            TextField { id: bulkInstrument; placeholderText: "Instrument (unchanged)"; Layout.fillWidth: true }
            TextField { id: bulkSection; placeholderText: "Section (unchanged)"; Layout.fillWidth: true }
            TextField { id: bulkColor; placeholderText: "Color, e.g. #38bdf8 (unchanged)"; Layout.fillWidth: true }
            CheckBox { id: bulkFacingEnabled; text: "Change facing for this set" }
            ComboBox {
                id: bulkFacing; Layout.fillWidth: true; enabled: bulkFacingEnabled.checked
                textRole: "text"; valueRole: "value"
                model: [{text:"Front field (0 deg)",value:0},{text:"Side 2 (90 deg)",value:90},{text:"Back field (180 deg)",value:180},{text:"Side 1 (270 deg)",value:270}]
            }
            CheckBox { id: bulkVisibilityEnabled; text: "Change visibility" }
            CheckBox { id: bulkVisible; text: "Visible"; checked: true; enabled: bulkVisibilityEnabled.checked }
            CheckBox { id: bulkLockEnabled; text: "Change locking" }
            CheckBox { id: bulkLocked; text: "Locked"; enabled: bulkLockEnabled.checked }
            AppButton {
                text: "Apply to " + drillProject.selectedCount + " performers"; highlighted: true; Layout.alignment: Qt.AlignRight
                onClicked: {
                    drillProject.updateSelectedPerformers(bulkInstrument.text, bulkSection.text, bulkColor.text,
                                                          bulkFacingEnabled.checked ? bulkFacing.currentValue : Number.NaN,
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
            const defaultNumber = s.number || String(drillProject.setCount + 1)
            setName.text = !s.name || /^Set \\d+[A-Z]?$/i.test(s.name) || s.name === "New set" ? "Set " + defaultNumber : s.name
            setCaption.text = s.caption || ""
            setMeasure.text = s.measure || ""
            setCounts.value = s.opening ? drillProject.openingCounts : (s.counts || 8)
            openingBehavior.currentIndex = openingBehavior.indexOfValue(s.openingBehavior || drillProject.openingBehavior)
            setSubset.checked = s.subset || false
        }
        contentItem: ColumnLayout {
            Label { text: "Set number" }
            TextField { id: setNumber; Layout.fillWidth: true; placeholderText: "1A" }
            Label { text: "Set name (double-click a set card to customize)" }
            TextField { id: setName; Layout.fillWidth: true }
            Label { text: "Caption" }
            TextField { id: setCaption; Layout.fillWidth: true; placeholderText: "Optional description" }
            Label { text: "Measures" }
            TextField { id: setMeasure; Layout.fillWidth: true; placeholderText: "17–20" }
            Label { text: "Opening behavior"; visible: setDialog.editing && drillProject.currentSetIndex === 0 }
            ComboBox { id: openingBehavior; visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Hold at Set 1",value:"hold"},{text:"Start moving immediately",value:"move"}] }
            Label { text: setDialog.editing && drillProject.currentSetIndex === 0 ? "Opening hold counts" : "Counts"; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            SpinBox { id: setCounts; from: 1; to: 2048; editable: true; Layout.fillWidth: true; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            Label { visible: !(setDialog.editing && drillProject.currentSetIndex === 0); Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#a5afbc"; text: "For a hold, use two consecutive sets with identical coordinates and enter the hold duration here." }
            Label { visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#a5afbc"; text: openingBehavior.currentValue === "hold" ? "The entire ensemble remains at Set 1 for these counts before the show clock and music begin moving." : "Playback begins the Set 1 to Set 2 transition immediately, with no opening standstill." }
            CheckBox { id: setSubset; text: "This is a subset" }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    visible: setDialog.editing
                    text: drillProject.currentVariantCount > 1 ? "Archive variant" : "Archive set"
                    enabled: drillProject.setCount > 1 || drillProject.currentVariantCount > 1
                    onClicked: { drillProject.archiveCurrentVariant(); setDialog.close() }
                }
                AppButton {
                    visible: setDialog.editing && drillProject.currentVariantCount > 1
                    text: "Archive entire set"
                    enabled: drillProject.setCount > 1
                    onClicked: { drillProject.archiveCurrentSet(); setDialog.close() }
                }
                Item { Layout.fillWidth: true }
                AppButton { text: setDialog.editing ? "Save" : "Add after current"; highlighted: true; onClicked: { if (setDialog.editing) { drillProject.updateCurrentSet(setNumber.text, setName.text, setCaption.text, setMeasure.text, setCounts.value, setSubset.checked); if (drillProject.currentSetIndex === 0) drillProject.setOpeningBehavior(openingBehavior.currentValue, setCounts.value) } else drillProject.addSet(setName.text, setCounts.value, setSubset.checked); setDialog.close() } }
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
                color: "#a5afbc"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            AppButton {
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
            Label { text: "Applies to the selected transition"; color: "#a5afbc" }
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
                color: "#a5afbc"
            }
            AppButton {
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
            Label { text: "ARCHIVED SETS"; color: "#a5afbc"; font.bold: true }
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
                        Label { text: info.caption || info.measure; color: "#a5afbc"; Layout.fillWidth: true; elide: Text.ElideRight }
                        AppButton { text: "Restore"; onClicked: drillProject.restoreArchivedSet(index) }
                        AppButton {
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
            Label { text: "ARCHIVED VARIANTS IN CURRENT SET"; color: "#a5afbc"; font.bold: true }
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
                        Label { text: info.caption; color: "#a5afbc"; Layout.fillWidth: true; elide: Text.ElideRight }
                        AppButton { text: "Restore"; onClicked: drillProject.restoreArchivedVariant(index) }
                        AppButton {
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
            AppButton { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: archiveDialog.close() }
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
            AppButton { text: "Create sets"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { drillProject.batchAddSets(batchSetCount.value, batchSetCounts.value); batchSetDialog.close() } }
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

    Dialog {
        id: historyDialog
        title: "Restore saved version"
        modal: true; anchors.centerIn: Overlay.overlay; width: 520
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Restoring a version keeps the current project open and can be undone before saving."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: MarchCraftTheme.textSecondary }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: 280; clip: true
                model: drillProject.projectHistory
                delegate: RowLayout {
                    required property var modelData
                    width: ListView.view.width; height: 42
                    Label { text: modelData.timestamp; Layout.fillWidth: true; color: MarchCraftTheme.textPrimary }
                    AppButton { text: "Restore"; onClicked: { if (drillProject.restoreHistoryVersion(index)) historyDialog.close() } }
                }
            }
            AppButton { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: historyDialog.close() }
        }
    }

    Dialog {
        id: diagnosticsDialog
        title: drillProject.diagnostics.length > 0 ? drillProject.diagnostics[0].operation : "Operation failed"
        modal: true; anchors.centerIn: Overlay.overlay; width: 520
        standardButtons: Dialog.Ok
        contentItem: Label {
            width: 460; wrapMode: Text.Wrap; color: MarchCraftTheme.textPrimary
            text: drillProject.diagnostics.length > 0
                ? drillProject.diagnostics[0].message + "\n\nLocation: " + drillProject.diagnostics[0].location + "\nSource: " + drillProject.diagnostics[0].source
                : ""
        }
    }

    Connections {
        target: drillProject
        function onDiagnosticsChanged() { if (drillProject.diagnostics.length > 0 && drillProject.diagnostics[0].severity === "error") diagnosticsDialog.open() }
    }

    FileDialog { id: openDialog; title: "Open MarchCraft project"; nameFilters: ["MarchCraft projects (*.marchcraft *.drill)"]; onAccepted: window.openProjectPath(selectedFile) }
    FileDialog {
        id: saveDialog
        title: "Save MarchCraft project"
        fileMode: FileDialog.SaveFile
        nameFilters: ["MarchCraft database (*.marchcraft)"]
        defaultSuffix: "marchcraft"
        onAccepted: {
            if (!drillProject.saveProject(selectedFile)) return
            workspaceController.recordRecentProject(drillProject.projectPath, drillProject.showName)
            if (window.savePurpose === "pending") workspaceState.confirmAfterSave()
            window.savePurpose = "normal"
        }
        onRejected: window.savePurpose = "normal"
    }
    FileDialog { id: importCoordinateDialog; title: "Import coordinate data"; nameFilters: ["Coordinate JSON (*.json)"]; onAccepted: drillProject.importCoordinateJson(selectedFile) }
    FileDialog { id: midiDialog; title: "Import MIDI score"; nameFilters: ["MIDI (*.mid *.midi)"]; onAccepted: { timelinePanel.exposedTimelineTabs.currentIndex = 1; drillProject.importMidiAsync(selectedFile) } }
    FileDialog { id: musicXmlDialog; title: "Import MusicXML score"; nameFilters: ["MusicXML (*.musicxml *.xml)"]; onAccepted: { timelinePanel.exposedTimelineTabs.currentIndex = 1; drillProject.importMusicXml(selectedFile) } }
    FileDialog { id: audioDialog; title: "Attach rehearsal audio"; nameFilters: ["Audio (*.wav *.mp3 *.m4a *.flac)"]; onAccepted: drillProject.attachAudio(selectedFile) }
    FileDialog { id: csvDialog; title: "Export analytics"; fileMode: FileDialog.SaveFile; nameFilters: ["CSV (*.csv)"]; defaultSuffix: "csv"; onAccepted: drillProject.exportCsv(selectedFile) }
    FileDialog { id: pdfDialog; title: "Export coordinate sheets"; fileMode: FileDialog.SaveFile; nameFilters: ["PDF (*.pdf)"]; defaultSuffix: "pdf"; onAccepted: drillProject.exportCoordinatePdf(selectedFile) }
    ColorDialog {
        id: uniformColorDialog
        title: "Choose marker color"
        onAccepted: {
            drillProject.setPerformerColor(window.activePerformer, selectedColor.toString())
            inspectorPanel.exposedInspector.refresh()
        }
    }

    Shortcut { sequence: "Left"; onActivated: drillProject.nudgeSelected(-0.25, 0) }
    Shortcut { sequence: "Right"; onActivated: drillProject.nudgeSelected(0.25, 0) }
    Shortcut { sequence: "Up"; onActivated: drillProject.nudgeSelected(0, -0.25) }
    Shortcut { sequence: "Down"; onActivated: drillProject.nudgeSelected(0, 0.25) }
    Shortcut { sequence: "Ctrl+Space"; context: Qt.ApplicationShortcut; onActivated: transport.playPause() }
    Shortcut { sequence: "Ctrl+,"; context: Qt.ApplicationShortcut; onActivated: projectSetupDialog.open() }
    Shortcut { sequence: "Ctrl+L"; context: Qt.ApplicationShortcut; onActivated: { rosterPanel.exposedRosterSearch.forceActiveFocus(); rosterPanel.exposedRosterSearch.selectAll() } }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: window.threeD = false }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: window.threeD = true }
    Shortcut { sequence: "Ctrl+Shift+R"; context: Qt.ApplicationShortcut; onActivated: workspaceSettings.rosterCollapsed = !workspaceSettings.rosterCollapsed }
    Shortcut { sequence: "Ctrl+Shift+I"; context: Qt.ApplicationShortcut; onActivated: workspaceSettings.inspectorCollapsed = !workspaceSettings.inspectorCollapsed }
    Shortcut { sequence: "Ctrl+Shift+T"; context: Qt.ApplicationShortcut; onActivated: workspaceSettings.timelineCollapsed = !workspaceSettings.timelineCollapsed }
    Shortcut { sequence: "+"; context: Qt.ApplicationShortcut; onActivated: fieldView.zoom = Math.min(3.5, fieldView.zoom * 1.12) }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: fieldView.zoom = Math.max(0.7, fieldView.zoom * 0.89) }
}
