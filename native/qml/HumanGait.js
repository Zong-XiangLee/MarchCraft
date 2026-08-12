.pragma library

// Deterministic two-count marching biomechanics. Distances are canonical
// metres and angles are degrees, so technique is independent of body height.
var thighLength = 0.39
var shinLength = 0.415
var standingHipHeight = 0.805

function clamp(value, low, high) { return Math.max(low, Math.min(high, value)) }
function mix(a, b, amount) { return a + (b - a) * amount }
function smoothStep(value) {
    value = clamp(value, 0, 1)
    return value * value * (3 - 2 * value)
}
function smootherStep(value) {
    value = clamp(value, 0, 1)
    return value * value * value * (value * (value * 6 - 15) + 10)
}
function wrap01(value) {
    value -= Math.floor(value)
    return value < 0 ? value + 1 : value
}

// The phrase begins left-foot-first: left recovers while right supports.
// Each leg's 0..0.5 interval is planted and 0.5..1 is recovery.
function legCycle(gaitPhase, leftSide) {
    return wrap01(gaitPhase + (leftSide ? 0.5 : 0.0))
}
function isLocomotion(mode) {
    return mode === "march.forward" || mode === "march.backward"
            || mode === "slide.left" || mode === "slide.right"
}

function footPitch(mode, cycle) {
    var stance = cycle < 0.5
    var t = stance ? cycle * 2 : (cycle - 0.5) * 2
    if (mode === "march.forward") {
        if (stance) {
            if (t < 0.22) return mix(15, 0, smootherStep(t / 0.22))
            if (t > 0.76) return mix(0, -10, smootherStep((t - 0.76) / 0.24))
            return 0
        }
        if (t < 0.28) return mix(-10, 1.5, smootherStep(t / 0.28))
        if (t < 0.72) return mix(1.5, 7, smootherStep((t - 0.28) / 0.44))
        return mix(7, 15, smootherStep((t - 0.72) / 0.28))
    }
    if (mode === "march.backward") {
        // Backward marching establishes the platform/ball first. It is not a
        // reversed heel-to-toe forward step.
        if (stance)
            return t < 0.28 ? mix(-7, -2, smootherStep(t / 0.28))
                            : mix(-2, -6, smootherStep((t - 0.28) / 0.72))
        return t < 0.55 ? mix(-6, 0.5, smootherStep(t / 0.55))
                        : mix(0.5, -7, smootherStep((t - 0.55) / 0.45))
    }
    return 0
}

function toePitch(mode, cycle) {
    var stance = cycle < 0.5
    var t = stance ? cycle * 2 : (cycle - 0.5) * 2
    if (mode === "march.forward" && stance && t > 0.72)
        return 10 * smootherStep((t - 0.72) / 0.28)
    if (mode === "march.backward" && stance)
        return 3 + 3 * smootherStep(t)
    return 0
}

function targetForLeg(mode, gaitPhase, leftSide, strideMeters, motionWeight) {
    var cycle = legCycle(gaitPhase, leftSide)
    var stance = cycle < 0.5
    var t = stance ? cycle * 2 : (cycle - 0.5) * 2
    var travel = stance ? t : smootherStep(t)
    var direction = mode === "march.backward" || mode === "slide.right" ? 1 : -1
    var along = direction * strideMeters * (stance ? 0.5 - travel : travel - 0.5)
    var lift = 0
    if (!stance) {
        var arc = Math.pow(Math.sin(Math.PI * t), 1.35)
        lift = arc * (mode === "march.backward" ? 0.020
                      : mode === "march.forward" ? 0.026 : 0.018)
    }
    var pitch = footPitch(mode, cycle)
    // Raising the ankle by the rotated sole's lowest extent keeps heel strike
    // and toe-off on the turf instead of rotating the shoe through it.
    var pitchRadians = pitch * Math.PI / 180
    var toeExtent = mode === "march.backward" ? 0.08 : 0.07
    lift += Math.max(0, Math.sin(pitchRadians) * 0.17,
                     -Math.sin(pitchRadians) * toeExtent)
    var sagittal = mode === "march.forward" || mode === "march.backward"
    return {
        stance: stance, progress: t,
        x: sagittal ? 0 : along * motionWeight,
        z: sagittal ? along * motionWeight : 0,
        lift: lift * motionWeight,
        footPitch: pitch * motionWeight,
        toePitch: toePitch(mode, cycle) * motionWeight
    }
}

function requiredPelvisDrop(leftTarget, rightTarget) {
    var reach = thighLength + shinLength - 0.001
    function availableVertical(target) {
        var horizontal = Math.sqrt(target.x * target.x + target.z * target.z)
        return Math.sqrt(Math.max(0, reach * reach - horizontal * horizontal)) + target.lift
    }
    // The support leg determines body height; the recovering leg is free to
    // flex instead of pulling the pelvis down toward its airborne target.
    var support = leftTarget.stance ? leftTarget : rightTarget
    return Math.min(0, availableVertical(support) - standingHipHeight)
}

