.pragma library

// Deterministic two-count marching biomechanics. Distances are canonical
// metres and angles are degrees, so technique is independent of body height.
var thighLength = 0.39
var shinLength = 0.415
var standingHipHeight = 0.805
var sole0 = [-0.00118,-0.00103,-0.00039,-0.00007,0.00001,0.00001,0.00001,0.00000,-0.00000,-0.00026,-0.00049,-0.00070,-0.00085,-0.00069,0.00044,0.00168,-0.00130,-0.00118,-0.00047,-0.00010,-0.00001,-0.00001,-0.00001,-0.00002,-0.00003,-0.00028,-0.00052,-0.00072,-0.00087,-0.00067,0.00049,0.00177]
var sole45 = [0.00148,0.00088,0.00077,0.00074,0.00060,0.00039,0.00023,-0.00098,-0.00257,-0.00401,-0.00532,-0.00649,-0.00754,-0.00857,-0.00874,-0.00493,-0.00143,-0.00207,-0.01241,-0.01001,-0.00764,-0.00621,-0.00485,-0.00344,-0.00215,-0.00096,0.00015,0.00034,0.00050,-0.00001,0.00099,0.00208]
var sole90 = [-0.00069,-0.01232,-0.02541,-0.02238,-0.01962,-0.01699,-0.01435,-0.01166,-0.00889,-0.00604,-0.00306,-0.00003,0.00010,0.00080,0.00201,0.00319,0.00136,0.00014,-0.00043,-0.00040,-0.00028,-0.00030,-0.00287,-0.00556,-0.00822,-0.01101,-0.01397,-0.01716,-0.02062,-0.02519,-0.02148,-0.01254]
var sole135 = [-0.00161,-0.00599,-0.01604,-0.01439,-0.01218,-0.00986,-0.00751,-0.00511,-0.00265,-0.00012,0.00009,0.00015,0.00021,0.00025,0.00028,0.00030,-0.00002,-0.00032,-0.00048,0.00023,0.00005,0.00000,-0.00003,-0.00004,-0.00218,-0.00466,-0.00727,-0.01003,-0.01294,-0.01603,-0.01928,-0.01443]
var sole180 = [-0.00068,-0.00063,-0.00052,-0.00020,-0.00012,-0.00008,-0.00003,0.00001,0.00003,0.00004,0.00004,-0.00008,-0.00018,-0.00027,-0.00039,0.00001,-0.00061,-0.00057,-0.00047,-0.00016,-0.00008,-0.00004,0.00000,0.00004,0.00007,0.00009,0.00009,-0.00004,-0.00013,-0.00023,-0.00034,0.00006]
var sole225 = [0.00007,-0.00027,-0.00047,0.00021,0.00003,-0.00002,-0.00005,-0.00007,-0.00247,-0.00510,-0.00784,-0.01069,-0.01363,-0.01667,-0.01981,-0.01485,-0.00172,-0.00580,-0.01562,-0.01376,-0.01150,-0.00921,-0.00694,-0.00466,-0.00235,0.00001,0.00011,0.00018,0.00023,0.00027,0.00031,0.00034]
var sole270 = [0.00152,0.00024,-0.00038,-0.00040,-0.00030,-0.00031,-0.00314,-0.00612,-0.00911,-0.01216,-0.01534,-0.01864,-0.02209,-0.02656,-0.02299,-0.01343,-0.00087,-0.01185,-0.02441,-0.02104,-0.01815,-0.01553,-0.01302,-0.01054,-0.00805,-0.00550,-0.00282,-0.00004,0.00009,0.00080,0.00205,0.00328]
var sole315 = [-0.00126,-0.00193,-0.01252,-0.01024,-0.00794,-0.00652,-0.00515,-0.00372,-0.00239,-0.00116,-0.00000,0.00037,0.00052,-0.00001,0.00097,0.00201,0.00128,0.00070,0.00066,0.00069,0.00056,0.00036,0.00020,-0.00077,-0.00232,-0.00374,-0.00503,-0.00621,-0.00727,-0.00832,-0.00852,-0.00477]
var halfStrideCorrections = [[0.00043,0.00000,0.00000,0.00017,0.00057,0.00000,0.00000,0.00016],[-0.00080,-0.00019,0.00000,0.00204,-0.00074,0.00286,0.00000,-0.00055],[-0.00083,0.00612,0.00000,-0.00304,-0.00109,-0.00248,0.00000,0.00644],[0.00157,0.00498,0.00000,0.00000,-0.00038,-0.00008,0.00000,0.00516],[-0.00002,-0.00001,0.00000,0.00019,-0.00003,-0.00001,0.00000,0.00019],[-0.00043,-0.00008,0.00000,0.00539,0.00158,0.00475,0.00000,0.00000],[-0.00124,-0.00267,0.00000,0.00688,-0.00086,0.00566,0.00000,-0.00283],[-0.00070,0.00291,0.00000,-0.00068,-0.00058,-0.00018,0.00000,0.00199]]
var extendedStrideCorrections = [[0.00040,0.00000,0.00000,-0.00008,0.00054,0.00000,0.00000,-0.00008],[0.00110,0.00016,0.00000,-0.00085,0.00064,-0.00131,0.00000,0.00010],[-0.00151,-0.00289,0.00000,-0.00004,0.00269,0.00009,0.00000,-0.00307],[-0.00458,-0.00206,0.00000,-0.00005,0.00050,0.00004,0.00000,-0.00215],[0.00002,0.00001,0.00000,0.00008,0.00002,0.00000,0.00000,0.00008],[0.00040,0.00003,0.00000,-0.00219,-0.00459,-0.00211,0.00000,-0.00005],[0.00273,0.00011,0.00000,-0.00327,-0.00157,-0.00267,0.00000,-0.00004],[0.00069,-0.00133,0.00000,0.00010,0.00134,0.00015,0.00000,-0.00082]]
var halfBackDiagonalCorrections = [[0.00054,-0.00202,0.00000,0.00206,0.00002,0.00205,0.00000,-0.00179],[0.00115,-0.00331,-0.00025,0.00283,0.00004,0.00234,-0.00022,-0.00149],[0.00004,0.00253,-0.00023,-0.00048,0.00115,-0.00330,-0.00025,0.00264],[0.00002,0.00208,0.00000,-0.00074,0.00054,-0.00200,0.00000,0.00203]]
var extendedBackDiagonalCorrections = [[-0.00065,-0.00518,0.00000,0.00012,-0.00666,-0.00041,0.00000,-0.00395],[0.00168,-0.00702,0.00014,0.00028,-0.00717,-0.00031,0.00014,-0.00656],[-0.00740,-0.00032,0.00015,-0.00623,0.00173,-0.00767,0.00011,0.00028],[-0.00690,-0.00042,0.00000,-0.00382,-0.00062,-0.00523,0.00000,0.00012]]
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
            // Establish the count with the heel, then place arch, ball and toe
            // onto the turf. The support foot stays whole-footed until weight
            // has transferred; forward marching never departs toe-only.
            if (t < 0.24) return mix(20, 0, smootherStep(t / 0.24))
            return 0
        }
        if (t < 0.24) return mix(0, 2, smootherStep(t / 0.24))
        if (t < 0.70) return mix(2, 8, smootherStep((t - 0.24) / 0.46))
        return mix(8, 20, smootherStep((t - 0.70) / 0.30))
    }
    if (mode === "march.backward") {
        // Backward marching keeps the platform close to the turf.  The ankle
        // stays slightly plantar-flexed instead of replaying heel-first
        // forward articulation in reverse.
        if (stance)
            return t < 0.24 ? mix(-5, -2, smootherStep(t / 0.24))
                            : mix(-2, -4, smootherStep((t - 0.24) / 0.76))
        return t < 0.55 ? mix(-4, -1, smootherStep(t / 0.55))
                        : mix(-1, -5, smootherStep((t - 0.55) / 0.45))
    }
    return 0
}

