import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: root
    objectName: "performerView"
    readonly property int insertColumnCount: drillProject.fieldInsertCount
    readonly property bool editorField: drillProject.fieldStyle === "editor"
    property real cameraZoom: 1.0
    property string carriagePose: "horn.up"
    property bool markTimeDuringHolds: false
    property real cameraDistance: Math.sqrt(cameraBasePosition.y * cameraBasePosition.y
                                            + cameraBasePosition.z * cameraBasePosition.z)
    property vector3d cameraBasePosition: Qt.vector3d(0, 51, 63)
    property vector3d cameraBaseRotation: Qt.vector3d(-42, 0, 0)

    function applyZoom() {
        camera.z = cameraDistance * cameraZoom
    }

    function setCameraPreset(preset) {
        if (preset === "performer" || preset === "performer-side") {
            cameraBasePosition = Qt.vector3d(0, 0, 2.6)
            cameraBaseRotation = Qt.vector3d(0, preset === "performer-side" ? 90 : 0, 0)
        } else if (preset === "overhead") {
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
        cameraOrigin.position = Qt.vector3d(0, preset.indexOf("performer") === 0 ? 0.9 : 0, 0)
        cameraOrigin.eulerRotation = cameraBaseRotation
        applyZoom()
    }

    function insertStep(column) {
        return drillProject.fieldInsertStep(column)
    }

    Rectangle { anchors.fill: parent; color: MarchCraftTheme.input; radius: 0; border.color: MarchCraftTheme.divider }

    View3D {
        id: view
        anchors.fill: parent
        anchors.margins: 2
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: root.editorField ? "#18212d" : drillProject.lightingPreset === "lighting.sunset" ? "#b16d57"
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
                         ? (drillProject.performerCount > 150 ? 2 : 0) : 1
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
            visible: !root.editorField
            venueId: drillProject.venuePreset
            primaryColor: drillProject.venuePrimaryColor
            secondaryColor: drillProject.venueSecondaryColor
            fieldWidthMeters: (drillProject.fieldWidthSteps + 32) * drillProject.metersPerStep
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
            // Keep the apron decisively below the turf.  The previous coplanar
            // placement caused depth fighting to flash black through the field.
            position: Qt.vector3d(0, -0.62, 0)
            scale: Qt.vector3d(2.08, 0.010, (drillProject.fieldDepthSteps + 16) / 100)
            materials: PrincipledMaterial { baseColor: root.editorField ? "#18212d" : "#17201c"; roughness: 1.0 }
        }

        Model {
            source: "#Cube"
            position: Qt.vector3d(0, -0.50, 0)
            scale: Qt.vector3d(1.6, 0.010, drillProject.fieldDepthSteps / 100)
            materials: PrincipledMaterial {
                lighting: root.editorField ? PrincipledMaterial.NoLighting : PrincipledMaterial.FragmentLighting
                baseColor: root.editorField ? "#18212d" : drillProject.fieldPreset === "indoor" ? "#a97543" : drillProject.turfColor
                roughness: drillProject.fieldPreset === "indoor" ? 0.68 : 0.96
            }
        }

        // Ten-yard end zones beyond the two goal lines.
        Repeater3D {
            model: 2
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(index === 0 ? -88 : 88, -0.49, 0)
                scale: Qt.vector3d(0.16, 0.010, drillProject.fieldDepthSteps / 100)
                materials: PrincipledMaterial {
                    lighting: root.editorField ? PrincipledMaterial.NoLighting : PrincipledMaterial.FragmentLighting
                    baseColor: root.editorField ? "#18212d" : drillProject.venuePrimaryColor
                    roughness: 0.96
                }
            }
        }

        Repeater3D {
            model: 2
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(index === 0 ? -96 : 96, 0.06, 0)
                scale: Qt.vector3d(0.0018, 0.0003, drillProject.fieldDepthSteps / 100)
                materials: PrincipledMaterial { baseColor: "#f4f7f5"; roughness: 0.88 }
            }
        }
        Repeater3D {
            model: 4
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(index < 2 ? -88 : 88, 0.06,
                                      index % 2 === 0 ? drillProject.fieldDepthSteps / 2
                                                      : -drillProject.fieldDepthSteps / 2)
                scale: Qt.vector3d(0.16, 0.0003, 0.0018)
                materials: PrincipledMaterial { baseColor: "#f4f7f5"; roughness: 0.88 }
            }
        }

        Repeater3D {
            model: root.editorField || drillProject.fieldPreset === "indoor" ? 0 : 20
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(-76 + index * 8, 0.005, 0)
                scale: Qt.vector3d(0.08, 0.0001, drillProject.fieldDepthSteps / 100)
                materials: PrincipledMaterial {
                    baseColor: index % 2 === 0 ? Qt.lighter(drillProject.turfColor, 1.08) : drillProject.turfColor
                    roughness: 0.98
                }
            }
        }

        // All grid and five-yard lines share one high-resolution transparent plane.
        // Keep markings above turf and end-zone tops to avoid depth fighting at oblique angles.
        Model {
            source: "#Rectangle"
            position: Qt.vector3d(0, 0.06, 0)
            eulerRotation.x: -90
            scale: Qt.vector3d(root.editorField ? 1.92 : 1.6, drillProject.fieldDepthSteps / 100, 1)
            materials: PrincipledMaterial {
                alphaMode: PrincipledMaterial.Blend
                lighting: PrincipledMaterial.NoLighting
                baseColorMap: Texture {
                    generateMipmaps: true
                    mipFilter: Texture.Linear
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
                            const minX = root.editorField ? -16 : 0
                            const spanX = root.editorField ? 192 : 160

                            if (root.editorField || drillProject.showFieldGrid) {
                                const interval = root.editorField ? 1 : drillProject.fieldGridInterval
                                ctx.strokeStyle = root.editorField ? "#8198b5" : drillProject.fieldGridColor
                                ctx.globalAlpha = root.editorField ? 0.6 : Math.min(0.36, drillProject.fieldGridOpacity)
                                ctx.lineWidth = root.editorField ? 2 : 1
                                for (let x = minX; x <= minX + spanX + 0.001; x += interval) {
                                    const px = (x - minX) / spanX * width
                                    ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                                }
                                for (let y = 0; y <= drillProject.fieldDepthSteps + 0.001; y += interval) {
                                    ctx.lineWidth = root.editorField ? (y % 8 === 0 ? 3 : 2) : 1
                                    const py = height - y / drillProject.fieldDepthSteps * height
                                    ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(width, py); ctx.stroke()
                                }
                            }

                            ctx.strokeStyle = "#edf4ef"
                            ctx.globalAlpha = 0.96
                            ctx.lineWidth = 5
                            for (let step = 0; step <= 160; step += 8) {
                                const px = (step - minX) / spanX * width
                                ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                            }

                            // Front/back sidelines and the short one-yard
                            // inserts are part of the same texture as the
                            // yard lines, so they stay aligned and avoid a
                            // large collection of thin depth-sensitive meshes.
                            ctx.globalAlpha = 1.0
                            ctx.lineWidth = 5
                            ctx.beginPath(); ctx.moveTo(0, 2.5); ctx.lineTo(width, 2.5); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(0, height - 2.5); ctx.lineTo(width, height - 2.5); ctx.stroke()

                            const markLength = (24.0 / 22.5) / drillProject.fieldDepthSteps * height
                            for (let column = 0; !root.editorField && column < drillProject.fieldInsertCount; ++column) {
                                const px = drillProject.fieldInsertStep(column) / 160 * width
                                ctx.lineWidth = 4
                                ctx.beginPath(); ctx.moveTo(px, 3); ctx.lineTo(px, 3 + markLength); ctx.stroke()
                                ctx.beginPath(); ctx.moveTo(px, height - 3); ctx.lineTo(px, height - 3 - markLength); ctx.stroke()
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
                 position: Qt.vector3d(index * 16 - 64, 0.08, drillProject.fieldDepthSteps / 2 - 12.8)
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
                 position: Qt.vector3d(index * 16 - 64, 0.08, -drillProject.fieldDepthSteps / 2 + 12.8)
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
                 position: Qt.vector3d(0, 0.06, index === 0
                                      ? drillProject.fieldDepthSteps / 2
                                      : -drillProject.fieldDepthSteps / 2)
                scale: Qt.vector3d(1.6, 0.0003, 0.0018)
                materials: PrincipledMaterial { baseColor: "#f4f7f5"; roughness: 0.88 }
            }
        }

        // Four short one-yard inserts in every five-yard interval on each hash row.
        // They extend outward, matching the original field geometry.
        Repeater3D {
            model: root.editorField ? 0 : root.insertColumnCount * 2
            delegate: Model {
                required property int index
                readonly property int column: index % root.insertColumnCount
                readonly property bool backHash: index >= root.insertColumnCount
                readonly property real markLength: 24.0 / 22.5
                readonly property real hashZ: drillProject.fieldDepthSteps / 2
                                                   - (backHash ? drillProject.backHashSteps
                                                               : drillProject.frontHashSteps)
                source: "#Cube"
                 position: Qt.vector3d(root.insertStep(column) - 80, 0.06,
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
                 position: Qt.vector3d(yardLine * 8 - 80, 0.06,
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
                required property bool performerVisible
                required property string bodyRigId
                required property string uniformId
                required property string skinPaletteId
                required property string instrumentAssetId
                required property string equipmentAssetId
                required property real performerHeightMeters
                required property real travelHeading
                required property real travelStepsPerCount
                required property string locomotionMode
                required property real gaitPhase
                required property real gaitElapsedCounts
                required property string travelPathType
                required property bool closingTransition
                readonly property real animationFacing:
                    travelPathType === "follow" && locomotionMode !== "idle"
                        ? travelHeading : facing
                readonly property bool markingTime: root.markTimeDuringHolds && locomotionMode === "idle"
                    && drillProject.playbackActive && drillProject.currentSetIndex > 0
                    && drillProject.currentSetCounts > 0
                visible: performerVisible
                position: Qt.vector3d(fieldX - 80, 0, drillProject.fieldDepthSteps / 2 - fieldY)
                eulerRotation.y: animationFacing
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
                    marching: (performerNode.locomotionMode !== "idle" || performerNode.markingTime) && drillProject.playbackActive
                    gaitPhase: performerNode.gaitPhase
                    gaitElapsedCounts: performerNode.gaitElapsedCounts
                    facingDegrees: performerNode.animationFacing
                    travelHeading: performerNode.travelHeading
                    transitionProgress: drillProject.playhead
                    countsInMove: drillProject.currentSetCounts
                    travelStepsPerCount: performerNode.travelStepsPerCount
                    locomotionMode: performerNode.markingTime ? "mark_time" : performerNode.locomotionMode
                    carriagePose: root.carriagePose
                    closingTransition: !performerNode.markingTime && performerNode.closingTransition
                    castBodyShadow: drillProject.graphicsProfile === "presentation" ||
                                    (drillProject.graphicsProfile !== "performance" &&
                                     drillProject.performerCount <= 150)
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

    // Keep camera zoom available through the viewport itself so the editor
    // stays visually quiet while retaining the familiar wheel gesture.
    MouseArea {
        anchors.fill: view
        acceptedButtons: Qt.NoButton
        onWheel: function(event) {
            root.cameraZoom = Math.max(0.55, Math.min(1.65,
                root.cameraZoom * (event.angleDelta.y > 0 ? 0.92 : 1.08)))
            root.applyZoom()
            event.accepted = true
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
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6
        AppButton { text: "Press box"; onClicked: root.setCameraPreset("press") }
        AppButton { text: "Overhead"; onClicked: root.setCameraPreset("overhead") }
        AppButton { text: "Field"; onClicked: root.setCameraPreset("field") }
    }

    Flow {
        z: 2
        anchors.left: parent.left
        anchors.right: cameraControls.left
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        anchors.topMargin: 14
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
        ComboBox {
            width: 135
            model: ["Horns up", "Horns down"]
            currentIndex: root.carriagePose === "horn.down" ? 1 : 0
            onActivated: root.carriagePose = currentIndex === 1 ? "horn.down" : "horn.up"
        }
        CheckBox {
            text: "Mark time on holds"
            checked: root.markTimeDuringHolds
            onToggled: root.markTimeDuringHolds = checked
        }
        Label {
            visible: !assetCatalog.valid || !sharedHumanGeometry.valid
            text: "Performer asset error"
            color: "#e07178"
            ToolTip.visible: hovered
            ToolTip.text: assetCatalog.validationErrors.join("\n") + "\n" + sharedHumanGeometry.errorString
            property bool hovered: catalogHover.hovered
            HoverHandler { id: catalogHover }
        }
    }

}
