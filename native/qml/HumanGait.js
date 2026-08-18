.pragma library

// Deterministic two-count marching biomechanics. Distances are canonical
// metres and angles are degrees, so technique is independent of body height.
var thighLength = 0.39
var shinLength = 0.415
var standingHipHeight = 0.805
var sole0 = [-0.00133,-0.00103,-0.00038,-0.00006,0.00002,0.00002,0.00002,0.00001,-0.00001,-0.00036,-0.00068,-0.00096,-0.00118,-0.00103,0.00008,0.00133,-0.00150,-0.00118,-0.00046,-0.00009,0,0,-0.00001,-0.00001,-0.00003,-0.00038,-0.00071,-0.00099,-0.00120,-0.00101,0.00013,0.00142]
var sole45 = [-0.00056,-0.00040,-0.00039,-0.00002,0.00007,0.00008,0.00006,0.00003,-0.00007,-0.00055,-0.00090,-0.00109,-0.00109,-0.00145,-0.00092,-0.00049,-0.00125,-0.00100,-0.00227,-0.00126,-0.00069,-0.00033,-0.00011,-0.00003,-0.00003,-0.00012,-0.00025,-0.00037,-0.00044,-0.00124,-0.00106,-0.00095]
var sole90 = [0.00161,0.00149,0.00087,0.00060,0.00039,0.00024,0.00013,0.00005,-0.00019,-0.00030,-0.00029,-0.00016,-0.00005,-0.00002,0.00004,0.00060,0.00024,0.00097,-0.00034,-0.00020,-0.00006,-0.00004,-0.00005,-0.00005,-0.00011,-0.00036,-0.00070,-0.00112,-0.00162,-0.00217,-0.00275,0.00028]
var sole135 = [-0.00189,-0.00168,-0.00091,-0.00050,-0.00030,-0.00025,-0.00018,-0.00007,0.00016,0.00019,0.00021,0.00023,0.00025,0.00030,0.00038,0.00040,-0.00013,-0.00001,-0.00010,0.00003,0.00009,0.00011,0.00013,0.00016,0.00019,0.00013,-0.00063,-0.00116,-0.00177,-0.00244,-0.00313,0.00024]
var sole180 = [-0.00062,-0.00055,-0.00036,-0.00005,0.00024,0.00032,0.00040,0.00046,0.00048,0.00046,0.00041,0.00024,0.00015,0.00001,-0.00017,0.00043,-0.00054,-0.00048,-0.00030,0,0.00027,0.00036,0.00043,0.00049,0.00052,0.00051,0.00046,0.00028,0.00020,0.00007,-0.00010,0.00050]
var sole225 = [-0.00017,-0.00004,-0.00012,0.00002,0.00008,0.00010,0.00012,0.00014,0.00016,-0.00001,-0.00086,-0.00145,-0.00210,-0.00276,-0.00341,0.00018,-0.00185,-0.00164,-0.00089,-0.00048,-0.00021,-0.00006,0.00005,0.00008,0.00019,0.00022,0.00024,0.00026,0.00029,0.00034,0.00043,0.00044]
var sole270 = [0.00022,0.00100,-0.00029,-0.00016,-0.00005,-0.00005,-0.00006,-0.00004,-0.00019,-0.00045,-0.00081,-0.00125,-0.00175,-0.00231,-0.00287,0.00027,0.00158,0.00143,0.00079,0.00052,0.00032,0.00017,0.00008,0.00002,-0.00011,-0.00025,-0.00026,-0.00015,-0.00004,0,0.00006,0.00060]
var sole315 = [-0.00115,-0.00092,-0.00245,-0.00150,-0.00094,-0.00056,-0.00028,-0.00011,-0.00003,-0.00003,-0.00008,-0.00014,-0.00018,-0.00104,-0.00091,-0.00089,-0.00065,-0.00050,-0.00047,-0.00008,0.00002,0.00004,0.00002,0,-0.00006,-0.00046,-0.00074,-0.00088,-0.00085,-0.00120,-0.00069,-0.00032]
var residual105 = [0.00166,0.00064,0.00017,-0.00017,0,0.00022,-0.00010,-0.00030,0.00294,0.00168,0.00044,0.00005,0,-0.00041,-0.00112,-0.00197]
var residual120 = [0.00037,-0.00058,-0.00078,-0.00087,-0.00039,-0.00008,-0.00023,-0.00009,0.00363,0.00163,0.00034,-0.00022,-0.00036,-0.00108,-0.00187,-0.00266]
var residual135 = [-0.00094,-0.00040,-0.00029,-0.00014,0,0.00001,0.00005,0.00020,0.00046,0.00006,0.00003,0.00001,0,-0.00010,-0.00028,-0.00054]
var residual150 = [-0.00223,-0.00088,-0.00026,-0.00003,0.00001,0.00002,0.00039,0.00102,0.00039,-0.00015,-0.00005,0.00003,0.00001,-0.00021,-0.00042,-0.00088]
var residual165 = [-0.00117,-0.00043,-0.00014,-0.00002,0,0,0.00035,0.00082,0.00062,0.00020,0.00004,0.00001,0,-0.00009,-0.00019,-0.00052]
var residual195 = [0.00063,0.00020,0.00004,0.00001,0,-0.00014,-0.00019,-0.00051,-0.00116,-0.00043,-0.00014,-0.00002,0,0,0.00036,0.00081]
var residual210 = [0.00053,-0.00013,-0.00002,0.00003,0.00001,-0.00021,-0.00043,-0.00088,-0.00223,-0.00088,-0.00027,-0.00004,0.00001,0.00002,0.00039,0.00102]
var residual225 = [0.00056,0.00008,0.00004,0,0,-0.00013,-0.00035,-0.00060,-0.00094,-0.00041,-0.00013,-0.00010,0,0,0.00005,0.00021]
var residual240 = [0.00368,0.00162,0.00033,-0.00022,-0.00038,-0.00139,-0.00233,-0.00303,0.00035,-0.00008,-0.00019,-0.00047,-0.00036,-0.00011,-0.00023,-0.00010]
var residual255 = [0.00299,0.00170,0.00044,0.00005,0,-0.00057,-0.00138,-0.00219,0.00165,0.00067,0.00019,0.00002,0,0.00020,-0.00010,-0.00032]

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
function normalizeDegrees(value) {
    value = ((value + 180) % 360 + 360) % 360 - 180
    return value
}

