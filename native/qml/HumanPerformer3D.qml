import QtQuick
import QtQuick3D

// Canonical grounded human performer. Geometry is shared by the scene; every
// instance owns only its lightweight joint hierarchy and deterministic pose.
Node {
    id: root

    property var geometrySource
    property real metersPerStep: 0.5715
    property real heightMeters: 1.75
    property color uniformColor: "#38bdf8"
    property string skinPaletteId: "skin.medium"
    property string bodyRigId: "performer.body.standard"
    property string instrumentAssetId: "instrument.generic"
    property string equipmentAssetId: ""
    property bool selected: false
    property bool marching: false
    property real gaitPhase: 0
    property real transitionProgress: 0
    property real travelStepsPerCount: 0
    property string locomotionMode: "idle"
    property bool debugOverlay: false

    readonly property real canonicalHeight: 1.75
    readonly property real bodyScale: heightMeters / canonicalHeight / metersPerStep
    readonly property real phaseAngle: gaitPhase * Math.PI * 2
    readonly property real strideDegrees: Math.min(32, Math.max(0, travelStepsPerCount * 24))
    readonly property bool forwardMotion: locomotionMode === "march.forward"
    readonly property bool backwardMotion: locomotionMode === "march.backward"
    readonly property bool leftSlide: locomotionMode === "slide.left"
    readonly property bool rightSlide: locomotionMode === "slide.right"
    readonly property bool directionChange: locomotionMode === "direction_change"
    readonly property real motionWeight: marching ? Math.min(1, transitionProgress * 12) : 0
    readonly property color skinColor: skinPaletteId === "skin.light" ? "#d9a37f"
                                       : skinPaletteId === "skin.deep" ? "#70412f" : "#a9674b"

    function wave(leftSide) {
        return Math.cos(phaseAngle) * (leftSide ? 1 : -1)
    }

    function hipX(leftSide) {
        if (!forwardMotion && !backwardMotion)
            return 0
        const direction = backwardMotion ? -0.78 : 1
        return wave(leftSide) * strideDegrees * direction * motionWeight
    }

    function kneeX(leftSide) {
        if (!forwardMotion && !backwardMotion)
            return 0
        const swing = Math.max(0, -wave(leftSide))
        return -swing * Math.max(4, strideDegrees * 0.30) * motionWeight
    }

    function footX(leftSide) {
        if (!forwardMotion && !backwardMotion)
            return 0
        const amount = wave(leftSide)
        if (backwardMotion)
            return (Math.max(0, amount) * 2 - Math.max(0, -amount) * 2) * motionWeight
        return (Math.max(0, amount) * 14 - Math.max(0, -amount) * 4) * motionWeight
    }

    function hipZ(leftSide) {
        if (!leftSlide && !rightSlide)
            return 0
        const direction = leftSlide ? -1 : 1
        const cadence = rightSlide ? -wave(leftSide) : wave(leftSide)
        return cadence * strideDegrees * 0.78 * direction * motionWeight
    }

    function armX(leftSide) {
        return -hipX(leftSide) * 0.28
    }

    PrincipledMaterial {
        id: skinMaterial
        baseColor: root.skinColor
        metalness: 0
        roughness: 0.72
        specularAmount: 0.32
        cullMode: Material.BackFaceCulling
    }

    // Ground indicators preserve roster and selection identity without tinting
    // the body. Their lower faces remain below the performer contact plane.
    Model {
        source: "#Cylinder"
        y: 0.003 / root.metersPerStep
        scale: Qt.vector3d(0.0082, 0.000015, 0.0082)
        opacity: root.selected ? 0.72 : 0.34
        materials: PrincipledMaterial {
            baseColor: root.selected ? "#ffd166" : root.uniformColor
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }
    Model {
        visible: root.selected
        source: "#Cylinder"
        y: 0.002 / root.metersPerStep
        scale: Qt.vector3d(0.0102, 0.000010, 0.0102)
        opacity: 0.38
        materials: PrincipledMaterial {
            baseColor: "#fff0a8"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }

    Node {
        id: scaledBody
        scale: Qt.vector3d(root.bodyScale, root.bodyScale, root.bodyScale)

        Skeleton {
            id: humanSkeleton
            Joint {
                id: jointRoot
                index: 0
                skeletonRoot: humanSkeleton
                Joint {
                    id: pelvis
                    index: 1
                    skeletonRoot: humanSkeleton
                    y: 0.91
                    eulerRotation.y: root.directionChange ? Math.sin(root.phaseAngle) * 8 * root.motionWeight
                                                               : (root.leftSlide || root.rightSlide ? -Math.cos(root.phaseAngle) * 4 * root.motionWeight : 0)
                    Joint {
                        id: spineLower
                        index: 2
                        skeletonRoot: humanSkeleton
                        y: 0.17
                        Joint {
                            id: spineUpper
                            index: 3
                            skeletonRoot: humanSkeleton
                            y: 0.23
                            eulerRotation.y: (root.leftSlide || root.rightSlide) ? Math.cos(root.phaseAngle) * 3 * root.motionWeight : 0
                            Joint {
                                id: neck
                                index: 4
                                skeletonRoot: humanSkeleton
                                y: 0.18
                                Joint {
                                    id: head
                                    index: 5
                                    skeletonRoot: humanSkeleton
                                    y: 0.12
                                    z: -0.01
                                    Node { id: headSocket }
                                }
                            }
                            Joint {
                                id: clavicleLeft
                                index: 14
                                skeletonRoot: humanSkeleton
                                x: -0.15; y: 0.09
                                Joint {
                                    id: upperArmLeft
                                    index: 15
                                    skeletonRoot: humanSkeleton
                                    x: -0.11; y: -0.03
                                    eulerRotation: Qt.vector3d(root.armX(true), 0, 50)
                                    Joint {
                                        id: forearmLeft
                                        index: 16
                                        skeletonRoot: humanSkeleton
                                        x: -0.24; y: -0.19
                                        eulerRotation.x: root.forwardMotion || root.backwardMotion ? -5 - root.wave(true) * 2 * root.motionWeight : 0
                                        Joint {
                                            id: handLeft
                                            index: 17
                                            skeletonRoot: humanSkeleton
                                            x: -0.23; y: -0.14
                                            Node { id: handLeftSocket }
                                        }
                                    }
                                }
                            }
                            Joint {
                                id: clavicleRight
                                index: 18
                                skeletonRoot: humanSkeleton
                                x: 0.15; y: 0.09
                                Joint {
                                    id: upperArmRight
                                    index: 19
                                    skeletonRoot: humanSkeleton
                                    x: 0.11; y: -0.03
                                    eulerRotation: Qt.vector3d(root.armX(false), 0, -50)
                                    Joint {
                                        id: forearmRight
                                        index: 20
                                        skeletonRoot: humanSkeleton
                                        x: 0.24; y: -0.19
                                        eulerRotation.x: root.forwardMotion || root.backwardMotion ? -5 - root.wave(false) * 2 * root.motionWeight : 0
                                        Joint {
                                            id: handRight
                                            index: 21
                                            skeletonRoot: humanSkeleton
                                            x: 0.23; y: -0.14
                                            Node { id: handRightSocket }
                                        }
                                    }
                                }
                            }
                            Node { id: chestSocket }
                            Node { id: equipmentSocket; z: -0.18 }
                        }
                    }
                    Joint {
                        id: thighLeft
                        index: 6
                        skeletonRoot: humanSkeleton
                        x: -0.105; y: -0.03
                        eulerRotation: Qt.vector3d(root.hipX(true), 0, root.hipZ(true))
                        Joint {
                            id: shinLeft
                            index: 7
                            skeletonRoot: humanSkeleton
                            y: -0.39
                            eulerRotation.x: root.kneeX(true)
                            Joint {
                                id: footLeft
                                index: 8
                                skeletonRoot: humanSkeleton
                                y: -0.415
                                eulerRotation: Qt.vector3d(root.footX(true),
                                                          root.directionChange ? Math.max(0, Math.sin(root.phaseAngle)) * 35 * root.motionWeight : 0, 0)
                                Joint {
                                    id: toeLeft
                                    index: 9
                                    skeletonRoot: humanSkeleton
                                    y: -0.04; z: -0.12
                                }
                            }
                        }
                    }
                    Joint {
                        id: thighRight
                        index: 10
                        skeletonRoot: humanSkeleton
                        x: 0.105; y: -0.03
                        eulerRotation: Qt.vector3d(root.hipX(false), 0, root.hipZ(false))
                        Joint {
                            id: shinRight
                            index: 11
                            skeletonRoot: humanSkeleton
                            y: -0.39
                            eulerRotation.x: root.kneeX(false)
                            Joint {
                                id: footRight
                                index: 12
                                skeletonRoot: humanSkeleton
                                y: -0.415
                                eulerRotation: Qt.vector3d(root.footX(false),
                                                          root.directionChange ? Math.min(0, Math.sin(root.phaseAngle)) * 35 * root.motionWeight : 0, 0)
                                Joint {
                                    id: toeRight
                                    index: 13
                                    skeletonRoot: humanSkeleton
                                    y: -0.04; z: -0.12
                                }
                            }
                        }
                    }
                    Node { id: waistSocket }
                    Node { id: backSocket; z: 0.16 }
                }
            }
        }

        Model {
            geometry: root.geometrySource
            skeleton: humanSkeleton
            inverseBindPoses: [
                Qt.matrix4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0, 0,1,0,-0.91, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0, 0,1,0,-1.08, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0, 0,1,0,-1.31, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0, 0,1,0,-1.49, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0, 0,1,0,-1.61, 0,0,1,0.01, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.105, 0,1,0,-0.88, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.105, 0,1,0,-0.49, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.105, 0,1,0,-0.075, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.105, 0,1,0,-0.035, 0,0,1,0.12, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.105, 0,1,0,-0.88, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.105, 0,1,0,-0.49, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.105, 0,1,0,-0.075, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.105, 0,1,0,-0.035, 0,0,1,0.12, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.15, 0,1,0,-1.40, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.26, 0,1,0,-1.37, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.50, 0,1,0,-1.18, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,0.73, 0,1,0,-1.04, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.15, 0,1,0,-1.40, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.26, 0,1,0,-1.37, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.50, 0,1,0,-1.18, 0,0,1,0, 0,0,0,1),
                Qt.matrix4x4(1,0,0,-0.73, 0,1,0,-1.04, 0,0,1,0, 0,0,0,1)
            ]
            materials: [skinMaterial]
            castsShadows: true
            receivesShadows: true
        }
    }

    // Authoring diagnostics: canonical root and a meter-accurate height guide.
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
        y: root.heightMeters / root.metersPerStep / 2
        scale: Qt.vector3d(0.0075, root.heightMeters / root.metersPerStep / 200, 0.0075)
        opacity: 0.22
        materials: PrincipledMaterial { baseColor: "#22d3ee"; alphaMode: PrincipledMaterial.Blend }
    }
}
