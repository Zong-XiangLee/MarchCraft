import QtQuick
import QtQuick.Window

Window {
    id: root
    title: "Rendering 3D export"
    width: exportController.videoWidth
    height: exportController.videoHeight
    visible: false
    property string framePath: ""
    property int warmupFrames: 0
    onClosing: function(close) { if(exportController.busy) { close.accepted=false;exportController.cancel() } }
    Loader {
        id: scene
        anchors.fill: parent
        active: root.visible && exportController.renderProject !== null
        sourceComponent: ThreeDView {
            projectModel: exportController.renderProject
            exportMode: true
            Component.onCompleted: setCameraPreset(exportController.camera)
        }
    }
    Connections {
        target: exportController
        function onCaptureFrame(path) {
            root.framePath=path
            root.visible=true
            root.warmupFrames=2
            captureTimer.start()
        }
        function onFinished(success) { root.visible=false;captureTimer.stop() }
    }
    Timer {
        id: captureTimer
        interval: 16
        repeat: true
        onTriggered: {
            if(!scene.item) return
            if(root.warmupFrames-- > 0) return
            stop()
            const started = scene.item.grabToImage(function(result) {
                if(result.saveToFile(root.framePath)) exportController.submitFrame(root.framePath)
                else exportController.submitFrame("")
            }, Qt.size(exportController.videoWidth,exportController.videoHeight))
            if(!started) exportController.submitFrame("")
        }
    }
}
