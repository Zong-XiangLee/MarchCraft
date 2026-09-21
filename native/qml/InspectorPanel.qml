import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Frame {
    // Explicit dependencies supplied by the application shell.
    required property var bulkEditDialogContext
    required property var drillProjectContext
    required property var formationDialogContext
    required property var performerDialogContext
    required property var uniformColorDialogContext
    required property var windowContext
    required property var workspaceSettingsContext
    property alias exposedInspector: inspector

    id: inspectorPanel
    visible: !workspaceSettingsContext.inspectorCollapsed
    SplitView.preferredWidth: 270
    SplitView.minimumWidth: 220
    SplitView.maximumWidth: 480
    padding: 0
    background: Rectangle { color: MarchCraftTheme.panel; radius: 0; border.color: MarchCraftTheme.divider }
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            id: inspector
            width: parent.width
            spacing: 10
            property var person: ({})
            // Start with actionable items. The author can still broaden
            // this to every severity when they want a diagnostic sweep.
            property string clinicSeverityFilter: "critical"
            property string clinicTypeFilter: "all"
            property bool nextSetSuggestionsExpanded: false
            property var nextSetCandidates: []
            function refresh() { person = drillProjectContext.performerInfo(windowContext.activePerformer) }
            function refreshNextSetCandidates() { nextSetCandidates = drillProjectContext.suggestNextSet() }
            Connections {
                target: drillProjectContext
                function onCurrentSetChanged() {
                    inspector.refresh()
                    if (inspector.nextSetSuggestionsExpanded) inspector.refreshNextSetCandidates()
                }
                function onSelectionChanged() {
                    inspector.refresh()
                    if (inspector.nextSetSuggestionsExpanded) inspector.refreshNextSetCandidates()
                }
            }

            RowLayout { Layout.fillWidth: true; Layout.margins: 8
                Label { text: "INSPECTOR"; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1; color: MarchCraftTheme.textSecondary; Layout.fillWidth: true }
                AppToolButton { text: "›"; ToolTip.text: "Collapse inspector"; ToolTip.visible: hovered; onClicked: workspaceSettingsContext.inspectorCollapsed = true }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                visible: drillProjectContext.selectedCount === 1
                Label { text: inspector.person.label || "No performer selected"; font.pixelSize: 22; font.bold: true }
                Label { text: inspector.person.instrument || "Select a performer on the field"; color: "#a5afbc" }
                Label { text: inspector.person.coordinate || ""; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; color: "#b8c8bf"; font.pixelSize: 11 }
                GridLayout {
                    columns: 2; Layout.fillWidth: true
                    Label { text: "Incoming"; color: "#a5afbc" }
                    Label { text: drillProjectContext.formatDistance(inspector.person.incomingDistance || 0); Layout.alignment: Qt.AlignRight }
                    Label { text: "Steps / count"; color: "#a5afbc" }
                    Label { text: Number(inspector.person.stepsPerCount || 0).toFixed(2); Layout.alignment: Qt.AlignRight; color: inspector.person.warning === "critical" ? "#e07178" : inspector.person.warning === "caution" ? "#d6a75d" : "#f2f5f7" }
                    Label { text: "Outgoing"; color: "#a5afbc" }
                    Label { text: drillProjectContext.formatDistance(inspector.person.outgoingDistance || 0); Layout.alignment: Qt.AlignRight }
                    Label { text: "Direction change"; color: "#a5afbc" }
                    Label { text: Number(inspector.person.directionChange || 0).toFixed(0) + " deg"; Layout.alignment: Qt.AlignRight }
                    Label { text: "Incoming path"; color: "#a5afbc" }
                    Label { text: inspector.person.pathType || "direct"; Layout.alignment: Qt.AlignRight }
                    Label { text: "Facing at this set"; color: "#a5afbc" }
                    Label { text: Number(inspector.person.facing || 0).toFixed(0) + " deg"; Layout.alignment: Qt.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true
                    AppButton { Layout.minimumWidth: 0; text: "Front"; Layout.fillWidth: true; onClicked: { drillProjectContext.faceSelected(0); inspector.refresh() } }
                    AppButton { Layout.minimumWidth: 0; text: "Back"; Layout.fillWidth: true; onClicked: { drillProjectContext.faceSelected(180); inspector.refresh() } }
                    AppButton { Layout.minimumWidth: 0; text: "S1"; Layout.fillWidth: true; onClicked: { drillProjectContext.faceSelected(270); inspector.refresh() } }
                    AppButton { Layout.minimumWidth: 0; text: "S2"; Layout.fillWidth: true; onClicked: { drillProjectContext.faceSelected(90); inspector.refresh() } }
                }
                Label { text: "Facing is saved independently for each set."; color: "#778392"; font.pixelSize: 10 }
                AppButton { Layout.minimumWidth: 0; text: "Edit performer…"; enabled: windowContext.activePerformer >= 0; Layout.fillWidth: true; onClicked: { performerDialogContext.editing = true; performerDialogContext.open() } }
                AppButton { Layout.minimumWidth: 0;
                    text: "Marker color…"
                    enabled: windowContext.activePerformer >= 0
                    Layout.fillWidth: true
                    onClicked: {
                        uniformColorDialogContext.selectedColor = inspector.person.color || "#38bdf8"
                        uniformColorDialogContext.open()
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                visible: drillProjectContext.selectedCount === 0
                Label { text: drillProjectContext.currentSetName; font.pixelSize: 20; font.bold: true }
                Label { text: drillProjectContext.currentSetIndex > 0 ? drillProjectContext.currentSetCounts + " counts / " + drillProjectContext.effectiveTempoText(drillProjectContext.currentSetIndex) : "Opening formation"; color: "#a5afbc" }
                Label { text: drillProjectContext.clinicIssueCount ? drillProjectContext.clinicIssueCount + " Clinic issue(s) in view" : "Active transition passes the current profile"; color: drillProjectContext.clinicIssueCount ? "#d6a75d" : "#5ead83"; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true }
                AppButton { Layout.minimumWidth: 0; text: "Scan whole show"; Layout.fillWidth: true; onClicked: drillProjectContext.scanShow() }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#293443" }
            Label { text: "SELECTION"; font.bold: true; color: "#a5afbc"; Layout.leftMargin: 12; visible: false }
            GridLayout {
                visible: false; columns: 3; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                AppButton { Layout.minimumWidth: 0; text: "←"; onClicked: drillProjectContext.nudgeSelected(-0.25, 0) }
                AppButton { Layout.minimumWidth: 0; text: "↑"; onClicked: drillProjectContext.nudgeSelected(0, -0.25) }
                AppButton { Layout.minimumWidth: 0; text: "→"; onClicked: drillProjectContext.nudgeSelected(0.25, 0) }
                AppButton { Layout.minimumWidth: 0; text: "Mirror X"; onClicked: drillProjectContext.mirrorSelected(true) }
                AppButton { Layout.minimumWidth: 0; text: "↓"; onClicked: drillProjectContext.nudgeSelected(0, 0.25) }
                AppButton { Layout.minimumWidth: 0; text: "Snap"; onClicked: drillProjectContext.snapSelected(1) }
            }
            AppButton { Layout.minimumWidth: 0; text: "Auto-label selection"; visible: false; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; onClicked: drillProjectContext.autoLabel("P") }
            AppButton { Layout.minimumWidth: 0; text: "Bulk edit selection…"; visible: drillProjectContext.selectedCount > 0; enabled: visible; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; onClicked: bulkEditDialogContext.open() }
            GridLayout { columns: 2; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; visible: drillProjectContext.selectedCount > 1
                Label { text: "Facing at this set"; color: "#a5afbc"; Layout.fillWidth: true; Layout.columnSpan: 2 }
                AppButton { Layout.minimumWidth: 0; Layout.fillWidth: true; text: "Front"; onClicked: drillProjectContext.faceSelected(0) }
                AppButton { Layout.minimumWidth: 0; Layout.fillWidth: true; text: "Back"; onClicked: drillProjectContext.faceSelected(180) }
                AppButton { Layout.minimumWidth: 0; text: "S1"; onClicked: drillProjectContext.faceSelected(270) }
                AppButton { Layout.minimumWidth: 0; text: "S2"; onClicked: drillProjectContext.faceSelected(90) }
            }
            RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; visible: drillProjectContext.selectedCount > 1
                Label { text: drillProjectContext.selectedCount + " performers"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
                AppButton { Layout.minimumWidth: 0; text: "Optimize..."; onClicked: formationDialogContext.open() }
            }
            Label { text: "FORMATION METRICS"; font.bold: true; color: "#a5afbc"; Layout.leftMargin: 12; visible: drillProjectContext.selectedCount > 1 }
            GridLayout {
                columns: 2; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                visible: drillProjectContext.selectedCount > 1
                property var metrics: drillProjectContext.selectionMetrics
                Label { text: "Formation"; color: "#a5afbc" }
                Label { text: parent.metrics.shapeType ? parent.metrics.shapeType + " / " + parent.metrics.count : parent.metrics.count + " performers"; font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Average spacing"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(parent.metrics.averageSpacing || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Minimum spacing"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(parent.metrics.minimumSpacing || 0); color: (parent.metrics.collisionCount || 0) > 0 ? drillProjectContext.markerWarningColor : "#e5eee9"; font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Average move"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(parent.metrics.averageMove || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Size"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(parent.metrics.width || 0) + " x " + drillProjectContext.formatDistance(parent.metrics.height || 0); font.bold: true; Layout.alignment: Qt.AlignRight }
            }
            Label { text: "TRANSITION PATH"; visible: drillProjectContext.selectedCount > 0; font.bold: true; color: "#a5afbc"; Layout.leftMargin: 12 }
            RowLayout {
                visible: drillProjectContext.selectedCount > 0
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                AppButton { Layout.minimumWidth: 0; text: "Direct"; enabled: drillProjectContext.selectedCount > 0 && drillProjectContext.currentSetIndex > 0; onClicked: drillProjectContext.setSelectedTransitionPath("direct") }
                AppButton { Layout.minimumWidth: 0; text: "Curve"; enabled: drillProjectContext.selectedCount > 0 && drillProjectContext.currentSetIndex > 0; onClicked: drillProjectContext.setSelectedTransitionPath("curved") }
                AppButton { Layout.minimumWidth: 0; text: "Delayed"; enabled: drillProjectContext.selectedCount > 0 && drillProjectContext.currentSetIndex > 0; onClicked: drillProjectContext.setSelectedTransitionPath("delayed") }
            }
            RowLayout {
                visible: drillProjectContext.selectedCount > 0
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                Label { text: drillProjectContext.currentShapeCount + " persistent shape(s)"; color: "#a5afbc"; Layout.fillWidth: true }
                AppButton { Layout.minimumWidth: 0; text: "Bake last"; enabled: drillProjectContext.currentShapeCount > 0; onClicked: drillProjectContext.removeShape(drillProjectContext.currentShapeCount - 1, true) }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#293443" }
            Label { text: "SHOW ANALYTICS"; font.bold: true; color: "#a5afbc"; Layout.leftMargin: 12 }
            GridLayout {
                columns: 2; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; rowSpacing: 10
                Label { text: "Average / performer"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(drillProjectContext.averageDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Ensemble total"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(drillProjectContext.totalDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Longest move"; color: "#a5afbc" }
                Label { text: drillProjectContext.formatDistance(drillProjectContext.longestDistance); font.bold: true; Layout.alignment: Qt.AlignRight }
                Label { text: "Current warnings"; color: "#a5afbc" }
                Label { text: drillProjectContext.warningCount; color: drillProjectContext.warningCount ? "#e07178" : "#5ead83"; font.bold: true; Layout.alignment: Qt.AlignRight }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#293443" }
            RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                Label { text: "DRILL CLINIC"; font.bold: true; color: "#a5afbc"; Layout.fillWidth: true }
                Label { text: "ANALYSIS"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 9; font.letterSpacing: 0.8 }
            }
            Label {
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                text: "Only real rehearsal risks are shown first. Dismiss a false alarm or preview a fix before changing the drill."
                color: "#a9bbb1"; font.pixelSize: 11; wrapMode: Text.Wrap
            }
            RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                ComboBox { Layout.fillWidth: true; currentIndex: 0; model: ["critical","caution","all","info"]; onActivated: inspector.clinicSeverityFilter = currentText }
                ComboBox { Layout.fillWidth: true; model: ["all","stride","collision","equipmentCollision","propCollision","crossing","direction","spacing","boundary","complexPath"]; onActivated: inspector.clinicTypeFilter = currentText }
            }
            Label {
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                visible: drillProjectContext.clinicIssueCount === 0
                text: drillProjectContext.currentSetIndex > 0 ? "No active issues at this capability profile." : "Choose a destination set to analyze its incoming transition."
                color: "#5ead83"
                wrapMode: Text.Wrap
            }
            Repeater {
                model: drillProjectContext.clinicIssues.filter(function(issue) {
                    return (inspector.clinicSeverityFilter === "all" || issue.severity === inspector.clinicSeverityFilter)
                        && (inspector.clinicTypeFilter === "all" || issue.type === inspector.clinicTypeFilter)
                })
                delegate: Frame {
                    Layout.minimumWidth: 0
                    required property var modelData
                    property string selectedSuggestionId: modelData.actions && modelData.actions.length ? modelData.actions[0].id : ""
                    Layout.fillWidth: true; Layout.leftMargin: 10; Layout.rightMargin: 10
                        background: Rectangle { color: modelData.severity === "critical" ? "#291821" : modelData.severity === "caution" ? "#292218" : "#15242b"; border.color: modelData.severity === "critical" ? "#e07178" : modelData.severity === "caution" ? "#d6a75d" : "#38bdf8"; radius: 10 }
                    ColumnLayout {
                        anchors.fill: parent
                        RowLayout { Layout.fillWidth: true
                            Label { text: modelData.severity.toUpperCase(); color: modelData.severity === "critical" ? "#e07178" : "#d6a75d"; font.bold: true; font.pixelSize: 10 }
                            Label { text: "SET " + modelData.setLabel; color: "#a5afbc"; font.pixelSize: 10; Layout.fillWidth: true }
                            AppToolButton { text: "?"; ToolTip.text: "This is a suggestion, not a required change."; ToolTip.visible: hovered }
                        }
                        Label { text: modelData.title; font.bold: true; font.pixelSize: 15; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true }
                        Label { text: modelData.count > 0 ? "First appears around count " + Number(modelData.count).toFixed(1) : "Review the highlighted transition"; color: "#a5afbc"; font.pixelSize: 10 }
                        Label { text: modelData.detail; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; color: "#d3dfd8"; font.pixelSize: 11 }
                        Label { text: modelData.performers ? "Affected: " + modelData.performers : ""; visible: text.length > 0; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; color: "#f1f5f9"; font.pixelSize: 10 }
                        Label { text: modelData.limit > 0 ? "Measured " + Number(modelData.measured).toFixed(2) + " / limit " + Number(modelData.limit).toFixed(2) : "Measured " + Number(modelData.measured).toFixed(2); color: "#a5afbc"; font.pixelSize: 10 }
                        ComboBox { id: clinicAction; visible: modelData.actions && modelData.actions.length > 0; Layout.fillWidth: true; model: modelData.actions || []; textRole: "label"; onActivated: selectedSuggestionId = modelData.actions[currentIndex].id }
                        RowLayout { Layout.fillWidth: true
                            AppButton { Layout.minimumWidth: 0; text: "Inspect"; onClicked: drillProjectContext.selectClinicIssue(modelData.id) }
                            AppButton { Layout.minimumWidth: 0; text: "Preview fix"; highlighted: true; enabled: selectedSuggestionId.length > 0; onClicked: drillProjectContext.previewSuggestion(selectedSuggestionId) }
                            AppButton { Layout.minimumWidth: 0; text: "Apply"; enabled: selectedSuggestionId.length > 0; onClicked: drillProjectContext.acceptSuggestion(selectedSuggestionId) }
                            AppToolButton { text: "x"; ToolTip.text: "Dismiss until this transition changes"; ToolTip.visible: hovered; onClicked: drillProjectContext.dismissIssue(modelData.id) }
                        }
                    }
                }
            }
            RowLayout { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                AppButton { Layout.minimumWidth: 0; text: "Scan show"; Layout.fillWidth: true; onClicked: drillProjectContext.scanShow() }
                AppButton { Layout.minimumWidth: 0;
                    text: inspector.nextSetSuggestionsExpanded ? "Hide next-set ideas" : "Suggest next set"
                    enabled: drillProjectContext.selectedCount > 1
                    Layout.fillWidth: true
                    onClicked: {
                        inspector.nextSetSuggestionsExpanded = !inspector.nextSetSuggestionsExpanded
                        if (inspector.nextSetSuggestionsExpanded) inspector.refreshNextSetCandidates()
                    }
                }
            }
            ColumnLayout {
                visible: inspector.nextSetSuggestionsExpanded
                Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                spacing: 6
                RowLayout { Layout.fillWidth: true
                    Label { text: "NEXT-SET IDEAS"; font.bold: true; color: "#a5afbc"; Layout.fillWidth: true }
                    Label { text: "SUGGESTIONS"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 9; font.letterSpacing: 0.8 }
                }
                Label {
                    text: "Pick a direction, preview it on the field, then keep or discard it. Nothing is committed until you apply it."
                    wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; color: "#a9bbb1"; font.pixelSize: 11
                }
                Repeater {
                    model: inspector.nextSetCandidates
                    delegate: Frame {
                    Layout.minimumWidth: 0
                        required property var modelData
                        Layout.fillWidth: true; padding: 10
                        background: Rectangle {
                            color: modelData.intent === "impact" ? "#21182b" : modelData.intent === "direction" ? "#122431" : "#111b20"
                            border.color: modelData.intent === "impact" ? "#8b5cf6" : modelData.intent === "direction" ? "#38bdf8" : "#2c3d46"
                            radius: 12
                        }
                        RowLayout {
                            anchors.fill: parent; spacing: 8
                            Rectangle {
                                Layout.preferredWidth: 58; Layout.preferredHeight: 58; radius: 10
                                color: "#0b1216"; border.color: "#263b44"
                                Canvas {
                                    anchors.fill: parent; anchors.margins: 8
                                    onPaint: {
                                        var c = getContext("2d"); c.clearRect(0,0,width,height); c.strokeStyle = modelData.intent === "impact" ? "#c084fc" : modelData.intent === "direction" ? "#67e8f9" : "#5ead83"; c.fillStyle = c.strokeStyle; c.lineWidth = 2.5;
                                        var cx = width/2, cy = height/2;
                                        if (modelData.type === "line") { c.beginPath(); c.moveTo(5,cy); c.lineTo(width-5,cy); c.stroke(); }
                                        else if (modelData.type === "arc") { c.beginPath(); c.arc(cx,cy+5,Math.min(width,height)/2-5,Math.PI*1.1,Math.PI*1.9); c.stroke(); }
                                        else if (modelData.type === "circle" || modelData.type === "ellipse") { c.beginPath(); c.ellipse(cx,cy,modelData.type === "ellipse" ? width/2-3 : height/2-5,height/2-5,0,0,Math.PI*2); c.stroke(); }
                                        else if (modelData.type === "spiral") { c.beginPath(); for (var i=0;i<22;i++){var a=i*.65, r=2+i*.7; var x=cx+Math.cos(a)*r, y=cy+Math.sin(a)*r; if(i===0)c.moveTo(x,y);else c.lineTo(x,y);} c.stroke(); }
                                        else { var n=modelData.type === "triangle" ? 3 : modelData.type === "diamond" ? 4 : modelData.type === "star" ? 5 : 6; c.beginPath(); for (var j=0;j<n;j++){var angle=-Math.PI/2+j*Math.PI*2/n, rr=Math.min(width,height)/2-4, px=cx+Math.cos(angle)*rr, py=cy+Math.sin(angle)*rr; if(j===0)c.moveTo(px,py);else c.lineTo(px,py);} c.closePath(); c.stroke(); }
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                RowLayout { Layout.fillWidth: true
                                    Label { text: modelData.label; font.bold: true; font.pixelSize: 14; Layout.fillWidth: true }
                                    Rectangle { implicitWidth: tagLabel.implicitWidth + 12; implicitHeight: 18; radius: 9; color: "#24343a"; Label { id: tagLabel; anchors.centerIn: parent; text: modelData.tag; color: "#a9bbb1"; font.pixelSize: 8; font.bold: true } }
                                }
                                Label { text: modelData.detail; color: "#a5afbc"; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true }
                                Label { text: "Fit " + Number(modelData.score).toFixed(1) + "  ·  " + modelData.intent; color: "#778392"; font.pixelSize: 10 }
                            }
                            AppButton { Layout.minimumWidth: 0;
                                text: drillProjectContext.formationPreviewBusy ? "Optimizing..." : "Preview"
                                enabled: !drillProjectContext.formationPreviewBusy
                                onClicked: drillProjectContext.requestFormationPreview(modelData.type, modelData.options, "rehearsalSafe")
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    AppButton { Layout.minimumWidth: 0; text: "Cancel preview"; enabled: drillProjectContext.formationPreviewActive; onClicked: drillProjectContext.cancelFormationPreview() }
                    Item { Layout.fillWidth: true }
                    AppButton { Layout.minimumWidth: 0; text: "Apply preview"; highlighted: true; enabled: drillProjectContext.formationPreviewActive; onClicked: drillProjectContext.commitFormationPreview() }
                }
            }
            Item { Layout.preferredHeight: 12 }
        }
    }
}
