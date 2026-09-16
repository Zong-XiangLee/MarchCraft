import QtQuick
import QtQuick.Controls

Item {
    id: root
    visible: false

    required property var drillProjectContext
    required property var transportContext
    property bool workspaceActive: false
    property bool textEditing: false

    readonly property bool available: workspaceActive && !textEditing
    property alias groupAction: groupAction
    property alias ungroupAction: ungroupAction
    property alias duplicateSetAction: duplicateSetAction
    property alias previousSetAction: previousSetAction
    property alias nextSetAction: nextSetAction
    property alias playPauseAction: playPauseAction
    property alias nudgeQuarterLeftAction: nudgeQuarterLeftAction
    property alias nudgeStepLeftAction: nudgeStepLeftAction

    function nudgeDistance(shifted) { return shifted ? 1.0 : 0.25 }

    Action {
        id: groupAction
        text: "Group"
        shortcut: "Ctrl+G"
        enabled: root.available && root.drillProjectContext.canGroupSelection
        onTriggered: root.drillProjectContext.groupSelected()
    }
    Action {
        id: ungroupAction
        text: "Ungroup"
        shortcut: "Ctrl+Shift+G"
        enabled: root.available && root.drillProjectContext.canUngroupSelection
        onTriggered: root.drillProjectContext.ungroupSelected()
    }
    Action {
        id: duplicateSetAction
        text: "Duplicate set"
        shortcut: "Ctrl+D"
        enabled: root.available && root.drillProjectContext.setCount > 0
        onTriggered: root.drillProjectContext.duplicateCurrentSet()
    }
    Action {
        id: previousSetAction
        text: "Previous set"
        shortcut: "PgUp"
        enabled: root.available && root.drillProjectContext.currentSetIndex > 0
        onTriggered: root.drillProjectContext.currentSetIndex--
    }
    Action {
        id: nextSetAction
        text: "Next set"
        shortcut: "PgDown"
        enabled: root.available && root.drillProjectContext.currentSetIndex + 1 < root.drillProjectContext.setCount
        onTriggered: root.drillProjectContext.currentSetIndex++
    }
    Action {
        id: playPauseAction
        text: "Play / pause"
        shortcut: "Space"
        enabled: root.available && root.drillProjectContext.setCount > 0
        onTriggered: root.transportContext.playPause()
    }

    Action { id: nudgeQuarterLeftAction; shortcut: "Left"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(-0.25, 0) }
    Action { shortcut: "Right"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(0.25, 0) }
    Action { shortcut: "Up"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(0, -0.25) }
    Action { shortcut: "Down"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(0, 0.25) }
    Action { id: nudgeStepLeftAction; shortcut: "Shift+Left"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(-1, 0) }
    Action { shortcut: "Shift+Right"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(1, 0) }
    Action { shortcut: "Shift+Up"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(0, -1) }
    Action { shortcut: "Shift+Down"; enabled: root.available && root.drillProjectContext.selectedCount > 0; onTriggered: root.drillProjectContext.nudgeSelected(0, 1) }
}
