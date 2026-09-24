import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Dialog {
    // Explicit dependencies supplied by the application shell.
    required property var drillProjectContext
    required property var fieldViewContext
    required property var windowContext
    required property var workspaceControllerContext

    id: settingsDialog
    title: "Configure MarchCraft"
    modal: true; anchors.centerIn: Overlay.overlay; width: 680; height: Math.min(windowContext.height - 64, 700)
    standardButtons: Dialog.Close
    onOpened: placementMode.currentIndex = placementMode.indexOfValue(drillProjectContext.shapePlacementMode)
    contentItem: ColumnLayout {
        TabBar { id: configureTabs; Layout.fillWidth: true; TabButton { text: "General" } TabButton { text: "Performers" } TabButton { text: "Field & Grid" } TabButton { text: "Overlays" } TabButton { text: "Formations" } TabButton { text: "Quick Actions" } TabButton { text: "Drill Clinic" } }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: configureTabs.currentIndex
            clip: true
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                Label { text: "Workspace"; font.pixelSize: 18; font.bold: true; color: MarchCraftTheme.textPrimary }
                Label { text: "Appearance"; color: MarchCraftTheme.textSecondary }
                ComboBox {
                    Layout.fillWidth: true
                    model: MarchCraftTheme.themes
                    textRole: "text"; valueRole: "value"
                    currentIndex: Math.max(0, indexOfValue(MarchCraftTheme.themeId))
                    onActivated: MarchCraftTheme.themeId = currentValue
                }
                Label {
                    text: "Themes apply immediately and are remembered on this computer."
                    color: MarchCraftTheme.textMuted
                }
                CheckBox {
                    text: "Play the MarchCraft startup sound"
                    checked: workspaceControllerContext.startupSoundEnabled
                    onToggled: workspaceControllerContext.startupSoundEnabled = checked
                }
                Label {
                    text: "The short launch sound plays once when the welcome screen first opens. Automated QA runs stay silent."
                    color: MarchCraftTheme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Item { Layout.fillHeight: true }
            }
            GridLayout {
                columns: 2
                Label { text: "Marker shape" }
                ComboBox {
                    Layout.fillWidth: true; textRole: "text"; valueRole: "value"
                    model: [{text:"Dot (compact)",value:"dot"},{text:"Circle",value:"circle"},{text:"Square",value:"square"},{text:"Diamond",value:"diamond"}]
                    Component.onCompleted: currentIndex=Math.max(0,indexOfValue(drillProjectContext.markerGeometry))
                    onActivated: drillProjectContext.markerGeometry=currentValue
                }
                Label { text: "Fill" }
                TextField { Layout.fillWidth: true; text: drillProjectContext.markerFillColor; placeholderText: "section, black, or #RRGGBB"; onEditingFinished: drillProjectContext.markerFillColor=text }
                Label { text: "Size" }
                Slider { Layout.fillWidth: true; from: 8; to: 28; stepSize: 1; value: drillProjectContext.performerMarkerSize; onMoved: drillProjectContext.performerMarkerSize=Math.round(value) }
                Label { text: "Outline color" }
                TextField { Layout.fillWidth: true; text: drillProjectContext.markerOutlineColor; onEditingFinished: drillProjectContext.markerOutlineColor=text }
                Label { text: "Outline width" }
                SpinBox { from: 0; to: 5; value: drillProjectContext.markerOutlineWidth; onValueModified: drillProjectContext.markerOutlineWidth=value }
                Label { text: "Labels" }
                ComboBox { Layout.fillWidth: true; model: ["off","selected","adaptive","always"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProjectContext.markerLabelMode)); onActivated: drillProjectContext.markerLabelMode=currentText }
                Label { text: "Label color" }
                TextField { Layout.fillWidth: true; text: drillProjectContext.markerLabelColor; onEditingFinished: drillProjectContext.markerLabelColor=text }
                Label { text: "Label font size" }
                RowLayout {
                    Layout.fillWidth: true
                    Slider { id: labelSizeSlider; Layout.fillWidth: true; from: 7; to: 32; stepSize: 1; value: drillProjectContext.markerLabelFontSize; onMoved: drillProjectContext.markerLabelFontSize = Math.round(value) }
                    Label { text: Math.round(labelSizeSlider.value) + " px"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight; color: MarchCraftTheme.textSecondary }
                }
                Label { text: "Warning color" }
                TextField { Layout.fillWidth: true; text: drillProjectContext.markerWarningColor; onEditingFinished: drillProjectContext.markerWarningColor=text }
                CheckBox { text: "Show facing indicator"; checked: drillProjectContext.markerFacingVisible; onToggled: drillProjectContext.markerFacingVisible=checked }
                TextField { Layout.fillWidth: true; text: drillProjectContext.markerFacingColor; onEditingFinished: drillProjectContext.markerFacingColor=text }
            }
            GridLayout {
                columns: 2
                Label { text: "Field style (2D + 3D)" }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["Realistic field", "Editor · 8 to 5"]
                    currentIndex: drillProjectContext.fieldStyle === "editor" ? 1 : 0
                    onActivated: drillProjectContext.fieldStyle = currentIndex === 1 ? "editor" : "realistic"
                }
                Label { Layout.columnSpan: 2; text: "Editor grid: 1 square = 1 step · 8 steps = 5 yards"; color: MarchCraftTheme.textSecondary }
                Label { text: "Field preset" }
                ComboBox { Layout.fillWidth: true; model: ["hs","college","nfl","indoor"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProjectContext.fieldPreset)); onActivated: drillProjectContext.fieldPreset=currentText }
                CheckBox { text: "Overlay grid (realistic)"; enabled: drillProjectContext.fieldStyle !== "editor"; checked: drillProjectContext.showFieldGrid; onToggled: drillProjectContext.showFieldGrid=checked }
                Item {}
                Button { text: "Standard 8-to-5 grid"; onClicked: { drillProjectContext.showFieldGrid=true; drillProjectContext.fieldGridInterval=1; drillProjectContext.fieldGridColor="#b8b8b8"; drillProjectContext.fieldGridOpacity=0.35 } }
                Item {}
                Label { text: "Custom realistic grid interval" }
                ComboBox { Layout.fillWidth: true; model: ["4","2","1","0.5","0.25"]; Component.onCompleted: currentIndex=Math.max(0,find(drillProjectContext.fieldGridInterval.toString())); onActivated: drillProjectContext.fieldGridInterval=Number(currentText) }
                Label { text: "Grid color" }
                TextField { Layout.fillWidth: true; text: drillProjectContext.fieldGridColor; onEditingFinished: drillProjectContext.fieldGridColor=text }
                Label { text: "Grid opacity" }
                Slider { Layout.fillWidth: true; from: .02; to: .8; value: drillProjectContext.fieldGridOpacity; onMoved: drillProjectContext.fieldGridOpacity=value }
                Label { text: "Measurement display" }
                ComboBox { Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Marching steps",value:"steps"},{text:"Yards",value:"yards"}]; Component.onCompleted: currentIndex=Math.max(0,indexOfValue(drillProjectContext.measurementUnit)); onActivated: drillProjectContext.measurementUnit=currentValue }
                CheckBox { text: "Enable snapping"; checked: fieldViewContext.snapEnabled; onToggled: fieldViewContext.snapEnabled=checked }
                ComboBox { Layout.fillWidth: true; model: ["4","2","1","0.5","0.25"]; Component.onCompleted: currentIndex=2; onActivated: fieldViewContext.gridSize=Number(currentText) }
            }
            ColumnLayout {
                CheckBox { text: "Show transition paths"; checked: drillProjectContext.showTransitionPaths; onToggled: drillProjectContext.showTransitionPaths=checked }
                CheckBox { text: "Show shape guides"; checked: drillProjectContext.showShapeGuides; onToggled: drillProjectContext.showShapeGuides=checked }
                Item { Layout.fillHeight: true }
            }
            ColumnLayout {
                Label { text: "Formation placement"; font.bold: true }
                ComboBox { id: placementMode; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Selection centered",value:"selection"},{text:"Nearest open space",value:"openSpace"},{text:"Field centered",value:"fieldCenter"}]; onActivated: drillProjectContext.shapePlacementMode=currentValue }
                Label { text: "Default spacing: four marching steps"; color: MarchCraftTheme.textSecondary }
                Item { Layout.fillHeight: true }
            }
            ColumnLayout {
                Label { text: "Shape quick actions"; font.bold: true; font.pixelSize: 16 }
                Label { text: "Check a shape to show its icon beside the Shapes button in the field toolbar."; color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Repeater {
                    model: [{text: "Line", kind: "line"}, {text: "Circle", kind: "circle"},
                            {text: "Arc", kind: "arc"}, {text: "Ellipse", kind: "ellipse"},
                            {text: "Rectangle", kind: "rectangle"}, {text: "Triangle", kind: "triangle"},
                            {text: "Diamond", kind: "diamond"}, {text: "Regular polygon", kind: "polygon"},
                            {text: "Star", kind: "star"}, {text: "Spiral", kind: "spiral"},
                            {text: "Block grid", kind: "block"}]
                    delegate: CheckBox {
                        required property var modelData
                        text: modelData.text
                        checked: windowContext.quickShapes.indexOf(modelData.kind) >= 0
                        onClicked: windowContext.toggleQuickShape(modelData.kind)
                    }
                }
                Label { text: "Tip: use Ctrl+, to reopen project setup quickly."; color: MarchCraftTheme.textMuted; font.pixelSize: 11; Layout.topMargin: 8 }
                Item { Layout.fillHeight: true }
            }
            GridLayout {
                columns: 2
                Label { text: "Capability profile" }
                ComboBox { id: clinicProfile; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Beginner",value:"beginner"},{text:"Intermediate",value:"intermediate"},{text:"Advanced",value:"advanced"},{text:"Custom",value:"custom"}]; Component.onCompleted: currentIndex=Math.max(0,indexOfValue(drillProjectContext.capabilityProfile)); onActivated: drillProjectContext.capabilityProfile=currentValue }
                Label { text: "Maximum steps / count" }
                SpinBox { from: 25; to: 400; stepSize: 5; value: Math.round(drillProjectContext.maximumStepsPerCount*100); editable: true; textFromValue: function(v){return (v/100).toFixed(2)}; valueFromText: function(t){return Math.round(Number(t)*100)}; onValueModified: drillProjectContext.maximumStepsPerCount=value/100 }
                Label { text: "Collision clearance (steps)" }
                SpinBox { from: 25; to: 800; stepSize: 5; value: Math.round(drillProjectContext.collisionClearance*100); editable: true; textFromValue: function(v){return (v/100).toFixed(2)}; valueFromText: function(t){return Math.round(Number(t)*100)}; onValueModified: drillProjectContext.collisionClearance=value/100 }
                Label { text: "Direction-change warning" }
                SpinBox { from: 15; to: 180; stepSize: 5; value: Math.round(drillProjectContext.directionChangeDegrees); editable: true; onValueModified: drillProjectContext.directionChangeDegrees=value }
                Label { text: "Caution threshold" }
                Label { text: "85% of the configured limit"; color: MarchCraftTheme.textSecondary }
                Item { Layout.columnSpan: 2; Layout.fillHeight: true }
            }
        }
    }
}
