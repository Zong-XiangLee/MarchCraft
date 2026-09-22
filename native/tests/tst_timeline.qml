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
        property real openingDurationMs: 1000
        property bool loopEnabled: false
        property int waveformPeakCount: 0
        property string audioSource: ""
        property int musicStart: 0
        property int musicEnd: 0
        property int movedFrom: -1
        property int movedTo: -1
        signal movementsChanged()
        signal setsChanged()
        signal timingChanged()
        signal musicChanged()
        signal setRangeChanged()
        signal waveformChanged()
        function setInfo(index) { return {number: String(index + 1), name: "Formation " + index, counts: 8, variantCount: 1} }
        function musicMeasureInfo(index) { return {startTick: index * 4000, endTick: (index + 1) * 4000, number: index + 1, numerator: 4, denominator: 4, counts: 4, selected: index >= musicStart && index <= musicEnd, sections: []} }
        function setMusicSelection(a, b) { musicStart = a; musicEnd = b; musicChanged() }
        function moveSet(a, b) { movedFrom = a; movedTo = b; setsChanged() }
        function waveformPeakAtMs(ms) { return 0 }
    }
    QtObject {
        id: mockTransport
        property bool playing: false
        property real currentMs: 0
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
            if (!shift) mockProject.selectedSetStartIndex = index
            mockProject.selectedSetEndIndex = index
            seekMs(setPositionMs(index)); mockProject.setRangeChanged()
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
        mockProject.musicStart = 0; mockProject.musicEnd = 0; mockProject.movedFrom = -1; mockProject.movedTo = -1
        timeline.cancelDrag(); timeline.followPlayhead = true; timeline.fitShow()
        editSpy.clear(); menuSpy.clear(); wait(20)
    }
    function test_openingMarkerNavigatesToFirstPage() {
        const marker = findChild(timeline, "pageMarker0")
        mouseClick(marker, 20, 7)
        compare(mockTransport.lastPage, 0); compare(mockTransport.currentMs, 0)
        const next = findChild(timeline, "pageMarker1")
        mouseClick(next, 22, 7)
        compare(mockTransport.lastPage, 1)
    }
    function test_pageClickKeepsPlayingAndShiftRange() {
        mockTransport.playing = true
        const hit = findChild(timeline, "pageHit1")
        mouseClick(hit, hit.width / 2, 30)
        compare(mockTransport.lastPage, 1); verify(mockTransport.playing)
        const next = findChild(timeline, "pageHit2")
        mouseClick(next, next.width / 2, 30, Qt.LeftButton, Qt.ShiftModifier)
        compare(mockProject.selectedSetStartIndex, 1); compare(mockProject.selectedSetEndIndex, 2)
        verify(mockTransport.extend)
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
        fuzzyCompare(page.x, timeline.timeX(mockProject.openingDurationMs), 0.01)
        timeline.fitShow(); compare(timeline.scrollX, 0); verify(timeline.fitEnabled)
    }
    function test_measureDragSelectsAndDoubleClickSeeks() {
        const hit = findChild(timeline, "measureHit0")
        mousePress(hit, 30, 20)
        mouseMove(hit, hit.width * 2.5, 20)
        mouseRelease(hit, hit.width * 2.5, 20)
        compare(mockProject.musicStart, 0); compare(mockProject.musicEnd, 2)
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
}
