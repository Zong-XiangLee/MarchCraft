import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var project
    required property var transport
    signal editPage(int index)
    signal pageMenu(int index, real viewX, real viewY)
    signal reordered()
    property real pixelsPerSecond: 36
    property bool fitEnabled: false
    property bool followPlayhead: true
    property int revision: 0
    property int selectionAnchor: 0
    property int dragFrom: -1
    property int dropSlot: -1
    property real dragViewportX: 0
    property string draggedLabel: ""
    readonly property real labelWidth: 72
    readonly property real rulerHeight: 30
    readonly property real laneHeight: Math.max(38, (height - rulerHeight - 14) / 3)
    readonly property real scale: fitEnabled ? Math.max(0.001, (viewport.width - 30) / Math.max(1, transport.durationMs / 1000)) : pixelsPerSecond
    readonly property real scrollX: viewport.contentX
    readonly property real viewportWidth: viewport.width
    readonly property real playheadX: timeX(transport.currentMs) - viewport.contentX
    function timeX(ms) { return 12 + ms * scale / 1000 }
    function xTime(x) { return Math.max(0, Math.min(transport.durationMs, (x - 12) * 1000 / scale)) }
    function formatTime(ms) { return Math.floor(ms / 60000) + ":" + String(Math.floor(ms / 1000) % 60).padStart(2, "0") }
    function scrollTo(x, manual) {
        viewport.contentX = Math.max(0, Math.min(Math.max(0, viewport.contentWidth - viewport.width), x))
        if (manual) followPlayhead = false
    }
    function zoomBy(factor, anchorX) {
        const ms = xTime(viewport.contentX + anchorX)
        const next = Math.max(0.5, Math.min(800, scale * factor))
        fitEnabled = false; pixelsPerSecond = next
        scrollTo(timeX(ms) - anchorX, false)
    }
    function fitShow() { fitEnabled = true; scrollTo(0, false) }
    function revealMs(ms) { scrollTo(timeX(ms) - viewport.width / 3, false) }
    function targetIndex() { return Math.max(0, Math.min(project.setCount - 1, dropSlot > dragFrom ? dropSlot - 1 : dropSlot)) }
    function updateDrop(viewX) {
        dragViewportX = viewX
        const time = xTime(viewport.contentX + viewX)
        let slot = 0
        while (slot < project.setCount && transport.setPositionMs(slot) < time) slot++
        dropSlot = slot
    }
    function finishDrag() {
        const from = dragFrom, target = targetIndex()
        cancelDrag()
        if (from >= 0 && target !== from) { project.moveSet(from, target); reordered() }
    }
    function cancelDrag() { dragFrom = -1; dropSlot = -1; draggedLabel = "" }
    Connections {
        target: root.project
        function onSetsChanged() { root.revision++; drawing.requestPaint() }
        function onTimingChanged() { root.revision++; drawing.requestPaint() }
        function onMusicChanged() { root.revision++; drawing.requestPaint() }
        function onSetRangeChanged() { root.revision++ }
        function onWaveformChanged() { drawing.requestPaint() }
        function onMovementsChanged() {
            root.selectionAnchor = 0
            root.cancelDrag()
            root.scrollTo(0, false)
            drawing.requestPaint()
        }
    }
    Connections {
        target: root.transport
        function onPositionChanged() {
            if (root.followPlayhead && root.transport.playing && (root.playheadX < 0 || root.playheadX > viewport.width - 20))
                root.revealMs(root.transport.currentMs)
        }
    }
    onScaleChanged: drawing.requestPaint()
    Rectangle { anchors.fill: parent; color: "#10171e"; border.color: "#303b48" }
    Column {
        y: root.rulerHeight; width: root.labelWidth
        Repeater {
            model: ["DRILL", "MUSIC", "AUDIO"]
            Rectangle {
                required property string modelData
                width: root.labelWidth; height: root.laneHeight; color: "#19232e"
                Label { anchors.centerIn: parent; text: modelData; color: "#9baaba"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.8 }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#303b48" }
            }
        }
    }
    Flickable {
        id: viewport; objectName: "timelineViewport"
        anchors.left: parent.left; anchors.leftMargin: root.labelWidth
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        clip: true; interactive: false
        contentWidth: Math.max(width, root.timeX(root.transport.durationMs) + 18)
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        onContentXChanged: drawing.requestPaint()
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOn
            onPressedChanged: if (pressed) root.followPlayhead = false
        }
        WheelHandler {
            onWheel: function(event) {
                if (event.modifiers & Qt.ControlModifier) root.zoomBy(event.angleDelta.y > 0 ? 1.2 : 1 / 1.2, event.x)
                else root.scrollTo(viewport.contentX - (event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y), true)
                event.accepted = true
            }
        }
        Canvas {
            id: drawing
            x: viewport.contentX; width: viewport.width; height: viewport.height - 14
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d"); ctx.reset()
                ctx.fillStyle = "#151f29"; ctx.fillRect(0, 0, width, root.rulerHeight)
                ctx.strokeStyle = "#2c3946"; ctx.lineWidth = 1
                for (let row = 0; row < 4; row++) {
                    const y = root.rulerHeight + row * root.laneHeight
                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                }
                const steps = [0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800]
                let step = steps[steps.length - 1]
                for (let i = 0; i < steps.length; i++) if (steps[i] * root.scale >= 65) { step = steps[i]; break }
                const first = Math.max(0, Math.floor(root.xTime(viewport.contentX) / (step * 1000)))
                ctx.font = "10px sans-serif"; ctx.fillStyle = "#a8b8c9"
                for (let sec = first * step; root.timeX(sec * 1000) < viewport.contentX + width; sec += step) {
                    const x = root.timeX(sec * 1000) - viewport.contentX
                    ctx.fillText(step < 1 ? sec.toFixed(1) + "s" : root.formatTime(sec * 1000), x + 4, 16)
                    ctx.beginPath(); ctx.moveTo(x, 22); ctx.lineTo(x, height); ctx.stroke()
                }
                const waveY = root.rulerHeight + root.laneHeight * 2.5
                ctx.strokeStyle = "#5ec6ad"
                if (root.project.waveformPeakCount > 0) {
                    for (let x = 0; x < width; x += 2) {
                        const ms = root.xTime(viewport.contentX + x)
                        if (ms < root.project.openingDurationMs) continue
                        const peak = root.project.waveformPeakAtMs(root.transport.audioMsAtShowMs(ms))
                        const h = peak * Math.max(4, root.laneHeight / 2 - 12)
                        ctx.beginPath(); ctx.moveTo(x, waveY - h); ctx.lineTo(x, waveY + h); ctx.stroke()
                    }
                }
            }
        }
        Rectangle {
            visible: root.project.loopEnabled && root.transport.loopEndTick > root.transport.loopStartTick
            x: root.timeX(root.transport.showMsAtTick(root.transport.loopStartTick)); y: 25; height: 4
            width: Math.max(0, root.timeX(root.transport.showMsAtTick(root.transport.loopEndTick)) - x)
            color: "#b79aff"
        }
        MouseArea {
            id: ruler; objectName: "timelineRuler"
            width: viewport.contentWidth; height: root.rulerHeight
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) { root.transport.beginScrub(); root.transport.seekMs(root.xTime(mouse.x)) }
            onPositionChanged: function(mouse) { if (pressed) root.transport.seekMs(root.xTime(mouse.x)) }
            onReleased: root.transport.endScrub()
            onCanceled: root.transport.endScrub()
        }
        Rectangle {
            x: root.timeX(0); y: root.rulerHeight + 5
            width: Math.max(0, root.timeX(root.project.openingDurationMs) - x); height: root.laneHeight - 10
            visible: root.project.openingDurationMs > 0; color: "#443d29"; border.color: "#b59b56"; radius: 3
            Label { anchors.fill: parent; anchors.margins: 5; text: "Opening hold"; color: "#e6d398"; elide: Text.ElideRight; font.pixelSize: 10 }
            MouseArea { anchors.fill: parent; onClicked: root.transport.navigateToSet(0, false) }
        }
        Repeater {
            model: root.project.setCount
            Item {
                id: page; objectName: "timelinePage" + index
                z: index === 0 ? 2 : 1
                required property int index
                property var info: { root.revision; return root.project.setInfo(index) }
                property real endMs: { root.revision; return root.transport.setPositionMs(index) }
                property real startMs: { root.revision; return index > 1 ? root.transport.setPositionMs(index - 1) : root.transport.showMsAtTick(root.project.setInfo(0).startTick || 0) }
                property bool selected: index >= Math.min(root.project.selectedSetStartIndex, root.project.selectedSetEndIndex)
                    && index <= Math.max(root.project.selectedSetStartIndex, root.project.selectedSetEndIndex)
                x: root.timeX(index === 0 ? 0 : startMs); y: root.rulerHeight
                width: index === 0 ? 20 : Math.max(1, (endMs - startMs) * root.scale / 1000)
                height: root.laneHeight
                visible: x + width >= viewport.contentX && x <= viewport.contentX + viewport.width
                opacity: root.dragFrom === index ? 0.35 : 1
                Rectangle {
                    anchors.fill: parent; anchors.topMargin: 17; anchors.bottomMargin: 3; anchors.rightMargin: 2
                    visible: page.index > 0; radius: 3
                    color: page.selected ? "#284e6d" : "#203447"
                    border.color: page.selected ? "#7fc7ff" : "#44617c"
                    Column {
                        anchors.fill: parent; anchors.margins: 6; spacing: 2; clip: true
                        Label { width: Math.max(0, parent.width - 14); text: "→ " + page.info.number + " · " + page.info.name; elide: Text.ElideRight; color: "#e0edfa"; font.pixelSize: 11; font.bold: true }
                        Label { visible: root.laneHeight > 60; width: parent.width; text: page.info.counts + " ct" + (page.info.variantCount > 1 ? " · Variant " + page.info.variantLabel : ""); elide: Text.ElideRight; color: "#99b3cd"; font.pixelSize: 10 }
                    }
                }
                Rectangle { x: page.index === 0 ? 0 : parent.width - 3; y: 3; width: 3; height: parent.height - 6; color: page.selected ? "#a7d9ff" : "#6c94b8" }

                MouseArea {
                    id: pageHit; objectName: "pageHit" + page.index
                    anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; hoverEnabled: true
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) { const p = mapToItem(root, mouse.x, mouse.y); root.pageMenu(page.index, p.x, p.y) }
                        else root.transport.navigateToSet(page.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    }
                    onDoubleClicked: function(mouse) { if (mouse.button === Qt.LeftButton) root.editPage(page.index) }
                    ToolTip.visible: containsMouse && !pressed
                    ToolTip.text: "Page " + page.info.number + " · " + page.info.name + "\n" + page.info.counts + " counts · " + root.formatTime(page.endMs) + "\nClick to jump · Double-click to edit"
                }
                Rectangle {
                    id: handle; objectName: "pageHandle" + page.index
                    visible: !root.transport.playing && page.width >= 18
                    width: 14; height: 18; x: Math.max(0, page.width - width - 4); y: 19; radius: 2; color: "#48637b"
                    Label { anchors.centerIn: parent; text: "⠿"; color: "#d3e1ec"; font.pixelSize: 12 }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.SizeHorCursor
                        property real pressX: 0
                        property bool dragging: false
                        onPressed: function(mouse) { pressX = mapToItem(viewport, mouse.x, mouse.y).x; dragging = false }
                        onPositionChanged: function(mouse) {
                            if (!pressed) return
                            const x = mapToItem(viewport, mouse.x, mouse.y).x
                            if (!dragging && Math.abs(x - pressX) > 6) { dragging = true; root.dragFrom = page.index; root.draggedLabel = "Page " + page.info.number }
                            if (dragging) root.updateDrop(x)
                        }
                        onReleased: { if (dragging) root.finishDrag(); else root.transport.navigateToSet(page.index, false); dragging = false }
                        onCanceled: { root.cancelDrag(); dragging = false }
                    }
                }
            }
        }
        Repeater {
            model: root.project.setCount
            Rectangle {
                id: marker
                required property int index
                property var info: { root.revision; return root.project.setInfo(index) }
                property real positionMs: { root.revision; return root.transport.setPositionMs(index) }
                property real intervalMs: { root.revision; return index === 0 ? 100000 : positionMs - root.transport.setPositionMs(index - 1) }
                x: root.timeX(positionMs) - (index === 0 ? 0 : width / 2)
                y: root.rulerHeight; width: intervalMs * root.scale / 1000 >= 36 ? 28 : 4
                height: 14; z: 5; radius: 2
                color: "#35516b"
                visible: x + width >= viewport.contentX && x <= viewport.contentX + viewport.width
                Label { anchors.centerIn: parent; visible: parent.width > 4; text: marker.info.number; font.pixelSize: 9; color: "#dceaff" }
                MouseArea {
                    objectName: "pageMarker" + marker.index
                    anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) { const p = mapToItem(root, mouse.x, mouse.y); root.pageMenu(marker.index, p.x, p.y) }
                        else root.transport.navigateToSet(marker.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    }
                    onDoubleClicked: function(mouse) { if (mouse.button === Qt.LeftButton) root.editPage(marker.index) }
                }
            }
        }
        Repeater {
            model: root.project.musicMeasureCount
            Rectangle {
                id: measure
                required property int index
                property var info: { root.revision; return root.project.musicMeasureInfo(index) }
                x: root.timeX(root.transport.showMsAtTick(info.startTick || 0)); y: root.rulerHeight + root.laneHeight + 5
                width: Math.max(1, root.timeX(root.transport.showMsAtTick(info.endTick || 0)) - x - 1)
                height: root.laneHeight - 10; radius: 2
                visible: x + width >= viewport.contentX && x <= viewport.contentX + viewport.width
                color: info.selected ? "#325847" : "#203e36"; border.color: info.selected ? "#85d9b3" : "#3b6857"
                Row {
                    width: parent.width; height: 3
                    Repeater {
                        model: measure.info.sections && measure.info.sections.length ? measure.info.sections : [{color: "#508574"}]
                        Rectangle { required property var modelData; width: measure.width / Math.max(1, (measure.info.sections || []).length); height: 3; color: modelData.color }
                    }
                }
                Label { anchors.fill: parent; anchors.margins: 5; color: "#c4e2d4"; font.pixelSize: 10; elide: Text.ElideRight
                    text: "m" + (measure.info.number || "") + " · " + (measure.info.numerator || 4) + "/" + (measure.info.denominator || 4)
                        + (measure.info.sections && measure.info.sections.length ? " · " + measure.info.sections[0].name : "") }
                Repeater {
                    model: measure.width > 80 ? Math.max(0, Math.ceil((measure.info.endTick - measure.info.startTick) / (measure.info.beatTicks || 960)) - 1) : 0
                    Item {
                        required property int index
                        x: root.timeX(root.transport.showMsAtTick(measure.info.startTick + (index + 1) * (measure.info.beatTicks || 960))) - measure.x
                        y: measure.height - 12
                        Rectangle { width: 1; height: 10; color: "#729789" }
                        Label { visible: measure.width > 150; x: 3; text: index + 2; font.pixelSize: 8; color: "#729789" }
                    }
                }
                MouseArea {
                    objectName: "measureHit" + measure.index
                    anchors.fill: parent; hoverEnabled: true
                    property real pressX: 0
                    property bool rangeDrag: false
                    onPressed: function(mouse) {
                        pressX = mouse.x; rangeDrag = false
                        if (!(mouse.modifiers & Qt.ShiftModifier)) root.selectionAnchor = measure.index
                        root.project.setMusicSelection(root.selectionAnchor, measure.index)
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed) return
                        if (Math.abs(mouse.x - pressX) > 4) rangeDrag = true
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        const tick = root.transport.tickAtShowMs(root.xTime(p.x))
                        let target = 0
                        while (target + 1 < root.project.musicMeasureCount && root.project.musicMeasureInfo(target).endTick <= tick) target++
                        root.project.setMusicSelection(root.selectionAnchor, target)
                    }
                    onClicked: function(mouse) {
                        if (!rangeDrag && !(mouse.modifiers & Qt.ShiftModifier)) root.transport.seekTick(measure.info.startTick)
                    }
                    ToolTip.visible: containsMouse && !pressed
                    ToolTip.text: "Measure " + measure.info.number + " · " + measure.info.counts + " counts\nClick to seek · Drag or Shift-click to select measures"
                }
            }
        }
        Label { x: viewport.contentX + 24; y: root.rulerHeight + root.laneHeight + 12; visible: root.project.musicMeasureCount === 0; text: "Import MIDI or MusicXML from Music tools"; color: "#697e90"; font.pixelSize: 11 }
        Label {
            x: viewport.contentX + 24; y: root.rulerHeight + root.laneHeight * 2 + 5
            text: root.project.audioSource ? root.project.audioSource.split(/[\\/]/).pop() : "Attach rehearsal audio from Music tools"
            color: "#7aa698"; font.pixelSize: 10
        }
        Rectangle {
            id: playhead; objectName: "timelinePlayhead"
            x: root.timeX(root.transport.currentMs); width: 2; height: viewport.height - 14; color: "#ffb961"; z: 10
            Rectangle { x: -4; width: 10; height: 8; radius: 2; color: "#ffb961" }
            MouseArea {
                x: -6; width: 14; height: parent.height; cursorShape: Qt.SizeHorCursor
                onPressed: root.transport.beginScrub()
                onPositionChanged: function(mouse) { if (pressed) { const p = mapToItem(viewport.contentItem, mouse.x, mouse.y); root.transport.seekMs(root.xTime(p.x)) } }
                onReleased: root.transport.endScrub()
                onCanceled: root.transport.endScrub()
            }
        }
        Rectangle {
            visible: root.dragFrom >= 0 && root.dropSlot >= 0; z: 20; width: 3
            x: root.timeX(root.transport.setPositionMs(Math.min(root.dropSlot, root.project.setCount - 1)))
            y: root.rulerHeight; height: root.laneHeight; color: "#ffffff"
        }
        Label {
            visible: root.dragFrom >= 0; z: 21
            x: viewport.contentX + Math.max(0, Math.min(viewport.width - width, root.dragViewportX)); y: root.rulerHeight + root.laneHeight - 20
            text: root.draggedLabel + " → position " + (root.targetIndex() + 1); color: "#ffffff"; padding: 4; font.pixelSize: 10
            background: Rectangle { color: "#314e69"; radius: 3 }
        }
    }
    Timer {
        interval: 40; repeat: true; running: root.dragFrom >= 0
        onTriggered: {
            if (root.dragViewportX < 30) root.scrollTo(viewport.contentX - 16, true)
            else if (root.dragViewportX > viewport.width - 30) root.scrollTo(viewport.contentX + 16, true)
            root.updateDrop(root.dragViewportX)
        }
    }
}
