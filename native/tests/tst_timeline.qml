import QtQuick
import QtTest
import "../qml"

TestCase {
    id: test
    name: "UnifiedTimeline"
    width: 900; height: 300; visible: true
    when: windowShown
    QtObject {
        id: mockProject
        property int setCount: 4
        property int musicMeasureCount: 4
        property int selectedSetStartIndex: 0
        property int selectedSetEndIndex: 0
        property var selectedSetIndices: [0]
        property string timelineSelectionKind: "set"
        property int selectedTransitionIndex: -1
        property int timelineRangeStartTick: 0
        property int timelineRangeEndTick: 0
        property int currentSetIndex: 0
        property real openingDurationMs: 1000
        property bool loopEnabled: false
        property int waveformPeakCount: 0
        property int timelineMarkerCount: 1
        property var timelineMarkers: [{id: "mark", tick: 8000, name: "Impact", type: "impact", color: "#f97316", absoluteCount: 8, measure: 3, beat: 1}]
        property bool setPlanPreviewActive: false
        property int setPlanCandidateCount: 0
        property var setPlanCandidates: []
        property string audioSource: ""
        property int musicStart: 0
        property int musicEnd: 0
        property int movedFrom: -1
        property int movedTo: -1
        property int resizedTransition: -1
        property int resizedCounts: -1
        property int movedCandidate: -1
        property int movedCandidateTick: -1
        signal movementsChanged()
        signal setsChanged()
        signal timingChanged()
        signal musicChanged()
        signal setRangeChanged()
        signal waveformChanged()
        signal timelineSelectionChanged()
        signal setPlanChanged()
        function setInfo(index) { return {number: String(index + 1), name: "Formation " + index, counts: 8, variantCount: 1,
            startTick: index * 4000, absoluteCount: index * 4, selected: selectedSetIndices.indexOf(index) >= 0,
            editing: index === currentSetIndex, playbackDestination: false} }
        function musicMeasureInfo(index) { return {startTick: index * 4000, endTick: (index + 1) * 4000, number: index + 1, numerator: 4, denominator: 4, counts: 4, selected: index >= musicStart && index <= musicEnd, sections: []} }
        function setMusicSelection(a, b) {
            musicStart = a; musicEnd = b
            timelineRangeStartTick = Math.min(a, b) * 4000
            timelineRangeEndTick = (Math.max(a, b) + 1) * 4000
            timelineSelectionKind = "measure"; selectedSetIndices = []; selectedTransitionIndex = -1
            musicChanged(); timelineSelectionChanged()
        }
        function moveSet(a, b) { movedFrom = a; movedTo = b; setsChanged() }
        function waveformPeakAtMs(ms) { return 0 }
        function absoluteCountAtTick(tick) { return Math.max(0, Math.round(tick / 1000)) }
        function snapTimelinePosition(tick, includeLandmarks) {
            const snapped = Math.max(0, Math.round(tick / 1000) * 1000)
            return {tick: snapped, absoluteCount: absoluteCountAtTick(snapped), measure: Math.floor(snapped / 4000) + 1,
                beat: Math.floor((snapped % 4000) / 1000) + 1, timeMs: 1000 + snapped,
                timeText: "0:00.0", snapType: "count", snapLabel: "Count " + absoluteCountAtTick(snapped)}
        }
        function selectTimelineSet(index, mode) {
            if (mode === 2) {
                const next = selectedSetIndices.slice()
                const found = next.indexOf(index)
                if (found >= 0 && next.length > 1) next.splice(found, 1); else if (found < 0) next.push(index)
                selectedSetIndices = next
            } else if (mode === 1) {
                const first = selectedSetIndices.length ? selectedSetIndices[0] : index
                const next = []
                for (let value = Math.min(first, index); value <= Math.max(first, index); ++value) next.push(value)
                selectedSetIndices = next
            } else selectedSetIndices = [index]
            selectedSetStartIndex = Math.min.apply(Math, selectedSetIndices)
            selectedSetEndIndex = Math.max.apply(Math, selectedSetIndices)
            currentSetIndex = index; timelineSelectionKind = "set"; selectedTransitionIndex = -1
            setRangeChanged(); timelineSelectionChanged()
        }
        function selectTimelineTransition(index) { selectedTransitionIndex = index; timelineSelectionKind = "transition"; selectedSetIndices = []; timelineSelectionChanged() }
        function selectTimelineRange(a, b) { timelineRangeStartTick = Math.min(a, b); timelineRangeEndTick = Math.max(a, b); timelineSelectionKind = "time"; selectedSetIndices = []; timelineSelectionChanged() }
        function clearTimelineSelection() { timelineSelectionKind = "none"; selectedTransitionIndex = -1; selectedSetIndices = []; timelineSelectionChanged() }
        function transitionInfo(index) { return {counts: 8, measure: index + 1, beat: 1, averageDistance: 3.2, maximumDistance: 5.5} }
        function previewTransitionResize(index, tick, snapping) {
            const start = (index - 1) * 4000
            const counts = Math.max(1, Math.round((tick - start) / 1000))
            return {counts: counts, tick: start + counts * 1000, absoluteCount: absoluteCountAtTick(start + counts * 1000),
                measure: index + 1, beat: 1, timeMs: 1000 + start + counts * 1000, timeText: "0:05.0",
                snapType: snapping ? "measure" : "count", snapLabel: snapping ? "Measure " + (index + 1) : "Count"}
        }
        function setTransitionCounts(index, counts) { resizedTransition = index; resizedCounts = counts; timingChanged(); return true }
        function moveSetPlanCandidate(index, tick) { movedCandidate = index; movedCandidateTick = tick; return true }
    }
    QtObject {
        id: mockTransport
        property bool playing: false
        property real currentMs: 0
        property real currentTick: Math.max(0, currentMs - 1000)
        property real durationMs: 17000
        property int lastPage: -1
        property bool extend: false
        property bool resume: false
        property int loopStartTick: 0
        property int loopEndTick: 8000
        signal positionChanged()
        function setPositionMs(index) { return index === 0 ? 0 : index * 4000 + 1000 }
        function showMsAtTick(tick) { return 1000 + tick }
        function tickAtShowMs(ms) { return Math.max(0, ms - 1000) }
        function audioMsAtShowMs(ms) { return ms - 1000 }
        function seekMs(ms) { currentMs = ms; positionChanged() }
        function seekTick(tick) { seekMs(showMsAtTick(tick)) }
        function navigateToSet(index, shift) {
            lastPage = index; extend = shift
            mockProject.selectTimelineSet(index, shift ? 1 : 0)
            seekMs(setPositionMs(index))
        }
        function navigateToSetWithMode(index, mode) {
            lastPage = index; extend = mode === 1
            mockProject.selectTimelineSet(index, mode)
            seekMs(setPositionMs(index))
        }
        function beginScrub() { resume = playing; playing = false }
        function endScrub() { playing = resume }
    }
    TimelineViewport { id: timeline; anchors.fill: parent; project: mockProject; transport: mockTransport }
    SignalSpy { id: editSpy; target: timeline; signalName: "editPage" }
    SignalSpy { id: menuSpy; target: timeline; signalName: "pageMenu" }
    function init() {
        mockTransport.playing = false; mockTransport.currentMs = 0; mockTransport.lastPage = -1
        mockProject.selectedSetStartIndex = 0; mockProject.selectedSetEndIndex = 0
        mockProject.selectedSetIndices = [0]; mockProject.timelineSelectionKind = "set"; mockProject.selectedTransitionIndex = -1
        mockProject.musicStart = 0; mockProject.musicEnd = 0; mockProject.movedFrom = -1; mockProject.movedTo = -1
        mockProject.resizedTransition = -1; mockProject.resizedCounts = -1
        mockProject.movedCandidate = -1; mockProject.movedCandidateTick = -1
        mockProject.setPlanPreviewActive = false; mockProject.setPlanCandidateCount = 0; mockProject.setPlanCandidates = []
        timeline.cancelDrag(); timeline.followPlayhead = true; timeline.fitShow()
        editSpy.clear(); menuSpy.clear(); wait(20)
    }
    function test_openingMarkerNavigatesToFirstPage() {
        const marker = findChild(timeline, "pageMarker0")
        mouseClick(marker, 10, 7)
        compare(mockTransport.lastPage, 0); compare(mockTransport.currentMs, 0)
        const next = findChild(timeline, "pageMarker1")
        mouseClick(next, 10, 7)
        compare(mockTransport.lastPage, 1)
    }
    function test_transitionRegionSelectsWithoutChangingPlaybackSet() {
        mockTransport.playing = true
        const hit = findChild(timeline, "pageHit1")
        mouseClick(hit, hit.width / 2, 30)
        compare(mockProject.selectedTransitionIndex, 1)
        compare(mockProject.timelineSelectionKind, "transition")
        compare(mockTransport.lastPage, -1)
        verify(mockTransport.playing)
    }
    function test_transitionRegionsSupportShiftRangeSelection() {
        const first = findChild(timeline, "pageHit1")
        mouseClick(first, first.width / 2, 30, Qt.LeftButton, Qt.ShiftModifier)
        compare(mockProject.selectedSetStartIndex, 0); compare(mockProject.selectedSetEndIndex, 1)
        const last = findChild(timeline, "pageHit3")
        mouseClick(last, last.width / 2, 30, Qt.LeftButton, Qt.ShiftModifier)
        compare(mockProject.selectedSetStartIndex, 0); compare(mockProject.selectedSetEndIndex, 3)
        compare(mockProject.selectedSetIndices.length, 4)
        compare(mockProject.timelineSelectionKind, "set")
    }
    function test_setMarkersSupportShiftAndControlSelection() {
        const first = findChild(timeline, "pageMarker1")
        mouseClick(first, 10, 7)
        const rangeEnd = findChild(timeline, "pageMarker3")
        mouseClick(rangeEnd, 10, 7, Qt.LeftButton, Qt.ShiftModifier)
        compare(mockProject.selectedSetStartIndex, 1); compare(mockProject.selectedSetEndIndex, 3)
        const toggle = findChild(timeline, "pageMarker2")
        mouseClick(toggle, 10, 7, Qt.LeftButton, Qt.ControlModifier)
        verify(mockProject.selectedSetIndices.indexOf(2) < 0)
    }
    function test_contextDoesNotNavigateAndDoubleClickEdits() {
        const hit = findChild(timeline, "pageHit2")
        mouseClick(hit, hit.width / 2, 30, Qt.RightButton)
        compare(menuSpy.count, 1); compare(mockTransport.lastPage, -1)
        mouseDoubleClickSequence(hit, hit.width / 2, 30)
        compare(editSpy.count, 1); compare(editSpy.signalArguments[0][0], 2)
    }
    function test_rulerScrubResumesOnlyWhenPlaying() {
        const ruler = findChild(timeline, "timelineRuler")
        mockTransport.playing = true
        mousePress(ruler, timeline.timeX(6000), 16)
        verify(!mockTransport.playing)
        mouseMove(ruler, timeline.timeX(8000), 16)
        mouseRelease(ruler, timeline.timeX(8000), 16)
        verify(mockTransport.playing); fuzzyCompare(mockTransport.currentMs, 8000, 30)
        mockTransport.playing = false
        mouseClick(ruler, timeline.timeX(3000), 16)
        verify(!mockTransport.playing)
    }
    function test_rulerSnapsToCountsAndAltBypasses() {
        const ruler = findChild(timeline, "timelineRuler")
        timeline.snapEnabled = true
        mouseClick(ruler, timeline.timeX(4250), 16)
        compare(mockTransport.currentMs, 4000)
        mouseClick(ruler, timeline.timeX(4250), 16, Qt.LeftButton, Qt.AltModifier)
        fuzzyCompare(mockTransport.currentMs, 4250, 1)
    }
    function test_playheadDragUsesSharedTimeCoordinates() {
        mockTransport.currentMs = 4000
        const head = findChild(timeline, "timelinePlayhead")
        const viewport = findChild(timeline, "timelineViewport")
        mockTransport.playing = true
        mousePress(head, 1, 18)
        verify(!mockTransport.playing)
        mouseMove(viewport, timeline.timeX(7000), 18)
        mouseRelease(viewport, timeline.timeX(7000), 18)
        verify(mockTransport.playing)
        fuzzyCompare(mockTransport.currentMs, 7000, 30)
    }
    function test_zoomAnchorAndSharedScroll() {
        const anchor = timeline.viewportWidth / 2
        const before = timeline.xTime(timeline.scrollX + anchor)
        timeline.zoomBy(2, anchor)
        fuzzyCompare(timeline.xTime(timeline.scrollX + anchor), before, 0.01)
        timeline.scrollTo(140, true)
        verify(!timeline.followPlayhead); compare(timeline.scrollX, 140)
        const page = findChild(timeline, "timelinePage1")
        fuzzyCompare(page.x, timeline.timeX(mockTransport.setPositionMs(0)), 0.01)
        timeline.fitShow(); compare(timeline.scrollX, 0); verify(timeline.fitEnabled)
    }
    function test_measureDragSelectsAndDoubleClickSeeks() {
        const hit = findChild(timeline, "measureHit0")
        mousePress(hit, 30, 20)
        mouseMove(hit, hit.width * 2.5, 20)
        mouseRelease(hit, hit.width * 2.5, 20)
        compare(mockProject.musicStart, 0); compare(mockProject.musicEnd, 2)
        compare(mockProject.timelineSelectionKind, "measure")
        timeline.fitSelection(); verify(!timeline.fitEnabled)
        compare(mockTransport.currentMs, 0)
        const next = findChild(timeline, "measureHit1")
        mouseDoubleClickSequence(next, 30, 20)
        compare(mockTransport.currentMs, 5000)
    }
    function test_handleReorderDoesNotSeek() {
        const handle = findChild(timeline, "pageHandle1")
        mousePress(handle, 7, 9)
        mouseMove(handle, 120, 9, 30)
        verify(timeline.dragFrom >= 0)
        mouseMove(handle, 300, 9, 30)
        mouseRelease(handle, 300, 9)
        compare(mockProject.movedFrom, 1); verify(mockProject.movedTo > 1)
        compare(mockTransport.lastPage, -1); compare(timeline.dragFrom, -1)
    }
    function test_transitionBoundaryResizesWithoutReordering() {
        const handle = findChild(timeline, "transitionHandle1")
        verify(handle !== null)
        mousePress(handle, 6, 9)
        mouseMove(handle, 80, 9, 30)
        verify(timeline.resizingTransition >= 0)
        mouseRelease(handle, 80, 9)
        compare(mockProject.resizedTransition, 1)
        verify(mockProject.resizedCounts > 0)
        compare(mockProject.movedFrom, -1)
        compare(timeline.resizingTransition, -1)
    }
    function test_markerLaneSelectsTimeRangeAndFitSelection() {
        const range = findChild(timeline, "timelineRangeSurface")
        mousePress(range, timeline.timeX(3000), 12)
        mouseMove(range, timeline.timeX(9000), 12)
        mouseRelease(range, timeline.timeX(9000), 12)
        compare(mockProject.timelineSelectionKind, "time")
        verify(mockProject.timelineRangeEndTick > mockProject.timelineRangeStartTick)
        timeline.fitSelection()
        verify(!timeline.fitEnabled)
        mouseClick(range, timeline.scrollX + timeline.viewportWidth / 2, 12)
        compare(mockProject.timelineSelectionKind, "none")
    }
    function test_setPlanGhostDragsAsTransientPreview() {
        mockProject.setPlanPreviewActive = true
        mockProject.setPlanCandidateCount = 1
        mockProject.setPlanCandidates = [{accepted: true, tick: 2500, timeMs: 2500,
            title: "Impact", reason: "Four tracks attack together", color: "#8b5cf6"}]
        mockProject.setPlanChanged()
        wait(0)
        const ghost = findChild(timeline, "planGhost0")
        verify(ghost !== null)
        mousePress(ghost, 7, 20)
        mouseMove(ghost, 87, 20, 30)
        verify(ghost.dragPosition >= 0)
        mouseRelease(ghost, 87, 20)
        compare(mockProject.movedCandidate, 0)
        verify(mockProject.movedCandidateTick > 2500)
        compare(timeline.draggingPlanCandidate, -1)
    }
}
