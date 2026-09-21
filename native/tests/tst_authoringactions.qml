import QtQuick
import QtTest
import "../qml"

TestCase {
    name: "AuthoringActions"

    QtObject {
        id: project
        property bool canGroupSelection: true
        property bool canUngroupSelection: false
        property int selectedCount: 3
        property int setCount: 4
        property int currentSetIndex: 1
        property int groupCalls: 0
        property real lastNudgeX: 0
        function groupSelected() { groupCalls++ }
        function ungroupSelected() {}
        function duplicateCurrentSet() {}
        function nudgeSelected(x, y) { lastNudgeX = x }
    }
    QtObject { id: transport; function playPause() {} }
    AuthoringActions {
        id: actions
        drillProjectContext: project
        transportContext: transport
        workspaceActive: true
    }

    function init() {
        actions.textEditing = false
        actions.workspaceActive = true
        project.canGroupSelection = true
        project.selectedCount = 3
        project.groupCalls = 0
        project.lastNudgeX = 0
    }

    function test_enabled_state_suppresses_text_editing() {
        verify(actions.groupAction.enabled)
        actions.textEditing = true
        verify(!actions.groupAction.enabled)
        verify(!actions.playPauseAction.enabled)
        verify(!actions.nudgeQuarterLeftAction.enabled)
    }

    function test_modifier_resolution() {
        compare(actions.nudgeDistance(false), 0.25)
        compare(actions.nudgeDistance(true), 1.0)
        actions.nudgeQuarterLeftAction.trigger()
        compare(project.lastNudgeX, -0.25)
        actions.nudgeStepLeftAction.trigger()
        compare(project.lastNudgeX, -1.0)
    }

    function test_group_action() {
        actions.groupAction.trigger()
        compare(project.groupCalls, 1)
    }
}
