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
        height: visible ? 94 : 0
        background: Rectangle { color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 52
                Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 8
                AppButton {
                    id: editorHomeButton
                    flat: true
                    implicitWidth: 126
                    contentItem: RowLayout {
                        spacing: 8
                        Image { source: "qrc:/branding/marchcraft-logo.png"; sourceSize.width: 26; sourceSize.height: 26; width: 26; height: 26; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
                        Label { text: "MarchCraft"; color: MarchCraftTheme.textPrimary; font.bold: true }
                    }
                    ToolTip.text: "Return Home"
                    ToolTip.visible: hovered
                    onClicked: windowContext.returnHome()
                }
                Rectangle { width: 1; height: 24; color: MarchCraftTheme.divider; Layout.leftMargin: 2; Layout.rightMargin: 4 }
                ColumnLayout {
                    spacing: 0; Layout.maximumWidth: 280
                    Label { text: drillProjectContext.showName; color: MarchCraftTheme.textPrimary; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: drillProjectContext.dirty ? "Unsaved changes" : "All changes saved"; color: drillProjectContext.dirty ? MarchCraftTheme.warning : MarchCraftTheme.textMuted; font.pixelSize: 10 }
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
                AppButton { text: "+ Performer"; onClicked: { performerDialogContext.editing = false; performerDialogContext.open() } }
                AppButton { text: "+ Batch"; onClicked: batchDialogContext.open() }
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
                Layout.fillWidth: true; Layout.preferredHeight: 41
                Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 6
                Label { text: drillProjectContext.selectedCount > 0 ? drillProjectContext.selectedCount + " selected" : "Select performers to edit formations"; color: drillProjectContext.selectedCount > 0 ? MarchCraftTheme.textSecondary : MarchCraftTheme.textMuted; font.pixelSize: 11; Layout.rightMargin: 6 }
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
                AppToolButton { text: "Grid"; checkable: true; checked: drillProjectContext.showFieldGrid; onToggled: drillProjectContext.showFieldGrid = checked }
            }
        }

}
