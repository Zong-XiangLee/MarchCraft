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

    Rectangle {
        width: 640
        height: 640
        radius: 320
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: -260
        anchors.topMargin: -310
        color: "#111a29"
        opacity: 0.72
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(1240, parent.width - 64)
        spacing: 22

        RowLayout {
            id: homeLogo
            Layout.fillWidth: true
            opacity: qaModeContext ? 1 : 0
            scale: qaModeContext ? 1 : 0.96
            spacing: 14
            Image { source: "qrc:/branding/marchcraft-logo.png"; sourceSize.width: 52; sourceSize.height: 52; width: 52; height: 52; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
            ColumnLayout {
                spacing: 0
                Label { text: "MarchCraft"; font.family: MarchCraftTheme.fontFamily; font.bold: true; font.pixelSize: 25; color: MarchCraftTheme.textPrimary }
                Label { text: "Professional drill design workspace"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 12; color: MarchCraftTheme.textSecondary }
            }
            Item { Layout.fillWidth: true }
            AppToolButton {
                text: workspaceControllerContext.startupSoundEnabled ? "Sound on" : "Sound off"
                ToolTip.text: "Play a quiet sound when MarchCraft starts"
                ToolTip.visible: hovered
                onClicked: workspaceControllerContext.startupSoundEnabled = !workspaceControllerContext.startupSoundEnabled
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
                color: MarchCraftTheme.surface
                border.color: MarchCraftTheme.divider
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    Label { text: "Start"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 18; font.bold: true; color: MarchCraftTheme.textPrimary }
                    Label { text: "Create a show, open a project, or explore the included sample."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: MarchCraftTheme.textSecondary }
                    AppButton { id: newProjectHomeButton; text: "New project"; highlighted: true; Layout.fillWidth: true; onClicked: windowContext.startNewProject() }
                    AppButton { text: "Open project…"; Layout.fillWidth: true; onClicked: windowContext.requestOpenProject() }
                    AppButton {
                        id: resumeButton
                        visible: workspaceStateContext.hasCurrentProject
                        text: "Resume " + drillProjectContext.showName
                        Layout.fillWidth: true
                        onClicked: workspaceStateContext.workspaceActive = true
                    }
                    Rectangle {
                        visible: drillProjectContext.recoveryCandidates.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: recoveryColumn.implicitHeight + 20
                        radius: MarchCraftTheme.radiusSmall
                        color: "#2b2419"
                        border.color: MarchCraftTheme.warning
                        ColumnLayout {
                            id: recoveryColumn
                            anchors.fill: parent; anchors.margins: 10; spacing: 6
                            Label { text: "RECOVERY AVAILABLE"; color: MarchCraftTheme.warning; font.bold: true; font.pixelSize: 10 }
                            Repeater {
                                model: drillProjectContext.recoveryCandidates
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label { text: modelData.showName + " · " + modelData.createdUtc; color: MarchCraftTheme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                                    AppToolButton { text: "Restore"; onClicked: { if (drillProjectContext.restoreRecovery(index)) workspaceStateContext.enteredProject() } }
                                    AppToolButton { text: "Discard"; onClicked: drillProjectContext.discardRecovery(index) }
                                }
                            }
                        }
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
                            Label { text: "BUNDLED SAMPLE"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1.2 }
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
                color: MarchCraftTheme.surface
                border.color: MarchCraftTheme.divider
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Recent projects"; font.family: MarchCraftTheme.fontFamily; font.pixelSize: 18; font.bold: true; color: MarchCraftTheme.textPrimary }
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
                                Rectangle { width: 28; height: 28; radius: 6; color: "#243653"; Label { anchors.centerIn: parent; text: "M"; color: MarchCraftTheme.accentHover; font.bold: true } }
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
            Layout.preferredHeight: 420
            visible: windowContext.homeMode === "new"
            enabled: windowContext.homeMode === "new"
            opacity: windowContext.homeMode === "new" ? 1 : 0
            scale: windowContext.homeMode === "new" ? 1 : 0.985
            onCancelled: { windowContext.homeMode = "dashboard"; Qt.callLater(function() { newProjectHomeButton.forceActiveFocus() }) }
            onCreateRequested: function(name, fieldPreset, lightingPreset) { windowContext.createProject(name, fieldPreset, lightingPreset) }
            Behavior on opacity { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: MarchCraftTheme.motionScreen; easing.type: Easing.OutCubic } }
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
