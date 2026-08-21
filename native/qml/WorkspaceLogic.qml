import QtQuick

QtObject {
    id: root
    property bool workspaceActive: false
    property bool hasCurrentProject: false
    property bool awaitingConfirmation: false
    property string pendingAction: ""
    property string pendingPath: ""
    signal actionReady(string action, string path)

    function request(action, path, dirty) {
        pendingAction = action
        pendingPath = path || ""
        if (dirty && hasCurrentProject) {
            awaitingConfirmation = true
            return false
        }
        awaitingConfirmation = false
        actionReady(pendingAction, pendingPath)
        return true
    }

    function confirmAfterSave() {
        awaitingConfirmation = false
        actionReady(pendingAction, pendingPath)
    }

    function confirmDiscard() {
        awaitingConfirmation = false
        actionReady(pendingAction, pendingPath)
    }

    function cancel() {
        awaitingConfirmation = false
        pendingAction = ""
        pendingPath = ""
    }

    function enteredProject() {
        workspaceActive = true
        hasCurrentProject = true
        pendingAction = ""
        pendingPath = ""
    }
}
