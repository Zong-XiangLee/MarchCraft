import QtQuick
import QtQuick3D

Node {
    id: root
    property string venueId: "venue.rehearsal"
    property color primaryColor: "#123f2c"
    property color secondaryColor: "#d7b95b"
    property real fieldWidthMeters: 91.44
    property real fieldDepthMeters: 48.768
    property real crowdDensity: 0.2
    property string scoreboardText: "MARCHCRAFT"

    readonly property bool outdoorStadium: venueId === "venue.high_school" || venueId === "venue.bowl"
    readonly property bool indoor: venueId === "venue.gym" || venueId === "venue.arena"
    readonly property real standHeight: venueId === "venue.bowl" ? 16 : 7
    readonly property real standDepth: venueId === "venue.bowl" ? 18 : 9

    // Track/apron remains visually outside the authoritative field root.
    Model {
        visible: root.outdoorStadium
        source: "#Cube"
        y: -0.22
        scale: Qt.vector3d((root.fieldWidthMeters + 18) / 100, 0.004,
                           (root.fieldDepthMeters + 16) / 100)
        materials: PrincipledMaterial { baseColor: "#7a2e2e"; roughness: 0.94 }
    }

    Repeater3D {
        // The authoring camera occupies the audience/front side. Keep that side
        // open so venue scenery never occludes the drill surface.
        model: root.outdoorStadium ? 12 : 0
        delegate: Model {
            required property int index
            source: "#Cube"
            position: Qt.vector3d(0, (index + 1) * root.standHeight / 24,
                                  -(root.fieldDepthMeters / 2 + 8 + index * root.standDepth / 12))
            scale: Qt.vector3d((root.fieldWidthMeters + 8) / 100,
                               (index + 1) * root.standHeight / 1200, root.standDepth / 1200)
            materials: PrincipledMaterial { baseColor: index % 3 === 0 ? root.secondaryColor : root.primaryColor; roughness: 0.8 }
        }
    }

    // Press box and scoreboard form recognizable but inexpensive stadium silhouettes.
    Model {
        visible: root.outdoorStadium
        source: "#Cube"
        position: Qt.vector3d(0, root.standHeight + 2.25, -(root.fieldDepthMeters / 2 + 8 + 11 * root.standDepth / 12))
        scale: Qt.vector3d(0.28, 0.045, 0.035)
        materials: PrincipledMaterial { baseColor: "#d7dde2"; metalness: 0.08; roughness: 0.58 }
    }
    Model {
        visible: root.outdoorStadium
        source: "#Cube"
        position: Qt.vector3d(root.fieldWidthMeters / 2 + 8, 4.2, 0)
        scale: Qt.vector3d(0.045, 0.075, 0.11)
        materials: PrincipledMaterial { baseColor: root.primaryColor; roughness: 0.65 }
    }

    // Floodlight towers sit beyond the playing area, in meter-based world space.
    Repeater3D {
        model: root.outdoorStadium ? 4 : 0
        delegate: Node {
            required property int index
            position: Qt.vector3d((index % 2 ? 1 : -1) * (root.fieldWidthMeters / 2 + 7),
                                  0, (index < 2 ? -1 : 1) * (root.fieldDepthMeters / 2 + 9))
            Model {
                source: "#Cylinder"
                y: 10
                scale: Qt.vector3d(0.006, 0.20, 0.006)
                materials: PrincipledMaterial { baseColor: "#a4afb9"; metalness: 0.6 }
            }
            Model {
                source: "#Cube"
                y: 20
                scale: Qt.vector3d(0.05, 0.018, 0.009)
                materials: PrincipledMaterial { baseColor: "#fff0c9"; lighting: PrincipledMaterial.NoLighting }
            }
        }
    }
    Model {
        visible: root.outdoorStadium
        source: "#Rectangle"
        position: Qt.vector3d(root.fieldWidthMeters / 2 + 5.72, 5, 0)
        eulerRotation.y: -90
        scale: Qt.vector3d(0.10, 0.035, 1)
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColorMap: Texture {
                sourceItem: Rectangle {
                    width: 512; height: 180; color: "#101820"
                    Text {
                        anchors.fill: parent; anchors.margins: 18
                        text: root.scoreboardText
                        color: "#ffe6a0"; font.pixelSize: 40; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap; fontSizeMode: Text.Fit; minimumPixelSize: 12
                    }
                }
            }
        }
    }

    // Indoor shell and retractable seating. The performance floor remains Y=0.
    Model {
        visible: root.indoor
        source: "#Cube"
        y: 8
        scale: Qt.vector3d((root.fieldWidthMeters + 16) / 100, 0.003,
                           (root.fieldDepthMeters + 16) / 100)
        materials: PrincipledMaterial { baseColor: "#d9dde2"; roughness: 0.92 }
    }
    Repeater3D {
        model: root.indoor ? 1 : 0
        delegate: Model {
            required property int index
            source: "#Cube"
            position: Qt.vector3d(0, 2.4, -(root.fieldDepthMeters / 2 + 5))
            eulerRotation.x: 15
            scale: Qt.vector3d((root.fieldWidthMeters + 5) / 100, 0.05, 0.075)
            materials: PrincipledMaterial { baseColor: root.primaryColor; roughness: 0.78 }
        }
    }
}
