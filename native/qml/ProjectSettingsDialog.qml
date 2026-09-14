import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Dialog {
    // Explicit dependencies supplied by the application shell.
    required property var drillProjectContext
    required property var workspaceStateContext

    id: projectSetupDialog
    property bool creationMode: false
    title: creationMode ? "Create a new project" : "Project setup"
    modal: true; anchors.centerIn: Overlay.overlay; width: 500; height: 330
    standardButtons: Dialog.NoButton
    onOpened: {
        projectNameField.text = creationMode ? "Untitled Show" : drillProjectContext.showName
        projectFieldPreset.currentIndex = projectFieldPreset.indexOfValue(creationMode ? "hs" : drillProjectContext.fieldPreset)
        projectLightingPreset.currentIndex = projectLightingPreset.indexOfValue(creationMode ? "lighting.daylight" : drillProjectContext.lightingPreset)
    }
    contentItem: GridLayout {
        columns: 2
        columnSpacing: 14
        rowSpacing: 10
        Label {
            text: "Set up the rehearsal environment before you start staging."
            color: "#a5afbc"; wrapMode: Text.Wrap
            Layout.fillWidth: true; Layout.columnSpan: 2; Layout.bottomMargin: 4
        }
        Label { text: "Project name" }
        TextField { id: projectNameField; Layout.fillWidth: true; text: drillProjectContext.showName; placeholderText: "Untitled show" }
        Label { text: "Field" }
        ComboBox {
            id: projectFieldPreset
            Layout.fillWidth: true; textRole: "text"; valueRole: "value"
            model: [{text:"High School",value:"hs"},{text:"College",value:"college"},{text:"Professional",value:"nfl"},{text:"Indoor",value:"indoor"}]
            Component.onCompleted: currentIndex = indexOfValue(drillProjectContext.fieldPreset)
        }
        Label { text: "Time of day" }
        ComboBox {
            id: projectLightingPreset
            Layout.fillWidth: true; textRole: "text"; valueRole: "value"
            model: [{text:"Daylight",value:"lighting.daylight"},{text:"Overcast",value:"lighting.overcast"},{text:"Sunset",value:"lighting.sunset"},{text:"Night game",value:"lighting.night"},{text:"Indoor",value:"lighting.indoor"}]
            Component.onCompleted: currentIndex = indexOfValue(drillProjectContext.lightingPreset)
        }
        Label {
            text: "Markers, grids, overlays, and 3D quality remain available in Editor preferences."
            color: "#778392"; wrapMode: Text.Wrap
            Layout.fillWidth: true; Layout.columnSpan: 2; Layout.topMargin: 4
        }
        RowLayout {
            Layout.fillWidth: true; Layout.columnSpan: 2; Layout.topMargin: 6
            Item { Layout.fillWidth: true }
            AppButton { text: "Cancel"; onClicked: { if (projectSetupDialog.creationMode) workspaceStateContext.cancel(); projectSetupDialog.close() } }
            AppButton {
                text: projectSetupDialog.creationMode ? "Create project" : "Save changes"
                highlighted: true
                onClicked: {
                    if (projectSetupDialog.creationMode) drillProjectContext.newProject()
                    drillProjectContext.showName = projectNameField.text.trim().length ? projectNameField.text.trim() : "Untitled Show"
                    drillProjectContext.fieldPreset = projectFieldPreset.currentValue
                    drillProjectContext.lightingPreset = projectLightingPreset.currentValue
                    if (projectSetupDialog.creationMode) workspaceStateContext.enteredProject()
                    projectSetupDialog.close()
                }
            }
        }
    }
}
