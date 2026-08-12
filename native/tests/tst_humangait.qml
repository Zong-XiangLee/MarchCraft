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
        verify(forwardContact.footPitch > 10)
        verify(backwardContact.footPitch < -4)
    }

    function test_swingPassesCloseAndDeceleratesIntoContact() {
        // Right recovery is 0.5..1.0 of its leg cycle; gait .75 is passing.
        var passing = HumanGait.targetForLeg("march.forward", 0.75, false,
                                             standardStride, 1)
        closeTo(passing.z, 0, 0.00001)
        verify(passing.lift > 0.02)
        var earlyDelta = HumanGait.smootherStep(0.02) - HumanGait.smootherStep(0)
        var middleDelta = HumanGait.smootherStep(0.52) - HumanGait.smootherStep(0.50)
        var contactDelta = HumanGait.smootherStep(1) - HumanGait.smootherStep(0.98)
        verify(earlyDelta < middleDelta)
        verify(contactDelta < middleDelta)
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
        var body = HumanGait.bodyPose("march.forward", 0, standardStride, 1)
        verify(body.pelvisY < -0.005)
        verify(Math.abs(body.pelvisY + body.spineLift) < 0.002)
        verify(Math.abs(body.spineYaw) < 2)
        verify(Math.abs(body.spineRoll) < 1)
    }

    function test_slideKeepsShouldersSquareAndKneesForward() {
        var body = HumanGait.bodyPose("slide.right", 0.125, standardStride, 1)
        verify(Math.abs(body.pelvisYaw) >= 7)
        verify(Math.abs(body.pelvisYaw + body.spineYaw) < 1)
        var leg = HumanGait.legPose("slide.right", 0.125, true,
                                    standardStride, 1, body.pelvisY)
        verify(Math.abs(leg.hipZ) > 1)
        verify(Math.abs(leg.kneeZ) < Math.abs(leg.hipZ) * 0.1)
        verify(leg.kneeX <= 0)
    }

    function test_secondaryMotionRemainsDisciplined() {
        var body = HumanGait.bodyPose("march.forward", 0.33, standardStride, 1)
        verify(Math.abs(body.pelvisX) <= 0.0066)
        verify(Math.abs(body.pelvisYaw) <= 1.81)
        verify(Math.abs(body.pelvisRoll) <= 0.56)
        verify(Math.abs(body.spinePitch) <= 0.29)
        verify(Math.abs(body.headPitch) <= 0.17)
    }
}
