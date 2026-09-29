import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: commandBar
    property alias shapeButton: shapesButton
    required property var batchDialogContext
    required property var copyActionContext
    required property var drillProjectContext
    required property var duplicateSetActionContext
    required property var editorActionsPopupContext
    required property var freehandDialogContext
    required property var pasteActionContext
    required property var performerDialogContext
    required property var shapePaletteContext
    required property var transitionActionContext
    required property var windowContext
    required property var workspaceSettingsContext
    required property var workspaceStateContext

    visible: workspaceStateContext.workspaceActive
    height: visible ? (workspaceSettingsContext.toolbarMode === "expanded" ? 82 : 46) : 0
    background: Rectangle { color: MarchCraftTheme.panelHeader; border.color: MarchCraftTheme.divider }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 6

            Label {
                text: drillProjectContext.selectedCount > 0
                      ? drillProjectContext.selectedCount + " SELECTED" : "FORMATION TOOLS"
                color: drillProjectContext.selectedCount > 0
                       ? MarchCraftTheme.accentHover : MarchCraftTheme.textMuted
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.7
                Layout.rightMargin: 4
            }
            AppToolButton {
                text: "Copy"
                enabled: copyActionContext.enabled
                ToolTip.text: copyActionContext.text + " (Ctrl+C)"; ToolTip.visible: hovered
                onClicked: copyActionContext.trigger()
            }
            AppToolButton {
                text: "Paste"
                enabled: pasteActionContext.enabled
                ToolTip.text: drillProjectContext.formationClipboardSummary || "Paste formation (Ctrl+V)"
                ToolTip.visible: hovered
                onClicked: pasteActionContext.trigger()
            }
            Rectangle { width: 1; height: 24; color: MarchCraftTheme.divider; Layout.leftMargin: 2; Layout.rightMargin: 2 }
            AppButton {
                id: shapesButton
                text: windowContext.shapeDrawing.length ? "Shape · " + windowContext.shapeDrawing : "Shapes"
                enabled: drillProjectContext.selectedCount > 0
                highlighted: windowContext.shapeDrawing.length > 0
                onClicked: shapePaletteContext.open()
            }
            AppButton {
                text: windowContext.freehandDrawing ? "Drawing…" : "Freehand"
                enabled: drillProjectContext.selectedCount > 0
                highlighted: windowContext.freehandDrawing
                onClicked: freehandDialogContext.open()
            }
            AppButton {
                text: drillProjectContext.transitionEditActive ? "Apply paths" : "Transition"
                enabled: transitionActionContext.enabled
                highlighted: drillProjectContext.transitionEditActive
                ToolTip.text: drillProjectContext.currentSetIndex > 0
                    ? "Edit incoming transition paths"
                    : "Choose a destination set first"
                ToolTip.visible: hovered
                onClicked: transitionActionContext.trigger()
            }
            AppToolButton {
                visible: workspaceSettingsContext.showTransformTools
                text: "Snap"
                enabled: drillProjectContext.selectedCount > 0
                ToolTip.text: "Snap selection to " + workspaceSettingsContext.snapGrid + "-step grid"
                ToolTip.visible: hovered
                onClicked: drillProjectContext.snapSelected(workspaceSettingsContext.snapGrid)
            }
            AppToolButton {
                visible: workspaceSettingsContext.showTransformTools
                text: "Mirror"
                enabled: drillProjectContext.selectedCount > 0
                ToolTip.text: "Mirror selection side-to-side"
                ToolTip.visible: hovered
                onClicked: drillProjectContext.mirrorSelected(true)
            }
            Item { Layout.fillWidth: true }
            AppToolButton { visible: workspaceSettingsContext.showViewTools; text: "2D"; checkable: true; checked: !windowContext.threeD; onClicked: windowContext.threeD = false }
            AppToolButton { visible: workspaceSettingsContext.showViewTools; text: "3D"; checkable: true; checked: windowContext.threeD; onClicked: windowContext.threeD = true }
            AppToolButton {
                text: workspaceSettingsContext.toolbarMode === "expanded" ? "⌃" : "⌄"
                ToolTip.text: workspaceSettingsContext.toolbarMode === "expanded" ? "Use compact toolbar" : "Expand toolbar"
                ToolTip.visible: hovered
                onClicked: workspaceSettingsContext.toolbarMode = workspaceSettingsContext.toolbarMode === "expanded" ? "compact" : "expanded"
            }
            AppToolButton {
                ToolTip.text: "More editor actions"; ToolTip.visible: hovered
                contentItem: AppIcon { name: "more" }
                onClicked: editorActionsPopupContext.open()
            }
        }

        Rectangle {
            visible: workspaceSettingsContext.toolbarMode === "expanded"
            Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider
        }
        RowLayout {
            visible: workspaceSettingsContext.toolbarMode === "expanded"
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 35 : 0
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 5

            AppButton { text: "Add performer…"; onClicked: { performerDialogContext.editing = false; performerDialogContext.open() } }
            AppButton { text: "Batch add…"; onClicked: batchDialogContext.open() }
            AppButton { text: "Duplicate set"; enabled: duplicateSetActionContext.enabled; onClicked: duplicateSetActionContext.trigger() }
            AppButton { text: "Formation builder…"; enabled: drillProjectContext.selectedCount > 0; onClicked: windowContext.formationDialogForToolbar() }
            Rectangle { width: 1; height: 20; color: MarchCraftTheme.divider; Layout.leftMargin: 2; Layout.rightMargin: 2 }
            Label { text: "Drag snap"; color: MarchCraftTheme.textSecondary; font.pixelSize: 10 }
            CheckBox {
                checked: workspaceSettingsContext.snapEnabled
                onToggled: workspaceSettingsContext.snapEnabled = checked
                ToolTip.text: "Enable snapping while dragging performers and drawing shapes"
                ToolTip.visible: hovered
            }
            ComboBox {
                Layout.preferredWidth: 76
                model: ["4", "2", "1", "0.5", "0.25"]
                currentIndex: Math.max(0, find(String(workspaceSettingsContext.snapGrid)))
                onActivated: workspaceSettingsContext.snapGrid = Number(currentText)
            }
            AppToolButton { text: "Align L"; enabled: drillProjectContext.selectedCount > 1; onClicked: drillProjectContext.alignSelected("left") }
            AppToolButton { text: "Center"; enabled: drillProjectContext.selectedCount > 1; onClicked: drillProjectContext.alignSelected("centerX") }
            AppToolButton { text: "Align R"; enabled: drillProjectContext.selectedCount > 1; onClicked: drillProjectContext.alignSelected("right") }
            Item { Layout.fillWidth: true }
            AppToolButton { text: "Paths"; checkable: true; checked: drillProjectContext.showTransitionPaths; onToggled: drillProjectContext.showTransitionPaths = checked }
            AppToolButton { text: "Guides"; checkable: true; checked: drillProjectContext.showShapeGuides; onToggled: drillProjectContext.showShapeGuides = checked }
            AppToolButton { text: "Previous"; checkable: true; checked: workspaceSettingsContext.showPreviousFormation; onToggled: workspaceSettingsContext.showPreviousFormation = checked }
            AppToolButton { text: "Grid"; enabled: drillProjectContext.fieldStyle !== "editor"; checkable: true; checked: drillProjectContext.showFieldGrid; onToggled: drillProjectContext.showFieldGrid = checked }
        }
    }
}
