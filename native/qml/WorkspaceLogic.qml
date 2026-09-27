import QtQuick

QtObject {
    id: root
    property bool workspaceActive: false
    property bool hasCurrentProject: false
    property string currentWorkspace: "editor"
    property string lastUsefulWorkspace: "editor"
    property bool awaitingConfirmation: false
    property string pendingAction: ""
    property string pendingPath: ""
    signal actionReady(string action, string path)

    readonly property var workspaceIds: ["roster", "music", "editor", "review", "export"]

    function isWorkspace(value) {
        return workspaceIds.indexOf(value) >= 0
    }

    function reopenWorkspace(value) {
        return isWorkspace(value) && value !== "export" ? value : "editor"
    }

    function switchWorkspace(value) {
        if (!isWorkspace(value))
            return false
        currentWorkspace = value
        if (value !== "export")
            lastUsefulWorkspace = value
        return true
    }

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

    function enteredProject(preferredWorkspace) {
        workspaceActive = true
        hasCurrentProject = true
        pendingAction = ""
        pendingPath = ""
        switchWorkspace(reopenWorkspace(preferredWorkspace || lastUsefulWorkspace))
    }
}
