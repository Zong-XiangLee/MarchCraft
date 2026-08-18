import QtQuick
import QtQuick3D
import "HumanGait.js" as HumanGait

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
    property real facingDegrees: 0
    property real travelHeading: 0
    property real transitionProgress: 0
    property int countsInMove: 1
    property real travelStepsPerCount: 0
    property string locomotionMode: "idle"
    property bool debugOverlay: false

    readonly property real canonicalHeight: 1.75
    readonly property real bodyScale: heightMeters / canonicalHeight / metersPerStep
    readonly property real phaseAngle: gaitPhase * Math.PI * 2
    readonly property real strideMeters: Math.min(0.70, Math.max(0, travelStepsPerCount * metersPerStep))
    readonly property bool forwardMotion: locomotionMode === "march.forward"
    readonly property bool backwardMotion: locomotionMode === "march.backward"
    readonly property bool leftSlide: locomotionMode === "slide.left"
    readonly property bool rightSlide: locomotionMode === "slide.right"
    readonly property bool directionChange: locomotionMode === "direction_change"
    readonly property real relativeTravelDegrees: HumanGait.normalizeDegrees(travelHeading - facingDegrees)
    readonly property real elapsedCounts: transitionProgress * Math.max(1, countsInMove)
    readonly property real motionWeight: marching ? HumanGait.smootherStep(elapsedCounts / 0.35) : 0
    readonly property var bodyPose: directionChange
                                    ? HumanGait.bodyPose(locomotionMode, gaitPhase, strideMeters, motionWeight)
                                    : HumanGait.directionalBodyPose(relativeTravelDegrees, gaitPhase,
                                                                    strideMeters, motionWeight)
    readonly property var leftLegPose: directionChange
                                       ? HumanGait.legPose(locomotionMode, gaitPhase, true,
                                                           strideMeters, motionWeight, bodyPose.pelvisY)
                                       : HumanGait.directionalLegPose(relativeTravelDegrees, gaitPhase, true,
                                                                      strideMeters, motionWeight, bodyPose)
    readonly property var rightLegPose: directionChange
                                        ? HumanGait.legPose(locomotionMode, gaitPhase, false,
                                                            strideMeters, motionWeight, bodyPose.pelvisY)
                                        : HumanGait.directionalLegPose(relativeTravelDegrees, gaitPhase, false,
                                                                       strideMeters, motionWeight, bodyPose)
    readonly property color skinColor: skinPaletteId === "skin.light" ? "#d9a37f"
                                       : skinPaletteId === "skin.deep" ? "#70412f" : "#a9674b"

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

        // Use the explicit Skin API instead of the legacy Skeleton/Joint path.
        // The joint-list order is the canonical GLB joint index, which makes
        // every count-driven Node transform directly observable by the skin.
        Node {
            id: jointRoot
            Node {
                id: pelvis
                x: root.bodyPose.pelvisX
                y: 0.91 + root.bodyPose.pelvisY
                eulerRotation: Qt.vector3d(0,
                                           root.directionChange
                                               ? Math.sin(root.phaseAngle) * 8 * root.motionWeight
                                               : root.bodyPose.pelvisYaw,
                                           root.bodyPose.pelvisRoll)
                    Node {
                        id: spineLower
                        y: 0.17 + root.bodyPose.spineLift + root.bodyPose.breath
                        eulerRotation: Qt.vector3d(root.bodyPose.spinePitch,
                                                   root.bodyPose.spineYaw * 0.42,
                                                   root.bodyPose.spineRoll * 0.45)
                        Node {
                            id: spineUpper
                            y: 0.23
                            eulerRotation: Qt.vector3d(-root.bodyPose.spinePitch * 0.62,
                                                       root.bodyPose.spineYaw * 0.58,
                                                       root.bodyPose.spineRoll * 0.55)
                            Node {
                                id: neck
                                y: 0.18
                                Node {
                                    id: head
                                    y: 0.12
                                    z: -0.01
                                    eulerRotation.x: root.bodyPose.headPitch
                                    Node { id: headSocket }
                                }
                            }
                            Node {
                                id: clavicleLeft
                                x: -0.15; y: 0.09
                                Node {
                                    id: upperArmLeft
                                    x: -0.11; y: -0.03
                                    eulerRotation: Qt.vector3d(HumanGait.armPitch(root.leftLegPose), 0, 50)
                                    Node {
                                        id: forearmLeft
                                        x: -0.24; y: -0.19
                                        eulerRotation.x: root.forwardMotion || root.backwardMotion
                                                                 ? -5 + HumanGait.armPitch(root.leftLegPose) * 0.28 : 0
                                        Node {
                                            id: handLeft
                                            x: -0.23; y: -0.14
                                            Node { id: handLeftSocket }
                                        }
                                    }
                                }
                            }
                            Node {
                                id: clavicleRight
                                x: 0.15; y: 0.09
                                Node {
                                    id: upperArmRight
                                    x: 0.11; y: -0.03
                                    eulerRotation: Qt.vector3d(HumanGait.armPitch(root.rightLegPose), 0, -50)
                                    Node {
                                        id: forearmRight
                                        x: 0.24; y: -0.19
                                        eulerRotation.x: root.forwardMotion || root.backwardMotion
                                                                 ? -5 + HumanGait.armPitch(root.rightLegPose) * 0.28 : 0
                                        Node {
                                            id: handRight
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
                    Node {
                        id: thighLeft
                        x: -0.105; y: -0.03
                        eulerRotation: Qt.vector3d(root.leftLegPose.hipX, 0, root.leftLegPose.hipZ)
                        Node {
                            id: shinLeft
                            y: -0.39
                            eulerRotation: Qt.vector3d(root.leftLegPose.kneeX, 0, root.leftLegPose.kneeZ)
                            Node {
                                id: footLeft
                                y: -0.415
                                eulerRotation: Qt.vector3d(root.leftLegPose.footX,
                                                          root.directionChange ? Math.max(0, Math.sin(root.phaseAngle)) * 35 * root.motionWeight : 0,
                                                          root.leftLegPose.footZ)
                                Node {
                                    id: toeLeft
                                    y: -0.04; z: -0.12
                                    eulerRotation.x: root.leftLegPose.toeX
                                }
                            }
                        }
                    }
                    Node {
                        id: thighRight
                        x: 0.105; y: -0.03
                        eulerRotation: Qt.vector3d(root.rightLegPose.hipX, 0, root.rightLegPose.hipZ)
                        Node {
                            id: shinRight
                            y: -0.39
                            eulerRotation: Qt.vector3d(root.rightLegPose.kneeX, 0, root.rightLegPose.kneeZ)
                            Node {
                                id: footRight
                                y: -0.415
                                eulerRotation: Qt.vector3d(root.rightLegPose.footX,
                                                          root.directionChange ? Math.min(0, Math.sin(root.phaseAngle)) * 35 * root.motionWeight : 0,
                                                          root.rightLegPose.footZ)
                                Node {
                                    id: toeRight
                                    y: -0.04; z: -0.12
                                    eulerRotation.x: root.rightLegPose.toeX
                                }
                            }
                        }
                    }
                    Node { id: waistSocket }
                    Node { id: backSocket; z: 0.16 }
            }
        }

        Skin {
            id: humanSkin
            joints: [
                jointRoot, pelvis, spineLower, spineUpper, neck, head,
                thighLeft, shinLeft, footLeft, toeLeft,
                thighRight, shinRight, footRight, toeRight,
                clavicleLeft, upperArmLeft, forearmLeft, handLeft,
                clavicleRight, upperArmRight, forearmRight, handRight
            ]
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
        }

        Model {
            geometry: root.geometrySource
            skin: humanSkin
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
