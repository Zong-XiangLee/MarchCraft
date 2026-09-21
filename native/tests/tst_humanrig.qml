import QtQuick
import QtQuick3D
import QtTest
import "../qml"

TestCase {
    name: "HumanRig"
    HumanPerformer3D { id: human; metersPerStep: 1 }
    // Centroids of the source vertices weighted to each hand, relative to its
    // wrist. These probes catch crossed palms that joint-origin tests miss.
    Node { id: leftGrip; parent: human.poseJoints[17]; position: Qt.vector3d(-0.078697, -0.046821, -0.001958) }
    Node { id: rightGrip; parent: human.poseJoints[21]; position: Qt.vector3d(0.078710, -0.046814, -0.001967) }

    function jointPosition(index) {
        const p = human.poseJoints[index].scenePosition
        return Qt.vector3d(p.x, p.y, p.z)
    }

    function init() {
        human.position = Qt.vector3d(0, 0, 0)
        human.eulerRotation = Qt.vector3d(0, 0, 0)
        human.marching = false
        human.closingTransition = false
        human.carriagePose = "horn.up"
        human.locomotionMode = "idle"
        human.heightMeters = 1.75
        human.travelStepsPerCount = 0
        human.gaitPhase = 0
        human.gaitElapsedCounts = 0
        human.facingDegrees = 0
        human.travelHeading = 0
        human.transitionProgress = 0
        human.countsInMove = 8
    }

    function test_loweredCarriage() {
        const raisedHeight = human.poseJoints[17].scenePosition.y
        human.carriagePose = "horn.down"
        verify(human.poseJoints[17].scenePosition.y < raisedHeight - 0.12)
        verify(human.poseJoints[17].scenePosition.y > 1.1)
        verify(human.poseJoints[17].scenePosition.minus(human.poseJoints[21].scenePosition).length() < 0.05)
    }

    function test_markTimeAlternatesWithoutMovingSupport() {
        human.marching = true
        human.locomotionMode = "mark_time"
        human.gaitElapsedCounts = 4
        human.gaitPhase = 0
        const left = jointPosition(8)
        const right = jointPosition(12)
        human.gaitPhase = 0.25
        verify(human.poseJoints[8].scenePosition.y > left.y + 0.015,
               "lift=" + (human.poseJoints[8].scenePosition.y - left.y))
        verify(human.poseJoints[12].scenePosition.minus(right).length() < 0.00001)
        human.gaitPhase = 0.75
        verify(human.poseJoints[12].scenePosition.y > right.y + 0.015)
        verify(human.poseJoints[8].scenePosition.minus(left).length() < 0.00001)
    }

    function test_actualQtPlantedAnkleInWorld() {
        human.marching = true
        human.locomotionMode = "march.forward"
        human.travelStepsPerCount = 0.5715
        human.gaitElapsedCounts = 4
        for (let heading of [0, 45, 90, 135, 180, 225, 270, 315]) {
            human.travelHeading = heading
            for (let facing of [0, 90, 180, 270]) {
                human.facingDegrees = facing
                human.eulerRotation.y = facing
                human.gaitPhase = 0.10
                human.position = Qt.vector3d(0, 0, 0)
                const planted = jointPosition(12)
                for (let phase of [0.12, 0.16, 0.20, 0.24]) {
                    human.gaitPhase = phase
                    const travel = (phase - 0.10) * 2 * 0.5715
                    human.position = Qt.vector3d(Math.sin(heading * Math.PI / 180) * travel, 0,
                                                 Math.cos(heading * Math.PI / 180) * travel)
                    const current = human.poseJoints[12].scenePosition
                    verify(Math.hypot(current.x - planted.x, current.z - planted.z) < 0.001,
                           "Planted ankle skates at heading=" + heading + " facing=" + facing)
                }
            }
        }
    }

    function test_actualQtAttentionJoints() {
        const left = human.poseJoints[17].scenePosition
        const right = human.poseJoints[21].scenePosition
        verify(left.y > 1.55 && right.y > 1.55, "Hands must reach face height")
        verify(left.minus(right).length() < 0.025, "Hands must meet in front of face")
        verify(left.z > 0.15 && right.z > 0.15, "Carriage faces the audience")
        verify(Math.abs(human.poseJoints[16].scenePosition.y
                        - human.poseJoints[15].scenePosition.y) < 0.015,
               "Upper arm must stay level")
        verify(human.poseJoints[8].scenePosition.x > 0,
               "Anatomical left is audience right")
        for (let pair of [[leftGrip, 17], [rightGrip, 21]]) {
            const direction = pair[0].scenePosition.minus(human.poseJoints[pair[1]].scenePosition)
            verify(direction.z > 0.08, "Fingers must extend toward the horn")
            verify(Math.abs(direction.x) < 0.005 && Math.abs(direction.y) < 0.005,
                   "Palms must not cross above the forehead")
        }
    }

    function test_heightScalingPreservesRequestedStride() {
        human.travelStepsPerCount = 0.5
        human.heightMeters = 1.4
        fuzzyCompare(human.strideMeters * human.bodyScale, 0.5, 0.00001)
        human.heightMeters = 2.0
        fuzzyCompare(human.strideMeters * human.bodyScale, 0.5, 0.00001)
        human.heightMeters = 1.75
        human.travelStepsPerCount = 0
    }

    function test_finalCountClosesToSetPosition() {
        const left = jointPosition(8)
        const right = jointPosition(12)
        human.marching = true
        human.locomotionMode = "march.forward"
        human.travelStepsPerCount = 0.5715
        human.closingTransition = true
        human.transitionProgress = 1
        for (let counts of [7, 8]) {
            human.countsInMove = counts
            human.gaitElapsedCounts = counts
            human.gaitPhase = (counts % 2) / 2
            verify(jointPosition(8).minus(left).length() < 0.0001)
            verify(jointPosition(12).minus(right).length() < 0.0001)
        }
    }
}
