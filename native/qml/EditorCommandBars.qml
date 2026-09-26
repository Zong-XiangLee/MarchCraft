import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

ToolBar {
    property alias shapeButton: shapesButton
    required property var batchDialogContext
    required property var drillProjectContext
    required property var editorActionsPopupContext
    required property var freehandDialogContext
    required property var performerDialogContext
    required property var shapePaletteContext
    required property var windowContext
    required property var workspaceStateContext

    visible: workspaceStateContext.workspaceActive
    height: visible ? 42 : 0
    background: Rectangle { color: MarchCraftTheme.panelHeader; border.color: MarchCraftTheme.divider }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6
        Label {
            text: drillProjectContext.selectedCount > 0 ? drillProjectContext.selectedCount + " SELECTED" : "FORMATION TOOLS"
            color: drillProjectContext.selectedCount > 0 ? MarchCraftTheme.textSecondary : MarchCraftTheme.textMuted
            font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.7; Layout.rightMargin: 6
        }
        AppButton {
            id: shapesButton
            text: windowContext.shapeDrawing.length ? "Shape · " + windowContext.shapeDrawing : "Shapes"
            enabled: drillProjectContext.selectedCount > 0
            highlighted: windowContext.shapeDrawing.length > 0
            onClicked: shapePaletteContext.open()
        }
        AppButton { text: windowContext.freehandDrawing ? "Drawing…" : "Freehand"; enabled: drillProjectContext.selectedCount > 0; highlighted: windowContext.freehandDrawing; onClicked: freehandDialogContext.open() }
        AppToolButton { text: "Snap"; enabled: drillProjectContext.selectedCount > 0; ToolTip.text: "Snap selection to one-step grid"; ToolTip.visible: hovered; onClicked: drillProjectContext.snapSelected(1) }
        AppToolButton { text: "Mirror"; enabled: drillProjectContext.selectedCount > 0; ToolTip.text: "Mirror selection side-to-side"; ToolTip.visible: hovered; onClicked: drillProjectContext.mirrorSelected(true) }
        Item { Layout.fillWidth: true }
        ComboBox {
            objectName: "fieldStyleSelector"
            Layout.preferredWidth: 150
            model: ["Realistic field", "Editor · 8 to 5"]
            currentIndex: drillProjectContext.fieldStyle === "editor" ? 1 : 0
            onActivated: drillProjectContext.fieldStyle = currentIndex === 1 ? "editor" : "realistic"
            ToolTip.visible: hovered
            ToolTip.text: "Field style for both 2D and 3D. Editor: one square = one 22.5-inch step; eight steps = five yards."
        }
        AppToolButton { text: "2D"; checkable: true; checked: !windowContext.threeD; onClicked: windowContext.threeD = false }
        AppToolButton { text: "3D"; checkable: true; checked: windowContext.threeD; onClicked: windowContext.threeD = true }
        AppToolButton { text: "Paths"; checkable: true; checked: drillProjectContext.showTransitionPaths; onToggled: drillProjectContext.showTransitionPaths = checked }
        AppToolButton { text: "Guides"; checkable: true; checked: drillProjectContext.showShapeGuides; onToggled: drillProjectContext.showShapeGuides = checked }
        AppToolButton { text: "Grid"; enabled: drillProjectContext.fieldStyle !== "editor"; checkable: true; checked: drillProjectContext.showFieldGrid; onToggled: drillProjectContext.showFieldGrid = checked }
        AppToolButton {
            ToolTip.text: "More editor actions"; ToolTip.visible: hovered
            contentItem: AppIcon { name: "more" }
            onClicked: editorActionsPopupContext.open()
        }
    }

}
