import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    signal cancelled()
    signal quickStartRequested(string name, string fieldPreset, string lightingPreset, int performerCount)
    signal guidedCreateRequested(string name, string fieldPreset, string lightingPreset, var rosterRows, string nextWorkspace)

    color: MarchCraftTheme.surface
    radius: MarchCraftTheme.radiusLarge
    border.color: MarchCraftTheme.divider
    focus: visible

    property string setupMode: "choose"
    property int guidedStep: 0
    readonly property string projectName: nameField.text.trim().length > 0 ? nameField.text.trim() : "Untitled Show"

    function reset() {
        setupMode = "choose"
        guidedStep = 0
        nameField.text = "Untitled Show"
        quickCount.value = 72
        musicNext.checked = true
        rosterEditor.resetDefaults()
    }

    onVisibleChanged: if (visible) reset()
    Component.onCompleted: reset()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 26
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            AppButton {
                text: root.setupMode === "choose" ? "‹ Home" : "‹ Back"
                flat: true
                onClicked: {
                    if (root.setupMode === "choose") root.cancelled()
                    else if (root.setupMode === "guided" && root.guidedStep > 0) root.guidedStep--
                    else root.setupMode = "choose"
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Label {
                    text: root.setupMode === "guided" ? "GUIDED SETUP" : root.setupMode === "quick" ? "QUICK START" : "NEW PROJECT"
                    color: MarchCraftTheme.textPrimary
                    font.pixelSize: 20
                    font.bold: true
                    font.letterSpacing: 1
                }
                Label {
                    text: root.setupMode === "guided" ? "Step " + (root.guidedStep + 1) + " of 3 · You can revisit every workspace later."
                          : root.setupMode === "quick" ? "Create the essentials and start writing immediately."
                          : "Choose as much setup as you want before entering the workspace."
                    color: MarchCraftTheme.textSecondary
                    font.pixelSize: 11
                }
            }
            Label {
                visible: root.setupMode === "guided"
                text: root.guidedStep === 0 ? "PROJECT" : root.guidedStep === 1 ? "ROSTER" : "MUSIC"
                color: MarchCraftTheme.accentHover
                font.bold: true
                font.pixelSize: 10
                font.letterSpacing: 1
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.setupMode === "choose" ? 0 : root.setupMode === "quick" ? 1 : 2 + root.guidedStep

            RowLayout {
                spacing: 18
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: MarchCraftTheme.radiusLarge
                    color: quickHover.hovered ? MarchCraftTheme.surfaceHover : MarchCraftTheme.panel
                    border.color: quickHover.hovered ? MarchCraftTheme.accent : MarchCraftTheme.divider
                    HoverHandler { id: quickHover }
                    TapHandler { onTapped: { root.setupMode = "quick"; Qt.callLater(function() { nameField.forceActiveFocus() }) } }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 10
                        Label { text: "QUICK START"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                        Label { text: "Get to the field"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 22 }
                        Label {
                            text: "Name the show, choose a field, and create a simple ensemble roster. Music and details can wait."
                            color: MarchCraftTheme.textSecondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Item { Layout.fillHeight: true }
                        Label { text: "Project details  ·  performer count  ·  Editor"; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                        AppButton { text: "Choose Quick Start"; highlighted: true; Layout.fillWidth: true; onClicked: root.setupMode = "quick" }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: MarchCraftTheme.radiusLarge
                    color: guidedHover.hovered ? MarchCraftTheme.surfaceHover : MarchCraftTheme.panel
                    border.color: guidedHover.hovered ? MarchCraftTheme.accent : MarchCraftTheme.divider
                    HoverHandler { id: guidedHover }
                    TapHandler { onTapped: root.setupMode = "guided" }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 10
                        Label { text: "GUIDED SETUP"; color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                        Label { text: "Prepare the show"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 22 }
                        Label {
                            text: "Set project details, build a section-based roster with label previews, then decide whether music comes next."
                            color: MarchCraftTheme.textSecondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Item { Layout.fillHeight: true }
                        Label { text: "Project  ·  Roster  ·  optional Music"; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                        AppButton { text: "Choose Guided Setup"; Layout.fillWidth: true; onClicked: root.setupMode = "guided" }
                    }
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: 18
                rowSpacing: 10
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                Label { text: "PROJECT NAME"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                TextField { id: nameField; Layout.fillWidth: true; selectByMouse: true; placeholderText: "Untitled Show" }
                Label { text: "FIELD"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                ComboBox {
                    id: field
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [{text:"High School",value:"hs"},{text:"College",value:"college"},{text:"Professional",value:"nfl"},{text:"Indoor",value:"indoor"}]
                }
                Label { text: "ENVIRONMENT"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                ComboBox {
                    id: lighting
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [{text:"Daylight",value:"lighting.daylight"},{text:"Overcast",value:"lighting.overcast"},{text:"Sunset",value:"lighting.sunset"},{text:"Night game",value:"lighting.night"},{text:"Indoor",value:"lighting.indoor"}]
                }
                Label { text: "PERFORMERS"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                SpinBox { id: quickCount; from: 0; to: 500; value: 72; editable: true; Layout.fillWidth: true }
                Label {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: quickCount.value > 0
                          ? "Quick Start labels the ensemble P1–P" + quickCount.value + ". Refine sections, instruments, and labels in Roster at any time."
                          : "Quick Start can begin with an empty roster. Add complete sections or individual performers in Roster at any time."
                    color: MarchCraftTheme.textSecondary
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: "Create and open Editor"
                        highlighted: true
                        enabled: nameField.text.trim().length > 0
                        onClicked: root.quickStartRequested(root.projectName, field.currentValue, lighting.currentValue, quickCount.value)
                    }
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: 18
                rowSpacing: 12
                Layout.alignment: Qt.AlignVCenter
                Label { text: "PROJECT NAME"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                TextField {
                    Layout.fillWidth: true
                    text: nameField.text
                    selectByMouse: true
                    onTextEdited: nameField.text = text
                }
                Label { text: "FIELD"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                ComboBox {
                    Layout.fillWidth: true
                    textRole: "text"; valueRole: "value"; model: field.model; currentIndex: field.currentIndex
                    onActivated: field.currentIndex = currentIndex
                }
                Label { text: "ENVIRONMENT"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                ComboBox {
                    Layout.fillWidth: true
                    textRole: "text"; valueRole: "value"; model: lighting.model; currentIndex: lighting.currentIndex
                    onActivated: lighting.currentIndex = currentIndex
                }
                Label {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: "These values are editable later in Project setup. Nothing in Guided Setup locks the workflow."
                    color: MarchCraftTheme.textSecondary
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    AppButton { text: "Next: Roster"; highlighted: true; enabled: root.projectName.length > 0; onClicked: root.guidedStep = 1 }
                }
            }

            ColumnLayout {
                spacing: 10
                RosterBatchEditor {
                    id: rosterEditor
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    showAction: false
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "One action creates every section and remains one undoable roster change."; color: MarchCraftTheme.textMuted; font.pixelSize: 10; Layout.fillWidth: true }
                    AppButton { text: "Next: Music"; highlighted: true; enabled: rosterEditor.totalPerformers > 0; onClicked: root.guidedStep = 2 }
                }
            }

            RowLayout {
                spacing: 18
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: MarchCraftTheme.radiusLarge
                    color: musicNext.checked ? MarchCraftTheme.selection : MarchCraftTheme.panel
                    border.color: musicNext.checked ? MarchCraftTheme.accent : MarchCraftTheme.divider
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 22; spacing: 9
                        RadioButton { id: musicNext; text: "Set up music next"; checked: true }
                        Label { text: "Open the Music workspace to import MIDI, MusicXML, or rehearsal audio and review timing before writing drill."; color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        Label { text: "Music remains optional."; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                    }
                    TapHandler { onTapped: musicNext.checked = true }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: MarchCraftTheme.radiusLarge
                    color: editorNext.checked ? MarchCraftTheme.selection : MarchCraftTheme.panel
                    border.color: editorNext.checked ? MarchCraftTheme.accent : MarchCraftTheme.divider
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 22; spacing: 9
                        RadioButton { id: editorNext; text: "Start in Editor" }
                        Label { text: "Begin arranging immediately. Roster and Music stay available in the workspace header whenever you need them."; color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        Label { text: "No setup step is required later."; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                    }
                    TapHandler { onTapped: editorNext.checked = true }
                }
                ButtonGroup { buttons: [musicNext, editorNext] }
            }
        }

        RowLayout {
            visible: root.setupMode === "guided" && root.guidedStep === 2
            Layout.fillWidth: true
            Label { text: rosterEditor.totalPerformers + " performers ready"; color: MarchCraftTheme.textSecondary; Layout.fillWidth: true }
            AppButton {
                text: musicNext.checked ? "Create and open Music" : "Create and open Editor"
                highlighted: true
                onClicked: root.guidedCreateRequested(root.projectName, field.currentValue, lighting.currentValue,
                                                       rosterEditor.rowsData(), musicNext.checked ? "music" : "editor")
            }
        }
    }
}
