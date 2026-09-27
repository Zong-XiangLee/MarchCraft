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

    function showQaExport(preset) {
        exportPanel.enter()
        if(preset === "csv")exportPanel.openFor("charts","csv")
        else exportPanel.applyPreset(preset)
    }
    function showQaSurface(surface) {
        if (surface === "export-dialog") { exportPanel.openFor("charts", "pdf"); return }
        if (surface === "preferences") settingsDialog.open()
        else if (surface === "performer") { performerDialog.editing = false; performerDialog.open() }
        else if (surface === "formation") { drillProject.selectAll(); formationDialog.open() }
        else if (surface === "music") timelinePanel.showMusic()
        else if (surface === "roster-workspace") requestWorkspace("roster")
        else if (surface === "music-workspace") requestWorkspace("music")
        else if (surface === "review-workspace") {
            if (drillProject.setCount > 1) transport.editSet(1)
            drillProject.scanShow()
            requestWorkspace("review")
        }
        else if (surface === "export-workspace") requestWorkspace("export")
    }
    function showQaTimeline(mode) {
        timelinePanel.showQaTimelineMode(mode)
    }
    function showQaEditor(mode) {
        requestWorkspace("editor")
        workspaceSettings.toolbarMode = mode === "expanded" ? "expanded" : "compact"
        workspaceSettings.rosterCollapsed = true
        workspaceSettings.inspectorCollapsed = false
        workspaceSettings.timelineCollapsed = false
        workspaceSettings.inspectorAnalyticsExpanded = false
        workspaceSettings.inspectorClinicExpanded = false
        workspaceSettings.showPreviousFormation = mode === "previous"
        drillProject.currentSetIndex = Math.min(1, drillProject.setCount - 1)
        drillProject.clearSelection()
        if (mode === "clean" || mode === "expanded") return
        for (let row = 0; row < Math.min(6, drillProject.performerCount); ++row)
            drillProject.selectPerformerMode(row, 1)
        if (mode === "paste") {
            drillProject.copySelectedFormation()
            drillProject.currentSetIndex = Math.min(2, drillProject.setCount - 1)
            Qt.callLater(function() { pasteSpecialDialog.open() })
        } else if (mode === "context") {
            fieldContextMenu.performerRow = 0
            Qt.callLater(function() { fieldContextMenu.popup(window.width * 0.43, window.height * 0.34) })
        } else if (mode === "editMenu") {
            Qt.callLater(function() { editMenu.open() })
        } else if (mode === "viewMenu") {
            Qt.callLater(function() { viewMenu.open() })
        }
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
    property string qaEditorState: ""
    readonly property bool editorSurfaceActive: workspaceState.workspaceActive
                                                && (workspaceState.currentWorkspace === "editor"
                                                    || workspaceState.currentWorkspace === "review")

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
        transport.pause()
        homeMode = "dashboard"
        workspaceState.workspaceActive = false
        workspaceController.refreshRecovery()
        Qt.callLater(function() { (homePage.exposedResumeButton.visible ? homePage.exposedResumeButton : homePage.exposedNewProjectHomeButton).forceActiveFocus() })
    }

    function activateWorkspace(workspace) {
        if (!workspaceState.isWorkspace(workspace)) return
        if (workspaceState.currentWorkspace !== workspace) transport.pause()
        workspaceState.switchWorkspace(workspace)
        if (workspace !== "export" && drillProject.projectPath)
            workspaceController.rememberWorkspace(drillProject.projectPath, workspace)
    }

    function requestWorkspace(workspace) {
        if (!workspaceState.hasCurrentProject) return
        if (!workspaceState.workspaceActive)
            workspaceState.enteredProject(workspace)
        if (workspace === "export") {
            exportPanel.enter()
            return
        }
        activateWorkspace(workspace)
    }

    function resumeProject() {
        workspaceState.enteredProject(workspaceState.lastUsefulWorkspace)
    }

    function createConfiguredProject(name, fieldPreset, lightingPreset, rosterRows, destination) {
        drillProject.newProject()
        drillProject.showName = name
        drillProject.fieldPreset = fieldPreset
        drillProject.lightingPreset = lightingPreset
        if (rosterRows && rosterRows.length > 0) drillProject.batchCreateRoster(rosterRows)
        homeMode = "dashboard"
        exportPanel.resetForProject()
        workspaceState.enteredProject(destination || "editor")
    }

    function createQuickProject(name, fieldPreset, lightingPreset, performerCount) {
        var rows = performerCount > 0
            ? [{section: "Ensemble", prefix: "P", count: performerCount, instrument: "Unassigned"}]
            : []
        createConfiguredProject(name, fieldPreset, lightingPreset, rows, "editor")
    }

    function createGuidedProject(name, fieldPreset, lightingPreset, rosterRows, destination) {
        createConfiguredProject(name, fieldPreset, lightingPreset, rosterRows, destination)
    }

    function recoverProject() {
        if (!workspaceController.recoveryAvailable || !drillProject.loadRecoveryProject(workspaceController.recoveryPath)) return
        exportPanel.resetForProject()
        workspaceController.refreshRecovery()
        homeMode = "dashboard"
        workspaceState.enteredProject("editor")
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
        exportPanel.resetForProject()
        workspaceState.enteredProject(workspaceController.workspaceForProject(drillProject.projectPath))
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
            exportPanel.resetForProject()
            workspaceState.enteredProject("editor")
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
            workspaceController.rememberWorkspace(drillProject.projectPath, workspaceState.lastUsefulWorkspace)
            workspaceController.refreshRecovery()
            if (savePurpose === "pending") workspaceState.confirmAfterSave()
        }
    }

    Settings {
        id: workspaceSettings
        property string themeId: "graphite"
        property var horizontalSplitState
        property var verticalSplitState
        property bool compactTimelineDefaultApplied: false
        property bool rosterCollapsed: true
        property bool inspectorCollapsed: false
        property bool timelineCollapsed: false
        property bool timelineMaximized: false
        property string toolbarMode: "compact"
        property bool showTransformTools: true
        property bool showViewTools: false
        property bool showPreviousFormation: false
        property real previousFormationOpacity: 0.28
        property bool showLabels: true
        property bool snapEnabled: true
        property real snapGrid: 1.0
        property bool inspectorAnalyticsExpanded: false
        property bool inspectorClinicExpanded: false
        property var quickShapes: ["line", "rectangle", "circle", "triangle"]
        property int workspaceChromeRevision: 0
    }

    property bool previewTheme: false
    function setQaTheme(theme) {
        if (["graphite", "midnight", "warm"].indexOf(theme) < 0) return
        previewTheme = true
        MarchCraftTheme.themeId = theme
    }
    Connections {
        target: MarchCraftTheme
        function onThemeIdChanged() {
            if (!window.previewTheme) workspaceSettings.themeId = MarchCraftTheme.themeId
        }
    }

    Component.onCompleted: {
        // The dedicated Roster workspace owns roster management. Existing
        // profiles get the cleaner editor layout once, while the optional
        // performer picker remains available from View or Ctrl+Shift+R.
        if (workspaceSettings.workspaceChromeRevision < 1) {
            workspaceSettings.rosterCollapsed = true
            workspaceSettings.horizontalSplitState = undefined
            workspaceSettings.workspaceChromeRevision = 1
        }
        MarchCraftTheme.themeId = workspaceSettings.themeId
        workspaceController.refreshRecentProjects()
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
            timelinePanel.showDragPreview()
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
        drillProject.cancelFormationPreview()
        threeD = false
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
    function formationDialogForToolbar() { formationDialog.open() }

    palette {
        window: MarchCraftTheme.panelHeader
        windowText: MarchCraftTheme.textPrimary
        base: MarchCraftTheme.input
        alternateBase: MarchCraftTheme.surfaceRaised
        text: MarchCraftTheme.textPrimary
        button: MarchCraftTheme.surfaceRaised
        buttonText: MarchCraftTheme.textPrimary
        highlight: MarchCraftTheme.accent
        highlightedText: "#ffffff"
        mid: MarchCraftTheme.divider
    }
    onQaShapePaletteChanged: if (qaShapePalette) Qt.callLater(function() { shapePalette.open() })
    onQaEditorStateChanged: if (qaEditorState.length > 0) Qt.callLater(function() { showQaEditor(qaEditorState) })

    Action { id: undoAction; text: "Undo"; shortcut: "Ctrl+Z"; enabled: drillProject.canUndo; onTriggered: drillProject.undo() }
    Action { id: redoAction; text: "Redo"; shortcut: "Ctrl+Y"; enabled: drillProject.canRedo; onTriggered: drillProject.redo() }
    Action {
        id: cutFormationAction; text: "Cut formation"; shortcut: "Ctrl+X"
        enabled: workspaceState.currentWorkspace === "editor"
                 && drillProject.selectedCount > 0 && drillProject.currentSetIndex > 0
        onTriggered: drillProject.cutSelectedFormation()
    }
    Action {
        id: copyFormationAction; text: drillProject.selectedCount > 0 ? "Copy selection" : "Copy formation"
        shortcut: "Ctrl+C"
        enabled: workspaceState.currentWorkspace === "editor" && drillProject.setCount > 0
        onTriggered: drillProject.copyFormation()
    }
    Action {
        id: pasteFormationAction; text: "Paste formation"; shortcut: "Ctrl+V"
        enabled: workspaceState.currentWorkspace === "editor" && drillProject.hasFormationClipboard
        onTriggered: drillProject.pasteFormation()
    }
    Action {
        id: pasteSpecialAction; text: "Paste Special…"; shortcut: "Ctrl+Shift+V"
        enabled: workspaceState.currentWorkspace === "editor" && drillProject.hasFormationClipboard
        onTriggered: pasteSpecialDialog.open()
    }
    Action { id: selectAllAction; text: "Select all"; shortcut: "Ctrl+A"; enabled: workspaceState.currentWorkspace === "editor"; onTriggered: drillProject.selectAll() }
    Action { id: clearSelectionAction; text: "Clear selection"; shortcut: "Esc"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.selectedCount > 0; onTriggered: drillProject.clearSelection() }
    Action { id: deleteSelectedAction; text: "Delete performers"; shortcut: "Delete"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.selectedCount > 0; onTriggered: drillProject.removeSelectedPerformers() }
    Action { id: groupAction; text: "Group selection"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.canGroupSelection; onTriggered: drillProject.groupSelected() }
    Action { id: ungroupAction; text: "Ungroup selection"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.canUngroupSelection; onTriggered: drillProject.ungroupSelected() }
    Action { id: removeFromGroupAction; text: "Remove from group"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.canRemoveSelectionFromGroup; onTriggered: drillProject.removeSelectedFromGroup() }
    Action { id: previousSetAction; text: "Previous set"; shortcut: "PgUp"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.currentSetIndex > 0; onTriggered: transport.previousSet() }
    Action { id: nextSetAction; text: "Next set"; shortcut: "PgDown"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.currentSetIndex + 1 < drillProject.setCount; onTriggered: transport.nextSet() }
    Action { id: duplicateSetAction; text: "Duplicate current set"; shortcut: "Ctrl+D"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.setCount > 0; onTriggered: { drillProject.duplicateCurrentSet(); if (drillProject.setLabelsNeedRenumbering()) renumberDialog.open() } }
    Action { id: formationBuilderAction; text: "Formation builder…"; enabled: workspaceState.currentWorkspace === "editor" && drillProject.selectedCount > 0; onTriggered: formationDialog.open() }
    Action {
        id: transitionEditAction; text: drillProject.transitionEditActive ? "Apply transition paths" : "Edit incoming transition…"
        enabled: workspaceState.currentWorkspace === "editor"
                 && (drillProject.transitionEditActive
                     || (drillProject.selectedCount > 0 && drillProject.currentSetIndex > 0))
        onTriggered: drillProject.transitionEditActive ? drillProject.applyTransitionEdit() : drillProject.beginTransitionEdit()
    }
    Action { id: view2DAction; text: "2D drill editor"; enabled: workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review"; checkable: true; checked: !window.threeD; shortcut: "Ctrl+1"; onTriggered: window.threeD = false }
    Action { id: view3DAction; text: "3D preview"; enabled: workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review"; checkable: true; checked: window.threeD; shortcut: "Ctrl+2"; onTriggered: window.threeD = true }
    Action { id: rosterPanelAction; text: "Roster"; enabled: workspaceState.currentWorkspace === "editor"; checkable: true; checked: !workspaceSettings.rosterCollapsed; shortcut: "Ctrl+Shift+R"; onTriggered: workspaceSettings.rosterCollapsed = !workspaceSettings.rosterCollapsed }
    Action { id: inspectorPanelAction; text: "Inspector"; enabled: workspaceState.currentWorkspace === "editor"; checkable: true; checked: !workspaceSettings.inspectorCollapsed; shortcut: "Ctrl+Shift+I"; onTriggered: workspaceSettings.inspectorCollapsed = !workspaceSettings.inspectorCollapsed }
    Action { id: timelinePanelAction; text: "Timeline"; enabled: workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review"; checkable: true; checked: !workspaceSettings.timelineCollapsed; shortcut: "Ctrl+Shift+T"; onTriggered: workspaceSettings.timelineCollapsed = !workspaceSettings.timelineCollapsed }
    Action { id: projectSetupAction; text: "Project setup…"; shortcut: "Ctrl+,"; onTriggered: { projectSetupDialog.creationMode = false; projectSetupDialog.open() } }

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
            MenuSeparator {}
            Action { text: "Import coordinate JSON…"; onTriggered: importCoordinateDialog.open() }
            Action { text: "Import MIDI…"; onTriggered: { window.requestWorkspace("music"); midiDialog.open() } }
            Action { text: "Import MusicXML…"; onTriggered: { window.requestWorkspace("music"); musicXmlDialog.open() } }
            Action { text: "Attach audio…"; onTriggered: { window.requestWorkspace("music"); audioDialog.open() } }
            MenuSeparator {}
            Action { text: "Export analytics CSV…"; onTriggered: exportPanel.openFor("charts", "csv") }
            Action { text: "Export drill charts, sheets, images or video…"; onTriggered: exportPanel.openFor("charts", "pdf") }
            Action { text: "Export coordinate sheets PDF…"; onTriggered: exportPanel.openFor("coordinates", "pdf") }
            MenuSeparator {}
            Action { text: "Exit"; shortcut: StandardKey.Quit; onTriggered: workspaceState.request("exit", "", drillProject.dirty) }
        }
        Menu {
            title: "&Workspace"
            Action { text: "Roster"; shortcut: "Alt+1"; checkable: true; checked: workspaceState.currentWorkspace === "roster"; onTriggered: window.requestWorkspace("roster") }
            Action { text: "Music"; shortcut: "Alt+2"; checkable: true; checked: workspaceState.currentWorkspace === "music"; onTriggered: window.requestWorkspace("music") }
            Action { text: "Editor"; shortcut: "Alt+3"; checkable: true; checked: workspaceState.currentWorkspace === "editor"; onTriggered: window.requestWorkspace("editor") }
            Action { text: "Review"; shortcut: "Alt+4"; checkable: true; checked: workspaceState.currentWorkspace === "review"; onTriggered: window.requestWorkspace("review") }
            Action { text: "Export"; shortcut: "Alt+5"; checkable: true; checked: workspaceState.currentWorkspace === "export"; onTriggered: window.requestWorkspace("export") }
        }
        Menu {
            id: editMenu
            title: "&Edit"
            AppMenuItem { action: undoAction }
            AppMenuItem { action: redoAction }
            MenuSeparator {}
            AppMenuItem { action: cutFormationAction; ToolTip.text: "Copies coordinates, then resets the selection to the previous set" }
            AppMenuItem { action: copyFormationAction }
            AppMenuItem { action: pasteFormationAction }
            AppMenuItem { action: pasteSpecialAction }
            MenuSeparator {}
            AppMenuItem { action: selectAllAction }
            AppMenuItem { action: clearSelectionAction }
            Menu { title: "Group"; AppMenuItem { action: groupAction } AppMenuItem { action: removeFromGroupAction } AppMenuItem { action: ungroupAction } }
            MenuSeparator {}
            AppMenuItem { action: deleteSelectedAction }
        }
        Menu {
            title: "&Set"
            enabled: workspaceState.currentWorkspace === "editor"
            AppMenuItem { action: previousSetAction }
            AppMenuItem { action: nextSetAction }
            MenuSeparator {}
            AppMenuItem { text: "Edit current set…"; onTriggered: { setDialog.editing = true; setDialog.open() } }
            AppMenuItem { text: "Copy full formation"; onTriggered: drillProject.copyCurrentFormation() }
            AppMenuItem { action: pasteFormationAction }
            AppMenuItem { action: pasteSpecialAction }
            AppMenuItem { action: duplicateSetAction }
            MenuSeparator {}
            AppMenuItem { text: "Add set after current…"; onTriggered: { setDialog.editing = false; setDialog.open() } }
            AppMenuItem { text: "Create formation variant…"; onTriggered: variantDialog.open() }
            AppMenuItem { text: "Open set archive…"; onTriggered: archiveDialog.open() }
        }
        Menu {
            title: "&Formation"
            enabled: workspaceState.currentWorkspace === "editor"
            AppMenuItem { action: formationBuilderAction }
            AppMenuItem { text: "Choose shape…"; enabled: drillProject.selectedCount > 0; onTriggered: shapePalette.open() }
            AppMenuItem { text: "Draw freehand…"; enabled: drillProject.selectedCount > 0; onTriggered: freehandDialog.open() }
            MenuSeparator {}
            Menu { title: "Align"
                AppMenuItem { text: "Left"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("left") }
                AppMenuItem { text: "Horizontal center"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("centerX") }
                AppMenuItem { text: "Right"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("right") }
                AppMenuItem { text: "Front"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("front") }
                AppMenuItem { text: "Vertical center"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("centerY") }
                AppMenuItem { text: "Back"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("back") }
            }
            Menu { title: "Distribute"
                AppMenuItem { text: "Horizontally"; enabled: drillProject.selectedCount > 2; onTriggered: drillProject.distributeSelected("horizontal") }
                AppMenuItem { text: "Vertically"; enabled: drillProject.selectedCount > 2; onTriggered: drillProject.distributeSelected("vertical") }
            }
            AppMenuItem { text: "Snap to " + workspaceSettings.snapGrid + "-step grid"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.snapSelected(workspaceSettings.snapGrid) }
            AppMenuItem { text: "Mirror side-to-side"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(true) }
            AppMenuItem { text: "Mirror front-to-back"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(false) }
            AppMenuItem { text: "Auto-label selected"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.autoLabel("P") }
            MenuSeparator {}
            AppMenuItem { text: "Face front"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(0) }
            AppMenuItem { text: "Face back"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(180) }
            AppMenuItem { text: "Face side 1"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(270) }
            AppMenuItem { text: "Face side 2"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.faceSelected(90) }
        }
        Menu {
            title: "&Transition"
            enabled: workspaceState.currentWorkspace === "editor"
            AppMenuItem { action: transitionEditAction }
            AppMenuItem { text: "Cancel transition edit"; enabled: drillProject.transitionEditActive; onTriggered: drillProject.cancelTransitionEdit() }
            MenuSeparator {}
            AppMenuItem { text: "Play current transition"; enabled: drillProject.currentSetIndex > 0; onTriggered: transport.playCurrentTransition() }
            AppMenuItem { text: "Analyze incoming transition"; enabled: drillProject.currentSetIndex > 0; onTriggered: drillProject.analyzeTransition() }
        }
        Menu {
            id: viewMenu
            title: "&View"
            AppMenuItem { action: view2DAction }
            AppMenuItem { action: view3DAction }
            MenuSeparator {}
            AppMenuItem { text: "Show paths"; checkable: true; checked: drillProject.showTransitionPaths; onToggled: drillProject.showTransitionPaths = checked }
            AppMenuItem { text: "Show shape guides"; checkable: true; checked: drillProject.showShapeGuides; onToggled: drillProject.showShapeGuides = checked }
            AppMenuItem { text: "Show previous formation"; checkable: true; checked: workspaceSettings.showPreviousFormation; onToggled: workspaceSettings.showPreviousFormation = checked }
            AppMenuItem { text: "Show field grid"; checkable: true; checked: drillProject.showFieldGrid; onToggled: drillProject.showFieldGrid = checked }
            AppMenuItem { text: "Show labels"; checkable: true; checked: workspaceSettings.showLabels; onToggled: workspaceSettings.showLabels = checked }
            MenuSeparator {}
            AppMenuItem { action: rosterPanelAction }
            AppMenuItem { action: inspectorPanelAction }
            AppMenuItem { action: timelinePanelAction }
            MenuSeparator {}
            AppMenuItem { text: "Compact toolbar"; checkable: true; checked: workspaceSettings.toolbarMode === "compact"; onTriggered: workspaceSettings.toolbarMode = "compact" }
            AppMenuItem { text: "Expanded toolbar"; checkable: true; checked: workspaceSettings.toolbarMode === "expanded"; onTriggered: workspaceSettings.toolbarMode = "expanded" }
            AppMenuItem { text: "Compact: Transform tools"; checkable: true; checked: workspaceSettings.showTransformTools; onToggled: workspaceSettings.showTransformTools = checked }
            AppMenuItem { text: "Compact: View tools"; checkable: true; checked: workspaceSettings.showViewTools; onToggled: workspaceSettings.showViewTools = checked }
        }
        Menu {
            title: "&Tools"
            AppMenuItem { text: "Add performer…"; onTriggered: { performerDialog.editing = false; performerDialog.open() } }
            AppMenuItem { text: "Batch add performers…"; onTriggered: batchDialog.open() }
            AppMenuItem { text: "Scan show in Drill Clinic"; onTriggered: { workspaceSettings.inspectorCollapsed = false; workspaceSettings.inspectorClinicExpanded = true; drillProject.scanShow() } }
            MenuSeparator {}
            AppMenuItem { action: projectSetupAction }
            AppMenuItem { text: "Editor preferences…"; onTriggered: settingsDialog.open() }
        }
        Menu {
            title: "&Help"
            Action { text: "Load sample show"; onTriggered: window.requestSampleProject() }
            Action { text: "About MarchCraft"; onTriggered: aboutDialog.open() }
        }
    }

    header: Column {
        visible: workspaceState.workspaceActive
        width: parent.width
        height: visible ? workspaceHeader.height + commandBars.height : 0
        WorkspaceHeader {
            id: workspaceHeader
            width: parent.width
            drillProjectContext: drillProject
            exportControllerContext: exportController
            workspaceStateContext: workspaceState
            onHomeRequested: window.returnHome()
            onSaveRequested: window.saveCurrentProject("normal")
            onWorkspaceRequested: function(workspace) { window.requestWorkspace(workspace) }
        }
        EditorCommandBars {
            width:parent.width
            visible: workspaceState.currentWorkspace === "editor" && workspaceState.workspaceActive
            id: commandBars
            batchDialogContext: batchDialog
            drillProjectContext: drillProject
            editorActionsPopupContext: editorActionsPopup
            freehandDialogContext: freehandDialog
            performerDialogContext: performerDialog
            shapePaletteContext: shapePalette
            copyActionContext: copyFormationAction
            duplicateSetActionContext: duplicateSetAction
            pasteActionContext: pasteFormationAction
            transitionActionContext: transitionEditAction
            windowContext: window
            workspaceSettingsContext: workspaceSettings
            workspaceStateContext: workspaceState
        }
    }

    Popup {
        id: shapePalette
        parent: Overlay.overlay
        onOpened: Qt.callLater(function() {
            const anchor = commandBars.shapeButton.mapToItem(shapePalette.parent, 0, commandBars.shapeButton.height)
            shapePalette.x = Math.max(8, Math.min(window.width - width - 8, anchor.x))
            shapePalette.y = anchor.y + 6
        })
        width: 380; height: 384; padding: 14; modal: false; focus: true
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
                AppButton { text: "Advanced…"; flat: true; onClicked: { shapePalette.close(); window.shapeDrawing = ""; window.freehandDrawing = false; formationDialog.open() } }
            }
            GridLayout {
                columns: 4; columnSpacing: 6; rowSpacing: 6; Layout.fillWidth: true; Layout.fillHeight: true
                Repeater {
                    model: [{kind:"line",label:"Line"},{kind:"rectangle",label:"Rectangle"},{kind:"circle",label:"Circle"},{kind:"triangle",label:"Triangle"},{kind:"arc",label:"Arc"},{kind:"ellipse",label:"Ellipse"},{kind:"diamond",label:"Diamond"},{kind:"block",label:"Block"},{kind:"polygon",label:"Polygon"},{kind:"star",label:"Star"},{kind:"spiral",label:"Spiral"}]
                    delegate: AppButton {
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentItem: ColumnLayout {
                            spacing: 5
                            ShapeIcon { kind: modelData.kind; iconColor: parent.parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled; Layout.alignment: Qt.AlignHCenter }
                            Label { text: modelData.label; color: MarchCraftTheme.textPrimary; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                        }
                        enabled: drillProject.selectedCount > 0
                        highlighted: window.shapeDrawing === modelData.kind
                        onClicked: { window.setShapeTool(modelData.kind); shapePalette.close() }
                    }
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap
                text: "Drag on the field, review, then Apply. Esc or right-click cancels drawing."
                color: MarchCraftTheme.textMuted; font.pixelSize: 11
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
            AppButton { text: "Preferences…"; flat: true; Layout.fillWidth: true; onClicked: { editorActionsPopup.close(); settingsDialog.open() } }
            AppButton { text: workspaceSettings.toolbarMode === "expanded" ? "Use compact toolbar" : "Expand toolbar"; flat: true; Layout.fillWidth: true; onClicked: { workspaceSettings.toolbarMode = workspaceSettings.toolbarMode === "expanded" ? "compact" : "expanded"; editorActionsPopup.close() } }
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
            ComboBox {
                Layout.fillWidth: true
                model: drillProject.currentVariantCount
                currentIndex: Math.max(0, drillProject.currentVariantIndex)
                displayText: "Variant " + (drillProject.variantInfo(currentIndex).label || "")
                delegate: ItemDelegate {
                    required property int index
                    text: "Variant " + drillProject.variantInfo(index).label + " · " + drillProject.variantInfo(index).name
                }
                onActivated: { transport.editSet(drillProject.currentSetIndex); drillProject.activateVariant(currentIndex) }
            }
            AppButton { text: "Play from selection"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); transport.playFromSelection() } }
            AppButton { text: "New variant…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); transport.editSet(drillProject.currentSetIndex); variantDialog.open() } }
            AppButton { text: "New set…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); transport.editSet(drillProject.currentSetIndex); setDialog.editing = false; setDialog.open() } }
            AppButton { text: "Edit current set…"; flat: true; enabled: drillProject.setCount > 0; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); transport.editSet(drillProject.currentSetIndex); setDialog.editing = true; setDialog.open() } }
            AppButton { text: "Add sets in batch…"; flat: true; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); batchSetDialog.open() } }
            AppButton { text: drillProject.currentVariantCount > 1 ? "Archive variant" : "Archive set"; flat: true; enabled: drillProject.setCount > 1 || drillProject.currentVariantCount > 1; Layout.fillWidth: true; onClicked: { timelineActionsPopup.close(); transport.editSet(drillProject.currentSetIndex); drillProject.archiveCurrentVariant() } }
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

    RosterWorkspace {
        id: rosterWorkspace
        anchors.fill: parent
        z: 2
        visible: workspaceState.workspaceActive && workspaceState.currentWorkspace === "roster"
        enabled: visible
        drillProjectContext: drillProject
        performerDialogContext: performerDialog
        bulkEditDialogContext: bulkEditDialog
        inspectorContext: inspectorPanel.exposedInspector
        windowContext: window
    }

    MusicWorkspace {
        id: musicWorkspace
        anchors.fill: parent
        z: 2
        visible: workspaceState.workspaceActive && workspaceState.currentWorkspace === "music"
        enabled: visible
        drillProjectContext: drillProject
        transportContext: transport
        onMidiImportRequested: midiDialog.open()
        onMusicXmlImportRequested: musicXmlDialog.open()
        onAudioImportRequested: audioDialog.open()
        onAdvancedToolRequested: function(tool) { timelinePanel.openMusicTool(tool) }
        onEditorRequested: window.requestWorkspace("editor")
    }

    MovementTabs {
        id: movementTabs
        project: drillProject
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.margins: 6; height: 32
        visible: window.editorSurfaceActive
    }

    SplitView {
        id: horizontalSplit
        enabled: window.editorSurfaceActive
        visible: window.editorSurfaceActive
        anchors.fill: parent
        anchors.margins: 6
        anchors.topMargin: 44
        orientation: Qt.Horizontal
        Component.onCompleted: if (workspaceSettings.horizontalSplitState) restoreState(workspaceSettings.horizontalSplitState)
        onResizingChanged: if (!resizing) workspaceSettings.horizontalSplitState = saveState()
        handle: Rectangle {
            implicitWidth: 5; color: SplitHandle.pressed ? MarchCraftTheme.accent : SplitHandle.hovered ? MarchCraftTheme.dividerStrong : MarchCraftTheme.divider
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
        visible: workspaceState.currentWorkspace === "editor" && !workspaceSettings.rosterCollapsed
    }

        SplitView {
            id: verticalSplit
            SplitView.fillWidth: true
            SplitView.minimumWidth: 520
            orientation: Qt.Vertical
            Component.onCompleted: {
                if (workspaceSettings.compactTimelineDefaultApplied && workspaceSettings.verticalSplitState)
                    restoreState(workspaceSettings.verticalSplitState)
                else {
                    timelinePanel.SplitView.preferredHeight = 190
                    workspaceSettings.verticalSplitState = undefined
                    workspaceSettings.compactTimelineDefaultApplied = true
                }
            }
            onResizingChanged: if (!resizing) workspaceSettings.verticalSplitState = saveState()
            handle: Rectangle {
                implicitHeight: 6; color: SplitHandle.pressed ? MarchCraftTheme.accent : SplitHandle.hovered ? MarchCraftTheme.dividerStrong : MarchCraftTheme.divider
                TapHandler { onDoubleTapped: {
                    timelinePanel.SplitView.preferredHeight = 190
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
                    drawMode: workspaceState.currentWorkspace === "editor" && window.freehandDrawing
                    shapeDrawMode: workspaceState.currentWorkspace === "editor" ? window.shapeDrawing : ""
                    showPaths: drillProject.showTransitionPaths
                    showShapeGuides: drillProject.showShapeGuides
                    showLabels: workspaceSettings.showLabels
                    showPreviousFormation: workspaceSettings.showPreviousFormation
                    previousFormationOpacity: workspaceSettings.previousFormationOpacity
                    snapEnabled: workspaceSettings.snapEnabled
                    gridSize: workspaceSettings.snapGrid
                    onPerformerActivated: function(row) { window.activePerformer = row; inspectorPanel.exposedInspector.refresh() }
                    onContextMenuRequested: function(screenX, screenY, performerRow) {
                        window.activePerformer = performerRow
                        fieldContextMenu.performerRow = performerRow
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
                    onShapeCompleted: function(kind, options) {
                        drillProject.requestFormationPreview(kind, options, drillProject.formationAssignmentMode)
                        window.shapeDrawing = ""
                        freehandPreviewDialog.applied = false
                        freehandPreviewDialog.open()
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
        visible: workspaceState.currentWorkspace === "editor" && !workspaceSettings.inspectorCollapsed
    }

    ReviewPanel {
        id: reviewPanel
        visible: workspaceState.currentWorkspace === "review"
        drillProjectContext: drillProject
        transportContext: transport
        windowContext: window
    }
    }

    footer: ToolBar {
        visible: workspaceState.workspaceActive
        height: visible ? 26 : 0
        background: Rectangle { color: MarchCraftTheme.panelHeader; border.color: MarchCraftTheme.divider }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10; anchors.rightMargin: 10
            Label { text: drillProject.statusMessage; color: MarchCraftTheme.textSecondary; font.pixelSize: 10; Layout.fillWidth: true }
            Label { visible: workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review" || workspaceState.currentWorkspace === "roster"; text: drillProject.selectedCount + " selected"; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
            Rectangle { width: 1; height: 12; color: MarchCraftTheme.divider }
            Label { text: workspaceState.currentWorkspace.toUpperCase(); color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 9; font.letterSpacing: 0.8 }
        }
    }

    FormationDialog {
        id: formationDialog
        parent: Overlay.overlay
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
            ComboBox { id: movementMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Rehearsal safe",value:"rehearsalSafe"},{text:"Shortest total travel",value:"shortest"},{text:"Preserve spatial order (no flip)",value:"preserveOrder"},{text:"Even individual effort",value:"evenEffort"},{text:"Readable feature move",value:"featureMove"},{text:"Fixed roster slots",value:"rosterOrder"}]; currentIndex: 0; onActivated: window.freehandMovementMode=currentValue }
            CheckBox { text: "Create as group"; checked: window.freehandCreateGroup; onToggled: window.freehandCreateGroup=checked }
            Label { text: "The stroke is smoothed, spaced by equal arc length, expanded if necessary, and shifted locally to avoid unselected performers."; color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
            AppButton { text: "Start drawing"; highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: { window.shapeDrawing="";window.freehandRecognitionMode=recognitionMode.currentValue;window.freehandMovementMode=movementMode.currentValue;window.freehandDrawing=true;freehandDialog.close() } }
        }
    }

    MovableDialog {
        id: freehandPreviewDialog
        property bool applied: false
        title: "Review formation assignment"; parent: Overlay.overlay; width: 460
        standardButtons: Dialog.NoButton
        onClosed: if (!applied) drillProject.cancelFormationPreview()
        contentItem: ColumnLayout {
            BusyIndicator { running: drillProject.formationPreviewBusy; visible: running; Layout.alignment: Qt.AlignHCenter }
            Label { text: drillProject.formationPreviewBusy ? "Finding a safe one-to-one assignment..." : "Review the ghost destinations and proposed paths on the field."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#a9bbb1" }
            GridLayout { columns: 2; Layout.fillWidth: true; visible: drillProject.formationPreviewActive
                property var metrics: drillProject.formationPreviewMetrics
                Label { text: "Average move"; color: MarchCraftTheme.textSecondary }
                Label { text: drillProject.formatDistance(parent.metrics.averageMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum move"; color: MarchCraftTheme.textSecondary }
                Label { text: drillProject.formatDistance(parent.metrics.maximumMove || 0); Layout.alignment: Qt.AlignRight }
                Label { text: "Maximum steps / count"; color: MarchCraftTheme.textSecondary }
                Label { text: Number(parent.metrics.maximumStepsPerCount || 0).toFixed(2); Layout.alignment: Qt.AlignRight }
            }
            RowLayout { Layout.alignment: Qt.AlignRight
                AppButton { text: "Cancel"; onClicked: freehandPreviewDialog.close() }
                AppButton { text: "Apply"; highlighted: true; enabled: drillProject.formationPreviewActive; onClicked: { freehandPreviewDialog.applied = drillProject.commitFormationPreview(); if (freehandPreviewDialog.applied) freehandPreviewDialog.close() } }
            }
        }
    }

    AppToolButton {
        id: rosterRevealButton
        visible: workspaceState.currentWorkspace === "editor" && workspaceSettings.rosterCollapsed
        z: 20
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 52
        width: 28
        height: 46
        text: "›"
        font.pixelSize: 17
        ToolTip.text: "Show performer picker (Ctrl+Shift+R)"
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
        property int performerRow: -1
        readonly property bool hasSelectionContext: performerRow >= 0 || drillProject.selectedCount > 0
        AppMenuItem { action: cutFormationAction; visible: fieldContextMenu.hasSelectionContext }
        AppMenuItem { action: copyFormationAction; visible: fieldContextMenu.hasSelectionContext }
        AppMenuItem { action: pasteFormationAction }
        AppMenuItem { action: pasteSpecialAction }
        MenuSeparator { visible: fieldContextMenu.hasSelectionContext }
        Menu {
            title: "Group"
            visible: fieldContextMenu.hasSelectionContext
            AppMenuItem { action: groupAction }
            AppMenuItem { action: removeFromGroupAction }
            AppMenuItem { action: ungroupAction }
        }
        Menu {
            title: "Formation"
            visible: fieldContextMenu.hasSelectionContext
            AppMenuItem { action: formationBuilderAction }
            AppMenuItem { text: "Snap to " + workspaceSettings.snapGrid + "-step grid"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.snapSelected(workspaceSettings.snapGrid) }
            AppMenuItem { text: "Mirror side-to-side"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(true) }
            AppMenuItem { text: "Mirror front-to-back"; enabled: drillProject.selectedCount > 0; onTriggered: drillProject.mirrorSelected(false) }
            AppMenuItem { text: "Align left"; enabled: drillProject.selectedCount > 1; onTriggered: drillProject.alignSelected("left") }
            AppMenuItem { text: "Distribute horizontally"; enabled: drillProject.selectedCount > 2; onTriggered: drillProject.distributeSelected("horizontal") }
        }
        Menu {
            title: "Facing"
            visible: fieldContextMenu.hasSelectionContext
            AppMenuItem { text: "Front"; onTriggered: drillProject.faceSelected(0) }
            AppMenuItem { text: "Back"; onTriggered: drillProject.faceSelected(180) }
            AppMenuItem { text: "Side 1"; onTriggered: drillProject.faceSelected(270) }
            AppMenuItem { text: "Side 2"; onTriggered: drillProject.faceSelected(90) }
        }
        AppMenuItem { action: transitionEditAction; visible: fieldContextMenu.hasSelectionContext }
        AppMenuItem { text: "Select all performers"; visible: !fieldContextMenu.hasSelectionContext; onTriggered: drillProject.selectAll() }
        Menu {
            title: "View"
            AppMenuItem { text: "Previous formation"; checkable: true; checked: workspaceSettings.showPreviousFormation; onToggled: workspaceSettings.showPreviousFormation = checked }
            AppMenuItem { text: "Transition paths"; checkable: true; checked: drillProject.showTransitionPaths; onToggled: drillProject.showTransitionPaths = checked }
            AppMenuItem { text: "Labels"; checkable: true; checked: workspaceSettings.showLabels; onToggled: workspaceSettings.showLabels = checked }
        }
    }

    Menu {
        id: setContextMenu; property int setIndex: -1
        AppMenuItem { text: "Edit set"; onTriggered: { transport.editSet(setContextMenu.setIndex); setDialog.editing=true; setDialog.open() } }
        AppMenuItem {
            text: "Edit incoming transition"
            enabled: setContextMenu.setIndex > 0
            onTriggered: drillProject.selectTimelineTransition(setContextMenu.setIndex)
        }
        MenuSeparator {}
        AppMenuItem { text: "Copy this formation"; onTriggered: drillProject.copySetFormation(setContextMenu.setIndex) }
        AppMenuItem { text: "Paste formation here"; enabled: drillProject.hasFormationClipboard; onTriggered: { transport.editSet(setContextMenu.setIndex); drillProject.pasteFormation() } }
        AppMenuItem { text: "Paste Special here…"; enabled: drillProject.hasFormationClipboard; onTriggered: { transport.editSet(setContextMenu.setIndex); pasteSpecialDialog.open() } }
        MenuSeparator {}
        AppMenuItem {
            text: "Insert 8 counts before"
            enabled: setContextMenu.setIndex > 0
            onTriggered: drillProject.insertCountsBeforeSet(setContextMenu.setIndex, 8)
        }
        AppMenuItem {
            text: "Delete 8 incoming counts"
            enabled: setContextMenu.setIndex > 0 && drillProject.setInfo(setContextMenu.setIndex).counts > 8
            onTriggered: drillProject.deleteCountsFromTransition(setContextMenu.setIndex, 8)
        }
        AppMenuItem {
            text: "Insert 8 counts after"
            enabled: setContextMenu.setIndex >= 0 && setContextMenu.setIndex + 1 < drillProject.setCount
            onTriggered: drillProject.insertCountsAfterSet(setContextMenu.setIndex, 8)
        }
        MenuSeparator {}
        AppMenuItem { text: "Duplicate set"; onTriggered: { transport.editSet(setContextMenu.setIndex); drillProject.duplicateSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Add before"; onTriggered: { transport.editSet(setContextMenu.setIndex); drillProject.insertSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Add after"; onTriggered: { transport.editSet(setContextMenu.setIndex); drillProject.insertSetAt(setContextMenu.setIndex+1); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
        AppMenuItem { text: "Create variant"; onTriggered: { transport.editSet(setContextMenu.setIndex); variantDialog.open() } }
        MenuSeparator {}
        AppMenuItem { text: "Delete set"; enabled: drillProject.setCount>1; onTriggered: { transport.editSet(setContextMenu.setIndex); drillProject.archiveSetAt(setContextMenu.setIndex); if(drillProject.setLabelsNeedRenumbering())renumberDialog.open() } }
    }

    Dialog {
        id: pasteSpecialDialog
        objectName: "pasteSpecialDialog"
        title: "Paste Special"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 480
        onOpened: { pasteOffsetX.value = 0; pasteOffsetY.value = 0; pasteMirrorHorizontal.checked = false; pasteMirrorVertical.checked = false }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: drillProject.formationClipboardSummary || "Formation clipboard"
                color: MarchCraftTheme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label { text: "Content"; font.bold: true }
            ComboBox {
                id: pasteMode
                Layout.fillWidth: true
                textRole: "text"; valueRole: "value"
                model: [
                    {text: "Positions and facing (recommended)", value: "positionsFacing"},
                    {text: "Positions only", value: "positions"},
                    {text: "Positions, facing, groups, and shape metadata", value: "positionsFacingMetadata"}
                ]
            }
            Label {
                text: "Incoming transition paths and timing in the destination set are always preserved."
                color: MarchCraftTheme.textMuted; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                CheckBox { id: pasteMirrorHorizontal; text: "Mirror side-to-side"; Layout.fillWidth: true }
                CheckBox { id: pasteMirrorVertical; text: "Mirror front-to-back"; Layout.fillWidth: true }
            }
            GridLayout {
                columns: 2; Layout.fillWidth: true
                Label { text: "Side-to-side offset" }
                SpinBox {
                    id: pasteOffsetX; from: -768; to: 768; stepSize: 1; editable: true; Layout.fillWidth: true
                    textFromValue: function(value) { return (value / 4).toFixed(2) + " steps" }
                    valueFromText: function(text) { return Math.round(Number(text.replace(/[^0-9.-]/g, "")) * 4) }
                }
                Label { text: "Front-to-back offset" }
                SpinBox {
                    id: pasteOffsetY; from: -400; to: 400; stepSize: 1; editable: true; Layout.fillWidth: true
                    textFromValue: function(value) { return (value / 4).toFixed(2) + " steps" }
                    valueFromText: function(text) { return Math.round(Number(text.replace(/[^0-9.-]/g, "")) * 4) }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: "Cancel"; onClicked: pasteSpecialDialog.close() }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: "Paste"; highlighted: true
                    onClicked: {
                        if (drillProject.pasteFormation(pasteMode.currentValue,
                                                        pasteMirrorHorizontal.checked,
                                                        pasteMirrorVertical.checked,
                                                        pasteOffsetX.value / 4,
                                                        pasteOffsetY.value / 4))
                            pasteSpecialDialog.close()
                    }
                }
            }
        }
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
        workspaceSettingsContext: workspaceSettings
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
            Label { text: "Leave text fields blank to preserve mixed values."; color: MarchCraftTheme.textSecondary }
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
            ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 90; TextArea { id: setCaption; placeholderText: "Chart instructions (multiple lines)"; wrapMode: TextEdit.Wrap } }
            Label { text: "Measures" }
            TextField { id: setMeasure; Layout.fillWidth: true; placeholderText: "17–20" }
            Label { text: "Opening behavior"; visible: setDialog.editing && drillProject.currentSetIndex === 0 }
            ComboBox { id: openingBehavior; visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Hold at Set 1",value:"hold"},{text:"Start moving immediately",value:"move"}] }
            Label { text: setDialog.editing && drillProject.currentSetIndex === 0 ? "Opening hold counts" : "Counts"; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            SpinBox { id: setCounts; from: 1; to: 2048; editable: true; Layout.fillWidth: true; visible: !(setDialog.editing && drillProject.currentSetIndex === 0 && openingBehavior.currentValue === "move") }
            Label { visible: !(setDialog.editing && drillProject.currentSetIndex === 0); Layout.fillWidth: true; wrapMode: Text.Wrap; color: MarchCraftTheme.textSecondary; text: "For a hold, use two consecutive sets with identical coordinates and enter the hold duration here." }
            Label { visible: setDialog.editing && drillProject.currentSetIndex === 0; Layout.fillWidth: true; wrapMode: Text.Wrap; color: MarchCraftTheme.textSecondary; text: openingBehavior.currentValue === "hold" ? "The entire ensemble remains at Set 1 for these counts before the show clock and music begin moving." : "Playback begins the Set 1 to Set 2 transition immediately, with no opening standstill." }
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
                color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true
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
            Label { text: "Applies to the selected transition"; color: MarchCraftTheme.textSecondary }
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
                color: MarchCraftTheme.textSecondary
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
            Label { text: "ARCHIVED SETS"; color: MarchCraftTheme.textSecondary; font.bold: true }
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
                        Label { text: info.caption || info.measure; color: MarchCraftTheme.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
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
            Label { text: "ARCHIVED VARIANTS IN CURRENT SET"; color: MarchCraftTheme.textSecondary; font.bold: true }
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
                        Label { text: info.caption; color: MarchCraftTheme.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
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
            workspaceController.rememberWorkspace(drillProject.projectPath, workspaceState.lastUsefulWorkspace)
            workspaceController.refreshRecovery()
            if (window.savePurpose === "pending") workspaceState.confirmAfterSave()
            window.savePurpose = "normal"
        }
        onRejected: window.savePurpose = "normal"
    }
    FileDialog { id: importCoordinateDialog; title: "Import coordinate data"; nameFilters: ["Coordinate JSON (*.json)"]; onAccepted: drillProject.importCoordinateJson(selectedFile) }
    FileDialog { id: midiDialog; title: "Import MIDI score"; nameFilters: ["MIDI (*.mid *.midi)"]; onAccepted: { window.activateWorkspace("music"); drillProject.importMidiAsync(selectedFile) } }
    FileDialog { id: musicXmlDialog; title: "Import MusicXML score"; nameFilters: ["MusicXML (*.musicxml *.xml)"]; onAccepted: { window.activateWorkspace("music"); drillProject.importMusicXml(selectedFile) } }
    FileDialog { id: audioDialog; title: "Attach rehearsal audio"; nameFilters: ["Audio (*.wav *.mp3 *.m4a *.flac)"]; onAccepted: { window.activateWorkspace("music"); drillProject.attachAudio(selectedFile) } }
    ExportWorkspace {
        id:exportPanel
        visible:workspaceState.workspaceActive && workspaceState.currentWorkspace === "export"
        onWorkspaceRequested: window.activateWorkspace("export")
    }
    ExportVideoWindow { }
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

    Shortcut { sequence: "Left"; enabled: workspaceState.workspaceActive && workspaceState.currentWorkspace === "editor"; onActivated: drillProject.nudgeSelected(-0.25, 0) }
    Shortcut { sequence: "Right"; enabled: workspaceState.workspaceActive && workspaceState.currentWorkspace === "editor"; onActivated: drillProject.nudgeSelected(0.25, 0) }
    Shortcut { sequence: "Up"; enabled: workspaceState.workspaceActive && workspaceState.currentWorkspace === "editor"; onActivated: drillProject.nudgeSelected(0, -0.25) }
    Shortcut { sequence: "Down"; enabled: workspaceState.workspaceActive && workspaceState.currentWorkspace === "editor"; onActivated: drillProject.nudgeSelected(0, 0.25) }
    Shortcut {
        sequence: "Ctrl+Space"
        enabled: workspaceState.workspaceActive
                 && ["music", "editor", "review"].indexOf(workspaceState.currentWorkspace) >= 0
        context: Qt.ApplicationShortcut
        onActivated: transport.playPause()
    }
    Shortcut { sequence: "Ctrl+L"; enabled: workspaceState.hasCurrentProject; context: Qt.ApplicationShortcut; onActivated: { window.requestWorkspace("roster"); Qt.callLater(function() { rosterWorkspace.focusSearch() }) } }
    Shortcut { sequence: "+"; enabled: workspaceState.workspaceActive && (workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review"); context: Qt.ApplicationShortcut; onActivated: fieldView.zoom = Math.min(3.5, fieldView.zoom * 1.12) }
    Shortcut { sequence: "-"; enabled: workspaceState.workspaceActive && (workspaceState.currentWorkspace === "editor" || workspaceState.currentWorkspace === "review"); context: Qt.ApplicationShortcut; onActivated: fieldView.zoom = Math.max(0.7, fieldView.zoom * 0.89) }
}
