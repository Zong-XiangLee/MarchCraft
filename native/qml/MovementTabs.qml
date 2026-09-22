import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var project
    spacing: 6
    Label { text: "Movements"; color: MarchCraftTheme.textSecondary; font.pixelSize: 11 }
    ListView {
        id: tabs
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: ListView.Horizontal
        spacing: 4; clip: true
        model: root.project.movements
        currentIndex: root.project.currentMovementIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: AppButton {
            required property var modelData
            required property int index
            objectName: "movementTab" + index
            width: Math.min(220, Math.max(100, implicitWidth)); height: tabs.height
            text: modelData.name
            highlighted: index === root.project.currentMovementIndex
            onClicked: root.project.activateMovement(index)
            ToolTip.visible: hovered; ToolTip.text: modelData.name
        }
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; height: 4 }
    }
    AppButton {
        objectName: "addMovementButton"
        text: "+ Movement"
        onClicked: root.openName("create")
    }
    AppButton { text: "Manage"; onClicked: actions.open() }
    Menu {
        id: actions
        x: root.width - width; y: root.height
        AppMenuItem { text: "Rename movement…"; onTriggered: root.openName("rename") }
        AppMenuItem { text: "Duplicate movement…"; onTriggered: root.openName("duplicate") }
        AppMenuItem { text: "Move left"; enabled: root.project.currentMovementIndex > 0; onTriggered: root.project.moveMovement(root.project.currentMovementIndex, root.project.currentMovementIndex - 1) }
        AppMenuItem { text: "Move right"; enabled: root.project.currentMovementIndex + 1 < root.project.movementCount; onTriggered: root.project.moveMovement(root.project.currentMovementIndex, root.project.currentMovementIndex + 1) }
        MenuSeparator {}
        AppMenuItem { text: "Delete movement…"; enabled: root.project.movementCount > 1; onTriggered: removeDialog.open() }
    }
    function openName(mode) {
        nameDialog.mode = mode
        nameDialog.target = root.project.currentMovementIndex
        nameField.text = mode === "create" ? "Movement " + (root.project.movementCount + 1)
            : root.project.currentMovementName + (mode === "duplicate" ? " copy" : "")
        nameDialog.open(); nameField.forceActiveFocus(); nameField.selectAll()
    }
    Dialog {
        id: nameDialog
        objectName: "movementNameDialog"
        property string mode: "create"
        property int target: 0
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: mode === "rename" ? "Rename movement" : mode === "duplicate" ? "Duplicate movement" : "New movement"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: standardButton(Dialog.Ok).enabled = nameField.text.trim().length > 0
        onAccepted: {
            if (mode === "rename") root.project.renameMovement(target, nameField.text)
            else root.project.createMovement(nameField.text, mode === "duplicate")
        }
        ColumnLayout {
            width: parent.width
            TextField {
                id: nameField
                objectName: "movementNameField"
                Layout.fillWidth: true; maximumLength: 80
                placeholderText: "Movement name"
                onTextChanged: if (nameDialog.visible) nameDialog.standardButton(Dialog.Ok).enabled = text.trim().length > 0
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: nameDialog.mode === "create" ? "Starts with the current formation and an empty music timeline. Performers and field settings are shared across movements."
                    : nameDialog.mode === "duplicate" ? "Copies this movement’s pages, music, and timing into an independent editor." : "Movement names appear on the editor tabs."
                color: MarchCraftTheme.textSecondary
            }
        }
    }
    Dialog {
        id: removeDialog
        implicitHeight: 210
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(400, parent.width - 32); modal: true
        title: "Delete movement?"
        standardButtons: Dialog.Yes | Dialog.Cancel
        contentItem: Label { wrapMode: Text.WordWrap; text: "Delete “" + root.project.currentMovementName + "” and its pages and music? You can undo this." }
        onAccepted: root.project.removeMovement(root.project.currentMovementIndex)
    }
}
