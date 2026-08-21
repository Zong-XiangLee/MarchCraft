import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: "Smart shape builder"
    modal: true
    standardButtons: Dialog.Close
    width: 500
    signal settingsRequested()

    property var estimate: drillProject.formationEstimate(kind.currentValue || "line", {
        width: shapeWidth.value, height: shapeHeight.value, radius: radius.value,
        sweepAngle: sweepAngle.value, turns: spiralTurns.value / 2,
        outerRadius: outerRadius.value, spacing: spacing.value,
        placementMode: drillProject.shapePlacementMode
    })

    function reloadDefaults() {
        const defaults = drillProject.formationDefaults(kind.currentValue || "line", drillProject.shapePlacementMode)
        centerX.value = Math.round(defaults.centerX || 80)
        centerY.value = Math.round(defaults.centerY || 42)
        shapeWidth.value = Math.round(defaults.width || 32)
        shapeHeight.value = Math.round(defaults.height || 24)
        radius.value = Math.round(defaults.radius || 16)
        rotation.value = Math.round(defaults.rotation || 0)
        startAngle.value = Math.round(defaults.startAngle || 0)
        sweepAngle.value = Math.round(defaults.sweepAngle || 180)
        polygonSides.value = defaults.sides || 6
        starPoints.value = defaults.points || 5
        spiralTurns.value = Math.round((defaults.turns || 2) * 2)
        spiralStyle.currentIndex = Math.max(0, spiralStyle.indexOfValue(defaults.spiralStyle || "drill"))
        outerRadius.value = Math.round(defaults.outerRadius || 16)
        gridRows.value = defaults.rows || 2
        spacing.value = Math.round(defaults.spacing || 4)
    }

    onOpened: { drillProject.cancelFormationPreview(); reloadDefaults(); assignmentMode.currentIndex = 0 }
    onClosed: drillProject.cancelFormationPreview()

    contentItem: ScrollView {
        implicitHeight: 580
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 10
            Label { text: drillProject.selectedCount + " selected performers"; color: "#a9bbb1" }
            ComboBox {
                id: kind
                Layout.fillWidth: true
                textRole: "text"; valueRole: "type"
                model: [
                    {text: "Line", type: "line"}, {text: "Circle", type: "circle"},
                    {text: "Arc", type: "arc"}, {text: "Ellipse", type: "ellipse"},
                    {text: "Rectangle", type: "rectangle"}, {text: "Triangle", type: "triangle"},
                    {text: "Diamond", type: "diamond"}, {text: "Regular polygon", type: "polygon"},
                    {text: "Star", type: "star"}, {text: "Spiral", type: "spiral"},
                    {text: "Block grid", type: "block"}
                ]
                onActivated: dialog.reloadDefaults()
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: "Placement: " + (drillProject.shapePlacementMode === "openSpace" ? "Nearest open space" :
                          drillProject.shapePlacementMode === "fieldCenter" ? "Field center" : "Selection centered")
                    color: "#a5afbc"
                }
                AppButton { text: "Settings…"; flat: true; onClicked: dialog.settingsRequested() }
            }

            GridLayout {
                columns: 2; Layout.fillWidth: true
                Label { text: "Width"; visible: !["circle", "arc", "spiral", "block"].includes(kind.currentValue) }
                SpinBox { id: shapeWidth; from: 2; to: 156; editable: true; Layout.fillWidth: true; visible: !["circle", "arc", "spiral", "block"].includes(kind.currentValue) }
                Label { text: "Height"; visible: ["ellipse", "rectangle"].includes(kind.currentValue) }
                SpinBox { id: shapeHeight; from: 2; to: 81; editable: true; Layout.fillWidth: true; visible: ["ellipse", "rectangle"].includes(kind.currentValue) }
                Label { text: "Radius"; visible: ["circle", "arc"].includes(kind.currentValue) }
                SpinBox { id: radius; from: 1; to: 78; editable: true; Layout.fillWidth: true; visible: ["circle", "arc"].includes(kind.currentValue) }
                Label { text: "Start angle"; visible: kind.currentValue === "arc" }
                SpinBox { id: startAngle; from: -360; to: 360; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "arc" }
                Label { text: "Sweep angle"; visible: kind.currentValue === "arc" }
                SpinBox { id: sweepAngle; from: -360; to: 360; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "arc" }
                Label { text: "Polygon sides"; visible: kind.currentValue === "polygon" }
                SpinBox { id: polygonSides; from: 3; to: 12; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "polygon" }
                Label { text: "Star points"; visible: kind.currentValue === "star" }
                SpinBox { id: starPoints; from: 3; to: 12; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "star" }
                Label { text: "Spiral turns"; visible: kind.currentValue === "spiral" }
                SpinBox {
                    id: spiralTurns; from: 1; to: 60; stepSize: 1; editable: true; Layout.fillWidth: true
                    visible: kind.currentValue === "spiral"
                    textFromValue: function(value) { return (value / 2).toFixed(1) }
                    valueFromText: function(text) { return Math.round(Number(text) * 2) }
                }
                Label { text: "Spiral style"; visible: kind.currentValue === "spiral" }
                ComboBox {
                    id: spiralStyle; Layout.fillWidth: true; visible: kind.currentValue === "spiral"
                    textRole: "text"; valueRole: "value"
                    model: [{text: "Drill spiral", value: "drill"},
                            {text: "Golden ratio", value: "golden"},
                            {text: "Galaxy arm", value: "galaxy"}]
                }
                Label { text: "Direction"; visible: kind.currentValue === "spiral" }
                ComboBox { id: spiralDirection; Layout.fillWidth: true; visible: kind.currentValue === "spiral"; model: ["Counterclockwise", "Clockwise"] }
                Label { text: "Core radius"; visible: kind.currentValue === "spiral" }
                Label { text: spiralStyle.currentValue === "golden" ? "Golden ratio derived" : spiralStyle.currentValue === "galaxy" ? "Two arms; extra turns limited by radius" : "Automatic tight core"; color: "#a5afbc"; visible: kind.currentValue === "spiral" }
                Label { text: "Outer radius"; visible: kind.currentValue === "spiral" }
                SpinBox { id: outerRadius; from: 1; to: 78; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "spiral" }
                Label { text: "Rows"; visible: kind.currentValue === "block" }
                SpinBox { id: gridRows; from: 1; to: Math.max(1, drillProject.selectedCount); editable: true; Layout.fillWidth: true; visible: kind.currentValue === "block" }
                Label { text: "Spacing (steps)"; visible: kind.currentValue === "block" }
                SpinBox { id: spacing; from: 1; to: 16; editable: true; Layout.fillWidth: true; visible: kind.currentValue === "block" }
                Label { text: "Rotation"; visible: !["circle", "arc", "block"].includes(kind.currentValue) }
                SpinBox { id: rotation; from: -360; to: 360; editable: true; Layout.fillWidth: true; visible: !["circle", "arc", "block"].includes(kind.currentValue) }
            }

            Label { text: "Performer assignment"; font.bold: true }
            ComboBox {
                id: assignmentMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"
                model: [
                    {text:"Rehearsal safe (recommended)",value:"rehearsalSafe"},
                    {text:"Shortest total distance",value:"shortest"},
                    {text:"Preserve form order / morph",value:"preserveOrder"},
                    {text:"Even effort",value:"evenEffort"},
                    {text:"Feature move",value:"featureMove"},
                    {text:"Roster order (legacy)",value:"rosterOrder"}
                ]
            }
            CheckBox { id: advanced; text: "Advanced position" }
            GridLayout {
                columns: 2; Layout.fillWidth: true; visible: advanced.checked
                Label { text: "Center X" }
                SpinBox { id: centerX; from: -8; to: 168; editable: true; Layout.fillWidth: true }
                Label { text: "Center Y" }
                SpinBox { id: centerY; from: -8; to: Math.round(drillProject.fieldDepthSteps + 8); editable: true; Layout.fillWidth: true }
            }
            CheckBox { id: createGroup; text: "Create as group"; checked: false }
            Frame {
                Layout.fillWidth: true
                background: Rectangle { color: "#111b20"; border.color: "#2c3d46"; radius: 6 }
                GridLayout {
                    anchors.fill: parent; columns: 2
                    property var metrics: drillProject.formationPreviewActive ? drillProject.formationPreviewMetrics : dialog.estimate
                    Label { text: drillProject.formationPreviewActive ? "Preview average move" : "Estimated equal spacing"; color: "#a5afbc" }
                    Label { text: drillProject.formatDistance(drillProject.formationPreviewActive ? (parent.metrics.averageMove || 0) : (parent.metrics.estimatedSpacing || 0)); font.bold: true; Layout.alignment: Qt.AlignRight }
                    Label { text: "Maximum move"; color: "#a5afbc"; visible: drillProject.formationPreviewActive }
                    Label { text: drillProject.formatDistance(parent.metrics.maximumMove || 0); font.bold: true; Layout.alignment: Qt.AlignRight; visible: drillProject.formationPreviewActive }
                    Label { text: "Max steps / count"; color: "#a5afbc"; visible: drillProject.formationPreviewActive }
                    Label { text: Number(parent.metrics.maximumStepsPerCount || 0).toFixed(2); color: (parent.metrics.maximumStepsPerCount || 0) > drillProject.maximumStepsPerCount ? drillProject.markerWarningColor : "#5ead83"; font.bold: true; Layout.alignment: Qt.AlignRight; visible: drillProject.formationPreviewActive }
                    Label { text: "Crossings / collisions"; color: "#a5afbc"; visible: drillProject.formationPreviewActive }
                    Label { text: (parent.metrics.crossings || 0) + " / " + (parent.metrics.predictedCollisions || 0); font.bold: true; Layout.alignment: Qt.AlignRight; visible: drillProject.formationPreviewActive }
                    Label { text: "Minimum spacing"; color: "#a5afbc"; visible: drillProject.formationPreviewActive }
                    Label { text: drillProject.formatDistance(parent.metrics.minimumSpacing || 0); font.bold: true; Layout.alignment: Qt.AlignRight; visible: drillProject.formationPreviewActive }
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#a5afbc"
                text: "The entire formation is fitted onto the field before performers are placed, so points never collapse at a sideline or corner."
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: "Cancel preview"; visible: drillProject.formationPreviewActive || drillProject.formationPreviewBusy; onClicked: drillProject.cancelFormationPreview() }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: drillProject.formationPreviewBusy ? "Optimizing..." : drillProject.formationPreviewActive ? "Try another mode" : "Preview formation"
                    enabled: drillProject.selectedCount > 0 && !drillProject.formationPreviewBusy
                    onClicked: drillProject.requestFormationPreview(kind.currentValue, {
                        placementMode: drillProject.shapePlacementMode,
                        centerX: centerX.value, centerY: centerY.value,
                        width: shapeWidth.value, height: shapeHeight.value, radius: radius.value,
                        rotation: rotation.value, startAngle: startAngle.value, sweepAngle: sweepAngle.value,
                        sides: polygonSides.value, points: starPoints.value, turns: spiralTurns.value / 2,
                        clockwise: spiralDirection.currentIndex === 1, spiralStyle: spiralStyle.currentValue, createGroup: createGroup.checked,
                        outerRadius: outerRadius.value,
                        rows: gridRows.value, spacing: spacing.value
                    }, assignmentMode.currentValue)
                }
                AppButton { text: "Apply"; highlighted: true; enabled: drillProject.formationPreviewActive; onClicked: { drillProject.commitFormationPreview(); dialog.close() } }
            }
        }
    }
}
