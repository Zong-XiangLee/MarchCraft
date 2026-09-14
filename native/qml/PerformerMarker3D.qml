import QtQuick
import QtQuick3D

// Dimensions are meters; the parent supplies drill-to-world scaling.
Node {
    id: marker
    property color markerColor: "#5b8def"
    property bool selected: false
    property real metersPerStep: 0.5715
    scale: Qt.vector3d(1 / metersPerStep, 1 / metersPerStep, 1 / metersPerStep)

    Model {
        objectName: "markerDisk"
        source: "#Cylinder"
        y: 0.09
        scale: Qt.vector3d(0.006, 0.0018, 0.006)
        materials: PrincipledMaterial {
            baseColor: marker.selected ? "#ffffff" : marker.markerColor
            roughness: 0.85
        }
    }
    Model {
        objectName: "facingIndicator"
        source: "#Cube"
        position: Qt.vector3d(0, 0.20, -0.22)
        scale: Qt.vector3d(0.0012, 0.0004, 0.0044)
        materials: PrincipledMaterial { baseColor: marker.selected ? "#17212d" : "#ffffff"; lighting: PrincipledMaterial.NoLighting }
    }
}
