import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Rectangle {
    // Explicit dependencies supplied by the application shell.
    required property var drillProjectContext
    required property var qaModeContext
    required property var windowContext
    required property var workspaceControllerContext
    required property var workspaceStateContext
    property alias exposedHomeIntro: homeIntro
    property alias exposedNewProjectHomeButton: newProjectHomeButton
    property alias exposedResumeButton: resumeButton

    id: homePage
    anchors.fill: parent
    enabled: !workspaceStateContext.workspaceActive
    visible: opacity > 0.01
    opacity: workspaceStateContext.workspaceActive ? 0 : 1
    color: MarchCraftTheme.canvas
    transform: Translate {
        y: workspaceStateContext.workspaceActive ? -8 : 0
        Behavior on y { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
    }
    Behavior on opacity { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(1240, parent.width - 64)
        spacing: 22

        RowLayout {
            id: homeLogo
            Layout.fillWidth: true
            opacity: qaModeContext ? 1 : 0
            scale: qaModeContext ? 1 : 0.96
            spacing: 12
            Image { source: "qrc:/branding/marchcraft-logo.png"; sourceSize.width: 52; sourceSize.height: 52; width: 52; height: 52; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
            ColumnLayout {
                spacing: 0
                Label { text: "MARCHCRAFT"; font.family: MarchCraftTheme.fontFamily; font.bold: true; font.pixelSize: 22; font.letterSpacing: 1.5; color: MarchCraftTheme.textPrimary }
                Label { text: "DRILL DESIGN WORKSPACE"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; color: MarchCraftTheme.textMuted }
            }
            Item { Layout.fillWidth: true }
            AppToolButton {
                text: workspaceControllerContext.startupSoundEnabled ? "Sound on" : "Sound off"
                ToolTip.text: "Play a quiet sound when MarchCraft starts"
                ToolTip.visible: hovered
                onClicked: workspaceControllerContext.startupSoundEnabled = !workspaceControllerContext.startupSoundEnabled
            }
        }

        Rectangle {
            visible: workspaceControllerContext.recoveryAvailable && windowContext.homeMode === "dashboard"
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 72 : 0
            radius: MarchCraftTheme.radiusLarge
            color: "#2a2419"
            border.color: MarchCraftTheme.warning
            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12
                Rectangle {
                    width: 34; height: 34; radius: 17; color: MarchCraftTheme.warning
                    Label { anchors.centerIn: parent; text: "↻"; color: MarchCraftTheme.canvas; font.bold: true; font.pixelSize: 18 }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Label { text: "Recover unsaved work"; color: MarchCraftTheme.textPrimary; font.bold: true }
                    Label {
                        text: workspaceControllerContext.recoveryDescription + " · Recovery opens as an unsaved project so you choose where to save it."
                        color: MarchCraftTheme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true
                    }
                }
                AppButton { text: "Discard"; flat: true; onClicked: discardRecoveryDialog.open() }
                AppButton { text: "Recover"; highlighted: true; onClicked: windowContext.recoverProject() }
            }
        }

        GridLayout {
            id: homeContent
            Layout.fillWidth: true
            visible: windowContext.homeMode === "dashboard"
            enabled: windowContext.homeMode === "dashboard"
            columns: 2
            columnSpacing: 24
            rowSpacing: 18
            opacity: windowContext.homeMode === "dashboard" ? (qaModeContext ? 1 : homeContent.baseOpacity) : 0
            property real baseOpacity: 0
            transform: Translate {
                id: homeContentTranslate
                y: windowContext.homeMode === "dashboard" ? 0 : -8
                Behavior on y { NumberAnimation { duration: MarchCraftTheme.motionMedium; easing.type: Easing.OutCubic } }
            }
            Behavior on opacity { NumberAnimation { duration: MarchCraftTheme.motionMedium; easing.type: Easing.OutCubic } }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.panel
                border.color: MarchCraftTheme.divider
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    Label { text: "PROJECTS"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1; color: MarchCraftTheme.textSecondary }
                    Label { text: "Create a show, open a project, or explore the included sample."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: MarchCraftTheme.textSecondary }
                    AppButton { id: newProjectHomeButton; text: "New project"; highlighted: true; Layout.fillWidth: true; onClicked: windowContext.startNewProject() }
                    AppButton { text: "Open project…"; Layout.fillWidth: true; onClicked: windowContext.requestOpenProject() }
                    AppButton {
                        id: resumeButton
                        visible: workspaceStateContext.hasCurrentProject
                        text: "Resume " + drillProjectContext.showName
                        Layout.fillWidth: true
                        onClicked: windowContext.resumeProject()
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider; Layout.topMargin: 4; Layout.bottomMargin: 4 }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: MarchCraftTheme.radiusSmall
                        color: sampleHover.hovered ? MarchCraftTheme.surfaceHover : MarchCraftTheme.surfaceRaised
                        border.color: sampleHover.hovered ? MarchCraftTheme.dividerStrong : MarchCraftTheme.divider
                        HoverHandler { id: sampleHover }
                        TapHandler { onTapped: windowContext.requestSampleProject() }
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 5
                            Label { text: "BUNDLED SAMPLE"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 9; font.letterSpacing: 1.2 }
                            Label { text: "Rancho Bernardo 2025"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 16 }
                            Label { text: "204 performers · 97 sets · Opens as an editable copy"; color: MarchCraftTheme.textSecondary; font.pixelSize: 11 }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.panel
                border.color: MarchCraftTheme.divider
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "RECENT PROJECTS"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1; color: MarchCraftTheme.textSecondary }
                        Item { Layout.fillWidth: true }
                        Label { text: workspaceControllerContext.recentProjects.length + " / 8"; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                    }
                    Label {
                        visible: workspaceControllerContext.recentProjects.length === 0
                        text: "Projects you open or save will appear here."
                        color: MarchCraftTheme.textSecondary
                        Layout.fillWidth: true
                        Layout.topMargin: 18
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: workspaceControllerContext.recentProjects
                        delegate: Rectangle {
                            id: recentRow
                            required property var modelData
                            width: ListView.view.width
                            height: 54
                            radius: MarchCraftTheme.radiusSmall
                            color: recentHover.hovered ? MarchCraftTheme.surfaceHover : "transparent"
                            border.color: recentHover.hovered ? MarchCraftTheme.divider : "transparent"
                            HoverHandler { id: recentHover }
                            TapHandler { onTapped: workspaceStateContext.request("openPath", recentRow.modelData.path, drillProjectContext.dirty) }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6; spacing: 10
                                Rectangle { width: 28; height: 28; radius: MarchCraftTheme.radiusSmall; color: MarchCraftTheme.selection; Label { anchors.centerIn: parent; text: "M"; color: MarchCraftTheme.accentHover; font.bold: true } }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 1
                                    Label { text: recentRow.modelData.name; color: MarchCraftTheme.textPrimary; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label { text: recentRow.modelData.folder; color: MarchCraftTheme.textMuted; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                }
                                AppToolButton {
                                    text: "×"; ToolTip.text: "Remove from recent projects"; ToolTip.visible: hovered
                                    onClicked: workspaceControllerContext.removeRecentProject(recentRow.modelData.path)
                                }
                            }
                        }
                    }
                }
            }
        }

        ProjectSetupPage {
            id: inlineProjectSetup
            Layout.fillWidth: true
            Layout.preferredHeight: 510
            visible: windowContext.homeMode === "new"
            enabled: windowContext.homeMode === "new"
            opacity: windowContext.homeMode === "new" ? 1 : 0
            scale: windowContext.homeMode === "new" ? 1 : 0.985
            onCancelled: { windowContext.homeMode = "dashboard"; Qt.callLater(function() { newProjectHomeButton.forceActiveFocus() }) }
            onQuickStartRequested: function(name, fieldPreset, lightingPreset, performerCount) {
                windowContext.createQuickProject(name, fieldPreset, lightingPreset, performerCount)
            }
            onGuidedCreateRequested: function(name, fieldPreset, lightingPreset, rosterRows, nextWorkspace) {
                windowContext.createGuidedProject(name, fieldPreset, lightingPreset, rosterRows, nextWorkspace)
            }
            Behavior on opacity { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
        }
    }

    Dialog {
        id: discardRecoveryDialog
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        width: 440
        modal: true
        title: "Discard recovered work?"
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                text: "The automatic recovery copy will be permanently removed. Saved project files are not affected."
                color: MarchCraftTheme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; onClicked: discardRecoveryDialog.close() }
                AppButton {
                    text: "Discard recovery"
                    highlighted: true
                    onClicked: {
                        if (workspaceControllerContext.discardRecovery())
                            discardRecoveryDialog.close()
                    }
                }
            }
        }
    }

    SequentialAnimation {
        id: homeIntro
        ParallelAnimation {
            NumberAnimation { target: homeLogo; property: "opacity"; to: 1; duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic }
            NumberAnimation { target: homeLogo; property: "scale"; to: 1; duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 60 }
        ParallelAnimation {
            NumberAnimation { target: homeContent; property: "baseOpacity"; to: 1; duration: MarchCraftTheme.motionMedium; easing.type: Easing.OutCubic }
            NumberAnimation { target: homeContentTranslate; property: "y"; to: 0; duration: MarchCraftTheme.motionMedium; easing.type: Easing.OutCubic }
        }
    }
}
