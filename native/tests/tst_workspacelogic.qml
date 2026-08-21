import QtQuick
import QtTest
import "../qml"

TestCase {
    name: "WorkspaceLogic"

    WorkspaceLogic { id: logic }
    SignalSpy { id: actionSpy; target: logic; signalName: "actionReady" }

    function init() {
        logic.workspaceActive = false
        logic.hasCurrentProject = false
        logic.awaitingConfirmation = false
        logic.pendingAction = ""
        logic.pendingPath = ""
        actionSpy.clear()
    }

    function test_clean_start_and_immediate_action() {
        compare(logic.workspaceActive, false)
        compare(logic.hasCurrentProject, false)
        verify(logic.request("new", "", false))
        compare(actionSpy.count, 1)
        compare(logic.pendingAction, "new")
    }

    function test_dirty_project_guard_cancel_discard_and_save() {
        logic.hasCurrentProject = true
        verify(!logic.request("openPath", "C:/show.marchcraft", true))
        verify(logic.awaitingConfirmation)
        compare(actionSpy.count, 0)

        logic.cancel()
        verify(!logic.awaitingConfirmation)
        compare(logic.pendingAction, "")

        verify(!logic.request("sample", "", true))
        logic.confirmDiscard()
        compare(actionSpy.count, 1)

        actionSpy.clear()
        verify(!logic.request("exit", "", true))
        logic.confirmAfterSave()
        compare(actionSpy.count, 1)
    }

    function test_entered_project_enables_resume_state() {
        logic.enteredProject()
        verify(logic.workspaceActive)
        verify(logic.hasCurrentProject)
    }
}
