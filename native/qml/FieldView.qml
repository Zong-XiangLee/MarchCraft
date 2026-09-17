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
    property string shapeDrawMode: ""
    property var selectionBounds: drillProject.selectedBounds()
    property int selectedShapeIndex: drillProject.selectedShapeIndex()
    signal performerActivated(int row)
    signal contextMenuRequested(real screenX, real screenY, int performerRow)
    signal freehandCompleted(var points)
    signal shapeCompleted(string kind, point start, point end)
    signal shapeDrawingCanceled()

    function toCanvasX(fieldX) { return (fieldX - drillProject.canvasMinX) * field.sx }
    function toCanvasY(fieldY) { return (drillProject.canvasMaxY - fieldY) * field.sy }
    function toFieldX(canvasX) { return canvasX / field.sx + drillProject.canvasMinX }
    function toFieldY(canvasY) { return drillProject.canvasMaxY - canvasY / field.sy }
    function snapLineEndpoint(start, end) {
        if (!root.snapEnabled) return end
        const dx = end.x - start.x, dy = end.y - start.y
        const length = Math.hypot(dx, dy)
        if (length < 1) return end
        const angle = Math.atan2(dy, dx)
        const rightAngle = Math.round(angle / (Math.PI / 2)) * (Math.PI / 2)
        const delta = Math.atan2(Math.sin(angle - rightAngle), Math.cos(angle - rightAngle))
        return Math.abs(delta) <= Math.PI / 12
            ? Qt.point(start.x + Math.cos(rightAngle) * length, start.y + Math.sin(rightAngle) * length)
            : end
    }

    focus: true
    Keys.onPressed: function(event) {
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && root.shapeDrawMode.length > 0) {
            root.shapeDrawingCanceled()
            event.accepted = true
            return
        }
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
                    const endZoneSteps = 16
                    const turf = ctx.createLinearGradient(0, 0, 0, h)
                    turf.addColorStop(0, "#123b2a")
                    turf.addColorStop(0.5, "#1b5037")
                    turf.addColorStop(1, "#123b2a")
                    ctx.fillStyle = turf
                    ctx.fillRect(0, 0, w, h)

                    // Regulation ten-yard end zones sit outside the two goal lines.
                    ctx.fillStyle = drillProject.fieldPreset === "indoor" ? "#8a5d38" : "#103522"
                    ctx.fillRect(-endZoneSteps * field.sx, 0, endZoneSteps * field.sx, h)
                    ctx.fillRect(w, 0, endZoneSteps * field.sx, h)
                    ctx.globalAlpha = 0.34
                    ctx.fillStyle = "#79a98a"
                    ctx.fillRect(-endZoneSteps * field.sx, 0, endZoneSteps * field.sx, h)
                    ctx.fillRect(w, 0, endZoneSteps * field.sx, h)

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

                    // Four short one-yard inserts per five-yard span on each hash row.
                    const hashRows = [drillProject.frontHashSteps, drillProject.backHashSteps]
                    const markLengthSteps = 24.0 / 22.5
                    const insertLength = markLengthSteps * field.sy
                    const hashLength = markLengthSteps * field.sx
                    for (let column = 0; column < drillProject.fieldInsertCount; ++column) {
                        const x = drillProject.fieldInsertStep(column) * field.sx
                        ctx.lineWidth = Math.max(1, 0.18 * field.sy)
                        const frontHashY = (drillProject.fieldDepthSteps - hashRows[0]) * field.sy
                        const backHashY = (drillProject.fieldDepthSteps - hashRows[1]) * field.sy
                        // Independent short marks at the back and front sidelines.
                        ctx.beginPath(); ctx.moveTo(x, 1); ctx.lineTo(x, 1 + insertLength); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(x, h - 1); ctx.lineTo(x, h - 1 - insertLength); ctx.stroke()
                        // Independent short marks at the two hashes.
                        ctx.beginPath(); ctx.moveTo(x, frontHashY); ctx.lineTo(x, frontHashY + insertLength); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(x, backHashY); ctx.lineTo(x, backHashY - insertLength); ctx.stroke()
                    }

                    // One horizontal hash per five-yard line on each hash row.
                    ctx.lineWidth = Math.max(1, 0.18 * field.sx)
                    for (let step = 0; step <= drillProject.fieldWidthSteps; step += 8) {
                        const x = step * field.sx
                        for (let hashIndex = 0; hashIndex < hashRows.length; ++hashIndex) {
                            const hashY = (drillProject.fieldDepthSteps - hashRows[hashIndex]) * field.sy
                            ctx.beginPath(); ctx.moveTo(x - hashLength / 2, hashY)
                            ctx.lineTo(x + hashLength / 2, hashY); ctx.stroke()
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
                    ctx.strokeRect(-endZoneSteps * field.sx + 1, 1,
                                   endZoneSteps * field.sx - 1, h - 2)
                    ctx.strokeRect(w, 1, endZoneSteps * field.sx - 1, h - 2)
                }
                Connections {
                    target: drillProject
                    function onProjectChanged() { fieldCanvas.requestPaint() }
                }
            }

            Canvas {
                id: gridCanvas
                anchors.fill: parent; z: 0.5; visible: drillProject.showFieldGrid || root.shapeDrawMode.length > 0
                antialiasing: false
                onPaint: {
                    const ctx = getContext("2d"); ctx.reset()
                    if (!drillProject.showFieldGrid && root.shapeDrawMode.length === 0) return
                    ctx.strokeStyle = drillProject.fieldGridColor
                    ctx.globalAlpha = drillProject.showFieldGrid ? drillProject.fieldGridOpacity : 0.24
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
                Connections { target: root; function onShapeDrawModeChanged() { gridCanvas.requestPaint() } }
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
                // Props are intentionally out of the editor surface until the
                // asset-import workflow is ready.
                visible: false
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
                        if (root.shapeDrawMode.length > 0 || root.drawMode) {
                            selecting = false
                            lassoPoints = []
                            shapePreviewCanvas.requestPaint()
                            lassoCanvas.requestPaint()
                            root.shapeDrawingCanceled()
                            return
                        }
                        const p = mapToItem(root, mouse.x, mouse.y)
                        root.contextMenuRequested(p.x, p.y, -1)
                        return
                    }
                    root.forceActiveFocus()
                    selecting = true
                    lasso = root.drawMode || (root.shapeDrawMode.length === 0 && (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    startPoint = Qt.point(mouse.x, mouse.y)
                    currentPoint = startPoint
                    lassoPoints = [Qt.point(root.toFieldX(mouse.x), root.toFieldY(mouse.y))]
                    lassoCanvas.requestPaint()
                    shapePreviewCanvas.requestPaint()
                }
                onPositionChanged: function(mouse) {
                    if (!selecting) return
                    currentPoint = Qt.point(mouse.x, mouse.y)
                    if (root.shapeDrawMode === "line")
                        currentPoint = root.snapLineEndpoint(startPoint, currentPoint)
                    if (lasso) {
                        const next = Qt.point(root.toFieldX(mouse.x), root.toFieldY(mouse.y))
                        const previous = lassoPoints[lassoPoints.length - 1]
                        const sampleSpacing = Math.max(0.06, 2.0 / Math.max(field.sx, field.sy))
                        if (!previous || Math.hypot(next.x - previous.x, next.y - previous.y) >= sampleSpacing) {
                            const copy = lassoPoints.slice()
                            copy.push(next)
                            lassoPoints = copy
                            lassoCanvas.requestPaint()
                        }
                    }
                    if (root.shapeDrawMode.length > 0) shapePreviewCanvas.requestPaint()
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
                    if (root.shapeDrawMode.length > 0) {
                        const start = Qt.point(root.toFieldX(startPoint.x), root.toFieldY(startPoint.y))
                        const rawEnd = Qt.point(root.toFieldX(currentPoint.x), root.toFieldY(currentPoint.y))
                        const end = root.shapeDrawMode === "line"
                            ? root.snapLineEndpoint(start, rawEnd) : rawEnd
                        const completed = Math.hypot(currentPoint.x - startPoint.x, currentPoint.y - startPoint.y) >= 5
                        if (completed)
                            root.shapeCompleted(root.shapeDrawMode, start, end)
                        selecting = false
                        lassoPoints = []
                        shapePreviewCanvas.requestPaint()
                        if (completed)
                            root.shapeDrawingCanceled()
                        return
                    }
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
                    shapePreviewCanvas.requestPaint()
                }
                onCanceled: {
                    selecting = false
                    lassoPoints = []
                    lassoCanvas.requestPaint()
                    shapePreviewCanvas.requestPaint()
                }
                onDoubleClicked: {
                    if (root.shapeDrawMode.length > 0 || root.drawMode) return
                    const fx = root.toFieldX(mouse.x)
                    const fy = root.toFieldY(mouse.y)
                    drillProject.addPerformer("P" + (drillProject.performerCount + 1), "Unassigned", "Unassigned", fx, fy)
                }
            }

            Canvas {
                id: shapePreviewCanvas
                anchors.fill: parent
                z: 6
                visible: selectionArea.selecting && root.shapeDrawMode.length > 0
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    if (!visible) return
                    const x1 = selectionArea.startPoint.x, y1 = selectionArea.startPoint.y
                    const x2 = selectionArea.currentPoint.x, y2 = selectionArea.currentPoint.y
                    const dx = x2 - x1, dy = y2 - y1
                    const radius = Math.hypot(dx, dy)
                    ctx.strokeStyle = "#fbbf24"
                    ctx.fillStyle = "#18fbbf24"
                    ctx.lineWidth = 2.5
                    ctx.setLineDash([7, 4])
                    ctx.beginPath()
                    if (root.shapeDrawMode === "line") {
                        ctx.moveTo(x1, y1); ctx.lineTo(x2, y2)
                    } else if (root.shapeDrawMode === "rectangle") {
                        ctx.rect(Math.min(x1, x2), Math.min(y1, y2), Math.abs(dx), Math.abs(dy))
                    } else if (root.shapeDrawMode === "triangle") {
                        ctx.moveTo((x1 + x2) / 2, Math.min(y1, y2))
                        ctx.lineTo(Math.min(x1, x2), Math.max(y1, y2))
                        ctx.lineTo(Math.max(x1, x2), Math.max(y1, y2))
                        ctx.closePath()
                    } else if (root.shapeDrawMode === "circle") {
                        ctx.arc(x1, y1, radius, 0, Math.PI * 2)
                    } else if (root.shapeDrawMode === "arc") {
                        const heading = Math.atan2(dy, dx)
                        ctx.arc(x1, y1, radius, heading - Math.PI / 2, heading + Math.PI / 2)
                    } else if (root.shapeDrawMode === "ellipse") {
                        ctx.ellipse((x1 + x2) / 2, (y1 + y2) / 2, Math.abs(dx) / 2, Math.abs(dy) / 2, 0, 0, Math.PI * 2)
                    } else if (root.shapeDrawMode === "diamond") {
                        ctx.moveTo((x1 + x2) / 2, Math.min(y1, y2)); ctx.lineTo(Math.max(x1, x2), (y1 + y2) / 2)
                        ctx.lineTo((x1 + x2) / 2, Math.max(y1, y2)); ctx.lineTo(Math.min(x1, x2), (y1 + y2) / 2); ctx.closePath()
                    } else if (root.shapeDrawMode === "polygon" || root.shapeDrawMode === "star") {
                        const n = root.shapeDrawMode === "star" ? 10 : 6, r = Math.max(Math.abs(dx), Math.abs(dy)) / 2
                        for (let i = 0; i < n; ++i) { const a = -Math.PI / 2 + i * Math.PI * 2 / n, rr = root.shapeDrawMode === "star" && i % 2 ? r * .45 : r; if (!i) ctx.moveTo((x1+x2)/2 + Math.cos(a)*rr, (y1+y2)/2 + Math.sin(a)*rr); else ctx.lineTo((x1+x2)/2 + Math.cos(a)*rr, (y1+y2)/2 + Math.sin(a)*rr) } ctx.closePath()
                    } else if (root.shapeDrawMode === "block") {
                        ctx.rect(Math.min(x1,x2), Math.min(y1,y2), Math.abs(dx), Math.abs(dy))
                    }
                    ctx.stroke()
                    // Show live performer destinations while drawing, so the
                    // tool behaves like a formation preview rather than a
                    // bare hashed guide.
                    const count = drillProject.selectedCount
                    if (count > 0) {
                        const previewPoint = function(px, py) {
                            ctx.beginPath(); ctx.arc(px, py, 4, 0, Math.PI * 2)
                            ctx.fillStyle = "#fbbf24"; ctx.fill()
                            ctx.lineWidth = 1; ctx.strokeStyle = "#14231d"; ctx.stroke()
                        }
                        if (root.shapeDrawMode === "line") {
                            for (let i = 0; i < count; ++i) {
                                const t = count === 1 ? 0.5 : i / (count - 1)
                                previewPoint(x1 + dx * t, y1 + dy * t)
                            }
                        } else if (root.shapeDrawMode === "rectangle") {
                            const left = Math.min(x1, x2), right = Math.max(x1, x2)
                            const top = Math.min(y1, y2), bottom = Math.max(y1, y2)
                            const perimeter = 2 * ((right - left) + (bottom - top))
                            for (let i = 0; i < count; ++i) {
                                let distance = perimeter * i / count
                                if (distance <= right - left) previewPoint(left + distance, top)
                                else if ((distance -= right - left) <= bottom - top) previewPoint(right, top + distance)
                                else if ((distance -= bottom - top) <= right - left) previewPoint(right - distance, bottom)
                                else { distance -= right - left; previewPoint(left, bottom - distance) }
                            }
                        } else if (root.shapeDrawMode === "circle" || root.shapeDrawMode === "arc") {
                            const heading = Math.atan2(dy, dx)
                            const startAngle = root.shapeDrawMode === "circle" ? 0 : heading - Math.PI / 2
                            const sweep = root.shapeDrawMode === "circle" ? Math.PI * 2 : Math.PI
                            for (let i = 0; i < count; ++i) {
                                const t = root.shapeDrawMode === "circle" ? i / count : (count === 1 ? 0.5 : i / (count - 1))
                                const angle = startAngle + sweep * t
                                previewPoint(x1 + Math.cos(angle) * radius, y1 + Math.sin(angle) * radius)
                            }
                        }
                    }
                    ctx.setLineDash([])
                    ctx.lineWidth = 1.5
                    ctx.beginPath(); ctx.moveTo(x1 - 8, y1); ctx.lineTo(x1 + 8, y1)
                    ctx.moveTo(x1, y1 - 8); ctx.lineTo(x1, y1 + 8); ctx.stroke()
                    ctx.beginPath(); ctx.arc(x1, y1, 3.5, 0, Math.PI * 2); ctx.fill()
                }
            }

            Rectangle {
                z: 6
                visible: selectionArea.selecting && !selectionArea.lasso && !root.drawMode && root.shapeDrawMode.length === 0
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
                visible: root.shapeDrawMode.length === 0 && !root.drawMode && root.selectedShapeIndex >= 0 && Object.keys(root.selectionBounds).length > 0
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
                        property real startingRotation: 0
                        onPressed: function(mouse) { const p=mapToItem(field,mouse.x,mouse.y);startAngle=Math.atan2(root.toFieldY(p.y)-root.selectionBounds.centerY,root.toFieldX(p.x)-root.selectionBounds.centerX)*180/Math.PI;const info=drillProject.shapeInfo(root.selectedShapeIndex);startingRotation=Number(info.rotation||0);rotationReadout.angle=startingRotation;rotationReadout.visible=true;drillProject.beginRotate() }
                        onPositionChanged: function(mouse) { if(!pressed)return;const p=mapToItem(field,mouse.x,mouse.y);let delta=Math.atan2(root.toFieldY(p.y)-root.selectionBounds.centerY,root.toFieldX(p.x)-root.selectionBounds.centerX)*180/Math.PI-startAngle;let finalAngle=startingRotation+delta;if(mouse.modifiers&Qt.ShiftModifier)finalAngle=Math.round(finalAngle/15)*15;else if(root.snapEnabled){const ninety=Math.round(finalAngle/90)*90;if(Math.abs(finalAngle-ninety)<=15)finalAngle=ninety}rotationReadout.angle=finalAngle;drillProject.previewRotate(finalAngle-startingRotation) }
                        onReleased: { rotationReadout.visible=false; drillProject.endRotate() }
                        onCanceled: { rotationReadout.visible=false; drillProject.endRotate() }
                    }
                }
                Rectangle {
                    id: rotationReadout
                    property real angle: 0
                    visible: false; width: 58; height: 24; radius: 5; color: "#d914231d"; border.color: "#fbbf24"
                    anchors.horizontalCenter: parent.horizontalCenter; y: -73
                    Text { anchors.centerIn: parent; text: Math.round(parent.angle) + " deg"; color: "#fde68a"; font.pixelSize: 11; font.bold: true }
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
                    readonly property real visualSize: Math.max(drillProject.markerGeometry === "dot" ? 4 : 7,
                        drillProject.performerMarkerSize * root.zoom
                        * (drillProject.markerGeometry === "dot" ? 0.42
                           : drillProject.performerMarkerStyle === "compact" ? 0.78 : 1.0))
                    z: 2
                    visible: performerVisible
                    opacity: performerLocked ? 0.55 : 1.0
                    x: root.toCanvasX(fieldX) - width / 2
                    y: root.toCanvasY(fieldY) - height / 2
                    width: Math.max(14, visualSize)
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
                        id: markerBody
                        anchors.centerIn: parent
                        width: marcher.visualSize
                        height: width
                        radius: drillProject.markerGeometry === "circle" || drillProject.markerGeometry === "dot" ? width / 2 : 1
                        rotation: drillProject.markerGeometry === "diamond" ? 45 : 0
                        scale: drillProject.markerGeometry === "diamond" ? 0.76 : 1
                        color: drillProject.performerMarkerStyle === "black" || drillProject.markerFillColor === "black" ? "#080b0a"
                             : drillProject.markerFillColor === "section" ? marcher.performerColor : drillProject.markerFillColor
                        border.width: drillProject.markerGeometry === "dot"
                            ? (marcher.isSelected ? 1.5 : marcher.hasWarning ? 1 : Math.min(0.75, drillProject.markerOutlineWidth))
                            : (marcher.isSelected ? Math.max(3, drillProject.markerOutlineWidth) : (marcher.hasWarning ? Math.max(2, drillProject.markerOutlineWidth) : drillProject.markerOutlineWidth))
                        border.color: marcher.isSelected ? "#fbbf24" : (marcher.hasWarning ? drillProject.markerWarningColor : drillProject.markerOutlineColor)
                        Text {
                            visible: drillProject.performerMarkerStyle === "colored" && drillProject.markerGeometry !== "dot"
                            anchors.centerIn: parent
                            text: marcher.symbol || "•"
                            color: "#07110d"
                            font.bold: true
                            font.pixelSize: Math.max(8, 10 * root.zoom)
                        }
                    }
                    Item {
                        id: facingIndicator
                        visible: drillProject.markerFacingVisible
                        anchors.centerIn: markerBody
                        width: marcher.visualSize
                        height: width
                        // 0 = front field (the bottom/audience edge), 180 = back field.
                        // Negating the stored clockwise field heading also maps 90 to Side 2.
                        rotation: -marcher.facing
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: parent.height - 0.5
                            width: drillProject.markerGeometry === "dot" ? 1.5 : 2
                            height: Math.max(3.5, parent.height * (drillProject.markerGeometry === "dot" ? 0.65 : 0.48))
                            radius: width / 2
                            color: drillProject.markerFacingColor
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
                        font.pixelSize: Math.max(7, drillProject.markerLabelFontSize * root.zoom)
                        font.bold: marcher.isSelected
                        style: Text.Outline
                        styleColor: "#07110d"
                    }

                    MouseArea {
                        id: dragger
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        enabled: !root.spaceHeld && !marcher.performerLocked && !root.drawMode && root.shapeDrawMode.length === 0
                        preventStealing: true
                        property point pressField
                        property point startPosition
                        onPressed: {
                            if (mouse.button === Qt.RightButton) {
                                const group = drillProject.performerGroupInfo(marcher.index)
                                if (Object.keys(group).length > 0)
                                    drillProject.selectGroupForPerformer(marcher.index)
                                else if (!marcher.isSelected)
                                    drillProject.selectPerformerMode(marcher.index, 0)
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
