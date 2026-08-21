import QtQuick
import QtTest
import "../qml/HumanGait.js" as HumanGait

TestCase {
    name: "HumanGait"

    readonly property real standardStride: 0.5715

    function closeTo(actual, expected, tolerance) {
        verify(Math.abs(actual - expected) <= tolerance,
               "expected " + expected + ", got " + actual)
    }

    function test_leftFootBeginsRecoveryOnCountZero() {
        verify(!HumanGait.targetForLeg("march.forward", 0, true,
                                      standardStride, 1).stance)
        verify(HumanGait.targetForLeg("march.forward", 0, false,
                                     standardStride, 1).stance)
        verify(HumanGait.targetForLeg("march.forward", 0.5, true,
                                     standardStride, 1).stance)
    }

    function test_countPhaseProducesVisibleJointMotion() {
        var bodyAtContact = HumanGait.directionalBodyPose(0, 0,
                                                           standardStride, 1)
        var bodyAtPassing = HumanGait.directionalBodyPose(0, 0.15,
                                                           standardStride, 1)
        var contact = HumanGait.directionalLegPose(0, 0, true,
                                                    standardStride, 1,
                                                    bodyAtContact)
        var passing = HumanGait.directionalLegPose(0, 0.15, true,
                                                    standardStride, 1,
                                                    bodyAtPassing)
        verify(Math.abs(contact.hipX - passing.hipX) > 5)
        verify(Math.abs(contact.kneeX - passing.kneeX) > 5)
        // The hip and knee create the visible step while the ankle remains a
        // finite, controlled rolling platform.
        verify(isFinite(contact.footX) && isFinite(passing.footX))
    }

    function test_plantedForwardFootIsWorldLocked() {
        var atContact = HumanGait.targetForLeg("march.forward", 0, false,
                                               standardStride, 1)
        var beforeRelease = HumanGait.targetForLeg("march.forward", 0.499999, false,
                                                   standardStride, 1)
        // The root advances -stride in local Z during one count. The planted
        // target advances +stride relative to the root, leaving world Z fixed.
        closeTo((-standardStride + beforeRelease.z) - atContact.z, 0, 0.00001)
    }

    function test_forwardUsesHeelAndBackwardUsesPlatform() {
        var forwardContact = HumanGait.targetForLeg("march.forward", 0, false,
                                                    standardStride, 1)
        var backwardContact = HumanGait.targetForLeg("march.backward", 0, false,
                                                     standardStride, 1)
        closeTo(forwardContact.footPitch, -32, 0.001)
        closeTo(backwardContact.footPitch, 8, 0.001)
    }

    function test_forwardAndSlideRollHeelThroughForefoot() {
        var stancePhases = [0, 0.06, 0.12, 0.25, 0.38, 0.499]
        for (var i = 0; i < stancePhases.length; ++i) {
            var forward = HumanGait.targetForDirection(0, stancePhases[i], false,
                                                        standardStride, 1)
            var side = HumanGait.targetForDirection(90, stancePhases[i], false,
                                                     standardStride, 1)
            closeTo(forward.toePitch, 0, 0.001)
            closeTo(side.footPitch, forward.footPitch, 0.001)
            closeTo(side.toePitch, 0, 0.001)
        }
        closeTo(HumanGait.targetForLeg("march.forward", 0.25, false,
                                       standardStride, 1).footPitch, 0, 0.001)
        closeTo(HumanGait.targetForLeg("march.forward", 0.499, false,
                                       standardStride, 1).footPitch, 0, 0.001)
        closeTo(HumanGait.targetForLeg("march.forward", 0.75, false,
                                       standardStride, 1).footPitch, 0, 0.001)
        verify(HumanGait.targetForLeg("march.forward", 0.999, false,
                                      standardStride, 1).footPitch < -31)
    }

    function test_attentionAndTenduClose() {
        var leftAttention = HumanGait.attentionLegPose(true)
        var rightAttention = HumanGait.attentionLegPose(false)
        closeTo(leftAttention.footY, -45, 0.001)
        closeTo(rightAttention.footY, 45, 0.001)
        verify(leftAttention.planted && rightAttention.planted)
        verify(leftAttention.hipZ > 0 && rightAttention.hipZ < 0)
        closeTo(leftAttention.footZ, -leftAttention.hipZ, 0.001)
        closeTo(rightAttention.footZ, -rightAttention.hipZ, 0.001)

        var body = HumanGait.directionalBodyPose(0, 0, standardStride, 1)
        var moving = HumanGait.directionalLegPose(0, 0, true,
                                                   standardStride, 1, body)
        var middle = HumanGait.applyClosingPose(moving, 0, true, 0.5, true)
        var closed = HumanGait.applyClosingPose(moving, 0, true, 1, true)
        verify(middle.footX < moving.footX - 10)
        closeTo(closed.footY, -45, 0.001)
        closeTo(closed.toeX, 0, 0.001)
        verify(closed.planted)
    }

    function test_leftHeelArrivesBeforeRightShoeReleases() {
        var leftContact = HumanGait.targetForLeg("march.forward", 0.5, true,
                                                 standardStride, 1)
        var rightFlat = HumanGait.targetForLeg("march.forward", 0.5, false,
                                                standardStride, 1)
        var leftRolling = HumanGait.targetForLeg("march.forward", 0.575, true,
                                                  standardStride, 1)
        var rightReleasing = HumanGait.targetForLeg("march.forward", 0.575, false,
                                                     standardStride, 1)
        closeTo(leftContact.footPitch, -32, 0.001)
        closeTo(rightFlat.footPitch, 0, 0.001)
        verify(leftRolling.footPitch > leftContact.footPitch)
        verify(rightReleasing.footPitch > rightFlat.footPitch)
    }

    function test_attentionPostureIsTallNeutralAndPerformanceReady() {
        var body = HumanGait.bodyPose("idle", 0, standardStride, 0)
        compare(body.pelvisYaw, 0)
        compare(body.pelvisRoll, 0)
        verify(body.pelvisY < 0 && body.pelvisY > -0.008)
        verify(Math.abs(body.pelvisY + body.spineLift) < 0.0005)
        verify(body.spinePitch > 0 && body.spinePitch < 2)
        closeTo(body.headPitch, 10, 0.001)

        var leftArm = HumanGait.handSetPose(true)
        var rightArm = HumanGait.handSetPose(false)
        // Both upper arms share a level, forward-projecting carriage. Their
        // mirrored yaw/roll and inward forearm rotations form the high,
        // centered instrument triangle without lifting either shoulder.
        closeTo(leftArm.upperX, rightArm.upperX, 0.001)
        closeTo(leftArm.upperY, -rightArm.upperY, 0.001)
        closeTo(leftArm.upperZ, -rightArm.upperZ, 0.001)
        verify(leftArm.upperX > 105)
        verify(leftArm.forearmX > 65 && rightArm.forearmX > 65)
        verify(leftArm.forearmY < -50 && rightArm.forearmY > 50)
        verify(leftArm.forearmZ < -40 && rightArm.forearmZ > 40)
        verify(rightArm.handX > leftArm.handX)
        verify(rightArm.handY > leftArm.handY)
        verify(rightArm.handZ > leftArm.handZ)
    }

    function test_evenPhraseKeepsRightFootForTenduClose() {
        var body = HumanGait.directionalBodyPose(0, 0.75, standardStride, 1)
        var left = HumanGait.directionalLegPose(0, 0.75, true,
                                                 standardStride, 1, body)
        var right = HumanGait.directionalLegPose(0, 0.75, false,
                                                  standardStride, 1, body)
        var closedLeft = HumanGait.applyClosingPose(left, 0.75, true, 0.5, false)
        var closingRight = HumanGait.applyClosingPose(right, 0.75, false, 0.5, false)
        verify(closingRight.footX < closedLeft.footX - 10)
    }

    function test_swingPassesCloseAndDeceleratesIntoContact() {
        // Right recovery is 0.5..1.0 of its leg cycle; gait .75 is passing.
        var passing = HumanGait.targetForLeg("march.forward", 0.75, false,
                                             standardStride, 1)
        closeTo(passing.z, 0, 0.00001)
        verify(passing.lift > 0 && passing.lift < 0.012)
        var earlyDelta = HumanGait.smootherStep(0.02) - HumanGait.smootherStep(0)
        var middleDelta = HumanGait.smootherStep(0.52) - HumanGait.smootherStep(0.50)
        var contactDelta = HumanGait.smootherStep(1) - HumanGait.smootherStep(0.98)
        verify(earlyDelta < middleDelta)
        verify(contactDelta < middleDelta)
    }

    function test_recoveryShoeIsParallelAndAdductedAtPassing() {
        var body = HumanGait.bodyPose("march.forward", 0.75,
                                      standardStride, 1)
        var support = HumanGait.legPose("march.forward", 0.75, true,
                                        standardStride, 1, body.pelvisY, body.pelvisZ)
        var passing = HumanGait.legPose("march.forward", 0.75, false,
                                        standardStride, 1, body.pelvisY, body.pelvisZ)
        closeTo(support.footY, 0, 0.001)
        closeTo(passing.footY, 0, 0.001)
        compare(support.hipZ, 0)
        verify(passing.hipZ < -6)
        closeTo(passing.footZ, -passing.hipZ, 0.5)
    }

    function test_forwardKneeFlexesBeforeCrossThenLengthens() {
        function rightLegAt(phase) {
            var body = HumanGait.bodyPose("march.forward", phase,
                                          standardStride, 1)
            return HumanGait.legPose("march.forward", phase, false,
                                     standardStride, 1, body.pelvisY, body.pelvisZ)
        }
        var beforeCross = rightLegAt(0.65)
        var atCross = rightLegAt(0.75)
        var forwardSwing = rightLegAt(0.85)
        var nextCount = rightLegAt(0)
        verify(beforeCross.kneeX < atCross.kneeX - 4)
        verify(Math.abs(atCross.kneeX) < 15)
        verify(Math.abs(forwardSwing.kneeX) < 12)
        verify(Math.abs(nextCount.kneeX) < 15)
    }

    function test_legJointsRemainContinuousThroughCountsAndDirections() {
        var headings = [0, 45, 90, 105, 120, 180, -90, -120]
        var samples = 2048
        for (var h = 0; h < headings.length; ++h) {
            var previousBody = HumanGait.directionalBodyPose(
                        headings[h], (samples - 1) / samples,
                        standardStride, 1)
            var previous = HumanGait.directionalLegPose(
                        headings[h], (samples - 1) / samples, true,
                        standardStride, 1, previousBody)
            for (var sample = 0; sample < samples; ++sample) {
                var phase = sample / samples
                var body = HumanGait.directionalBodyPose(
                            headings[h], phase, standardStride, 1)
                var pose = HumanGait.directionalLegPose(
                            headings[h], phase, true,
                            standardStride, 1, body)
                verify(Math.abs(pose.hipX - previous.hipX) < 1.0,
                       "hip X discontinuity at " + headings[h] + " / " + phase)
                verify(Math.abs(pose.hipZ - previous.hipZ) < 0.3,
                       "hip Z discontinuity at " + headings[h] + " / " + phase)
                verify(Math.abs(pose.kneeX - previous.kneeX) < 1.5,
                       "knee discontinuity at " + headings[h] + " / " + phase)
                previous = pose
            }
        }
    }

    function test_weightTransferKeepsContactKneesLongAndUpperBodyQuiet() {
        var headings = [0, 45, 75, 90, 105, 120, 135, 180,
                        -45, -90, -120, -135]
        for (var i = 0; i < headings.length; ++i) {
            var body = HumanGait.directionalBodyPose(
                        headings[i], 0, standardStride, 1)
            var left = HumanGait.directionalLegPose(
                        headings[i], 0, true, standardStride, 1, body)
            var right = HumanGait.directionalLegPose(
                        headings[i], 0, false, standardStride, 1, body)
            verify(Math.abs(left.kneeX) < 15,
                   "left contact knee at " + headings[i] + ": " + left.kneeX)
            verify(Math.abs(right.kneeX) < 15,
                   "right contact knee at " + headings[i] + ": " + right.kneeX)
            var yaw = body.pelvisYaw * Math.PI / 180
            var spineWorldX = Math.cos(yaw) * body.spineX
                    + Math.sin(yaw) * body.spineZ
            var spineWorldZ = -Math.sin(yaw) * body.spineX
                    + Math.cos(yaw) * body.spineZ
            closeTo(body.pelvisX + spineWorldX, 0, 0.0001)
            closeTo(body.pelvisZ + spineWorldZ, 0, 0.0001)
        }
    }

    function test_kneesOnlyFlexForward() {
        var phases = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
        for (var i = 0; i < phases.length; ++i) {
            var body = HumanGait.bodyPose("march.forward", phases[i], standardStride, 1)
            var left = HumanGait.legPose("march.forward", phases[i], true,
                                         standardStride, 1, body.pelvisY, body.pelvisZ)
            var right = HumanGait.legPose("march.forward", phases[i], false,
                                          standardStride, 1, body.pelvisY, body.pelvisZ)
            verify(isFinite(left.hipX) && isFinite(left.kneeX) && isFinite(left.footX))
            verify(isFinite(right.hipX) && isFinite(right.kneeX) && isFinite(right.footX))
            verify(left.kneeX <= 0.001)
            verify(right.kneeX <= 0.001)
            compare(left.kneeZ, 0)
            compare(right.kneeZ, 0)
        }
    }

    function test_upperBodyCancelsMostPelvisMotion() {
        var body = HumanGait.bodyPose("march.forward", 0.25, standardStride, 1)
        verify(body.pelvisY <= 0 && body.pelvisY > -0.06,
               "pelvis Y " + body.pelvisY)
        verify(Math.abs(body.pelvisY + body.spineLift) < 0.003,
               "pelvis Y " + body.pelvisY + ", spine lift " + body.spineLift)
        verify(Math.abs(body.spineYaw) < 2)
        verify(Math.abs(body.spineRoll) < 1)
    }

    function test_slideReusesForwardTechniqueUnderSquareShoulders() {
        var body = HumanGait.bodyPose("slide.right", 0.125, standardStride, 1)
        verify(Math.abs(body.pelvisYaw) >= 59 && Math.abs(body.pelvisYaw) <= 62)
        verify(Math.abs(body.pelvisYaw + body.spineYaw * 0.5) >= 29)
        verify(Math.abs(body.pelvisYaw + body.spineYaw * 0.5) <= 31)
        verify(Math.abs(body.pelvisYaw + body.spineYaw) < 0.01)

        var forwardContact = HumanGait.targetForDirection(0, 0, false,
                                                           standardStride, 1)
        var slideContact = HumanGait.targetForDirection(90, 0, false,
                                                         standardStride, 1)
        closeTo(slideContact.footPitch, forwardContact.footPitch, 0.0001)
        closeTo(slideContact.toePitch, forwardContact.toePitch, 0.0001)

        var directionalBody = HumanGait.directionalBodyPose(90, 0.125,
                                                              standardStride, 1)
        var leg = HumanGait.directionalLegPose(90, 0.125, true,
                                                standardStride, 1,
                                                directionalBody)
        verify(Math.abs(leg.hipZ) < 5)
        compare(leg.kneeZ, 0)
        verify(leg.kneeX <= 0)
    }

    function test_backwardIsLowStraightLegPlatformTechnique() {
        var forwardPassing = HumanGait.targetForLeg("march.forward", 0.15, true,
                                                    standardStride, 1)
        var backwardPassing = HumanGait.targetForLeg("march.backward", 0.15, true,
                                                     standardStride, 1)
        verify(backwardPassing.lift < forwardPassing.lift * 0.55)
        verify(backwardPassing.footPitch > 0)

        var body = HumanGait.bodyPose("march.backward", 0.25,
                                      standardStride, 1)
        var leg = HumanGait.legPose("march.backward", 0.25, true,
                                    standardStride, 1, body.pelvisY, body.pelvisZ)
        verify(leg.kneeX > -15)
        verify(Math.abs(body.spinePitch) < 0.25)
    }

    function test_backwardKeepsForefootLowAcrossCycle() {
        var phases = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
        for (var i = 0; i < phases.length; ++i) {
            var target = HumanGait.targetForLeg("march.backward", phases[i],
                                                false, standardStride, 1)
            verify(target.footPitch > 0.9)
            // The ankle rises just enough to rotate the visible +Z toe edge
            // onto the turf; the validator checks the shoe itself stays low.
            verify(target.lift < 0.03)
        }
    }

    function test_secondaryMotionRemainsDisciplined() {
        var body = HumanGait.bodyPose("march.forward", 0.33, standardStride, 1)
        verify(Math.abs(body.pelvisX) <= 0.0066)
        verify(Math.abs(body.pelvisYaw) <= 1.81)
        verify(Math.abs(body.pelvisRoll) <= 0.56)
        verify(Math.abs(body.spinePitch) <= 0.29)
        verify(Math.abs(body.headPitch - 10) <= 0.13)
    }

    function test_directionalWeightsMatchCardinalTechnique() {
        var forward = HumanGait.directionalWeights(0)
        var right = HumanGait.directionalWeights(90)
        var backward = HumanGait.directionalWeights(180)
        var left = HumanGait.directionalWeights(-90)
        closeTo(forward.forward, 1, 0.000001)
        closeTo(right.right, 1, 0.000001)
        closeTo(backward.backward, 1, 0.000001)
        closeTo(left.left, 1, 0.000001)
    }

    function test_rearDiagonalsUseBackwardTechnique() {
        closeTo(HumanGait.backwardTechniqueWeight(75), 0, 0.001)
        closeTo(HumanGait.backwardTechniqueWeight(90), 0, 0.001)
        closeTo(HumanGait.backwardTechniqueWeight(97.5), 0.5, 0.001)
        closeTo(HumanGait.backwardTechniqueWeight(105), 1, 0.001)
        closeTo(HumanGait.backwardTechniqueWeight(135), 1, 0.001)
        closeTo(HumanGait.backwardTechniqueWeight(-135), 1, 0.001)

        var rearDiagonal = HumanGait.targetForDirection(120, 0, false,
                                                         standardStride, 1)
        var backward = HumanGait.targetForLeg("march.backward", 0, false,
                                               standardStride, 1)
        closeTo(rearDiagonal.footPitch, backward.footPitch, 0.001)
    }

    function test_directionalPoseIsContinuousAcrossOldModeBoundary() {
        var beforeBody = HumanGait.directionalBodyPose(44.9, 0.31,
                                                        standardStride, 1)
        var afterBody = HumanGait.directionalBodyPose(45.1, 0.31,
                                                       standardStride, 1)
        var before = HumanGait.directionalLegPose(44.9, 0.31, true,
                                                   standardStride, 1, beforeBody)
        var after = HumanGait.directionalLegPose(45.1, 0.31, true,
                                                  standardStride, 1, afterBody)
        verify(Math.abs(before.hipX - after.hipX) < 0.2)
        verify(Math.abs(before.hipZ - after.hipZ) < 0.2)
        verify(Math.abs(before.kneeX - after.kneeX) < 0.2)
        verify(Math.abs(before.footX - after.footX) < 0.3)
    }

    function test_spatialTargetKeepsFullStrideAtEveryHeading() {
        var headings = [0, 30, 45, 60, 90, 120, 135, 180, -45, -90]
        for (var i = 0; i < headings.length; ++i) {
            var target = HumanGait.targetForDirection(headings[i], 0, false,
                                                       standardStride, 1)
            closeTo(Math.sqrt(target.x * target.x + target.z * target.z),
                    standardStride * 0.5, 0.000001)
            var body = HumanGait.directionalBodyPose(headings[i], 0.25,
                                                      standardStride, 1)
            var leg = HumanGait.directionalLegPose(headings[i], 0.25, false,
                                                    standardStride, 1, body)
            verify(leg.kneeX <= 0.001)
            compare(leg.kneeZ, 0)
        }
    }

    function test_diagonalSlideStillStabilizesShoulders() {
        var body = HumanGait.directionalBodyPose(45, 0.125,
                                                  standardStride, 1)
        verify(Math.abs(body.pelvisYaw + body.spineYaw) < 0.1)
        verify(Math.abs(body.pelvisRoll + body.spineRoll) < 0.1)
        verify(Math.abs(body.spinePitch) < 0.3)
    }
}
