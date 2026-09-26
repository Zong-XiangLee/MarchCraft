import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var project
    required property var transport

    signal editPage(int index)
    signal pageMenu(int index, real viewX, real viewY)
    signal reordered()
    signal transitionSelected(int index)

    property real pixelsPerSecond: 44
    property bool fitEnabled: false
    property bool followPlayhead: true
    property bool snapEnabled: true
    property var snapFeedback: ({})
    property int revision: 0
    property int selectionAnchor: 0

    // Page reordering remains available from the dedicated grip. Transition
    // boundaries use a different handle and never reorder formations.
    property int dragFrom: -1
    property int dropSlot: -1
    property real dragViewportX: 0
    property string draggedLabel: ""

    property int resizingTransition: -1
    property var resizePreview: ({})
    property real resizePointerX: 0
    property int draggingPlanCandidate: -1

    readonly property bool compactLanes: height < 180
    readonly property real labelWidth: compactLanes ? 62 : 78
    readonly property real rulerHeight: compactLanes ? 24 : 34
    readonly property real markerLaneHeight: compactLanes ? 16 : 28
    readonly property real drillLaneHeight: compactLanes
        ? Math.max(26, (height - rulerHeight - markerLaneHeight) * 0.44)
        : Math.max(72, (height - rulerHeight - markerLaneHeight - 58) * 0.48)
    readonly property real musicLaneHeight: compactLanes
        ? Math.max(18, (height - rulerHeight - markerLaneHeight - drillLaneHeight) * 0.54)
        : Math.max(48, (height - rulerHeight - markerLaneHeight - drillLaneHeight - 14) * 0.55)
    readonly property real audioLaneHeight: compactLanes
        ? Math.max(18, height - rulerHeight - markerLaneHeight - drillLaneHeight - musicLaneHeight)
        : Math.max(42, height - rulerHeight - markerLaneHeight - drillLaneHeight - musicLaneHeight - 14)
    readonly property real drillY: rulerHeight + markerLaneHeight
    readonly property real musicY: drillY + drillLaneHeight
    readonly property real audioY: musicY + musicLaneHeight
    readonly property real laneBottom: audioY + audioLaneHeight
    readonly property real scale: fitEnabled
        ? Math.max(0.001, (viewport.width - 30) / Math.max(1, transport.durationMs / 1000))
        : pixelsPerSecond
    readonly property real scrollX: viewport.contentX
    readonly property real viewportWidth: viewport.width
    readonly property real playheadX: timeX(transport.currentMs) - viewport.contentX

    function timeX(ms) {
        return 12 + ms * scale / 1000
    }

    function xTime(x) {
        return Math.max(0, Math.min(transport.durationMs, (x - 12) * 1000 / scale))
    }

    function formatTime(ms) {
        const safe = Math.max(0, Math.round(ms))
        return Math.floor(safe / 60000) + ":" + String(Math.floor(safe / 1000) % 60).padStart(2, "0")
            + "." + Math.floor((safe % 1000) / 100)
    }

    function selectionMode(modifiers) {
        if (modifiers & Qt.ControlModifier)
            return 2
        if (modifiers & Qt.ShiftModifier)
            return 1
        return 0
    }

    function navigateSet(index, modifiers) {
        const mode = selectionMode(modifiers)
        if (typeof transport.navigateToSetWithMode === "function")
            transport.navigateToSetWithMode(index, mode)
        else
            transport.navigateToSet(index, mode === 1)
    }

    function snappedPosition(contentX, temporarySnapDisabled) {
        const showMs = xTime(contentX)
        const tick = transport.tickAtShowMs(showMs)
        if (!snapEnabled || temporarySnapDisabled || showMs < project.openingDurationMs
                || typeof project.snapTimelinePosition !== "function")
            return { tick: tick, timeMs: showMs, snapType: "free", snapLabel: "Free" }
        return project.snapTimelinePosition(tick, true)
    }

    function showSnapFeedback(info) {
        snapFeedback = info || ({})
        snapFeedbackTimer.restart()
    }

    function seekAt(contentX, temporarySnapDisabled) {
        const info = snappedPosition(contentX, temporarySnapDisabled)
        showSnapFeedback(info)
        if (snapEnabled && !temporarySnapDisabled && info.snapType !== "free")
            transport.seekTick(info.tick)
        else
            transport.seekMs(info.timeMs)
    }

    function scrollTo(x, manual) {
        viewport.contentX = Math.max(0, Math.min(Math.max(0, viewport.contentWidth - viewport.width), x))
        if (manual)
            followPlayhead = false
    }

    function zoomBy(factor, anchorX) {
        const ms = xTime(viewport.contentX + anchorX)
        const next = Math.max(0.5, Math.min(900, scale * factor))
        fitEnabled = false
        pixelsPerSecond = next
        scrollTo(timeX(ms) - anchorX, false)
    }

    function fitShow() {
        fitEnabled = true
        scrollTo(0, false)
    }

    function fitSelection() {
        let startMs = 0
        let endMs = transport.durationMs
        if ((project.timelineSelectionKind === "time" || project.timelineSelectionKind === "measure")
                && project.timelineRangeEndTick > project.timelineRangeStartTick) {
            startMs = transport.showMsAtTick(project.timelineRangeStartTick)
            endMs = transport.showMsAtTick(project.timelineRangeEndTick)
        } else if (project.selectedSetIndices && project.selectedSetIndices.length > 0) {
            const values = project.selectedSetIndices.slice().sort(function(a, b) { return a - b })
            startMs = transport.setPositionMs(Math.max(0, values[0] - 1))
            endMs = transport.setPositionMs(values[values.length - 1])
        } else if (project.selectedTransitionIndex > 0) {
            startMs = transport.setPositionMs(project.selectedTransitionIndex - 1)
            endMs = transport.setPositionMs(project.selectedTransitionIndex)
        }
        if (endMs <= startMs)
            return fitShow()
        fitEnabled = false
        pixelsPerSecond = Math.max(0.5, Math.min(900, (viewport.width - 80) * 1000 / (endMs - startMs)))
        scrollTo(timeX(startMs) - 36, false)
    }

    function revealMs(ms) {
        scrollTo(timeX(ms) - viewport.width / 3, false)
    }

    function revealCurrentSet() {
        revealMs(transport.setPositionMs(project.currentSetIndex))
    }

    function targetIndex() {
        return Math.max(0, Math.min(project.setCount - 1, dropSlot > dragFrom ? dropSlot - 1 : dropSlot))
    }

    function updateDrop(viewX) {
        dragViewportX = viewX
        const time = xTime(viewport.contentX + viewX)
        let slot = 0
        while (slot < project.setCount && transport.setPositionMs(slot) < time)
            slot++
        dropSlot = slot
    }

    function finishDrag() {
        const from = dragFrom
        const target = targetIndex()
        cancelDrag()
        if (from >= 0 && target !== from) {
            project.moveSet(from, target)
            reordered()
        }
    }

    function cancelDrag() {
        dragFrom = -1
        dropSlot = -1
        draggedLabel = ""
    }

    function previewResize(index, contentX, temporarySnapDisabled) {
        resizePointerX = contentX
        resizePreview = project.previewTransitionResize(index, transport.tickAtShowMs(xTime(contentX)),
                                                        snapEnabled && !temporarySnapDisabled)
        if (snapEnabled && !temporarySnapDisabled)
            showSnapFeedback(resizePreview)
    }

    function commitResize(index) {
        const preview = resizePreview
        resizingTransition = -1
        resizePreview = ({})
        if (preview && preview.counts !== undefined)
            project.setTransitionCounts(index, preview.counts)
    }

    Connections {
        target: root.project
        function onSetsChanged() { root.revision++; drawing.requestPaint() }
        function onTimingChanged() { root.revision++; drawing.requestPaint() }
        function onMusicChanged() { root.revision++; drawing.requestPaint() }
        function onSetRangeChanged() { root.revision++ }
        function onTimelineSelectionChanged() { root.revision++ }
        function onTimelineMarkersChanged() { root.revision++ }
        function onSetPlanChanged() { root.revision++ }
        function onWaveformChanged() { drawing.requestPaint() }
        function onMovementsChanged() {
            root.selectionAnchor = 0
            root.cancelDrag()
            root.resizingTransition = -1
            root.scrollTo(0, false)
            drawing.requestPaint()
        }
    }

    Connections {
        target: root.transport
        function onPositionChanged() {
            if (root.followPlayhead && root.transport.playing
                    && (root.playheadX < 0 || root.playheadX > viewport.width - 20))
                root.revealMs(root.transport.currentMs)
        }
    }

    onScaleChanged: drawing.requestPaint()

    Timer {
        id: snapFeedbackTimer
        interval: 900
        onTriggered: root.snapFeedback = ({})
    }

    Rectangle {
        anchors.fill: parent
        color: MarchCraftTheme.input
        border.color: MarchCraftTheme.divider
    }

    Column {
        y: root.rulerHeight
        width: root.labelWidth

        Repeater {
            model: [
                { label: "MARKERS", height: root.markerLaneHeight },
                { label: "DRILL", height: root.drillLaneHeight },
                { label: "MUSIC", height: root.musicLaneHeight },
                { label: "AUDIO", height: root.audioLaneHeight }
            ]
            Rectangle {
                required property var modelData
                width: root.labelWidth
                height: modelData.height
                color: MarchCraftTheme.panelHeader
                Label {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: MarchCraftTheme.textSecondary
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: MarchCraftTheme.divider }
            }
        }
    }

    Flickable {
        id: viewport
        objectName: "timelineViewport"
        anchors.left: parent.left
        anchors.leftMargin: root.labelWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true
        interactive: false
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
                if (event.modifiers & Qt.ControlModifier)
                    root.zoomBy(event.angleDelta.y > 0 ? 1.2 : 1 / 1.2, event.x)
                else
                    root.scrollTo(viewport.contentX - (event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y), true)
                event.accepted = true
            }
        }

        Canvas {
            id: drawing
            x: viewport.contentX
            width: viewport.width
            height: root.laneBottom

            Connections {
                target: MarchCraftTheme
                function onThemeIdChanged() { Qt.callLater(function() { drawing.requestPaint() }) }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = MarchCraftTheme.panel
                ctx.fillRect(0, 0, width, root.rulerHeight)
                ctx.strokeStyle = MarchCraftTheme.divider
                ctx.lineWidth = 1
                const boundaries = [root.rulerHeight, root.drillY, root.musicY, root.audioY, root.laneBottom]
                for (let row = 0; row < boundaries.length; ++row) {
                    ctx.beginPath()
                    ctx.moveTo(0, boundaries[row])
                    ctx.lineTo(width, boundaries[row])
                    ctx.stroke()
                }

                const steps = [0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800]
                let step = steps[steps.length - 1]
                for (let i = 0; i < steps.length; ++i) {
                    if (steps[i] * root.scale >= 76) {
                        step = steps[i]
                        break
                    }
                }
                const first = Math.max(0, Math.floor(root.xTime(viewport.contentX) / (step * 1000)))
                ctx.font = "10px sans-serif"
                ctx.fillStyle = MarchCraftTheme.textSecondary
                for (let sec = first * step; root.timeX(sec * 1000) < viewport.contentX + width; sec += step) {
                    const x = root.timeX(sec * 1000) - viewport.contentX
                    const tick = root.transport.tickAtShowMs(sec * 1000)
                    let countLabel = ""
                    if (sec * 1000 >= root.project.openingDurationMs
                            && typeof root.project.absoluteCountAtTick === "function")
                        countLabel = " · c" + root.project.absoluteCountAtTick(tick)
                    ctx.fillText((step < 1 ? sec.toFixed(1) + "s" : root.formatTime(sec * 1000)) + countLabel, x + 4, 14)
                    ctx.beginPath()
                    ctx.moveTo(x, 22)
                    ctx.lineTo(x, height)
                    ctx.stroke()
                }

                const waveY = root.audioY + root.audioLaneHeight / 2
                ctx.strokeStyle = "#5ec6ad"
                if (root.project.waveformPeakCount > 0) {
                    for (let x = 0; x < width; x += 2) {
                        const ms = root.xTime(viewport.contentX + x)
                        if (ms < root.project.openingDurationMs)
                            continue
                        const peak = root.project.waveformPeakAtMs(root.transport.audioMsAtShowMs(ms))
                        const h = peak * Math.max(4, root.audioLaneHeight / 2 - 9)
                        ctx.beginPath()
                        ctx.moveTo(x, waveY - h)
                        ctx.lineTo(x, waveY + h)
                        ctx.stroke()
                    }
                }
            }
        }

        Rectangle {
            visible: root.project.loopEnabled && root.transport.loopEndTick > root.transport.loopStartTick
            x: root.timeX(root.transport.showMsAtTick(root.transport.loopStartTick))
            y: 25
            height: 4
            width: Math.max(0, root.timeX(root.transport.showMsAtTick(root.transport.loopEndTick)) - x)
            color: "#b79aff"
        }

        MouseArea {
            id: ruler
            objectName: "timelineRuler"
            width: viewport.contentWidth
            height: root.rulerHeight
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) {
                root.transport.beginScrub()
                root.seekAt(mouse.x, (mouse.modifiers & Qt.AltModifier) !== 0)
            }
            onPositionChanged: function(mouse) {
                if (pressed) root.seekAt(mouse.x, (mouse.modifiers & Qt.AltModifier) !== 0)
            }
            onReleased: root.transport.endScrub()
            onCanceled: root.transport.endScrub()
        }

        // The marker lane also acts as a range-selection surface. It is kept
        // separate from ruler scrubbing so both interactions are predictable.
        MouseArea {
            id: rangeSurface
            objectName: "timelineRangeSurface"
            x: 0
            y: root.rulerHeight
            width: viewport.contentWidth
            height: root.markerLaneHeight
            property int anchorTick: 0
            property real anchorX: 0
            property bool dragged: false
            onPressed: function(mouse) {
                anchorX = mouse.x
                dragged = false
                const info = root.snappedPosition(mouse.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                root.showSnapFeedback(info)
                anchorTick = info.tick
                root.project.selectTimelineRange(anchorTick, anchorTick)
            }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    if (Math.abs(mouse.x - anchorX) > 3)
                        dragged = true
                    const info = root.snappedPosition(mouse.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                    root.showSnapFeedback(info)
                    root.project.selectTimelineRange(anchorTick, info.tick)
                }
            }
            onClicked: if (!dragged) root.project.clearTimelineSelection()
        }

        Rectangle {
            x: root.timeX(0)
            y: root.drillY + 8
            width: Math.max(0, root.timeX(root.project.openingDurationMs) - x)
            height: root.drillLaneHeight - 16
            visible: root.project.openingDurationMs > 0
            color: "#443d29"
            border.color: "#b59b56"
            radius: 3
            Label {
                anchors.fill: parent
                anchors.margins: 5
                text: "Opening hold"
                color: "#e6d398"
                elide: Text.ElideRight
                font.pixelSize: 10
            }
            MouseArea { anchors.fill: parent; onClicked: root.navigateSet(0, mouse.modifiers) }
        }

        // Transition intervals are spatial regions. Their right edge is the
        // destination set marker and is the only timing-resize handle.
        Repeater {
            model: root.project.setCount
            Item {
                id: transition
                objectName: "timelinePage" + index
                required property int index
                property var info: { root.revision; return root.project.setInfo(index) }
                property real startMs: { root.revision; return index > 0 ? root.transport.setPositionMs(index - 1) : 0 }
                property real endMs: { root.revision; return root.transport.setPositionMs(index) }
                property bool selected: root.project.selectedTransitionIndex === index
                property real intervalWidth: Math.max(1, (endMs - startMs) * root.scale / 1000)
                x: root.timeX(startMs)
                y: root.drillY
                width: index === 0 ? 1 : intervalWidth
                height: root.drillLaneHeight
                visible: index > 0 && x + width >= viewport.contentX && x <= viewport.contentX + viewport.width
                opacity: root.dragFrom === index ? 0.35 : 1

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    anchors.rightMargin: 1
                    radius: 3
                    color: transition.selected ? MarchCraftTheme.selection : (index % 2 ? MarchCraftTheme.surfaceRaised : MarchCraftTheme.surface)
                    border.color: transition.selected ? MarchCraftTheme.accentHover : MarchCraftTheme.dividerStrong

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 7
                        anchors.right: parent.right
                        anchors.rightMargin: 7
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        clip: true
                        Label {
                            text: transition.info.counts + " ct"
                            visible: transition.width >= 30
                            color: MarchCraftTheme.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Label {
                            visible: transition.width >= 92
                            text: "→ " + transition.info.number + " " + transition.info.name
                            color: MarchCraftTheme.textSecondary
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            width: Math.max(0, parent.width - x)
                        }
                    }
                }

                MouseArea {
                    id: pageHit
                    objectName: "pageHit" + transition.index
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            const p = mapToItem(root, mouse.x, mouse.y)
                            root.pageMenu(transition.index, p.x, p.y)
                        } else if (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) {
                            root.navigateSet(transition.index, mouse.modifiers)
                        } else {
                            root.project.selectTimelineTransition(transition.index)
                            root.transitionSelected(transition.index)
                        }
                    }
                    onDoubleClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton)
                            root.editPage(transition.index)
                    }
                    ToolTip.visible: containsMouse && !pressed && root.resizingTransition < 0
                    ToolTip.delay: 300
                    ToolTip.text: {
                        const details = typeof root.project.transitionInfo === "function"
                            ? root.project.transitionInfo(transition.index) : transition.info
                        return "Transition to Set " + transition.info.number + " · " + transition.info.name
                            + "\n" + transition.info.counts + " counts · " + root.formatTime(transition.startMs)
                            + " → " + root.formatTime(transition.endMs)
                            + (details.measure ? "\nMeasure " + details.measure + ", beat " + Number(details.beat).toFixed(2) : "")
                            + (details.averageDistance !== undefined ? "\nTravel avg " + Number(details.averageDistance).toFixed(1)
                                + " yd · max " + Number(details.maximumDistance).toFixed(1) + " yd" : "")
                            + "\nShift-click selects a page range · Ctrl-click toggles a page"
                    }
                }
            }
        }

        // Destination set markers are narrow navigation targets, independent
        // of the width available for the incoming transition.
        Repeater {
            model: root.project.setCount
            Item {
                id: setMarker
                required property int index
                property var info: { root.revision; return root.project.setInfo(index) }
                property real positionMs: { root.revision; return root.transport.setPositionMs(index) }
                property real spacingBefore: index > 0
                    ? Math.max(1, (positionMs - root.transport.setPositionMs(index - 1)) * root.scale / 1000)
                    : 1000
                property int labelStride: Math.max(1, Math.ceil(28 / spacingBefore))
                x: root.timeX(positionMs) - 10
                y: root.drillY
                width: 20
                height: root.drillLaneHeight
                z: 45
                visible: x + width >= viewport.contentX && x <= viewport.contentX + viewport.width

                Rectangle {
                    x: 9
                    width: 2
                    height: parent.height
                    color: info.playbackDestination ? "#f1b35d"
                        : info.editing ? MarchCraftTheme.accentHover
                        : info.selected ? "#76b7ff" : MarchCraftTheme.textMuted
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2
                    width: Math.max(18, label.implicitWidth + 8)
                    height: 18
                    radius: 3
                    color: info.playbackDestination ? "#7a5229"
                        : info.editing ? MarchCraftTheme.accent
                        : info.selected ? MarchCraftTheme.selection : MarchCraftTheme.surfaceHover
                    border.color: info.selected ? "#76b7ff" : MarchCraftTheme.dividerStrong
                    visible: setMarker.index === 0 || markerHit.containsMouse
                        || setMarker.spacingBefore >= 24 || setMarker.index % setMarker.labelStride === 0
                    Label {
                        id: label
                        anchors.centerIn: parent
                        text: setMarker.info.number
                        font.pixelSize: 9
                        font.bold: true
                        color: MarchCraftTheme.textPrimary
                    }
                }
                MouseArea {
                    id: markerHit
                    objectName: "pageMarker" + setMarker.index
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            const p = mapToItem(root, mouse.x, mouse.y)
                            root.pageMenu(setMarker.index, p.x, p.y)
                        } else {
                            root.navigateSet(setMarker.index, mouse.modifiers)
                        }
                    }
                    onDoubleClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton)
                            root.editPage(setMarker.index)
                    }
                    ToolTip.visible: containsMouse && !pressed
                    ToolTip.delay: 350
                    ToolTip.text: "Set " + setMarker.info.number + " · " + setMarker.info.name
                        + "\nShift-click selects a page range · Ctrl-click toggles a page"
                }

                // Boundary timing handle. This intentionally overlays only the
                // lower part of the marker, leaving its label for navigation.
                Rectangle {
                    id: boundaryHandle
                    objectName: "transitionHandle" + setMarker.index
                    visible: setMarker.index > 0 && !root.transport.playing
                        && (markerHit.containsMouse || setMarker.spacingBefore >= 28)
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - 21
                    width: 12
                    height: 18
                    radius: 3
                    color: root.resizingTransition === setMarker.index ? MarchCraftTheme.accentHover : MarchCraftTheme.surfaceHover
                    border.color: MarchCraftTheme.dividerStrong
                    Label { anchors.centerIn: parent; text: "↔"; font.pixelSize: 9; color: MarchCraftTheme.textPrimary }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.SizeHorCursor
                        onPressed: function(mouse) {
                            root.resizingTransition = setMarker.index
                            const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                            root.previewResize(setMarker.index, p.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed)
                                return
                            const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                            root.previewResize(setMarker.index, p.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                        }
                        onReleased: root.commitResize(setMarker.index)
                        onCanceled: { root.resizingTransition = -1; root.resizePreview = ({}) }
                    }
                }

                // Reordering is intentionally a separate, compact grip.
                Rectangle {
                    id: reorderHandle
                    objectName: "pageHandle" + setMarker.index
                    visible: !root.transport.playing
                        && (markerHit.containsMouse || setMarker.spacingBefore >= 32)
                    x: 12
                    y: 25
                    width: 9
                    height: 18
                    radius: 2
                    color: MarchCraftTheme.surfaceHover
                    Label { anchors.centerIn: parent; text: "⋮"; color: MarchCraftTheme.textSecondary; font.pixelSize: 12 }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.OpenHandCursor
                        property real pressX: 0
                        property bool dragging: false
                        onPressed: function(mouse) {
                            pressX = mapToItem(viewport, mouse.x, mouse.y).x
                            dragging = false
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed)
                                return
                            const x = mapToItem(viewport, mouse.x, mouse.y).x
                            if (!dragging && Math.abs(x - pressX) > 6) {
                                dragging = true
                                root.dragFrom = setMarker.index
                                root.draggedLabel = "Set " + setMarker.info.number
                            }
                            if (dragging)
                                root.updateDrop(x)
                        }
                        onReleased: {
                            if (dragging)
                                root.finishDrag()
                            dragging = false
                        }
                        onCanceled: { root.cancelDrag(); dragging = false }
                    }
                }
            }
        }

        // Imported score markers and user annotations share the marker lane.
        Repeater {
            model: root.project.timelineMarkers || []
            Item {
                id: markerPin
                required property int index
                required property var modelData
                property real markerMs: root.transport.showMsAtTick(modelData.tick || 0)
                x: root.timeX(markerMs) - 6
                y: root.rulerHeight
                width: 12
                height: root.markerLaneHeight
                z: 12
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 2
                    height: parent.height
                    color: markerPin.modelData.color || "#d89b5b"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2
                    width: 9
                    height: 9
                    rotation: 45
                    color: markerPin.modelData.color || "#d89b5b"
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.transport.seekTick(markerPin.modelData.tick || 0)
                    ToolTip.visible: containsMouse
                    ToolTip.text: markerPin.modelData.name + (markerPin.modelData.type ? " · " + markerPin.modelData.type : "")
                        + "\n" + root.formatTime(markerPin.markerMs)
                }
            }
        }

        Repeater {
            model: root.project.musicMeasureCount
            Rectangle {
                id: measure
                required property int index
                property var info: { root.revision; return root.project.musicMeasureInfo(index) }
                x: root.timeX(root.transport.showMsAtTick(info.startTick || 0))
                y: root.musicY + 5
                width: Math.max(1, root.timeX(root.transport.showMsAtTick(info.endTick || 0)) - x - 1)
                height: root.musicLaneHeight - 10
                radius: 2
                visible: x + width >= viewport.contentX && x <= viewport.contentX + viewport.width
                color: info.selected ? "#325847" : "#203e36"
                border.color: info.selected ? "#85d9b3" : "#3b6857"
                Row {
                    width: parent.width
                    height: 3
                    Repeater {
                        model: measure.info.sections && measure.info.sections.length ? measure.info.sections : [{color: "#508574"}]
                        Rectangle {
                            required property var modelData
                            width: measure.width / Math.max(1, (measure.info.sections || []).length)
                            height: 3
                            color: modelData.color
                        }
                    }
                }
                Label {
                    anchors.fill: parent
                    anchors.margins: 5
                    color: "#c4e2d4"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    text: "m" + (measure.info.number || "") + " · " + (measure.info.numerator || 4) + "/" + (measure.info.denominator || 4)
                        + (measure.info.sections && measure.info.sections.length ? " · " + measure.info.sections[0].name : "")
                }
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
                    anchors.fill: parent
                    hoverEnabled: true
                    property real pressX: 0
                    property bool rangeDrag: false
                    onPressed: function(mouse) {
                        pressX = mouse.x
                        rangeDrag = false
                        if (!(mouse.modifiers & Qt.ShiftModifier))
                            root.selectionAnchor = measure.index
                        root.project.setMusicSelection(root.selectionAnchor, measure.index)
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed)
                            return
                        if (Math.abs(mouse.x - pressX) > 4)
                            rangeDrag = true
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        const tick = root.transport.tickAtShowMs(root.xTime(p.x))
                        let target = 0
                        while (target + 1 < root.project.musicMeasureCount
                               && root.project.musicMeasureInfo(target).endTick <= tick)
                            target++
                        root.project.setMusicSelection(root.selectionAnchor, target)
                    }
                    onDoubleClicked: root.transport.seekTick(measure.info.startTick)
                    ToolTip.visible: containsMouse && !pressed
                    ToolTip.text: "Measure " + measure.info.number + " · " + measure.info.counts + " counts"
                }
            }
        }

        // Analysis preview markers are deliberately translucent and remain
        // non-destructive until the user applies the reviewed plan.
        Repeater {
            model: root.project.setPlanCandidates || []
            Item {
                id: planGhost
                objectName: "planGhost" + index
                required property int index
                required property var modelData
                property real planMs: modelData.timeMs !== undefined ? modelData.timeMs + root.project.openingDurationMs
                    : root.transport.showMsAtTick(modelData.tick || 0)
                property real dragPosition: -1
                x: dragPosition >= 0 ? dragPosition : root.timeX(planMs) - 7
                y: root.markerLaneHeight + root.rulerHeight
                width: 14
                height: root.drillLaneHeight
                z: 15
                visible: modelData.accepted
                opacity: 0.72
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 2
                    height: parent.height
                    color: planGhost.modelData.color || "#d990ea"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 1
                    width: 12
                    height: 12
                    radius: 6
                    color: planGhost.modelData.color || "#d990ea"
                    Label { anchors.centerIn: parent; text: "+"; font.bold: true; font.pixelSize: 9; color: "#15171b" }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true
                    onPressed: {
                        root.draggingPlanCandidate = planGhost.index
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        planGhost.dragPosition = p.x - planGhost.width / 2
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed)
                            return
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        planGhost.dragPosition = p.x - planGhost.width / 2
                        const info = root.snappedPosition(p.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                        if (root.snapEnabled && info.snapType !== "free") root.showSnapFeedback(info)
                    }
                    onReleased: function(mouse) {
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        const info = root.snappedPosition(p.x, (mouse.modifiers & Qt.AltModifier) !== 0)
                        root.project.moveSetPlanCandidate(planGhost.index, info.tick)
                        planGhost.dragPosition = -1
                        root.draggingPlanCandidate = -1
                    }
                    onCanceled: {
                        planGhost.dragPosition = -1
                        root.draggingPlanCandidate = -1
                    }
                    ToolTip.visible: containsMouse
                    ToolTip.text: planGhost.modelData.title + "\n" + planGhost.modelData.reason
                }
            }
        }

        Rectangle {
            visible: root.resizingTransition >= 0 && root.resizePreview && root.resizePreview.counts !== undefined
            x: root.timeX(root.transport.setPositionMs(Math.max(0, root.resizingTransition - 1)))
            y: root.drillY + 4
            width: Math.max(1, root.resizePointerX - x)
            height: root.drillLaneHeight - 8
            color: "#284c7377"
            border.color: MarchCraftTheme.accentHover
            z: 30
            Label {
                anchors.centerIn: parent
                text: root.resizePreview.counts + " ct · " + root.resizePreview.timeText
                    + " · abs " + root.resizePreview.absoluteCount
                    + (root.resizePreview.measure ? " · m" + root.resizePreview.measure + " b" + root.resizePreview.beat : "")
                    + (root.resizePreview.snapLabel ? " · " + root.resizePreview.snapLabel : "")
                color: MarchCraftTheme.textPrimary
                font.bold: true
                font.pixelSize: 10
            }
        }

        Rectangle {
            visible: root.dragFrom >= 0 && root.dropSlot >= 0
            x: root.timeX(root.dropSlot >= root.project.setCount ? root.transport.durationMs : root.transport.setPositionMs(root.dropSlot)) - 1
            y: root.drillY
            width: 3
            height: root.drillLaneHeight
            z: 31
            color: MarchCraftTheme.accentHover
        }

        Rectangle {
            visible: root.snapEnabled && root.snapFeedback && root.snapFeedback.tick !== undefined
                && root.snapFeedback.snapType !== "free"
            x: root.timeX(root.snapFeedback.timeMs !== undefined
                ? root.snapFeedback.timeMs : root.transport.showMsAtTick(root.snapFeedback.tick))
            y: root.rulerHeight
            width: 1
            height: Math.max(0, root.laneBottom - y)
            z: 38
            color: "#fbbf24"
            Rectangle {
                x: 4
                y: 2
                width: snapLabel.implicitWidth + 10
                height: 20
                radius: 3
                color: "#3b2b16"
                border.color: "#fbbf24"
                Label {
                    id: snapLabel
                    anchors.centerIn: parent
                    text: root.snapFeedback.snapLabel || "Count snap"
                    color: "#ffe2a8"
                    font.pixelSize: 9
                }
            }
        }

        Rectangle {
            id: playhead
            objectName: "timelinePlayhead"
            x: root.timeX(root.transport.currentMs) - 1
            y: 0
            width: 2
            height: root.laneBottom
            z: 40
            color: "#ff9b56"
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 0
                width: 10
                height: 12
                radius: 2
                color: parent.color
            }
            Rectangle {
                x: 6
                y: 14
                width: playheadFeedback.implicitWidth + 12
                height: 22
                radius: 3
                visible: playheadHit.containsMouse || playheadHit.pressed
                color: "#35251d"
                border.color: playhead.color
                Label {
                    id: playheadFeedback
                    anchors.centerIn: parent
                    text: root.formatTime(root.transport.currentMs) + " · c"
                        + root.project.absoluteCountAtTick(root.transport.currentTick || root.transport.tickAtShowMs(root.transport.currentMs))
                    color: "#ffd0ad"
                    font.pixelSize: 10
                    font.family: "Consolas"
                }
            }
            MouseArea {
                id: playheadHit
                anchors.horizontalCenter: parent.horizontalCenter
                width: 16
                height: parent.height
                hoverEnabled: true
                cursorShape: Qt.SizeHorCursor
                onPressed: root.transport.beginScrub()
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        const p = mapToItem(viewport.contentItem, mouse.x, mouse.y)
                        root.transport.seekMs(root.xTime(p.x))
                    }
                }
                onReleased: root.transport.endScrub()
                onCanceled: root.transport.endScrub()
            }
        }
    }
}
