import QtQuick
import QtTest
import "../qml"

TestCase {
    id: test
    name: "ExportWorkspace"
    width: 1120
    height: 720
    visible: true
    when: windowShown
    QtObject {
        id: drillProject
        property string showName: "Test Show"
    }
    QtObject {
        id: workspaceController
        property bool systemAnimationsEnabled: false
    }
    QtObject {
        id: exportController
        property bool busy: false
        property real progress: 0
        property string message: "Ready"
        property var branding: ({
                company: "Saved company"
            })
        property var presetNames: []
        property var choices: []
        property var pages: ["First page"]
        property var tableRows: []
        property string previewUrl: ""
        property string previewOverlay: ""
        property string ffmpeg: ""
        property real duration: 3
        property var renderProject: null
        property int preparations: 0
        property int saves: 0
        property var lastOptions: ({})
        function refresh() {
        }
        function prepare(options) {
            preparations++;
            lastOptions = options;
            return options.sets !== "missing";
        }
        function preview(index) {
        }
        function previewTime(seconds) {
        }
        function plannedFiles(path) {
            return [path];
        }
        function fileExists(path) {
            return false;
        }
        function destinationFolder() {
            return "C:/Exports";
        }
        function suggestedName(content, format) {
            return content === "both" ? "Test Show" : "Test Show - " + content + "." + (format.indexOf("video") === 0 ? "mp4" : format);
        }
        function setBranding(name, path, remove) {
            saves++;
            return true;
        }
        function setBasename(name) {
        }
    }
    ExportWorkspace {
        id: workspace
    }
    function init() {
        exportController.busy = false;
        workspace.resetForProject();
        workspace.openFor("charts", "pdf");
        wait(300);
        exportController.preparations = 0;
        exportController.saves = 0;
    }
    function test_debounceAndPreviewDoesNotSaveBranding() {
        workspace.change("sets", "1");
        workspace.change("sets", "1-2");
        workspace.change("performerLabelSize", 5);
        wait(300);
        compare(exportController.preparations, 1);
        compare(exportController.lastOptions.sets, "1-2");
        compare(exportController.saves, 0);
        verify(workspace.prepared);
        workspace.change("sets", "missing");
        wait(300);
        verify(!workspace.prepared);
    }
    function test_presetsPreserveSelectionsAndSeedFilename() {
        workspace.change("sets", "2-4");
        workspace.applyPreset("Coordinates + drill diagrams");
        wait(300);
        compare(workspace.options.sets, "2-4");
        compare(workspace.options.content, "both");
        verify(workspace.folderOutput);
        compare(findChild(workspace, "exportFilename").text, "Test Show");
        workspace.applyPreset("Performer coordinates");
        wait(300);
        verify(findChild(workspace, "exportFilename").text.endsWith(".pdf"));
    }
    function test_destinationDoesNotRebuildPreviewAndCustomStemSurvives() {
        findChild(workspace, "exportFolder").text = "C:/Other";
        wait(300);
        compare(exportController.preparations, 0);
        findChild(workspace, "exportFilename").text = "My packet.pdf";
        workspace.customFilename = true;
        workspace.change("format", "video2d");
        wait(300);
        compare(findChild(workspace, "exportFilename").text, "My packet.mp4");
    }
    function test_busyJobCannotBeReconfigured() {
        exportController.busy = true;
        workspace.openFor("coordinates", "pdf");
        wait(300);
        compare(workspace.options.content, "charts");
        compare(exportController.preparations, 0);
    }
}
