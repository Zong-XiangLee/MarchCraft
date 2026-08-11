import QtQuick
import QtQuick3D

Node {
    id: root
    property string definitionId: "prop.box"
    property color primaryColor: "#d7b95b"

    readonly property vector3d baseDimensions: definitionId === "prop.panel" ? Qt.vector3d(2.4, 2.4, 0.15)
                                                : definitionId === "prop.platform" ? Qt.vector3d(2.4, 0.4, 2.4)
                                                : definitionId === "prop.podium" ? Qt.vector3d(1.2, 1.4, 1.2)
                                                : Qt.vector3d(1, 1, 1)
    Model {
        source: "#Cube"
        y: root.baseDimensions.y / 2
        scale: Qt.vector3d(root.baseDimensions.x / 100,
                           root.baseDimensions.y / 100,
                           root.baseDimensions.z / 100)
        materials: PrincipledMaterial {
            baseColor: root.primaryColor
            roughness: 0.72
            metalness: root.definitionId === "prop.panel" ? 0.12 : 0.02
        }
    }
}