function bodyPose(mode, gaitPhase, strideMeters, motionWeight) {
    if (!isLocomotion(mode) || motionWeight <= 0.0001)
        return { pelvisX: 0, pelvisY: 0, pelvisYaw: 0, pelvisRoll: 0,
                 spineYaw: 0, spineRoll: 0, spinePitch: 0,
                 spineLift: 0, headPitch: 0, breath: 0 }
    var left = targetForLeg(mode, gaitPhase, true, strideMeters, motionWeight)
    var right = targetForLeg(mode, gaitPhase, false, strideMeters, motionWeight)
    var rhythm = gaitPhase * Math.PI * 2
    var slideDirection = mode === "slide.right" ? 1 : mode === "slide.left" ? -1 : 0
    var pelvisDrop = requiredPelvisDrop(left, right) * motionWeight
    if (slideDirection !== 0) {
        var supportIsLeft = left.stance
        var supportTarget = supportIsLeft ? left : right
        var normalizedReach = strideMeters > 0.0001
                ? supportTarget.x / (strideMeters * 0.5) : 0
        var soleFactor
        if (slideDirection > 0)
            soleFactor = supportIsLeft ? 0.012 + 0.010 * normalizedReach
                                       : 0.026 + 0.009 * normalizedReach
        else
            soleFactor = supportIsLeft ? 0.025 - 0.010 * normalizedReach
                                       : 0.010 - 0.010 * normalizedReach
        pelvisDrop += Math.abs(supportTarget.x) * Math.max(0, soleFactor) * motionWeight
    }
    var pelvisYaw = slideDirection !== 0
            ? slideDirection * (8 + Math.sin(rhythm) * 1.5) * motionWeight
            : Math.sin(rhythm) * 1.8 * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.55 * motionWeight
    return {
        pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
        pelvisY: pelvisDrop,
        pelvisYaw: pelvisYaw,
        pelvisRoll: pelvisRoll,
        spineYaw: -pelvisYaw * (slideDirection !== 0 ? 0.92 : 0.72),
        spineRoll: -pelvisRoll * 0.88,
        spinePitch: Math.sin(rhythm + 0.18) * 0.28 * motionWeight,
        spineLift: -pelvisDrop * 0.88,
        headPitch: -Math.sin(rhythm + 0.18) * 0.16 * motionWeight,
        breath: Math.sin(rhythm * 0.5) * 0.0007 * motionWeight
    }
}

function sagittalIk(targetZ, lift, pelvisDrop) {
    var vertical = standingHipHeight + pelvisDrop - lift
    var distance = clamp(Math.sqrt(vertical * vertical + targetZ * targetZ),
                         0.08, thighLength + shinLength - 0.001)
    var targetAngle = Math.atan2(-targetZ, vertical)
    var hipOffset = Math.acos(clamp((thighLength * thighLength + distance * distance
                                     - shinLength * shinLength)
                                    / (2 * thighLength * distance), -1, 1))
    var interior = Math.acos(clamp((thighLength * thighLength + shinLength * shinLength
                                    - distance * distance)
                                   / (2 * thighLength * shinLength), -1, 1))
    return { hip: (targetAngle + hipOffset) * 180 / Math.PI,
             knee: (interior - Math.PI) * 180 / Math.PI }
}

function legPose(mode, gaitPhase, leftSide, strideMeters, motionWeight, pelvisDrop) {
    var target = targetForLeg(mode, gaitPhase, leftSide, strideMeters, motionWeight)
    if (mode === "march.forward" || mode === "march.backward") {
        var ik = sagittalIk(target.z, target.lift, pelvisDrop)
        return { hipX: ik.hip, hipZ: 0, kneeX: ik.knee, kneeZ: 0,
                 footX: target.footPitch - ik.hip - ik.knee, footZ: 0,
                 toeX: target.toePitch, planted: target.stance }
    }
    if (mode === "slide.left" || mode === "slide.right") {
        // Hip ab/adduction carries lateral travel. Knee flexion stays in the
        // anatomical forward plane, avoiding sideways-hinging knees.
        var roll = Math.asin(clamp(target.x / (thighLength + shinLength), -0.62, 0.62)) * 180 / Math.PI
        var bend = target.stance ? -2 : -7 * Math.sin(Math.PI * target.progress)
        return { hipX: -bend * 0.38, hipZ: roll,
                 kneeX: bend, kneeZ: -roll * 0.08,
                 footX: -bend * 0.62, footZ: -roll * 0.92,
                 toeX: 0, planted: target.stance }
    }
    return { hipX: 0, hipZ: 0, kneeX: 0, kneeZ: 0,
             footX: 0, footZ: 0, toeX: 0, planted: true }
}

function armPitch(legPoseValue) {
    return clamp(-legPoseValue.hipX * 0.075, -2.4, 2.4)
}
