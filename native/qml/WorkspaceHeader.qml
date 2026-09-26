import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    height: 62

    required property var drillProjectContext
    required property var exportControllerContext
    required property var workspaceStateContext
    signal homeRequested()
    signal saveRequested()
    signal workspaceRequested(string workspace)

    readonly property var destinations: [
        {id: "roster", label: "Roster", shortcut: "Alt+1"},
        {id: "music", label: "Music", shortcut: "Alt+2"},
        {id: "editor", label: "Editor", shortcut: "Alt+3"},
        {id: "review", label: "Review", shortcut: "Alt+4"},
        {id: "export", label: "Export", shortcut: "Alt+5"}
    ]
    readonly property bool compact: width < 1300

    function summaryFor(workspace) {
        if (workspace === "roster")
            return drillProjectContext.performerCount + " performers"
        if (workspace === "music") {
            if (drillProjectContext.musicLoaded)
                return drillProjectContext.musicMeasureCount + " measures"
            return drillProjectContext.audioSource ? "Audio attached" : "Optional"
        }
        if (workspace === "editor")
            return drillProjectContext.setCount + " sets"
        if (workspace === "review") {
            if (drillProjectContext.clinicIssueCount > 0)
                return drillProjectContext.clinicIssueCount + " issues"
            return drillProjectContext.warningCount > 0 ? drillProjectContext.warningCount + " warnings" : "Ready to scan"
        }
        return exportControllerContext.busy
            ? Math.round(exportControllerContext.progress * 100) + "%"
            : "Publish"
    }

    background: Rectangle {
        color: MarchCraftTheme.panelHeader
        border.color: MarchCraftTheme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 6

        AppButton {
            id: homeButton
            flat: true
            Layout.preferredWidth: root.compact ? 54 : 150
            Layout.fillHeight: true
            contentItem: RowLayout {
                spacing: 8
                Image {
                    source: "qrc:/branding/marchcraft-logo.png"
                    sourceSize.width: 28
                    sourceSize.height: 28
                    width: 28
                    height: 28
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Label {
                    visible: !root.compact
                    text: "MARCHCRAFT"
                    color: MarchCraftTheme.textPrimary
                    font.bold: true
                    font.pixelSize: 11
                    font.letterSpacing: 1
                }
            }
            ToolTip.text: "Home and project switcher"
            ToolTip.visible: hovered
            Accessible.name: "Home and project switcher"
            onClicked: root.homeRequested()
        }

        Rectangle { width: 1; Layout.fillHeight: true; Layout.topMargin: 12; Layout.bottomMargin: 12; color: MarchCraftTheme.divider }

        ColumnLayout {
            Layout.preferredWidth: root.compact ? 150 : 200
            Layout.minimumWidth: 130
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: root.drillProjectContext.showName
                color: MarchCraftTheme.textPrimary
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
                ToolTip.text: text
                ToolTip.visible: truncated && hoverHandler.hovered
                HoverHandler { id: hoverHandler }
            }
            Label {
                text: root.drillProjectContext.dirty ? "MODIFIED" : "SAVED"
                color: root.drillProjectContext.dirty ? MarchCraftTheme.warning : MarchCraftTheme.textMuted
                font.bold: true
                font.pixelSize: 9
                font.letterSpacing: 0.8
            }
        }

        Repeater {
            model: root.destinations
            delegate: Button {
                id: destinationButton
                required property var modelData
                required property int index
                Layout.fillHeight: true
                Layout.preferredWidth: root.compact ? 88 : 112
                flat: true
                checkable: true
                checked: root.workspaceStateContext.currentWorkspace === modelData.id
                hoverEnabled: true
                Accessible.name: modelData.label + " workspace"
                ToolTip.text: modelData.label + " workspace (" + modelData.shortcut + ")"
                ToolTip.visible: hovered
                contentItem: ColumnLayout {
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: destinationButton.modelData.label
                        color: destinationButton.checked ? MarchCraftTheme.textPrimary : MarchCraftTheme.textSecondary
                        font.bold: destinationButton.checked
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        visible: !root.compact
                        Layout.fillWidth: true
                        text: root.summaryFor(destinationButton.modelData.id)
                        color: destinationButton.checked ? MarchCraftTheme.accentHover : MarchCraftTheme.textMuted
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
                background: Rectangle {
                    color: destinationButton.checked ? MarchCraftTheme.surfaceRaised
                        : destinationButton.hovered ? MarchCraftTheme.surfaceHover : "transparent"
                    radius: MarchCraftTheme.radiusSmall
                    border.color: destinationButton.checked ? MarchCraftTheme.dividerStrong : "transparent"
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 3
                        radius: 2
                        color: MarchCraftTheme.accent
                        visible: destinationButton.checked
                    }
                }
                onClicked: root.workspaceRequested(modelData.id)
            }
        }

        Item { Layout.fillWidth: true }

        AppToolButton {
            ToolTip.text: "Undo"
            ToolTip.visible: hovered
            enabled: root.drillProjectContext.canUndo
            contentItem: AppIcon { name: "undo"; iconColor: parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled }
            onClicked: root.drillProjectContext.undo()
        }
        AppToolButton {
            ToolTip.text: "Redo"
            ToolTip.visible: hovered
            enabled: root.drillProjectContext.canRedo
            contentItem: AppIcon { name: "redo"; iconColor: parent.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled }
            onClicked: root.drillProjectContext.redo()
        }
        AppButton {
            text: root.drillProjectContext.dirty ? "Save" : "Saved"
            highlighted: root.drillProjectContext.dirty
            enabled: root.drillProjectContext.dirty
            Layout.preferredWidth: 72
            onClicked: root.saveRequested()
        }
    }
}