function toePitch(mode, cycle) {
    var stance = cycle < 0.5
    var t = stance ? cycle * 2 : (cycle - 0.5) * 2
    // Forward and slide technique articulates the whole foot at the ankle.
    // Keeping this joint neutral makes heel/arch/ball/toe support together.
    if (mode === "march.backward" && stance)
        return 2 + 2 * smootherStep(t)
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
        lift = arc * (mode === "march.backward" ? 0.005
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
    // Slides use the same heel-to-toe step as forward marching.  Direction is
    // supplied by the target path while the pelvis turns the leg chain into
    // that path.  Only genuinely backward travel blends to the low platform
    // technique.
    var forwardTechnique = targetForLeg("march.forward", gaitPhase, leftSide,
                                        strideMeters, motionWeight)
    var backwardTechnique = targetForLeg("march.backward", gaitPhase, leftSide,
                                         strideMeters, motionWeight)
    var backwardAmount = weights.backward
    var lift = mix(forwardTechnique.lift
                   - soleLiftForPitch(forwardTechnique.footPitch, 0.07),
                   backwardTechnique.lift
                   - soleLiftForPitch(backwardTechnique.footPitch, 0.08),
                   backwardAmount)
    var pitch = mix(forwardTechnique.footPitch, backwardTechnique.footPitch,
                    backwardAmount)
    var toe = mix(forwardTechnique.toePitch, backwardTechnique.toePitch,
                  backwardAmount)
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
            ? slideDirection * (68 + Math.sin(rhythm) * 2.0) * motionWeight
            : Math.sin(rhythm) * 1.4 * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.38 * motionWeight
    return {
        pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
        pelvisY: pelvisDrop,
        ikPelvisY: pelvisDrop,
        pelvisYaw: pelvisYaw,
        pelvisRoll: pelvisRoll,
        spineYaw: -pelvisYaw * (slideDirection !== 0 ? 0.985 : 0.78),
        spineRoll: -pelvisRoll * 0.92,
        spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
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
    function correctionFrom(tables) {
        var longitudinalCorrection = quadrant === 0 || quadrant === 3 ? tables[0] : tables[4]
        var sideCorrection = quadrant < 2 ? tables[2] : tables[6]
        var diagonalCorrection = tables[(quadrant * 2 + 1) % 8]
        return lateralWeight <= 0.5
                ? mix(samplePeriodicTable(longitudinalCorrection, gaitPhase),
                      samplePeriodicTable(diagonalCorrection, gaitPhase), lateralWeight * 2)
                : mix(samplePeriodicTable(diagonalCorrection, gaitPhase),
                      samplePeriodicTable(sideCorrection, gaitPhase), (lateralWeight - 0.5) * 2)
    }
    function backDiagonalCorrection(tables) {
        var zero = [0,0,0,0,0,0,0,0]
        var first
        var second
        var amount
        if (angle >= 90 && angle <= 135) {
            if (angle <= 105) { first = zero; second = tables[0]; amount = (angle - 90) / 15 }
            else if (angle <= 120) { first = tables[0]; second = tables[1]; amount = (angle - 105) / 15 }
            else { first = tables[1]; second = zero; amount = (angle - 120) / 15 }
        } else if (angle >= 225 && angle <= 270) {
            if (angle <= 240) { first = zero; second = tables[2]; amount = (angle - 225) / 15 }
            else if (angle <= 255) { first = tables[2]; second = tables[3]; amount = (angle - 240) / 15 }
            else { first = tables[3]; second = zero; amount = (angle - 255) / 15 }
        } else return 0
        return mix(samplePeriodicTable(first, gaitPhase), samplePeriodicTable(second, gaitPhase), amount)
    }
    if (strideMeters < 0.5715)
        baseOffset += (correctionFrom(halfStrideCorrections)
                       + backDiagonalCorrection(halfBackDiagonalCorrections))
                * clamp((0.5715 - strideMeters) / 0.28575, 0, 1)
    else if (strideMeters > 0.5715)
        baseOffset += (correctionFrom(extendedStrideCorrections)
                       + backDiagonalCorrection(extendedBackDiagonalCorrections))
                * clamp((strideMeters - 0.5715) / 0.1285, 0, 1)
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
    var pelvisYaw = (slideStrength * (68 + Math.sin(rhythm) * 2.0)
                     + longitudinalWeight * Math.sin(rhythm) * 1.4) * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.38 * motionWeight
    return { pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
             pelvisY: pelvisDrop, ikPelvisY: pelvisDrop,
             pelvisYaw: pelvisYaw, pelvisRoll: pelvisRoll,
             spineYaw: -pelvisYaw * mix(0.78, 0.985,
                                         smootherStep(Math.abs(slideStrength) * 2)),
             spineRoll: -pelvisRoll * 0.92,
             spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
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
        var minimumFlex = mode === "march.backward" ? -0.7 : -1.4
        var knee = Math.min(ik.knee, minimumFlex)
        var hip = ik.hip - (knee - ik.knee) * 0.5
        return { hipX: hip, hipZ: 0, kneeX: knee, kneeZ: 0,
                 footX: target.footPitch - hip - knee, footY: 0, footZ: 0,
                 toeX: target.toePitch, planted: target.stance }
    }
    if (mode === "slide.left" || mode === "slide.right") {
        // Hip ab/adduction carries lateral travel. Knee flexion stays in the
        // anatomical forward plane, avoiding sideways-hinging knees.
        var roll = Math.asin(clamp(target.x / (thighLength + shinLength), -0.62, 0.62)) * 180 / Math.PI
        var bend = target.stance ? -2 : -7 * Math.sin(Math.PI * target.progress)
        return { hipX: -bend * 0.38, hipZ: roll,
                 kneeX: bend, kneeZ: -roll * 0.08,
                 footX: -bend * 0.62, footY: 0, footZ: -roll * 0.92,
                 toeX: 0, planted: target.stance }
    }
    return attentionLegPose(leftSide)
}

function attentionLegPose(leftSide) {
    var inward = leftSide ? 5.5 : -5.5
    return { hipX: 0, hipZ: inward, kneeX: -0.8, kneeZ: 0,
             footX: 0.8, footY: leftSide ? 45 : -45, footZ: -inward,
             toeX: 0, planted: true }
}

function applyClosingPose(pose, gaitPhase, leftSide, amount) {
    var blend = smootherStep(amount)
    if (blend <= 0) return pose
    var attention = attentionLegPose(leftSide)
    var recovering = legCycle(gaitPhase, leftSide) >= 0.5
    // The closing foot reaches with a pointed toe, then returns to a flat,
    // turned-out attention position. Only the recovery foot performs tendu.
    var tendu = recovering ? -18 * Math.sin(Math.PI * clamp(amount, 0, 1)) : 0
    return { hipX: mix(pose.hipX, attention.hipX, blend),
             hipZ: mix(pose.hipZ, attention.hipZ, blend),
             kneeX: mix(pose.kneeX, attention.kneeX, blend),
             kneeZ: mix(pose.kneeZ, attention.kneeZ, blend),
             footX: mix(pose.footX, attention.footX, blend) + tendu,
             footY: mix(pose.footY || 0, attention.footY, blend),
             footZ: mix(pose.footZ, attention.footZ, blend),
             toeX: mix(pose.toeX, attention.toeX, blend),
             planted: amount >= 0.98 ? true : pose.planted }
}

function applyClosingBodyPose(pose, amount) {
    var blend = smootherStep(amount)
    return { pelvisX: mix(pose.pelvisX, 0, blend),
             pelvisY: mix(pose.pelvisY, 0, blend),
             ikPelvisY: mix(pose.ikPelvisY, 0, blend),
             pelvisYaw: mix(pose.pelvisYaw, 0, blend),
             pelvisRoll: mix(pose.pelvisRoll, 0, blend),
             spineYaw: mix(pose.spineYaw, 0, blend),
             spineRoll: mix(pose.spineRoll, 0, blend),
             spinePitch: mix(pose.spinePitch, 0, blend),
             spineLift: mix(pose.spineLift, 0, blend),
             headPitch: mix(pose.headPitch, 0, blend), breath: pose.breath }
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
    var weights = directionalWeights(relativeDegrees)
    var minimumFlex = mix(-1.4, -0.7, weights.backward)
    var knee = Math.min(ik.kneeX, minimumFlex)
    var hip = ik.hipX - (knee - ik.kneeX) * 0.5
    return { hipX: hip, hipZ: ik.hipZ,
             kneeX: knee, kneeZ: 0,
             footX: target.footPitch - hip - knee,
             footY: 0,
             footZ: -ik.hipZ - bodyPoseValue.pelvisRoll,
             toeX: target.toePitch, planted: target.stance }
}

function armPitch(legPoseValue) {
    return clamp(-legPoseValue.hipX * 0.075, -2.4, 2.4)
}
