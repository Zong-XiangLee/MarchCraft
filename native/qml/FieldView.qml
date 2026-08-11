import QtQuick
import QtQuick.Controls

Item {
    id: root
    property real zoom: 1.0
    property bool showPaths: true
    property bool showShapeGuides: false
    property bool showLabels: true
    property bool snapEnabled: true
    property real gridSize: 1.0
    property string hoverCoordinate: ""
    property bool spaceHeld: false
    property bool drawMode: false
    property var selectionBounds: drillProject.selectedBounds()
    property int selectedShapeIndex: drillProject.selectedShapeIndex()
    signal performerActivated(int row)
    signal contextMenuRequested(real screenX, real screenY, int performerRow)
    signal freehandCompleted(var points)

    function toCanvasX(fieldX) { return (fieldX - drillProject.canvasMinX) * field.sx }
    function toCanvasY(fieldY) { return (drillProject.canvasMaxY - fieldY) * field.sy }
    function toFieldX(canvasX) { return canvasX / field.sx + drillProject.canvasMinX }
    function toFieldY(canvasY) { return drillProject.canvasMaxY - canvasY / field.sy }

    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Space) {
            root.spaceHeld = true
            event.accepted = true
        }
    }
    Keys.onReleased: function(event) {
        if (event.key === Qt.Key_Space) {
            root.spaceHeld = false
            event.accepted = true
        }
    }

    function zoomAt(pointerX, pointerY, wheelDelta) {
        const oldZoom = root.zoom
        const nextZoom = Math.max(0.7, Math.min(3.5,
            oldZoom * (wheelDelta > 0 ? 1.12 : 0.89)))
        if (nextZoom === oldZoom)
            return

        // Use field-local coordinates for both modes. That makes the point
        // invariant while the field resizes, instead of trying to correct a
        // screen-space point after the fact.
        let anchorX = pointerX + viewport.contentX - field.x
        let anchorY = pointerY + viewport.contentY - field.y
        let selectedTotal = 0
        let selectedX = 0
        let selectedY = 0
        for (let i = 0; i < performerRepeater.count; ++i) {
            const performer = performerRepeater.itemAt(i)
            if (performer && performer.isSelected) {
                selectedX += root.toCanvasX(performer.fieldX)
                selectedY += root.toCanvasY(performer.fieldY)
                selectedTotal++
            }
        }
        if (selectedTotal > 0) {
            anchorX = selectedX / selectedTotal
            anchorY = selectedY / selectedTotal
        }

        root.zoom = nextZoom
        Qt.callLater(function() {
            const maxX = Math.max(0, viewport.contentWidth - viewport.width)
            const maxY = Math.max(0, viewport.contentHeight - viewport.height)
            const targetX = field.x + anchorX - viewport.width / 2
            const targetY = field.y + anchorY - viewport.height / 2
            viewport.contentX = Math.max(0, Math.min(maxX, targetX))
            viewport.contentY = Math.max(0, Math.min(maxY, targetY))
        })
    }

    Rectangle {
        anchors.fill: parent
        color: "#07110d"
        radius: 10
        border.color: "#27352f"
    }

    Flickable {
        id: viewport
        anchors.fill: parent
        anchors.margins: 12
        clip: true
        contentWidth: Math.max(width, field.width)
        contentHeight: Math.max(height, field.height)
        boundsBehavior: Flickable.StopAtBounds
        // The field is zoomed with WheelHandler; prevent Flickable from
        // consuming the same wheel event as a scroll gesture.
        interactive: false

        Item {
            id: field
            readonly property real canvasWidthSteps: drillProject.canvasMaxX - drillProject.canvasMinX
            readonly property real canvasDepthSteps: drillProject.canvasMaxY - drillProject.canvasMinY
            readonly property real aspect: canvasWidthSteps / canvasDepthSteps
            width: Math.min(viewport.width - 4, (viewport.height - 4) * aspect) * root.zoom
            height: width / aspect
            x: Math.max(2, (viewport.width - width) / 2)
            y: Math.max(2, (viewport.height - height) / 2)
            readonly property real sx: width / canvasWidthSteps
            readonly property real sy: height / canvasDepthSteps

            Canvas {
                id: fieldCanvas
                anchors.fill: parent
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = "#17201c"
                    ctx.fillRect(0, 0, width, height)
                    ctx.translate((0 - drillProject.canvasMinX) * field.sx,
                                  (drillProject.canvasMaxY - drillProject.fieldDepthSteps) * field.sy)
                    const w = drillProject.fieldWidthSteps * field.sx
                    const h = drillProject.fieldDepthSteps * field.sy
                    const turf = ctx.createLinearGradient(0, 0, 0, h)
                    turf.addColorStop(0, "#123b2a")
                    turf.addColorStop(0.5, "#1b5037")
                    turf.addColorStop(1, "#123b2a")
                    ctx.fillStyle = turf
                    ctx.fillRect(0, 0, w, h)

                    // Subtle five-yard mowing bands add depth without changing the field geometry.
                    ctx.globalAlpha = 0.13
                    for (let panel = 0; panel < drillProject.fieldWidthSteps; panel += 8) {
                        if ((panel / 8) % 2 === 0) {
                            ctx.fillStyle = "#5b8d6e"
                            ctx.fillRect(panel * field.sx, 0, 8 * field.sx, h)
                        }
                    }

                    // Regulation four-inch yard lines at five-yard intervals.
                    ctx.strokeStyle = "#edf4ef"
                    ctx.globalAlpha = 0.82
                    ctx.lineWidth = Math.max(1, 0.18 * field.sx)
                    for (let step = 0; step <= drillProject.fieldWidthSteps; step += 8) {
                        const x = step * field.sx
                        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke()
                    }

                    // Front/back sidelines. The audience/front side is the bottom edge.
                    ctx.globalAlpha = 0.98
                    ctx.strokeStyle = "#f4f7f5"
                    ctx.lineWidth = Math.max(1.5, 0.22 * field.sy)
                    ctx.beginPath(); ctx.moveTo(0, 1); ctx.lineTo(w, 1); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(0, h - 1); ctx.lineTo(w, h - 1); ctx.stroke()

                    // There are four one-yard inserts between adjacent five-yard lines.
                    // At 8-to-5 they fall at 1.6-step intervals, never on the full yard line.
                    // All short-yardage marks run vertically, perpendicular to the sidelines.
                    ctx.lineWidth = Math.max(1, 0.18 * field.sx)
                    const hashRows = [drillProject.frontHashSteps, drillProject.backHashSteps]
                    const markLength = (24.0 / 22.5) * field.sy
                    for (let column = 0; column < drillProject.fieldInsertCount; ++column) {
                        const x = drillProject.fieldInsertStep(column) * field.sx
                        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, markLength); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(x, h); ctx.lineTo(x, h - markLength); ctx.stroke()
                        for (let hashIndex = 0; hashIndex < hashRows.length; ++hashIndex) {
                            const hashY = (drillProject.fieldDepthSteps - hashRows[hashIndex]) * field.sy
                            ctx.beginPath()
                            ctx.moveTo(x, hashY - markLength / 2)
                            ctx.lineTo(x, hashY + markLength / 2)
                            ctx.stroke()
                        }
                    }

                    // Six-foot yard numbers centered eight yards from each sideline.
                    ctx.globalAlpha = 1
                    ctx.fillStyle = "#eaf2ed"
                    ctx.strokeStyle = "#dce8e1"
                    ctx.lineWidth = Math.max(0.8, 0.12 * field.sy)
                    ctx.font = "700 " + Math.max(11, 3.2 * field.sy) + "px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    const numberCenterSteps = 12.8
                    for (let numberStep = 16; numberStep < drillProject.fieldWidthSteps; numberStep += 16) {
                        const x = numberStep * field.sx
                        const yardNumber = Math.round(50 - Math.abs(80 - numberStep) * 5 / 8)
                        ctx.save()
                        ctx.translate(x, (drillProject.fieldDepthSteps - numberCenterSteps) * field.sy)
                        ctx.rotate(Math.PI)
                        ctx.strokeText(yardNumber.toString(), 0, 0)
                        ctx.fillText(yardNumber.toString(), 0, 0)
                        ctx.restore()
                        ctx.save()
                        ctx.translate(x, numberCenterSteps * field.sy)
                        ctx.strokeText(yardNumber.toString(), 0, 0)
                        ctx.fillText(yardNumber.toString(), 0, 0)
                        ctx.restore()
                    }

                    ctx.strokeStyle = "#f2f7f4"
                    ctx.lineWidth = Math.max(2, 0.3 * field.sy)
                    ctx.strokeRect(1, 1, w - 2, h - 2)
                }
                Connections {
                    target: drillProject
                    function onProjectChanged() { fieldCanvas.requestPaint() }
                }
            }

            Canvas {
                id: gridCanvas
                anchors.fill: parent; z: 0.5; visible: drillProject.showFieldGrid
                antialiasing: false
                onPaint: {
                    const ctx = getContext("2d"); ctx.reset()
                    if (!drillProject.showFieldGrid) return
                    ctx.strokeStyle = drillProject.fieldGridColor
                    ctx.globalAlpha = drillProject.fieldGridOpacity
                    ctx.lineWidth = 1
                    const step = drillProject.fieldGridInterval
                    for (let x = Math.ceil(drillProject.canvasMinX / step) * step; x <= drillProject.canvasMaxX; x += step) {
                        const px = root.toCanvasX(x); ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, height); ctx.stroke()
                    }
                    for (let y = Math.ceil(drillProject.canvasMinY / step) * step; y <= drillProject.canvasMaxY; y += step) {
                        const py = root.toCanvasY(y); ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(width, py); ctx.stroke()
                    }
                }
                Connections { target: drillProject; function onEditorSettingsChanged() { gridCanvas.requestPaint() } function onProjectChanged() { gridCanvas.requestPaint() } }
                onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
            }

            Canvas {
                id: geometryCanvas
                anchors.fill: parent
                z: 1
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.lineJoin = "round"
                    ctx.lineCap = "round"
                    for (let shapeIndex = 0; root.showShapeGuides && shapeIndex < drillProject.currentShapeCount; ++shapeIndex) {
                        const shape = drillProject.shapeInfo(shapeIndex)
                        const points = shape.points || []
                        if (points.length < 2) continue
                        ctx.strokeStyle = "#67e8f9"
                        ctx.globalAlpha = 0.72
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        ctx.moveTo(root.toCanvasX(points[0].x), root.toCanvasY(points[0].y))
                        for (let i = 1; i < points.length; ++i)
                            ctx.lineTo(root.toCanvasX(points[i].x), root.toCanvasY(points[i].y))
                        ctx.stroke()
                    }
                    if (!root.showPaths || drillProject.currentSetIndex <= 0) return
                    for (let row = 0; row < performerRepeater.count; ++row) {
                        const marcher = performerRepeater.itemAt(row)
                        if (!marcher) continue
                        const points = drillProject.transitionPathSamples(row, 24)
                        if (points.length < 2) continue
                        ctx.strokeStyle = marcher.isSelected ? "#fbbf24" : "#d2e5da"
                        ctx.globalAlpha = marcher.isSelected ? 0.9 : 0.25
                        ctx.lineWidth = marcher.isSelected ? 2.5 : 1
                        ctx.beginPath()
                        ctx.moveTo(root.toCanvasX(points[0].x), root.toCanvasY(points[0].y))
                        for (let i = 1; i < points.length; ++i)
                            ctx.lineTo(root.toCanvasX(points[i].x), root.toCanvasY(points[i].y))
                        ctx.stroke()
                    }
                }
                Connections {
                    target: drillProject
                    function onShapesChanged() { geometryCanvas.requestPaint() }
                    function onCurrentSetChanged() { geometryCanvas.requestPaint() }
                    function onProjectChanged() { geometryCanvas.requestPaint() }
                    function onSelectionChanged() { geometryCanvas.requestPaint() }
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: root
                    function onShowPathsChanged() { geometryCanvas.requestPaint() }
                    function onShowShapeGuidesChanged() { geometryCanvas.requestPaint() }
                }
            }

            Canvas {
                id: formationPreviewCanvas
                anchors.fill: parent
                z: 4
                visible: drillProject.formationPreviewActive
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const points = drillProject.formationPreviewPoints
                    ctx.setLineDash([7, 5])
                    ctx.lineWidth = 1.5
                    ctx.strokeStyle = "#99a7f3d0"
                    for (let i = 0; i < points.length; ++i) {
                        const point = points[i]
                        ctx.beginPath()
                        ctx.moveTo(root.toCanvasX(point.fromX), root.toCanvasY(point.fromY))
                        ctx.lineTo(root.toCanvasX(point.x), root.toCanvasY(point.y))
                        ctx.stroke()
                    }
                    ctx.setLineDash([])
                    for (let i = 0; i < points.length; ++i) {
                        const point = points[i]
                        const x = root.toCanvasX(point.x), y = root.toCanvasY(point.y)
                        ctx.beginPath(); ctx.arc(x, y, 6, 0, Math.PI * 2)
                        ctx.fillStyle = "#cc22d3a7"; ctx.fill()
                        ctx.lineWidth = 2; ctx.strokeStyle = "#ecfdf5"; ctx.stroke()
                    }
                }
                Connections {
                    target: drillProject
                    function onFormationPreviewChanged() { formationPreviewCanvas.requestPaint() }
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            Repeater {
                model: drillProject.props
                delegate: Rectangle {
                    required property var modelData
                    z: 3
                    width: Math.max(10, modelData.widthSteps * field.sx)
                    height: Math.max(8, modelData.depthSteps * field.sy)
                    x: root.toCanvasX(modelData.fieldX) - width / 2
                    y: root.toCanvasY(modelData.fieldY) - height / 2
                    rotation: -modelData.rotation
                    radius: 2
                    color: modelData.highlighted ? "#aaef4444" : "#aa9a6b3f"
                    border.width: modelData.highlighted ? 3 : 1.5
                    border.color: modelData.highlighted ? "#fecaca" : "#fde68a"
                    Label {
                        anchors.centerIn: parent
                        text: parent.modelData.label.toUpperCase()
                        color: "#fff7ed"; font.bold: true; font.pixelSize: 8
                        rotation: -parent.rotation
                    }
                    SequentialAnimation on opacity {
                        running: modelData.highlighted; loops: Animation.Infinite
                        NumberAnimation { to: 0.55; duration: 450 }
                        NumberAnimation { to: 1.0; duration: 450 }
                    }
                }
            }

            MouseArea {
                id: selectionArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                z: 0
                enabled: !root.spaceHeld
                property bool selecting: false
                property bool lasso: false
                property point startPoint: Qt.point(0, 0)
                property point currentPoint: Qt.point(0, 0)
                property var lassoPoints: []
                onPressed: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        const p = mapToItem(root, mouse.x, mouse.y)
                        root.contextMenuRequested(p.x, p.y, -1)
                        return
                    }
                    root.forceActiveFocus()
                    selecting = true
                    lasso = root.drawMode || (mouse.modifiers & Qt.ShiftModifier) !== 0
                    startPoint = Qt.point(mouse.x, mouse.y)
                    currentPoint = startPoint
                    lassoPoints = [Qt.point(root.toFieldX(mouse.x), root.toFieldY(mouse.y))]
                    lassoCanvas.requestPaint()
                }
                onPositionChanged: function(mouse) {
                    if (!selecting) return
                    currentPoint = Qt.point(mouse.x, mouse.y)
                    if (lasso) {
                        const next = Qt.point(root.toFieldX(mouse.x), root.toFieldY(mouse.y))
                        const previous = lassoPoints[lassoPoints.length - 1]
                        if (!previous || Math.hypot(next.x - previous.x, next.y - previous.y) > 0.35) {
                            const copy = lassoPoints.slice()
                            copy.push(next)
                            lassoPoints = copy
                            lassoCanvas.requestPaint()
                        }
                    }
                    root.hoverCoordinate = Math.round(root.toFieldX(mouse.x) * 4) / 4 + ", " +
                        Math.round(root.toFieldY(mouse.y) * 4) / 4 + " steps"
                }
                onReleased: function(mouse) {
                    if (!selecting) return
                    if (root.drawMode) {
                        if (lassoPoints.length >= 2) root.freehandCompleted(lassoPoints.slice())
                        selecting = false; lassoPoints = []; lassoCanvas.requestPaint(); return
                    }
                    currentPoint = Qt.point(mouse.x, mouse.y)
                    const dx = currentPoint.x - startPoint.x
                    const dy = currentPoint.y - startPoint.y
                    const additive = (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0
                    if (Math.hypot(dx, dy) < 4) {
                        if (!additive) drillProject.clearSelection()
                    } else if (lasso && lassoPoints.length >= 3) {
                        drillProject.selectInPolygon(lassoPoints, true)
                    } else {
                        drillProject.selectInRect(root.toFieldX(startPoint.x), root.toFieldY(startPoint.y),
                                                  root.toFieldX(currentPoint.x), root.toFieldY(currentPoint.y),
                                                  additive)
                    }
                    selecting = false
                    lassoPoints = []
                    lassoCanvas.requestPaint()
                }
                onCanceled: {
                    selecting = false
                    lassoPoints = []
                    lassoCanvas.requestPaint()
                }
                onDoubleClicked: {
                    const fx = root.toFieldX(mouse.x)
                    const fy = root.toFieldY(mouse.y)
                    drillProject.addPerformer("P" + (drillProject.performerCount + 1), "Unassigned", "Unassigned", fx, fy)
                }
            }

            Rectangle {
                z: 6
                visible: selectionArea.selecting && !selectionArea.lasso && !root.drawMode
                x: Math.min(selectionArea.startPoint.x, selectionArea.currentPoint.x)
                y: Math.min(selectionArea.startPoint.y, selectionArea.currentPoint.y)
                width: Math.abs(selectionArea.currentPoint.x - selectionArea.startPoint.x)
                height: Math.abs(selectionArea.currentPoint.y - selectionArea.startPoint.y)
                color: "#4438bdf8"
                border.color: "#7dd3fc"
                border.width: 1
            }

            Rectangle {
                id: selectionBox; z: 5; color: "transparent"; border.color: "#fbbf24"; border.width: 1
                visible: root.selectedShapeIndex >= 0 && Object.keys(root.selectionBounds).length > 0
                x: root.toCanvasX(root.selectionBounds.left || 0) - 7
                y: root.toCanvasY(root.selectionBounds.bottom || 0) - 7
                width: (root.selectionBounds.right - root.selectionBounds.left) * field.sx + 14
                height: (root.selectionBounds.bottom - root.selectionBounds.top) * field.sy + 14
                Rectangle {
                    width: 2; height: 18; color: "#fbbf24"; anchors.horizontalCenter: parent.horizontalCenter; y: -18
                }
                Rectangle {
                    id: rotateHandle; width: 26; height: 26; radius: 13; color: "#14231d"; border.color: "#fbbf24"; border.width: 2
                    anchors.horizontalCenter: parent.horizontalCenter; y: -42
                    Text { anchors.centerIn: parent; text: "↻"; color: "#fbbf24"; font.pixelSize: 18; font.bold: true }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.CrossCursor; preventStealing: true
                        property real startAngle
                        onPressed: function(mouse) { const p=mapToItem(field,mouse.x,mouse.y); startAngle=Math.atan2(root.toFieldY(p.y)-root.selectionBounds.centerY,root.toFieldX(p.x)-root.selectionBounds.centerX)*180/Math.PI; drillProject.beginRotate() }
                        onPositionChanged: function(mouse) { if(!pressed)return; const p=mapToItem(field,mouse.x,mouse.y); let a=Math.atan2(root.toFieldY(p.y)-root.selectionBounds.centerY,root.toFieldX(p.x)-root.selectionBounds.centerX)*180/Math.PI-startAngle; if(mouse.modifiers & Qt.ShiftModifier)a=Math.round(a/15)*15; drillProject.previewRotate(a) }
                        onReleased: drillProject.endRotate(); onCanceled: drillProject.endRotate()
                    }
                }
                Repeater {
                    model: [{right:false,bottom:false},{right:true,bottom:false},{right:false,bottom:true},{right:true,bottom:true}]
                    delegate: Rectangle {
                        required property var modelData
                        width: 14; height: 14; radius: 2; color: "#fbbf24"; border.color: "#07110d"
                        x: modelData.right ? selectionBox.width-width/2 : -width/2
                        y: modelData.bottom ? selectionBox.height-height/2 : -height/2
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.SizeFDiagCursor; preventStealing: true
                            property real startRadius: 1
                            property real startCenterX
                            property real startCenterY
                            onPressed: function(mouse) { const p=mapToItem(field,mouse.x,mouse.y);startCenterX=root.selectionBounds.centerX;startCenterY=root.selectionBounds.centerY;const fx=root.toFieldX(p.x),fy=root.toFieldY(p.y);startRadius=Math.max(.01,Math.hypot(fx-startCenterX,fy-startCenterY));drillProject.beginScale() }
                            onPositionChanged: function(mouse) { if(!pressed)return;const p=mapToItem(field,mouse.x,mouse.y);const radius=Math.hypot(root.toFieldX(p.x)-startCenterX,root.toFieldY(p.y)-startCenterY);drillProject.previewScale(radius/startRadius) }
                            onReleased: drillProject.endScale(); onCanceled: drillProject.endScale()
                        }
                    }
                }
            }

            Canvas {
                id: lassoCanvas
                anchors.fill: parent
                z: 6
                visible: selectionArea.selecting && (selectionArea.lasso || root.drawMode)
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const points = selectionArea.lassoPoints
                    if (points.length < 2) return
                    ctx.strokeStyle = root.drawMode ? "#fbbf24" : "#7dd3fc"
                    ctx.fillStyle = root.drawMode ? "#10fbbf24" : "#2238bdf8"
                    ctx.lineWidth = root.drawMode ? 3 : 1.5
                    ctx.beginPath()
                    ctx.moveTo(root.toCanvasX(points[0].x), root.toCanvasY(points[0].y))
                    for (let i = 1; i < points.length; ++i)
                        ctx.lineTo(root.toCanvasX(points[i].x), root.toCanvasY(points[i].y))
                    if (!root.drawMode) { ctx.closePath(); ctx.fill() }
                    ctx.stroke()
                }
            }

            Repeater {
                id: performerRepeater
                model: drillProject
                delegate: Item {
                    id: marcher
                    required property int index
                    required property real fieldX
                    required property real fieldY
                    required property real fromX
                    required property real fromY
                    required property string label
                    required property string symbol
                    required property color performerColor
                    required property bool isSelected
                    required property bool hasWarning
                    required property bool performerVisible
                    required property bool performerLocked
                    required property real facing
                    z: 2
                    visible: performerVisible
                    opacity: performerLocked ? 0.55 : 1.0
                    x: root.toCanvasX(fieldX) - width / 2
                    y: root.toCanvasY(fieldY) - height / 2
                    width: Math.max(7, drillProject.performerMarkerSize * root.zoom
                                    * (drillProject.performerMarkerStyle === "compact" ? 0.78 : 1.0))
                    height: width

                    Rectangle {
                        visible: false
                        x: marcher.width / 2
                        y: marcher.height / 2
                        width: Math.hypot((marcher.fieldX - marcher.fromX) * field.sx,
                                          (marcher.fieldY - marcher.fromY) * field.sy)
                        height: marcher.isSelected ? 2.5 : 1
                        color: marcher.isSelected ? "#fbbf24" : "#d2e5da"
                        opacity: marcher.isSelected ? 0.9 : 0.28
                        transformOrigin: Item.Left
                        rotation: Math.atan2((marcher.fieldY - marcher.fromY) * field.sy,
                                             (marcher.fromX - marcher.fieldX) * field.sx) * 180 / Math.PI
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: drillProject.markerGeometry === "circle" ? width / 2 : 1
                        rotation: drillProject.markerGeometry === "diamond" ? 45 : 0
                        scale: drillProject.markerGeometry === "diamond" ? 0.76 : 1
                        color: drillProject.performerMarkerStyle === "black" || drillProject.markerFillColor === "black" ? "#080b0a"
                             : drillProject.markerFillColor === "section" ? marcher.performerColor : drillProject.markerFillColor
                        border.width: marcher.isSelected ? Math.max(3, drillProject.markerOutlineWidth) : (marcher.hasWarning ? Math.max(2, drillProject.markerOutlineWidth) : drillProject.markerOutlineWidth)
                        border.color: marcher.isSelected ? "#fbbf24" : (marcher.hasWarning ? drillProject.markerWarningColor : drillProject.markerOutlineColor)
                        Rectangle {
                            visible: drillProject.markerFacingVisible
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: -5
                            width: 2; height: 8
                            color: drillProject.markerFacingColor
                            rotation: marcher.facing
                            transformOrigin: Item.Bottom
                        }
                        Text {
                            visible: drillProject.performerMarkerStyle === "colored"
                            anchors.centerIn: parent
                            text: marcher.symbol || "•"
                            color: "#07110d"
                            font.bold: true
                            font.pixelSize: Math.max(8, 10 * root.zoom)
                        }
                    }
                    Label {
                            visible: root.showLabels && drillProject.markerLabelMode !== "off" &&
                                     (drillProject.markerLabelMode === "always" || marcher.isSelected ||
                                      (drillProject.markerLabelMode === "adaptive" && (drillProject.performerCount <= 150 || dragger.containsMouse)))
                        anchors.top: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: marcher.label
                        color: marcher.isSelected ? "#fde68a" : drillProject.markerLabelColor
                        font.pixelSize: Math.max(8, 10 * root.zoom)
                        font.bold: marcher.isSelected
                        style: Text.Outline
                        styleColor: "#07110d"
                    }

                    MouseArea {
                        id: dragger
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        enabled: !root.spaceHeld && !marcher.performerLocked && !root.drawMode
                        preventStealing: true
                        property point pressField
                        property point startPosition
                        onPressed: {
                            if (mouse.button === Qt.RightButton) {
                                const p = mapToItem(root, mouse.x, mouse.y)
                                root.contextMenuRequested(p.x, p.y, marcher.index)
                                return
                            }
                            drillProject.playbackActive = false
                            const p = mapToItem(field, mouse.x, mouse.y)
                            pressField = Qt.point(root.toFieldX(p.x), root.toFieldY(p.y))
                            startPosition = Qt.point(marcher.fieldX, marcher.fieldY)
                            drillProject.beginMove(marcher.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                            root.performerActivated(marcher.index)
                        }
                        onPositionChanged: {
                            if (!pressed) return
                            const p = mapToItem(field, mouse.x, mouse.y)
                            let dx = root.toFieldX(p.x) - pressField.x
                            let dy = root.toFieldY(p.y) - pressField.y
                            if (root.snapEnabled) {
                                dx = Math.round((startPosition.x + dx) / root.gridSize) * root.gridSize - startPosition.x
                                dy = Math.round((startPosition.y + dy) / root.gridSize) * root.gridSize - startPosition.y
                            }
                            drillProject.previewMove(dx, dy,
                                                     (mouse.modifiers & Qt.ShiftModifier) !== 0,
                                                     (mouse.modifiers & Qt.ControlModifier) !== 0)
                        }
                        onReleased: drillProject.endMove()
                        onCanceled: drillProject.endMove()
                    }
                }
            }
        }
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        text: "BACK SIDELINE · FAR SIDE"
        color: "#dcebe2"
        font.bold: true
        font.pixelSize: 10
        opacity: 0.82
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        text: "FRONT SIDELINE · AUDIENCE VIEW"
        color: "#b8cdbf"
        font.bold: true
        font.pixelSize: 10
        opacity: 0.72
    }

    Label {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 18
        text: root.hoverCoordinate
        color: "#b7c9bf"
        font.pixelSize: 11
        padding: 5
        background: Rectangle { color: "#bb07110d"; radius: 4 }
    }

    MouseArea {
        // MouseArea receives wheel events reliably even when Flickable is
        // present. No mouse buttons are accepted, so performer clicks and
        // drags continue to reach the controls underneath it.
        anchors.fill: viewport
        acceptedButtons: root.spaceHeld ? (Qt.LeftButton | Qt.MiddleButton) : Qt.MiddleButton
        z: 10
        property point panStart
        property real startContentX: 0
        property real startContentY: 0
        onPressed: function(mouse) {
            root.forceActiveFocus()
            panStart = Qt.point(mouse.x, mouse.y)
            startContentX = viewport.contentX
            startContentY = viewport.contentY
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            const maxX = Math.max(0, viewport.contentWidth - viewport.width)
            const maxY = Math.max(0, viewport.contentHeight - viewport.height)
            viewport.contentX = Math.max(0, Math.min(maxX, startContentX - (mouse.x - panStart.x)))
            viewport.contentY = Math.max(0, Math.min(maxY, startContentY - (mouse.y - panStart.y)))
        }
        onWheel: function(event) {
            root.zoomAt(event.x, event.y, event.angleDelta.y)
            event.accepted = true
        }
    }
    Connections { target: drillProject; function refreshSelection(){root.selectionBounds=drillProject.selectedBounds();root.selectedShapeIndex=drillProject.selectedShapeIndex()} function onSelectionChanged(){refreshSelection()} function onDataChanged(){refreshSelection()} function onShapesChanged(){refreshSelection()} }
}
