import QtQuick
import QtQuick.Controls
import QtTest
import "../qml"

TestCase {
    id: testCase
    name: "ShapeDrawer"
    width: 1120; height: 720
    when: windowShown

    QtObject {
        id: drillProject
        property string formationAssignmentMode: "rehearsalSafe"
        property int selectedCount: 12
        property string shapePlacementMode: "selection"
        property real fieldDepthSteps: 84
        property bool formationPreviewActive: false
        property bool formationPreviewBusy: false
        property var formationPreviewMetrics: ({})
        property real maximumStepsPerCount: 2
        property color markerWarningColor: "red"
        function formationDefaults() { return {centerX: shapePlacementMode === "fieldCenter" ? 80 : 0, centerY: 0, width: 32, height: 24, radius: 16} }
        function formationEstimate() { return {estimatedSpacing: 4} }
        function formatDistance(value) { return String(value) }
        function cancelFormationPreview() { formationPreviewActive = false; formationPreviewBusy = false }
        function commitFormationPreview() { return formationPreviewActive }
    }
    ShapeDrawLogic { id: logic }
    FormationDialog { id: builder; parent: testCase }

    function cleanup() { builder.close(); wait(1); drillProject.shapePlacementMode = "selection" }

    function test_assignment_survives_reopen_data() {
        return ["rehearsalSafe", "shortest", "preserveOrder", "evenEffort", "featureMove", "rosterOrder"]
            .map(function(mode) { return {tag: mode, mode: mode} })
    }
    function test_assignment_survives_reopen(data) {
        builder.open(); tryCompare(builder, "opened", true)
        const assignment = findChild(builder, "assignmentMode")
        assignment.currentIndex = assignment.indexOfValue(data.mode)
        assignment.activated(assignment.currentIndex)
        compare(drillProject.formationAssignmentMode, data.mode)
        builder.close(); tryCompare(builder, "visible", false)
        assignment.currentIndex = 0
        builder.open(); tryCompare(builder, "opened", true)
        compare(assignment.currentValue, data.mode)
    }

    function test_drag_preserves_preview_and_stays_in_window() {
        builder.open(); tryCompare(builder, "opened", true)
        const handle = findChild(builder, "dialogDragHandle")
        verify(handle !== null)
        const oldX = builder.x
        drillProject.formationPreviewActive = true
        mousePress(handle, 100, 20)
        mouseMove(handle, 200, 20, 30)
        mouseRelease(handle, 100, 20)
        verify(builder.x > oldX)
        verify(builder.x >= 8 && builder.x + builder.width <= testCase.width - 8)
        verify(drillProject.formationPreviewActive)
        verify(builder.visible)
        compare(builder.dim, false)
    }

    function test_placement_settings_refresh_center() {
        builder.open(); tryCompare(builder, "opened", true)
        drillProject.formationPreviewActive = true
        drillProject.shapePlacementMode = "fieldCenter"
        compare(findChild(builder, "centerX").value, 80)
        compare(drillProject.formationPreviewActive, false)
    }

    function test_drag_options_data() {
        return ["line", "rectangle", "circle", "arc", "ellipse", "triangle", "diamond", "polygon", "star", "spiral", "block"]
            .map(function(kind) { return {tag: kind, kind: kind} })
    }
    function test_drag_options(data) {
        const a = Qt.point(20, 30), b = Qt.point(60, 50)
        const forward = logic.options(data.kind, a, b, 12)
        const reverse = logic.options(data.kind, b, a, 12)
        verify(forward.width > 0); verify(forward.height > 0)
        if (data.kind === "circle" || data.kind === "arc") {
            compare(forward.centerX, 20); compare(forward.centerY, 30)
            fuzzyCompare(forward.radius, Math.sqrt(2000), 0.00001)
        } else {
            compare(forward.centerX, reverse.centerX)
            compare(forward.centerY, reverse.centerY)
            compare(forward.width, reverse.width)
        }
        if (data.kind === "block") { compare(forward.rows, 4); fuzzyCompare(forward.spacing, 20 / 3, 0.00001) }
        if (data.kind === "line") fuzzyCompare(forward.rotation, 26.565051, 0.00001)
    }

    function test_builder_invalidates_every_setting_data() {
        return ["kind", "shapeWidth", "shapeHeight", "radius", "rotation", "startAngle", "sweepAngle",
                "polygonSides", "starPoints", "spiralTurns", "spiralStyle", "spiralDirection", "outerRadius",
                "gridRows", "spacing", "assignmentMode", "centerX", "centerY", "createGroup"]
            .map(function(name) { return {tag: name, control: name} })
    }
    function test_builder_invalidates_every_setting(data) {
        builder.open(); tryCompare(builder, "opened", true)
        compare(findChild(builder, "centerX").value, 0)
        compare(findChild(builder, "centerY").value, 0)
        const control = findChild(builder, data.control)
        verify(control !== null)
        drillProject.formationPreviewActive = true
        drillProject.formationPreviewBusy = true
        if (data.control === "createGroup") control.checked = !control.checked
        else if (["kind", "spiralStyle", "spiralDirection", "assignmentMode"].includes(data.control))
            control.currentIndex = (control.currentIndex + 1) % control.count
        else control.value = control.value === control.to ? control.value - 1 : control.value + 1
        tryCompare(drillProject, "formationPreviewActive", false)
        compare(drillProject.formationPreviewBusy, false)
        compare(findChild(builder, "applyFormation").enabled, false)
    }
    function test_close_cancels_preview() {
        builder.open(); tryCompare(builder, "opened", true)
        drillProject.formationPreviewActive = true
        builder.close(); tryCompare(builder, "opened", false)
        tryCompare(drillProject, "formationPreviewActive", false)
    }
}
