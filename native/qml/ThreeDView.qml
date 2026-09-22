import QtQuick
import QtQuick.Controls
import QtQuick3D
import "CameraMotion.js" as CameraMotion

Item {
    id: root
    objectName: "performerView"
    readonly property int insertColumnCount: drillProject.fieldInsertCount
    readonly property bool editorField: drillProject.fieldStyle === "editor"
    property string carriagePose: "horn.up"
    property bool markTimeDuringHolds: false
    property real orbitPitch: -42
    property real orbitYaw: 0
    property real cameraDistance: 108
    property real focusX: 0
    property real focusY: 0
    property real focusZ: 0
    property string activeCameraPreset: "press"
    property bool cameraReady: false
    readonly property int cameraDuration: cameraReady && workspaceController.systemAnimationsEnabled
                                         ? (cameraInput.pressed ? 90 : 320) : 0
    Component.onCompleted: cameraReady = true

    function setCameraPreset(preset) {
        activeCameraPreset = preset
        focusX = 0; focusZ = 0
        focusY = preset.indexOf("performer") === 0 ? 0.9 : 0
        // Take the shortest route back to the audience side after a full orbit.
        const baseYaw = Math.round(orbitYaw / 360) * 360
        orbitYaw = baseYaw + (preset === "performer-side" ? 90 : 0)
        if (preset === "performer" || preset === "performer-side") {
            orbitPitch = 0; cameraDistance = 2.6
        } else if (preset === "overhead") {
            orbitPitch = -89.5; cameraDistance = 100
        } else if (preset === "field") {
            orbitPitch = -10; cameraDistance = 50
        } else {
            orbitPitch = -42; cameraDistance = 108
        }
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
            property real pitch: root.orbitPitch
            property real yaw: root.orbitYaw
            property real targetX: root.focusX
            property real targetY: root.focusY
            property real targetZ: root.focusZ
            eulerRotation: Qt.vector3d(pitch, yaw, 0)
            position: Qt.vector3d(targetX, targetY, targetZ)
            Behavior on pitch { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
            Behavior on yaw { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
            Behavior on targetX { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
            Behavior on targetY { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
            Behavior on targetZ { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
            PerspectiveCamera {
                id: camera
                z: root.cameraDistance
                fieldOfView: 45
                clipNear: 0.1
                clipFar: 2000
                Behavior on z { NumberAnimation { duration: root.cameraDuration; easing.type: Easing.OutCubic } }
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

        // Deterministic surface detail is baked once into a mipmapped texture.
        Model {
            visible: !root.editorField
            source: "#Rectangle"; y: 0.018; eulerRotation.x: -90
            scale: Qt.vector3d(1.6, drillProject.fieldDepthSteps / 100, 1)
            materials: PrincipledMaterial {
                roughness: drillProject.fieldPreset === "indoor" ? 0.65 : 0.98
                baseColorMap: Texture {
                    generateMipmaps: true; mipFilter: Texture.Linear
                    sourceItem: Canvas {
                        id: surfaceTexture
                        width: 2048; height: 1024
                        onPaint: {
                            const ctx = getContext("2d"); ctx.reset()
                            const indoor = drillProject.fieldPreset === "indoor"
                            ctx.fillStyle = indoor ? "#b98b58" : drillProject.turfColor
                            ctx.fillRect(0, 0, width, height)
                            for (let band = 0; band < 20; ++band) {
                                ctx.fillStyle = band % 2 ? "#ffffff" : "#000000"
                                ctx.globalAlpha = indoor ? 0.015 : 0.045
                                ctx.fillRect(band * width / 20, 0, width / 20, height)
                            }
                            let seed = 29
                            function random() { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296 }
                            for (let i = 0; i < 42000; ++i) {
                                ctx.globalAlpha = indoor ? 0.07 : 0.10
                                ctx.fillStyle = i % 2 ? "#ffffff" : "#172d1b"
                                ctx.fillRect(random() * width, random() * height, indoor ? 14 : 1, indoor ? 1 : 2)
                            }
                            if (indoor) {
                                ctx.globalAlpha = 0.18; ctx.strokeStyle = "#65482b"; ctx.lineWidth = 1
                                for (let row = 0; row < 80; ++row) {
                                    const y = row * height / 80
                                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                                    for (let x = (row % 3) * 85; x < width; x += 256) {
                                        ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x, y + height / 80); ctx.stroke()
                                    }
                                }
                            }
                            ctx.globalAlpha = 1
                        }
                        Component.onCompleted: requestPaint()
                        Connections {
                            target: drillProject
                            function onEditorSettingsChanged() { surfaceTexture.requestPaint() }
                            function onProjectChanged() { surfaceTexture.requestPaint() }
                            function onSceneChanged() { surfaceTexture.requestPaint() }
                        }
                    }
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
                    && drillProject.playbackActive && drillProject.playbackSetIndex > 0
                    && drillProject.playbackSetCounts > 0
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
                    countsInMove: drillProject.playbackSetCounts
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

    MouseArea {
        id: cameraInput
        anchors.fill: view
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        property real lastX: 0
        property real lastY: 0
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onPressed: function(mouse) { lastX = mouse.x; lastY = mouse.y }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            const dx = mouse.x - lastX, dy = mouse.y - lastY
            lastX = mouse.x; lastY = mouse.y
            root.activeCameraPreset = "custom"
            if ((pressedButtons & (Qt.MiddleButton | Qt.RightButton)) || (mouse.modifiers & Qt.ShiftModifier)) {
                const target = CameraMotion.pan(root.focusX, root.focusZ, root.orbitYaw,
                    root.orbitPitch, root.cameraDistance, height, dx, dy)
                root.focusX = target.x; root.focusZ = target.z
            } else {
                const target = CameraMotion.orbit(root.orbitPitch, root.orbitYaw, dx, dy)
                root.orbitPitch = target.pitch; root.orbitYaw = target.yaw
            }
        }
        onDoubleClicked: root.setCameraPreset("press")
        onWheel: function(event) {
            const delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y * 3 : event.angleDelta.y
            root.cameraDistance = CameraMotion.zoom(root.cameraDistance, delta)
            root.activeCameraPreset = "custom"
            event.accepted = true
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 14
        width: cameraHelp.implicitWidth + 24; height: 30; radius: 6
        color: MarchCraftTheme.panel; opacity: 0.92
        Label {
            id: cameraHelp; anchors.centerIn: parent
            text: "Drag to orbit  ·  Shift-drag to pan  ·  Scroll to dolly  ·  Double-click to reset"
            color: MarchCraftTheme.textSecondary; font.pixelSize: 11
        }
    }

    Row {
        id: cameraControls
        z: 2
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 6
        AppButton { text: "Press box"; highlighted: root.activeCameraPreset === "press"; onClicked: root.setCameraPreset("press") }
        AppButton { text: "Overhead"; highlighted: root.activeCameraPreset === "overhead"; onClicked: root.setCameraPreset("overhead") }
        AppButton { text: "Field"; highlighted: root.activeCameraPreset === "field"; onClicked: root.setCameraPreset("field") }
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
