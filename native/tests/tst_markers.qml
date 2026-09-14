import QtQuick
import QtQuick3D
import QtTest
import "../qml"

TestCase {
    name: "PerformerMarkers"
    PerformerMarker3D { id: marker }

    function test_grounding_scale_and_facing() {
        const disk = findChild(marker, "markerDisk")
        const indicator = findChild(marker, "facingIndicator")
        verify(disk !== null)
        verify(indicator !== null)
        // Qt primitive cylinder is 100 units high and rests exactly on the field.
        fuzzyCompare(disk.y - disk.scale.y * 50, 0, 0.00001)
        fuzzyCompare(marker.scale.x * marker.metersPerStep, 1, 0.00001)
        verify(indicator.z < 0) // Front-facing indicator points along world -Z.
        marker.selected = true
        compare(disk.materials[0].baseColor, Qt.rgba(1, 1, 1, 1))
        verify(indicator.materials[0].baseColor !== disk.materials[0].baseColor)
    }
}