// Blend adjacent technique families by travel direction. The normalized
// weights are continuous through diagonals, so a curved/follow path cannot
// snap from a forward pose to a slide pose at an arbitrary 45-degree line.
function directionalWeights(relativeDegrees) {
    var radians = normalizeDegrees(relativeDegrees) * Math.PI / 180
    var longitudinal = Math.abs(Math.cos(radians))
    var lateral = Math.abs(Math.sin(radians))
    var quadrantDegrees = Math.atan2(lateral, longitudinal) * 180 / Math.PI
    var lateralWeight = smootherStep((quadrantDegrees - 22.5) / 45)
    var longitudinalWeight = 1 - lateralWeight
    return { forward: Math.cos(radians) >= 0 ? longitudinalWeight : 0,
             backward: Math.cos(radians) < 0 ? longitudinalWeight : 0,
             left: Math.sin(radians) < 0 ? lateralWeight : 0,
             right: Math.sin(radians) >= 0 ? lateralWeight : 0 }
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

function soleLiftForPitch(pitch, toeExtent) {
    var radians = pitch * Math.PI / 180
    return Math.max(0, Math.sin(radians) * 0.17,
                    -Math.sin(radians) * toeExtent)
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
    var toeExtent = mode === "march.backward" ? 0.08 : 0.07
    lift += soleLiftForPitch(pitch, toeExtent)
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

function targetForDirection(relativeDegrees, gaitPhase, leftSide, strideMeters, motionWeight) {
    var cycle = legCycle(gaitPhase, leftSide)
    var stance = cycle < 0.5
    var t = stance ? cycle * 2 : (cycle - 0.5) * 2
    var travel = stance ? t : smootherStep(t)
    var offset = strideMeters * (stance ? 0.5 - travel : travel - 0.5) * motionWeight
    var radians = normalizeDegrees(relativeDegrees) * Math.PI / 180
    var weights = directionalWeights(relativeDegrees)
    var modes = ["march.forward", "march.backward", "slide.left", "slide.right"]
    var amounts = [weights.forward, weights.backward, weights.left, weights.right]
    var lift = 0
    var pitch = 0
    var toe = 0
    for (var index = 0; index < modes.length; ++index) {
        var technique = targetForLeg(modes[index], gaitPhase, leftSide,
                                     strideMeters, motionWeight)
        var techniqueToeExtent = modes[index] === "march.backward" ? 0.08 : 0.07
        lift += (technique.lift
                 - soleLiftForPitch(technique.footPitch, techniqueToeExtent)) * amounts[index]
        pitch += technique.footPitch * amounts[index]
        toe += technique.toePitch * amounts[index]
    }
    lift += soleLiftForPitch(pitch, mix(0.07, 0.08, weights.backward))
            + directionalSoleOffset(relativeDegrees, gaitPhase, strideMeters) * motionWeight
    return { stance: stance, progress: t,
             x: Math.sin(radians) * offset,
             z: -Math.cos(radians) * offset,
             lift: lift, footPitch: pitch, toePitch: toe }
}

function requiredPelvisDrop(leftTarget, rightTarget) {
    var reach = thighLength + shinLength - 0.001
    function availableVertical(target) {
        var horizontal = Math.sqrt(target.x * target.x + target.z * target.z)
        return Math.sqrt(Math.max(0, reach * reach - horizontal * horizontal)) + target.lift
    }
    // The support leg determines body height through most of the count. Near
    // toe departure and heel/platform arrival, both legs constrain the pelvis
    // so the support exchange remains continuous instead of popping vertically.
    var support = leftTarget.stance ? leftTarget : rightTarget
    var moving = leftTarget.stance ? rightTarget : leftTarget
    var supportDrop = Math.min(0, availableVertical(support) - standingHipHeight)
    var movingDrop = Math.min(0, availableVertical(moving) - standingHipHeight)
    var edgeDistance = Math.min(moving.progress, 1 - moving.progress)
    var doubleSupportWeight = 1 - smootherStep(edgeDistance / 0.12)
    return mix(supportDrop, Math.min(supportDrop, movingDrop), doubleSupportWeight)
}

function bodyPose(mode, gaitPhase, strideMeters, motionWeight) {
    if (!isLocomotion(mode) || motionWeight <= 0.0001)
        return { pelvisX: 0, pelvisY: 0, ikPelvisY: 0, pelvisYaw: 0, pelvisRoll: 0,
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
        if (slideDirection < 0)
            pelvisDrop += 0.00002 * clamp(strideMeters / 0.70, 0, 1) * motionWeight
    }
    var pelvisYaw = slideDirection !== 0
            ? slideDirection * (8 + Math.sin(rhythm) * 1.5) * motionWeight
            : Math.sin(rhythm) * 1.8 * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.55 * motionWeight
    return {
        pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
        pelvisY: pelvisDrop,
        ikPelvisY: pelvisDrop,
        pelvisYaw: pelvisYaw,
        pelvisRoll: pelvisRoll,
        spineYaw: -pelvisYaw * (slideDirection !== 0 ? 0.92 : 0.72),
        spineRoll: -pelvisRoll * 0.88,
        spinePitch: Math.sin(rhythm + 0.18) * 0.28 * motionWeight,
        spineLift: -pelvisDrop * 0.95,
        headPitch: -Math.sin(rhythm + 0.18) * 0.16 * motionWeight,
        breath: Math.sin(rhythm * 0.5) * 0.0007 * motionWeight
    }
}

function samplePeriodicTable(table, phase) {
    var position = wrap01(phase) * table.length
    var index = Math.floor(position) % table.length
    var next = (index + 1) % table.length
    return mix(table[index], table[next], position - Math.floor(position))
}

function directionalSoleOffset(relativeDegrees, gaitPhase, strideMeters) {
    var tables = [sole0, sole45, sole90, sole135, sole180, sole225, sole270, sole315]
    var angle = ((relativeDegrees % 360) + 360) % 360
    var quadrant = Math.floor(angle / 90) % 4
    var longitudinalTable = quadrant === 0 || quadrant === 3 ? sole0 : sole180
    var sideTable = quadrant < 2 ? sole90 : sole270
    var diagonalTable = tables[(quadrant * 2 + 1) % 8]
    var weights = directionalWeights(relativeDegrees)
    var lateralWeight = weights.left + weights.right
    var baseOffset = lateralWeight <= 0.5
            ? mix(samplePeriodicTable(longitudinalTable, gaitPhase),
                  samplePeriodicTable(diagonalTable, gaitPhase), lateralWeight * 2)
            : mix(samplePeriodicTable(diagonalTable, gaitPhase),
                  samplePeriodicTable(sideTable, gaitPhase), (lateralWeight - 0.5) * 2)
    var extendedWeight = clamp((strideMeters - 0.5715) / 0.1285, 0, 1)
    if (extendedWeight <= 0 || angle < 90 || angle > 270)
        return baseOffset
    var zeroResidual = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
    var residualTables = [zeroResidual, residual105, residual120, residual135,
                          residual150, residual165, zeroResidual, residual195,
                          residual210, residual225, residual240, residual255, zeroResidual]
    var residualPosition = (angle - 90) / 15
    var residualIndex = Math.min(11, Math.floor(residualPosition))
    var residual = mix(samplePeriodicTable(residualTables[residualIndex], gaitPhase),
                       samplePeriodicTable(residualTables[residualIndex + 1], gaitPhase),
                       residualPosition - residualIndex)
    return baseOffset + residual * extendedWeight
}

function directionalBodyPose(relativeDegrees, gaitPhase, strideMeters, motionWeight) {
    if (motionWeight <= 0.0001)
        return bodyPose("idle", gaitPhase, strideMeters, 0)
    var weights = directionalWeights(relativeDegrees)
    var left = targetForDirection(relativeDegrees, gaitPhase, true, strideMeters, motionWeight)
    var right = targetForDirection(relativeDegrees, gaitPhase, false, strideMeters, motionWeight)
    var pelvisDrop = requiredPelvisDrop(left, right) * motionWeight
    var rhythm = gaitPhase * Math.PI * 2
    var slideStrength = weights.right - weights.left
    var longitudinalWeight = weights.forward + weights.backward
    var pelvisYaw = (slideStrength * (8 + Math.sin(rhythm) * 1.5)
                     + longitudinalWeight * Math.sin(rhythm) * 1.8) * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.55 * motionWeight
    return { pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
             pelvisY: pelvisDrop, ikPelvisY: pelvisDrop,
             pelvisYaw: pelvisYaw, pelvisRoll: pelvisRoll,
             spineYaw: -pelvisYaw * (0.72 + 0.20 * Math.abs(slideStrength)),
             spineRoll: -pelvisRoll * 0.88,
             spinePitch: Math.sin(rhythm + 0.18) * 0.28 * motionWeight,
             spineLift: -pelvisDrop * 0.95,
             headPitch: -Math.sin(rhythm + 0.18) * 0.16 * motionWeight,
             breath: Math.sin(rhythm * 0.5) * 0.0007 * motionWeight }
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

function spatialIkVector(targetX, targetZ, vertical) {
    var planeVertical = Math.sqrt(vertical * vertical + targetX * targetX)
    var distance = clamp(Math.sqrt(planeVertical * planeVertical + targetZ * targetZ),
                         0.08, thighLength + shinLength - 0.001)
    var targetAngle = Math.atan2(-targetZ, planeVertical)
    var hipOffset = Math.acos(clamp((thighLength * thighLength + distance * distance
                                     - shinLength * shinLength)
                                    / (2 * thighLength * distance), -1, 1))
    var interior = Math.acos(clamp((thighLength * thighLength + shinLength * shinLength
                                    - distance * distance)
                                   / (2 * thighLength * shinLength), -1, 1))
    return { hipX: (targetAngle + hipOffset) * 180 / Math.PI,
             hipZ: Math.atan2(targetX, vertical) * 180 / Math.PI,
             kneeX: (interior - Math.PI) * 180 / Math.PI }
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

function directionalLegPose(relativeDegrees, gaitPhase, leftSide, strideMeters,
                            motionWeight, bodyPoseValue) {
    if (motionWeight <= 0.0001)
        return legPose("idle", gaitPhase, leftSide, strideMeters, 0, 0)
    var target = targetForDirection(relativeDegrees, gaitPhase, leftSide,
                                    strideMeters, motionWeight)
    var hipBindX = leftSide ? -0.105 : 0.105
    var worldX = hipBindX + target.x - bodyPoseValue.pelvisX
    var worldY = 0.075 + target.lift - (0.91 + bodyPoseValue.pelvisY)
    var worldZ = target.z
    var roll = bodyPoseValue.pelvisRoll * Math.PI / 180
    var yaw = bodyPoseValue.pelvisYaw * Math.PI / 180
    var rollX = Math.cos(roll) * worldX + Math.sin(roll) * worldY
    var rollY = -Math.sin(roll) * worldX + Math.cos(roll) * worldY
    var localX = Math.cos(yaw) * rollX - Math.sin(yaw) * worldZ - hipBindX
    var localZ = Math.sin(yaw) * rollX + Math.cos(yaw) * worldZ
    var localY = rollY + 0.03
    var vertical = -localY
    var horizontalSquared = localX * localX + localZ * localZ
    var reach = thighLength + shinLength - 0.001
    if (vertical * vertical + horizontalSquared > reach * reach)
        vertical = Math.sqrt(Math.max(0, reach * reach - horizontalSquared))
    var ik = spatialIkVector(localX, localZ, vertical)
    return { hipX: ik.hipX, hipZ: ik.hipZ,
             kneeX: ik.kneeX, kneeZ: 0,
             footX: target.footPitch - ik.hipX - ik.kneeX,
             footZ: -ik.hipZ - bodyPoseValue.pelvisRoll,
             planted: target.stance }
}

function armPitch(legPoseValue) {
    return clamp(-legPoseValue.hipX * 0.075, -2.4, 2.4)
}
