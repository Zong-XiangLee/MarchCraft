import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    required property var drillProjectContext
    required property var transportContext
    required property var windowContext

    SplitView.preferredWidth: 410
    SplitView.minimumWidth: 330
    SplitView.maximumWidth: 560
    padding: 0
    background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider }

    property string severityFilter: "actionable"
    property string typeFilter: "all"

    function filteredIssues() {
        return drillProjectContext.clinicIssues.filter(function(issue) {
            var severityMatches = root.severityFilter === "all"
                || (root.severityFilter === "actionable" && (issue.severity === "critical" || issue.severity === "caution"))
                || issue.severity === root.severityFilter
            return severityMatches && (root.typeFilter === "all" || issue.type === root.typeFilter)
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 122
            color: MarchCraftTheme.panelHeader
            border.color: MarchCraftTheme.divider
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 7
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 1
                        Label { text: "REVIEW & CLINIC"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 15 }
                        Label { text: "Rehearse, inspect risks, and preview fixes."; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                    }
                    AppButton { text: "2D"; checkable: true; checked: !windowContext.threeD; onClicked: windowContext.threeD = false }
                    AppButton { text: "3D"; checkable: true; checked: windowContext.threeD; onClicked: windowContext.threeD = true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    AppToolButton { text: "|◀"; ToolTip.text: "First set"; ToolTip.visible: hovered; onClicked: transportContext.firstSet() }
                    AppToolButton { text: "◀"; ToolTip.text: "Previous set"; ToolTip.visible: hovered; onClicked: transportContext.previousSet() }
                    AppButton { text: transportContext.playing ? "Pause" : "Play"; highlighted: transportContext.playing; Layout.fillWidth: true; onClicked: transportContext.playPause() }
                    AppToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transportContext.stop() }
                    AppToolButton { text: "▶"; ToolTip.text: "Next set"; ToolTip.visible: hovered; onClicked: transportContext.nextSet() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Capability"; color: MarchCraftTheme.textSecondary; font.pixelSize: 10 }
                    ComboBox {
                        Layout.fillWidth: true
                        textRole: "text"; valueRole: "value"
                        model: [{text:"Beginner",value:"beginner"},{text:"Intermediate",value:"intermediate"},{text:"Advanced",value:"advanced"},{text:"Custom",value:"custom"}]
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(drillProjectContext.capabilityProfile))
                        onActivated: drillProjectContext.capabilityProfile = currentValue
                    }
                    AppButton {
                        text: "Analyze transition"
                        enabled: drillProjectContext.currentSetIndex > 0
                        onClicked: drillProjectContext.analyzeTransition(drillProjectContext.currentSetIndex)
                    }
                    AppButton { text: "Scan show"; highlighted: true; onClicked: drillProjectContext.scanShow() }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 7
            Rectangle {
                Layout.fillWidth: true; height: 48; radius: MarchCraftTheme.radiusSmall
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.clinicIssueCount; color: drillProjectContext.clinicIssueCount ? MarchCraftTheme.warning : MarchCraftTheme.success; font.bold: true; font.pixelSize: 16; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "CLINIC ISSUES"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true }
                }
            }
            Rectangle {
                Layout.fillWidth: true; height: 48; radius: MarchCraftTheme.radiusSmall
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.formatDistance(drillProjectContext.averageDistance); color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "AVG / PERFORMER"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true }
                }
            }
            Rectangle {
                Layout.fillWidth: true; height: 48; radius: MarchCraftTheme.radiusSmall
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.formatDistance(drillProjectContext.longestDistance); color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "LONGEST MOVE"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 8
            ComboBox {
                id: severityChoice
                Layout.fillWidth: true
                textRole: "text"; valueRole: "value"
                model: [{text:"Actionable",value:"actionable"},{text:"Critical",value:"critical"},{text:"Caution",value:"caution"},{text:"All severities",value:"all"},{text:"Info",value:"info"}]
                onActivated: root.severityFilter = currentValue
            }
            ComboBox {
                Layout.fillWidth: true
                textRole: "text"; valueRole: "value"
                model: [{text:"All types",value:"all"},{text:"Stride",value:"stride"},{text:"Collisions",value:"collision"},{text:"Crossings",value:"crossing"},{text:"Direction",value:"direction"},{text:"Spacing",value:"spacing"},{text:"Boundary",value:"boundary"},{text:"Paths",value:"complexPath"}]
                onActivated: root.typeFilter = currentValue
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 8

                ColumnLayout {
                    visible: root.filteredIssues().length === 0
                    Layout.fillWidth: true
                    Layout.topMargin: 32
                    spacing: 6
                    Label {
                        text: drillProjectContext.clinicIssueCount === 0 ? "No active issues" : "No issues match these filters"
                        color: drillProjectContext.clinicIssueCount === 0 ? MarchCraftTheme.success : MarchCraftTheme.textPrimary
                        font.bold: true; font.pixelSize: 17; Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: drillProjectContext.currentSetIndex > 0
                              ? "Analyze this transition or scan the show to refresh Clinic results."
                              : "Choose a destination set, then analyze its incoming transition."
                        color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20
                    }
                }

                Repeater {
                    model: root.filteredIssues()
                    delegate: Frame {
                        id: issueCard
                        required property var modelData
                        property string selectedSuggestionId: modelData.actions && modelData.actions.length > 0 ? modelData.actions[0].id : ""
                        Layout.fillWidth: true
                        Layout.leftMargin: 10; Layout.rightMargin: 10
                        padding: 10
                        background: Rectangle {
                            radius: MarchCraftTheme.radiusLarge
                            color: issueCard.modelData.severity === "critical" ? "#291a20" : issueCard.modelData.severity === "caution" ? "#2a2419" : MarchCraftTheme.surface
                            border.color: issueCard.modelData.severity === "critical" ? MarchCraftTheme.danger : issueCard.modelData.severity === "caution" ? MarchCraftTheme.warning : MarchCraftTheme.accent
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: issueCard.modelData.severity.toUpperCase()
                                    color: issueCard.modelData.severity === "critical" ? MarchCraftTheme.danger : issueCard.modelData.severity === "caution" ? MarchCraftTheme.warning : MarchCraftTheme.accentHover
                                    font.bold: true; font.pixelSize: 9; font.letterSpacing: 0.8
                                }
                                Item { Layout.fillWidth: true }
                                Label { text: "SET " + issueCard.modelData.setLabel; color: MarchCraftTheme.textMuted; font.pixelSize: 9 }
                            }
                            Label { text: issueCard.modelData.title; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 14; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            Label { text: issueCard.modelData.detail; color: MarchCraftTheme.textSecondary; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            Label { visible: issueCard.modelData.performers; text: "Affected: " + issueCard.modelData.performers; color: MarchCraftTheme.textPrimary; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            ComboBox {
                                visible: issueCard.modelData.actions && issueCard.modelData.actions.length > 0
                                Layout.fillWidth: true
                                model: issueCard.modelData.actions || []
                                textRole: "label"
                                onActivated: issueCard.selectedSuggestionId = issueCard.modelData.actions[currentIndex].id
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                AppButton { text: "Inspect"; onClicked: drillProjectContext.selectClinicIssue(issueCard.modelData.id) }
                                AppButton { text: "Preview"; highlighted: true; enabled: issueCard.selectedSuggestionId.length > 0; onClicked: drillProjectContext.previewSuggestion(issueCard.selectedSuggestionId) }
                                AppButton { text: "Apply"; enabled: issueCard.selectedSuggestionId.length > 0; onClicked: drillProjectContext.acceptSuggestion(issueCard.selectedSuggestionId) }
                                Item { Layout.fillWidth: true }
                                AppToolButton { text: "×"; ToolTip.text: "Dismiss until this transition changes"; ToolTip.visible: hovered; onClicked: drillProjectContext.dismissIssue(issueCard.modelData.id) }
                            }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
