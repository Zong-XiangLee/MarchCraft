import QtQuick
import QtTest
import "../qml/CameraMotion.js" as Motion

TestCase {
    name: "CameraMotion"

    function test_zoomIsContinuousAndReversible() {
        compare(Motion.zoom(80, 0), 80)
        verify(Motion.zoom(80, 1) < 80)
        verify(Math.abs(Motion.zoom(Motion.zoom(80, 120), -120) - 80) < 0.0001)
        compare(Motion.zoom(80, 100000), 2)
        compare(Motion.zoom(80, -100000), 240)
    }

    function test_orbitStaysAboveGround() {
        compare(Motion.orbit(-42, 0, 0, 10000).pitch, -89.5)
        compare(Motion.orbit(-42, 0, 0, -10000).pitch, -3)
        compare(Motion.orbit(-42, 360, 0, 0).yaw, 360)
    }

    function test_panFollowsCameraAndDistance() {
        const front = Motion.pan(0, 0, 0, -42, 80, 800, 10, 0)
        const side = Motion.pan(0, 0, 90, -42, 80, 800, 10, 0)
        verify(front.x < 0); compare(front.z, 0)
        verify(Math.abs(side.x) < 0.0001); verify(side.z > 0)
        const close = Motion.pan(0, 0, 0, -42, 40, 800, 10, 0)
        verify(Math.abs(front.x - close.x * 2) < 0.0001)
        const extreme = Motion.pan(0, 0, 0, -3, 240, 0, 100000, 100000)
        compare(extreme.x, -100); compare(extreme.z, -70)
    }
}
