import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Frame {
    // Explicit dependencies supplied by the application shell.
    required property var drillProjectContext
    required property var fieldContextMenuContext
    required property var inspectorContext
    required property var performerDialogContext
    required property var windowContext
    required property var workspaceSettingsContext
    property alias exposedRoster: roster
    property alias exposedRosterSearch: rosterSearch

    id: rosterPanel
    visible: !workspaceSettingsContext.rosterCollapsed
    SplitView.preferredWidth: 220
    SplitView.minimumWidth: 180
    SplitView.maximumWidth: 460
    padding: 0
    background: Rectangle { color: MarchCraftTheme.panel; radius: 0; border.color: MarchCraftTheme.divider }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            Label { text: "ROSTER"; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1; color: MarchCraftTheme.textSecondary }
            Item { Layout.fillWidth: true }
            Label { text: drillProjectContext.performerCount; color: MarchCraftTheme.textMuted; font.pixelSize: 11 }
            AppToolButton { text: "‹"; ToolTip.text: "Collapse roster"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.rosterCollapsed = true }
        }
        TextField {
                    placeholderTextColor: MarchCraftTheme.textMuted
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
            model: drillProjectContext
            currentIndex: windowContext.activePerformer
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
                        Label { text: rosterDelegate.instrument; color: MarchCraftTheme.textSecondary; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Label { visible: rosterDelegate.hasWarning; text: "⚠"; color: MarchCraftTheme.danger }
                    Label { visible: rosterDelegate.performerLocked; text: "🔒" }
                    Label { visible: !rosterDelegate.performerVisible; text: "◌"; color: MarchCraftTheme.textSecondary }
                    Label { text: rosterDelegate.totalDistance.toFixed(1); color: MarchCraftTheme.textSecondary; font.pixelSize: 11 }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            const group = drillProjectContext.performerGroupInfo(index)
                            if (Object.keys(group).length > 0) drillProjectContext.selectGroupForPerformer(index)
                            else if (!rosterDelegate.isSelected) drillProjectContext.selectPerformerMode(index, 0)
                            windowContext.activePerformer = index; inspectorContext.refresh()
                            const p = mapToItem(windowContext.contentItem, mouse.x, mouse.y)
                            fieldContextMenuContext.popup(p.x, p.y); return
                        }
                        const ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                        const shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
                        if (shift && roster.selectionAnchor >= 0)
                            drillProjectContext.selectPerformerRange(roster.selectionAnchor, index, ctrl)
                        else {
                            drillProjectContext.selectPerformerMode(index, ctrl ? 1 : 0)
                            roster.selectionAnchor = index
                        }
                        windowContext.activePerformer = index
                        inspectorContext.refresh()
                    }
                }
            }
        }
    }
}
