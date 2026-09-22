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

    readonly property real seatingOffset: fieldDepthMeters / 2 + 13
    PrincipledMaterial { id: concrete; baseColor: "#8c9697"; roughness: 0.93 }
    PrincipledMaterial { id: steel; baseColor: "#a9b7bb"; metalness: 0.65; roughness: 0.4 }
    PrincipledMaterial { id: seats; baseColor: root.primaryColor; roughness: 0.72 }

    // Landscape and track are grounded below the authoritative playing surface.
    Model {
        source: "#Cube"; y: -0.45
        scale: Qt.vector3d((root.fieldWidthMeters + 100) / 100, 0.004,
                          (root.fieldDepthMeters + 90) / 100)
        materials: PrincipledMaterial { baseColor: root.indoor ? "#434b51" : "#364d38"; roughness: 1 }
    }
    Model {
        visible: root.outdoorStadium
        source: "#Rectangle"; y: -0.20; eulerRotation.x: -90
        scale: Qt.vector3d((root.fieldWidthMeters + 64) / 100,
                          (root.fieldDepthMeters + 26) / 100, 1)
        materials: PrincipledMaterial {
            roughness: 0.98
            baseColorMap: Texture {
                generateMipmaps: true; mipFilter: Texture.Linear
                sourceItem: Canvas {
                    width: 2048; height: 1024
                    function oval(ctx, inset) {
                        const r = height / 2 - inset
                        ctx.beginPath()
                        ctx.moveTo(height / 2, inset)
                        ctx.lineTo(width - height / 2, inset)
                        ctx.arc(width - height / 2, height / 2, r, -Math.PI / 2, Math.PI / 2)
                        ctx.lineTo(height / 2, height - inset)
                        ctx.arc(height / 2, height / 2, r, Math.PI / 2, Math.PI * 1.5)
                        ctx.closePath()
                    }
                    onPaint: {
                        const ctx = getContext("2d"); ctx.reset()
                        ctx.fillStyle = "#364d38"; ctx.fillRect(0, 0, width, height)
                        oval(ctx, 4); ctx.fillStyle = "#945e4e"; ctx.fill()
                        ctx.strokeStyle = "#e4cfc0"; ctx.lineWidth = 2
                        for (let lane = 0; lane <= 6; ++lane) { oval(ctx, 8 + lane * 16); ctx.stroke() }
                        oval(ctx, 112); ctx.fillStyle = "#364d38"; ctx.fill()
                        // Start/finish stripes across the straight, outside the sideline.
                        ctx.fillStyle = "#f2e8dc"; ctx.fillRect(width * 0.68, 8, 4, 96)
                    }
                    Component.onCompleted: requestPaint()
                }
            }
        }
    }

    component SeatingBank: Node {
        id: bank
        property real bankWidth: 100
        property int rows: 12
        property int sections: 8
        property real rise: 0.48
        property real tread: 0.85
        Repeater3D {
            model: bank.rows
            delegate: Node {
                id: tier
                required property int index
                readonly property real rowWidth: bank.bankWidth
                readonly property int sectionCount: bank.sections
                position: Qt.vector3d(0, (index + 1) * bank.rise, -index * bank.tread)
                Model {
                    source: "#Cube"; y: -0.14
                    scale: Qt.vector3d(tier.rowWidth / 100, 0.0028, 0.009)
                    materials: concrete
                }
                Repeater3D {
                    model: tier.sectionCount
                    delegate: Model {
                        required property int index
                        source: "#Cube"
                        position: Qt.vector3d(-tier.rowWidth / 2 + (index + 0.5) * tier.rowWidth / tier.sectionCount, 0.16, -0.22)
                        scale: Qt.vector3d((tier.rowWidth / tier.sectionCount - 1.4) / 100, 0.0032, 0.0035)
                        materials: seats
                    }
                }
            }
        }
        Model {
            source: "#Cube"
            position: Qt.vector3d(0, 0.9, 0.55)
            scale: Qt.vector3d(bank.bankWidth / 100, 0.0006, 0.0006)
            materials: steel
        }
        Repeater3D {
            model: bank.sections + 1
            delegate: Model {
                required property int index
                source: "#Cube"
                position: Qt.vector3d(-bank.bankWidth / 2 + index * bank.bankWidth / bank.sections, 0.45, 0.55)
                scale: Qt.vector3d(0.0006, 0.009, 0.0006)
                materials: steel
            }
        }
    }
    SeatingBank {
        objectName: "backSeating"
        visible: root.outdoorStadium || root.indoor
        z: -root.seatingOffset
        bankWidth: root.fieldWidthMeters + (root.indoor ? 0 : 8)
        rows: root.venueId === "venue.bowl" ? 22 : root.venueId === "venue.arena" ? 16 : root.venueId === "venue.gym" ? 6 : 12
        rise: root.venueId === "venue.bowl" ? 0.65 : 0.48
    }
    Repeater3D {
        model: root.venueId === "venue.bowl" || root.venueId === "venue.arena" ? 2 : 0
        delegate: SeatingBank {
            required property int index
            x: (index === 0 ? -1 : 1) * (root.fieldWidthMeters / 2 + 28)
            eulerRotation.y: index === 0 ? 90 : -90
            bankWidth: root.fieldDepthMeters + 10
            rows: root.venueId === "venue.bowl" ? 18 : 12
            sections: 5
        }
    }

    // Press box and scoreboard form recognizable but inexpensive stadium silhouettes.
    Model {
        visible: root.outdoorStadium
        source: "#Cube"
        position: Qt.vector3d(0, root.standHeight + 1.5, -(root.seatingOffset + (root.venueId === "venue.bowl" ? 20 : 11)))
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

    // Open audience-side cutaway keeps both orbit and overhead views usable.
    Model {
        visible: root.indoor
        source: "#Cube"
        position: Qt.vector3d(0, 7, -(root.seatingOffset + 16))
        scale: Qt.vector3d((root.fieldWidthMeters + 52) / 100, 0.14, 0.004)
        materials: PrincipledMaterial { baseColor: root.venueId === "venue.gym" ? "#c8bfb0" : "#626d78"; roughness: 0.9 }
    }
    Repeater3D {
        model: root.indoor ? 12 : 0
        delegate: Node {
            required property int index
            x: (index - 5.5) * (root.fieldWidthMeters + 42) / 12
            z: -(root.seatingOffset + 15.6)
            Model {
                source: "#Cube"; y: 7
                scale: Qt.vector3d(0.003, 0.14, 0.006)
                materials: steel
            }
            Model {
                source: "#Cube"; y: 11
                scale: Qt.vector3d(0.025, 0.035, 0.001)
                materials: PrincipledMaterial { baseColor: root.secondaryColor; roughness: 0.85 }
            }
        }
    }
    // Press-box glazing, roof overhang, and facade mullions.
    Node {
        visible: root.outdoorStadium
        position: Qt.vector3d(0, root.standHeight + 1.5,
                              -(root.seatingOffset + (root.venueId === "venue.bowl" ? 20 : 11)))
        Model {
            source: "#Cube"; z: 1.8
            scale: Qt.vector3d(0.265, 0.023, 0.001)
            materials: PrincipledMaterial { baseColor: "#263f50"; metalness: 0.35; roughness: 0.22 }
        }
        Model {
            source: "#Cube"; y: 2.45
            scale: Qt.vector3d(0.30, 0.004, 0.055)
            materials: steel
        }
        Repeater3D {
            model: 9
            delegate: Model {
                required property int index
                source: "#Cube"; x: (index - 4) * 3; z: 1.9
                scale: Qt.vector3d(0.001, 0.025, 0.001)
                materials: steel
            }
        }
    }
    // Rehearsal grounds: perimeter fence, simple benches, and a coaching platform.
    Repeater3D {
        model: root.venueId === "venue.rehearsal" ? 25 : 0
        delegate: Model {
            required property int index
            source: "#Cube"
            position: Qt.vector3d((index - 12) * (root.fieldWidthMeters + 16) / 24, 0.75, -root.fieldDepthMeters / 2 - 7)
            scale: Qt.vector3d(0.0008, 0.015, 0.0008)
            materials: steel
        }
    }
    Repeater3D {
        model: root.venueId === "venue.rehearsal" ? 2 : 0
        delegate: Model {
            required property int index
            source: "#Cube"
            position: Qt.vector3d(0, 0.4 + index * 0.9, -root.fieldDepthMeters / 2 - 7)
            scale: Qt.vector3d((root.fieldWidthMeters + 16) / 100, 0.0006, 0.0006)
            materials: steel
        }
    }
    Repeater3D {
        model: root.indoor ? 0 : 4
        delegate: Node {
            required property int index
            position: Qt.vector3d((index - 1.5) * 19, 0, -root.fieldDepthMeters / 2 - 3)
            Model { source: "#Cube"; y: 0.48; scale: Qt.vector3d(0.065, 0.0012, 0.006); materials: steel }
            Model { source: "#Cube"; x: -2.5; y: 0.22; scale: Qt.vector3d(0.001, 0.0044, 0.005); materials: steel }
            Model { source: "#Cube"; x: 2.5; y: 0.22; scale: Qt.vector3d(0.001, 0.0044, 0.005); materials: steel }
        }
    }
}
