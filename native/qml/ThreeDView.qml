import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: root
    readonly property int insertColumnCount: drillProject.fieldInsertCount
    property real cameraZoom: 1.0
    property real cameraDistance: Math.sqrt(cameraBasePosition.y * cameraBasePosition.y
                                            + cameraBasePosition.z * cameraBasePosition.z)
    property vector3d cameraBasePosition: Qt.vector3d(0, 51, 63)
    property vector3d cameraBaseRotation: Qt.vector3d(-42, 0, 0)

    function applyZoom() {
        camera.z = cameraDistance * cameraZoom
    }

    function setCameraPreset(preset) {
        if (preset === "overhead") {
            cameraBasePosition = Qt.vector3d(0, 86, 0)
            cameraBaseRotation = Qt.vector3d(-90, 0, 0)
        } else if (preset === "field") {
            cameraBasePosition = Qt.vector3d(0, 10, 49)
            cameraBaseRotation = Qt.vector3d(-10, 0, 0)
        } else {
            cameraBasePosition = Qt.vector3d(0, 51, 63)
            cameraBaseRotation = Qt.vector3d(-42, 0, 0)
        }
        cameraZoom = 1.0
        cameraDistance = Math.sqrt(cameraBasePosition.y * cameraBasePosition.y
                                   + cameraBasePosition.z * cameraBasePosition.z)
        cameraOrigin.position = Qt.vector3d(0, 0, 0)
        cameraOrigin.eulerRotation = cameraBaseRotation
        applyZoom()
    }

    function insertStep(column) {
        return drillProject.fieldInsertStep(column)
    }

    Rectangle { anchors.fill: parent; color: "#071019"; radius: 10 }

    View3D {
        id: view
        anchors.fill: parent
        anchors.margins: 2
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: drillProject.lightingPreset === "lighting.sunset" ? "#b16d57"
                        : drillProject.lightingPreset === "lighting.night" ? "#06101d"
                        : drillProject.lightingPreset === "lighting.overcast" ? "#91a0aa"
                        : drillProject.lightingPreset === "lighting.indoor" ? "#3d4650" : "#75a9d1"
            antialiasingMode: drillProject.graphicsProfile === "performance" ||
                              (drillProject.graphicsProfile === "automatic" && drillProject.performerCount > 150)
                              ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
            antialiasingQuality: drillProject.graphicsProfile === "presentation" ? SceneEnvironment.VeryHigh
                                 : drillProject.performerCount > 150 ? SceneEnvironment.Medium : SceneEnvironment.High
        }

        HumanGeometry {
            id: sharedHumanGeometry
            detailLevel: drillProject.graphicsProfile === "presentation" ? 0
                       : drillProject.graphicsProfile === "performance" ? 2
                       : drillProject.graphicsProfile === "automatic"
                         ? (drillProject.performerCount > 150 ? 1 : 0) : 1
        }

        Node {
            id: cameraOrigin
            eulerRotation: root.cameraBaseRotation
            PerspectiveCamera {
                id: camera
                z: root.cameraDistance
                clipFar: 2000
                onZChanged: {
                    if (root.cameraDistance > 0)
                        root.cameraZoom = z / root.cameraDistance
                }
            }
        }

        DirectionalLight {
            eulerRotation.x: -48
            eulerRotation.y: -25
            brightness: drillProject.lightingPreset === "lighting.night" ? 0.5
                        : drillProject.lightingPreset === "lighting.overcast" ? 0.9 : 1.35
            castsShadow: drillProject.graphicsProfile === "presentation" ||
                         (drillProject.graphicsProfile !== "performance" && drillProject.performerCount <= 150)
            shadowFactor: 45
        }
        DirectionalLight { eulerRotation.x: 35; eulerRotation.y: 145; brightness: drillProject.lightingPreset === "lighting.night" ? 0.25 : 0.45 }

        Venue3D {
            venueId: drillProject.venuePreset
            primaryColor: drillProject.venuePrimaryColor
            secondaryColor: drillProject.venueSecondaryColor
            fieldWidthMeters: drillProject.fieldWidthSteps * drillProject.metersPerStep
            fieldDepthMeters: drillProject.fieldDepthSteps * drillProject.metersPerStep
            crowdDensity: drillProject.crowdDensity
            scoreboardText: drillProject.scoreboardText
        }

        // All drill-space geometry below is authored in marching steps and scaled
        // once here into the renderer's meter-based world.
        Node {
            id: drillWorld
            scale: Qt.vector3d(drillProject.metersPerStep, drillProject.metersPerStep,
                               drillProject.metersPerStep)

        Model {
            source: "#Cube"
            position: Qt.vector3d(0, -0.5, 0)
            scale: Qt.vector3d(1.76, 0.010, (drillProject.fieldDepthSteps + 16) / 100)
            materials: PrincipledMaterial { baseColor: "#17201c"; roughness: 1.0 }
        }

        Model {
            source: "#Cube"
            position: Qt.vector3d(0, -0.5, 0)
            scale: Qt.vector3d(1.6, 0.010, drillProject.fieldDepthSteps / 100)
            materials: PrincipledMaterial {
                baseColor: drillProject.fieldPreset === "indoor" ? "#a97543" : drillProject.turfColor
                roughness: drillProject.fieldPreset === "indoor" ? 0.68 : 0.96
            }
        }

        Repeater3D {
            model: 20
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(-76 + index * 8, 0.005, 0)
                scale: Qt.vector3d(0.08, 0.0001, drillProject.fieldDepthSteps / 100)
                materials: PrincipledMaterial {
                    baseColor: index % 2 === 0 ? "#215b3e" : "#194e35"
                    roughness: 0.98
                }
            }
        }

        // All grid and five-yard lines share one high-resolution transparent plane.
        // A single coplanar texture avoids the aliasing and depth fights of thin cubes.
        Model {
            source: "#Rectangle"
            position: Qt.vector3d(0, 0.012, 0)
            eulerRotation.x: -90
            scale: Qt.vector3d(1.6, drillProject.fieldDepthSteps / 100, 1)
            materials: PrincipledMaterial {
                alphaMode: PrincipledMaterial.Blend
                lighting: PrincipledMaterial.NoLighting
                baseColorMap: Texture {
                    generateMipmaps: true
                    minFilter: Texture.Linear
                    magFilter: Texture.Linear
                    sourceItem: Canvas {
                        id: fieldMarkTextureCanvas
                        width: 2048
                        height: 1092
                        antialiasing: true
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.clearRect(0, 0, width, height)

                            if (drillProject.showFieldGrid) {
                                const interval = Math.max(0.5, drillProject.fieldGridInterval)
                                ctx.strokeStyle = drillProject.fieldGridColor
                                ctx.globalAlpha = Math.min(0.36, drillProject.fieldGridOpacity)
                                ctx.lineWidth = 1
                                for (let x = 0; x <= 160.001; x += interval) {
                                    const px = x / 160 * width
                                    ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                                }
                                for (let y = 0; y <= drillProject.fieldDepthSteps + 0.001; y += interval) {
                                    const py = y / drillProject.fieldDepthSteps * height
                                    ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(width, py); ctx.stroke()
                                }
                            }

                            ctx.strokeStyle = "#edf4ef"
                            ctx.globalAlpha = 0.96
                            ctx.lineWidth = 3
                            for (let step = 0; step <= 160; step += 8) {
                                const px = step / 160 * width
                                ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                            }
                        }
                        Component.onCompleted: requestPaint()
                        Connections {
                            target: drillProject
                            function onProjectChanged() { fieldMarkTextureCanvas.requestPaint() }
                            function onEditorSettingsChanged() { fieldMarkTextureCanvas.requestPaint() }
                        }
                    }
                }
                roughness: 1.0
            }
        }

        // Regulation six-foot yard numbers: transparent text textures, centered eight
        // yards from each sideline. Alpha blending prevents any backing plaque.
        Repeater3D {
            model: 9
            delegate: Model {
                required property int index
                readonly property int yardNumber: index < 5 ? (index + 1) * 10 : (9 - index) * 10
                source: "#Rectangle"
                 position: Qt.vector3d(index * 16 - 64, 0.015, drillProject.fieldDepthSteps / 2 - 12.8)
                eulerRotation.x: -90
                scale: Qt.vector3d(0.050, 0.032, 1)
                materials: PrincipledMaterial {
                    baseColor: "#eef6f0"
                    alphaMode: PrincipledMaterial.Blend
                    baseColorMap: Texture {
                        sourceItem: Text {
                            width: 160; height: 100; text: yardNumber.toString(); color: "#ffffff"
                            font.pixelSize: 82; font.bold: true
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }
                    roughness: 0.9
                }
            }
        }
        Repeater3D {
            model: 9
            delegate: Model {
                required property int index
                readonly property int yardNumber: index < 5 ? (index + 1) * 10 : (9 - index) * 10
                source: "#Rectangle"
                 position: Qt.vector3d(index * 16 - 64, 0.015, -drillProject.fieldDepthSteps / 2 + 12.8)
                eulerRotation.x: -90
                scale: Qt.vector3d(0.050, 0.032, 1)
                materials: PrincipledMaterial {
                    baseColor: "#eef6f0"
                    alphaMode: PrincipledMaterial.Blend
                    baseColorMap: Texture {
                        sourceItem: Text {
                            width: 160; height: 100; text: yardNumber.toString(); color: "#ffffff"
                            font.pixelSize: 82; font.bold: true
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }
                    roughness: 0.9
                }
            }
        }

        // Front and back sidelines.
        Repeater3D {
            model: 2
            delegate: Model {
                required property int index
                source: "#Cube"
                 position: Qt.vector3d(0, 0.012, index === 0
                                      ? drillProject.fieldDepthSteps / 2
                                      : -drillProject.fieldDepthSteps / 2)
                scale: Qt.vector3d(1.6, 0.0003, 0.0018)
                materials: PrincipledMaterial { baseColor: "#f4f7f5"; roughness: 0.88 }
            }
        }

        // Four vertical one-yard inserts in every five-yard interval on each hash row.
        // They extend outward so the hashes are on their closest, inward ends.
        Repeater3D {
            model: root.insertColumnCount * 2
            delegate: Model {
                required property int index
                readonly property int column: index % root.insertColumnCount
                readonly property bool backHash: index >= root.insertColumnCount
                readonly property real markLength: 24.0 / 22.5
                readonly property real hashZ: drillProject.fieldDepthSteps / 2
                                                   - (backHash ? drillProject.backHashSteps
                                                               : drillProject.frontHashSteps)
                source: "#Cube"
                 position: Qt.vector3d(root.insertStep(column) - 80, 0.012,
                                      hashZ + (backHash ? -markLength / 2 : markLength / 2))
                scale: Qt.vector3d(0.0018, 0.0003, markLength / 100)
                materials: PrincipledMaterial { baseColor: "#f4f7f5"; roughness: 0.88 }
            }
        }

        // Two-foot marks parallel to the sidelines on the two regulation hash rows.
        Repeater3D {
            model: (Math.floor(drillProject.fieldWidthSteps / 8) + 1) * 2
            delegate: Model {
                required property int index
                readonly property int yardLineCount: Math.floor(drillProject.fieldWidthSteps / 8) + 1
                readonly property int yardLine: index % yardLineCount
                readonly property bool backHash: index >= yardLineCount
                readonly property real markLength: 24.0 / 22.5
                source: "#Cube"
                 position: Qt.vector3d(yardLine * 8 - 80, 0.012,
                                      drillProject.fieldDepthSteps / 2
                                      - (backHash ? drillProject.backHashSteps
                                                  : drillProject.frontHashSteps))
                scale: Qt.vector3d(markLength / 100, 0.0003, 0.0018)
                materials: PrincipledMaterial { baseColor: "#eef5f0"; roughness: 0.88 }
            }
        }

        Repeater3D {
            model: drillProject
            delegate: Node {
                id: performerNode
                required property real fieldX
                required property real fieldY
                required property real facing
                required property color performerColor
                required property bool isSelected
                required property string instrument
                required property string bodyRigId
                required property string uniformId
                required property string skinPaletteId
                required property string instrumentAssetId
                required property string equipmentAssetId
                required property real performerHeightMeters
                required property real setDistance
                required property real travelHeading
                required property real travelStepsPerCount
                required property string locomotionMode
                required property real gaitPhase
                required property string travelPathType
                required property bool closingTransition
                readonly property real animationFacing:
                    travelPathType === "follow" && locomotionMode !== "idle"
                        ? travelHeading : facing
                position: Qt.vector3d(fieldX - 80, 0,
                                      drillProject.fieldDepthSteps / 2 - fieldY)
                eulerRotation.y: -animationFacing

                HumanPerformer3D {
                    geometrySource: sharedHumanGeometry
                    metersPerStep: drillProject.metersPerStep
                    heightMeters: performerNode.performerHeightMeters
                    uniformColor: drillProject.performerMarkerStyle === "black" ? "#080b0a" : performerNode.performerColor
                    bodyRigId: performerNode.bodyRigId
                    skinPaletteId: performerNode.skinPaletteId
                    instrumentAssetId: performerNode.instrumentAssetId
                    equipmentAssetId: performerNode.equipmentAssetId
                    selected: performerNode.isSelected
                    marching: performerNode.locomotionMode !== "idle" && drillProject.playbackActive
                    gaitPhase: performerNode.gaitPhase
                    facingDegrees: performerNode.animationFacing
                    travelHeading: performerNode.travelHeading
                    transitionProgress: drillProject.playhead
                    countsInMove: drillProject.currentSetCounts
                    travelStepsPerCount: performerNode.travelStepsPerCount
                    locomotionMode: performerNode.locomotionMode
                    closingTransition: performerNode.closingTransition
                    debugOverlay: drillProject.debug3D
                }
            }
        }
        } // drillWorld

        Repeater3D {
            model: drillProject.props
            delegate: Prop3D {
                required property var modelData
                position: Qt.vector3d(modelData.worldX, 0, modelData.worldZ)
                eulerRotation.y: -modelData.rotation
                scale: Qt.vector3d(modelData.scaleX, modelData.scaleY, modelData.scaleZ)
                definitionId: modelData.definitionId
                primaryColor: drillProject.venueSecondaryColor
            }
        }
    }

    OrbitCameraController {
        anchors.fill: view
        z: 1.5
        origin: cameraOrigin
        camera: camera
        xSpeed: 0.16
        ySpeed: 0.16
        xInvert: true
        yInvert: false
        panEnabled: true
    }

    Row {
        id: cameraControls
        z: 2
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6
        Button { text: "Press box"; onClicked: root.setCameraPreset("press") }
        Button { text: "Overhead"; onClicked: root.setCameraPreset("overhead") }
        Button { text: "Field"; onClicked: root.setCameraPreset("field") }
        ToolButton { text: "−"; onClicked: { root.cameraZoom = Math.max(0.55, root.cameraZoom - 0.1); root.applyZoom() } }
        Slider { id: zoomSlider; width: 125; from: 0.55; to: 1.65; value: root.cameraZoom; onMoved: { root.cameraZoom = value; root.applyZoom() } }
        ToolButton { text: "+"; onClicked: { root.cameraZoom = Math.min(1.65, root.cameraZoom + 0.1); root.applyZoom() } }
        Label { text: "Zoom"; color: "#a9bbb0"; verticalAlignment: Text.AlignVCenter }
    }

    Flow {
        z: 2
        anchors.left: parent.left
        anchors.top: cameraControls.bottom
        anchors.margins: 14
        anchors.topMargin: 7
        width: parent.width - 28
        spacing: 6

        ComboBox {
            width: 175
            textRole: "text"; valueRole: "value"
            model: [
                { text: "Rehearsal field", value: "venue.rehearsal" },
                { text: "High school stadium", value: "venue.high_school" },
                { text: "Bowl stadium", value: "venue.bowl" },
                { text: "School gym", value: "venue.gym" },
                { text: "Indoor arena", value: "venue.arena" }
            ]
            Component.onCompleted: currentIndex = indexOfValue(drillProject.venuePreset)
            onActivated: drillProject.venuePreset = currentValue
        }
        ComboBox {
            width: 130
            textRole: "text"; valueRole: "value"
            model: [
                { text: "Daylight", value: "lighting.daylight" },
                { text: "Overcast", value: "lighting.overcast" },
                { text: "Sunset", value: "lighting.sunset" },
                { text: "Night game", value: "lighting.night" },
                { text: "Indoor", value: "lighting.indoor" }
            ]
            Component.onCompleted: currentIndex = indexOfValue(drillProject.lightingPreset)
            onActivated: drillProject.lightingPreset = currentValue
        }
        ComboBox {
            width: 135
            textRole: "text"; valueRole: "value"
            model: [
                { text: "Auto quality", value: "automatic" },
                { text: "Performance", value: "performance" },
                { text: "Balanced", value: "balanced" },
                { text: "Presentation", value: "presentation" }
            ]
            Component.onCompleted: currentIndex = indexOfValue(drillProject.graphicsProfile)
            onActivated: drillProject.graphicsProfile = currentValue
        }
        Button { text: "Add box prop"; onClicked: drillProject.addProp("prop.box", 80, drillProject.fieldDepthSteps / 2) }
        Button { text: "Add panel"; onClicked: drillProject.addProp("prop.panel", 80, drillProject.fieldDepthSteps / 2) }
        CheckBox { text: "Ground debug"; checked: drillProject.debug3D; onToggled: drillProject.debug3D = checked }
        Label {
            visible: !assetCatalog.valid
            text: "Asset catalog error"
            color: "#fb7185"
            ToolTip.visible: hovered
            ToolTip.text: assetCatalog.validationErrors.join("\n")
            property bool hovered: catalogHover.hovered
            HoverHandler { id: catalogHover }
        }
    }

}
