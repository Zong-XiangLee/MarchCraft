import QtQuick
import QtQuick3D

// Grounded placeholder for every future Blender rig. This component's origin is
// exactly between the feet at Y=0; all animation is in-place.
Node {
    id: root
    property real metersPerStep: 0.5715
    property real heightMeters: 1.75
    property color uniformColor: "#38bdf8"
    property string skinPaletteId: "skin.medium"
    property string bodyRigId: "performer.body.standard"
    property string instrumentAssetId: "instrument.generic"
    property string equipmentAssetId: ""
    property bool selected: false
    property bool marching: false
    property real animationPhase: 0
    property bool debugOverlay: false

    readonly property real h: heightMeters / metersPerStep
    readonly property real widthFactor: bodyRigId === "performer.body.broad" ? 1.16
                                        : bodyRigId === "performer.body.youth" ? 0.88 : 1.0
    readonly property real legSwing: marching ? Math.sin(animationPhase) * 18 : 0
    readonly property color skinColor: skinPaletteId === "skin.light" ? "#efc39f"
                                       : skinPaletteId === "skin.deep" ? "#75452f" : "#b97853"

    PrincipledMaterial { id: uniformMaterial; baseColor: root.uniformColor; roughness: 0.68; metalness: 0.02 }
    PrincipledMaterial { id: accentMaterial; baseColor: root.selected ? "#ffd166" : "#17202b"; roughness: 0.62 }
    PrincipledMaterial { id: skinMaterial; baseColor: root.skinColor; roughness: 0.82 }
    PrincipledMaterial { id: brassMaterial; baseColor: "#d6ac3e"; metalness: 0.72; roughness: 0.24 }

    // Shoes make foot contact visually unambiguous: their lower faces are Y=0.
    Repeater3D {
        model: 2
        delegate: Model {
            required property int index
            source: "#Cube"
            x: (index === 0 ? -0.16 : 0.16) / root.metersPerStep
            y: 0.055 / root.metersPerStep
            z: -0.055 / root.metersPerStep
            scale: Qt.vector3d(0.0022 * root.widthFactor, 0.0011, 0.0040)
            materials: accentMaterial
        }
    }

    Repeater3D {
        model: 2
        delegate: Node {
            required property int index
            x: (index === 0 ? -0.145 : 0.145) / root.metersPerStep
            y: root.h * 0.285
            eulerRotation.x: index === 0 ? root.legSwing : -root.legSwing
            Model {
                source: "#Cylinder"
                y: 0
                scale: Qt.vector3d(0.0025 * root.widthFactor, root.h * 0.0051, 0.0025)
                materials: uniformMaterial
            }
        }
    }

    Model {
        source: "#Cylinder"
        y: root.h * 0.63
        scale: Qt.vector3d(0.0062 * root.widthFactor, root.h * 0.0031, 0.0042 * root.widthFactor)
        materials: uniformMaterial
    }
    Model {
        source: "#Sphere"
        y: root.h * 0.905
        scale: Qt.vector3d(root.h * 0.00105, root.h * 0.00115, root.h * 0.00105)
        materials: skinMaterial
    }
    Model {
        source: "#Cylinder"
        y: root.h * 0.975
        scale: Qt.vector3d(root.h * 0.00118, root.h * 0.00038, root.h * 0.00118)
        materials: accentMaterial
    }

    // Arms pivot in place. Their motion never contributes to drill displacement.
    Repeater3D {
        model: 2
        delegate: Node {
            required property int index
            x: (index === 0 ? -0.31 : 0.31) * root.widthFactor / root.metersPerStep
            y: root.h * 0.70
            eulerRotation.x: index === 0 ? -root.legSwing : root.legSwing
            eulerRotation.z: index === 0 ? -7 : 7
            Model {
                source: "#Cylinder"
                y: -root.h * 0.09
                scale: Qt.vector3d(0.0018, root.h * 0.0020, 0.0018)
                materials: uniformMaterial
            }
        }
    }

    // Semantic equipment socket. Primitive instruments are temporary catalog fallbacks.
    Node {
        id: equipmentSocket
        y: root.h * 0.66
        z: -0.24 / root.metersPerStep
        Model {
            visible: root.instrumentAssetId !== "equipment.guard.flag"
            source: root.instrumentAssetId === "instrument.tuba" ? "#Sphere" : "#Cylinder"
            eulerRotation.z: 90
            scale: root.instrumentAssetId === "instrument.tuba"
                   ? Qt.vector3d(0.0065, 0.0065, 0.0030)
                   : Qt.vector3d(0.0020, 0.0070, 0.0020)
            materials: brassMaterial
        }
        Model {
            visible: root.instrumentAssetId === "equipment.guard.flag"
            source: "#Cylinder"
            y: 0.45 / root.metersPerStep
            eulerRotation.z: -18
            scale: Qt.vector3d(0.0009, 0.018, 0.0009)
            materials: PrincipledMaterial { baseColor: "#edf2f7"; roughness: 0.5 }
        }
    }

    // Authoring diagnostics: root, foot contacts, height, and collision capsule.
    Model {
        visible: root.debugOverlay
        source: "#Sphere"
        y: 0
        scale: Qt.vector3d(0.0018, 0.0018, 0.0018)
        materials: PrincipledMaterial { baseColor: "#ff3b30"; lighting: PrincipledMaterial.NoLighting }
    }
    Model {
        visible: root.debugOverlay
        source: "#Cylinder"
        y: root.h / 2
        scale: Qt.vector3d(0.0075, root.h / 200, 0.0075)
        opacity: 0.22
        materials: PrincipledMaterial { baseColor: "#22d3ee"; alphaMode: PrincipledMaterial.Blend }
    }
}
