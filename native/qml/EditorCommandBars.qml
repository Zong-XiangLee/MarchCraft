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
        height: visible ? 82 : 0
        background: Rectangle { color: MarchCraftTheme.panelHeader; border.color: MarchCraftTheme.divider }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 46
                Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 8
                AppButton {
                    id: editorHomeButton
                    flat: true
                    implicitWidth: 142
                    contentItem: RowLayout {
                        spacing: 8
                        Image { source: "qrc:/branding/marchcraft-logo.png"; sourceSize.width: 26; sourceSize.height: 26; width: 26; height: 26; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
                        Label { text: "MARCHCRAFT"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 12; font.letterSpacing: 1.1 }
                    }
                    ToolTip.text: "Return Home"
                    ToolTip.visible: hovered
                    onClicked: windowContext.returnHome()
                }
                Rectangle { width: 1; height: 24; color: MarchCraftTheme.divider; Layout.leftMargin: 2; Layout.rightMargin: 4 }
                ColumnLayout {
                    spacing: 0; Layout.maximumWidth: 280
                    Label { text: drillProjectContext.showName; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: drillProjectContext.dirty ? "MODIFIED" : "SAVED"; color: drillProjectContext.dirty ? MarchCraftTheme.warning : MarchCraftTheme.textMuted; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.8 }
                }
                AppToolButton {
                    ToolTip.text: "Undo"; ToolTip.visible: hovered; enabled: drillProjectContext.canUndo
                    contentItem: AppIcon { name: "undo"; iconColor: parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled }
                    onClicked: drillProjectContext.undo()
                }
                AppToolButton {
                    ToolTip.text: "Redo"; ToolTip.visible: hovered; enabled: drillProjectContext.canRedo
                    contentItem: AppIcon { name: "redo"; iconColor: parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled }
                    onClicked: drillProjectContext.redo()
                }
                Item { Layout.fillWidth: true }
                AppButton { text: "Add performer"; onClicked: { performerDialogContext.editing = false; performerDialogContext.open() } }
                AppButton { text: "Batch add"; onClicked: batchDialogContext.open() }
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
                AppToolButton {
                    ToolTip.text: "More editor actions"; ToolTip.visible: hovered
                    contentItem: AppIcon { name: "more" }
                    onClicked: editorActionsPopupContext.open()
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider }
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 34
                Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 6
                Label { text: drillProjectContext.selectedCount > 0 ? drillProjectContext.selectedCount + " SELECTED" : "FORMATION TOOLS"; color: drillProjectContext.selectedCount > 0 ? MarchCraftTheme.textSecondary : MarchCraftTheme.textMuted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.7; Layout.rightMargin: 6 }
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
                AppToolButton { text: "Paths"; checkable: true; checked: drillProjectContext.showTransitionPaths; onToggled: drillProjectContext.showTransitionPaths = checked }
                AppToolButton { text: "Guides"; checkable: true; checked: drillProjectContext.showShapeGuides; onToggled: drillProjectContext.showShapeGuides = checked }
                AppToolButton { text: "Grid"; enabled: drillProjectContext.fieldStyle !== "editor"; checkable: true; checked: drillProjectContext.showFieldGrid; onToggled: drillProjectContext.showFieldGrid = checked }
            }
        }

}
