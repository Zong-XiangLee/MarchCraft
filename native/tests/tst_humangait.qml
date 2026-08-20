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
        closeTo(forwardContact.footPitch, 32, 0.001)
        closeTo(backwardContact.footPitch, -8, 0.001)
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
        verify(HumanGait.targetForLeg("march.forward", 0.499, false,
                                      standardStride, 1).footPitch < -11)
        closeTo(HumanGait.targetForLeg("march.forward", 0.75, false,
                                       standardStride, 1).footPitch, 0, 0.001)
        verify(HumanGait.targetForLeg("march.forward", 0.999, false,
                                      standardStride, 1).footPitch > 31)
    }

    function test_attentionAndTenduClose() {
        var leftAttention = HumanGait.attentionLegPose(true)
        var rightAttention = HumanGait.attentionLegPose(false)
        closeTo(leftAttention.footY, 45, 0.001)
        closeTo(rightAttention.footY, -45, 0.001)
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
        closeTo(closed.footY, 45, 0.001)
        closeTo(closed.toeX, 0, 0.001)
        verify(closed.planted)
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
        verify(leftArm.upperX > 65 && rightArm.upperX > 65)
        verify(leftArm.upperZ > 80 && rightArm.upperZ < -80)
        verify(leftArm.forearmZ > 105 && rightArm.forearmZ < -105)
        verify(leftArm.forearmY > 40 && rightArm.forearmY < -35)
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
        verify(passing.lift > 0 && passing.lift < 0.005)
        var earlyDelta = HumanGait.smootherStep(0.02) - HumanGait.smootherStep(0)
        var middleDelta = HumanGait.smootherStep(0.52) - HumanGait.smootherStep(0.50)
        var contactDelta = HumanGait.smootherStep(1) - HumanGait.smootherStep(0.98)
        verify(earlyDelta < middleDelta)
        verify(contactDelta < middleDelta)
    }

    function test_forwardKneeFlexesBeforeCrossThenLengthens() {
        function rightLegAt(phase) {
            var body = HumanGait.bodyPose("march.forward", phase,
                                          standardStride, 1)
            return HumanGait.legPose("march.forward", phase, false,
                                     standardStride, 1, body.pelvisY)
        }
        var beforeCross = rightLegAt(0.65)
        var atCross = rightLegAt(0.75)
        var forwardSwing = rightLegAt(0.85)
        var nextCount = rightLegAt(0)
        verify(beforeCross.kneeX < atCross.kneeX - 4)
        verify(Math.abs(atCross.kneeX) < 15)
        verify(Math.abs(forwardSwing.kneeX) < 8)
        verify(Math.abs(nextCount.kneeX) < 15)
    }

    function test_kneesOnlyFlexForward() {
        var phases = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
        for (var i = 0; i < phases.length; ++i) {
            var body = HumanGait.bodyPose("march.forward", phases[i], standardStride, 1)
            var left = HumanGait.legPose("march.forward", phases[i], true,
                                         standardStride, 1, body.pelvisY)
            var right = HumanGait.legPose("march.forward", phases[i], false,
                                          standardStride, 1, body.pelvisY)
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
        verify(backwardPassing.footPitch < 0)

        var body = HumanGait.bodyPose("march.backward", 0.25,
                                      standardStride, 1)
        var leg = HumanGait.legPose("march.backward", 0.25, true,
                                    standardStride, 1, body.pelvisY)
        verify(leg.kneeX > -15)
        verify(Math.abs(body.spinePitch) < 0.25)
    }

    function test_backwardKeepsForefootLowAcrossCycle() {
        var phases = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
        for (var i = 0; i < phases.length; ++i) {
            var target = HumanGait.targetForLeg("march.backward", phases[i],
                                                false, standardStride, 1)
            verify(target.footPitch < -0.9)
            verify(target.lift < 0.015)
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
