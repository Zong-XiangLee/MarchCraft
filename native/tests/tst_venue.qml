import QtQuick
import QtQuick3D
import QtTest
import "../qml"

TestCase {
    name: "VenueLayout"
    Venue3D { id: venue }

    function init() { failOnWarning(/.*/) }

    function test_venueSeating_data() {
        return [
            { tag: "rehearsal", venueId: "venue.rehearsal", rows: 12, shown: false },
            { tag: "school", venueId: "venue.high_school", rows: 12, shown: true },
            { tag: "bowl", venueId: "venue.bowl", rows: 22, shown: true },
            { tag: "gym", venueId: "venue.gym", rows: 6, shown: true },
            { tag: "arena", venueId: "venue.arena", rows: 16, shown: true }
        ]
    }

    function test_venueSeating(data) {
        venue.venueId = data.venueId
        const seating = findChild(venue, "backSeating")
        verify(seating !== null)
        compare(seating.rows, data.rows)
        compare(seating.visible, data.shown)
        verify(seating.z < -venue.fieldDepthMeters / 2)
        venue.fieldDepthMeters = 60
        compare(seating.z, -43)
        venue.fieldDepthMeters = 48.768
    }
}
