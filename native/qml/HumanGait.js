.pragma library

// Deterministic two-count marching biomechanics. Distances are canonical
// metres and angles are degrees, so technique is independent of body height.
var thighLength = 0.39
var shinLength = 0.415
var standingHipHeight = 0.805
var attentionToeOutDegrees = 45.0
var attentionHeadPitchDegrees = 10.0
var attentionPelvisDropMeters = -0.00141
var slidePelvisFromFacingDegrees = 60.0
var sole0 = [-0.00077,-0.00475,-0.00215,-0.00578,-0.00154,0.00001,0.00001,0.00000,-0.00000,-0.00026,-0.00049,-0.00070,-0.00085,-0.00094,-0.00096,-0.00089,-0.00075,-0.00453,-0.00191,-0.00568,-0.00151,-0.00001,-0.00001,-0.00002,-0.00003,-0.00028,-0.00052,-0.00072,-0.00087,-0.00096,-0.00098,-0.00092]
var sole45 = [0.00176,0.00019,-0.00296,-0.00129,-0.00081,0.00036,0.00021,-0.00048,-0.00198,-0.00335,-0.00458,-0.00568,-0.00664,-0.00746,-0.00816,-0.00378,-0.00545,-0.00459,-0.00166,-0.01182,-0.00880,-0.00556,-0.00424,-0.00292,-0.00172,0.00109,0.00029,0.00032,0.00048,0.00069,0.00097,0.00132]
var sole90 = [-0.00736,-0.00617,-0.01086,-0.02503,-0.01844,-0.01313,-0.01114,-0.00835,-0.00698,-0.00473,-0.00246,-0.00009,-0.00000,0.00007,0.00013,0.00018,0.00023,-0.00233,-0.00160,-0.00217,-0.00036,-0.00029,-0.00239,-0.00455,-0.00658,-0.00900,-0.01144,-0.01408,-0.01696,-0.02007,-0.02342,-0.00876]
var sole135 = [-0.01649,-0.00603,-0.01249,-0.01679,-0.01032,-0.00789,-0.00606,-0.00420,-0.00229,-0.00041,-0.00050,-0.00059,-0.00068,-0.00075,-0.00081,-0.00086,-0.00083,-0.00073,-0.00050,-0.00020,-0.00058,-0.00060,-0.00056,-0.00048,-0.00207,-0.00415,-0.00635,-0.00868,-0.01117,-0.01382,-0.01665,-0.01967]
var sole180 = [-0.00061,-0.00058,-0.00058,-0.00060,-0.00062,-0.00062,-0.00063,-0.00064,-0.00065,-0.00067,-0.00071,-0.00077,-0.00084,-0.00093,-0.00106,-0.00121,-0.00071,-0.00069,-0.00069,-0.00070,-0.00072,-0.00073,-0.00073,-0.00074,-0.00075,-0.00078,-0.00082,-0.00087,-0.00094,-0.00104,-0.00116,-0.00131]
var sole225 = [-0.00096,-0.00086,-0.00059,-0.00021,-0.00053,-0.00055,-0.00051,-0.00042,-0.00213,-0.00428,-0.00653,-0.00890,-0.01140,-0.01403,-0.01681,-0.01973,-0.01635,-0.00576,-0.01217,-0.01648,-0.01007,-0.00766,-0.00587,-0.00407,-0.00223,-0.00047,-0.00056,-0.00065,-0.00073,-0.00080,-0.00087,-0.00092]
var sole270 = [0.00024,-0.00236,-0.00182,-0.00212,-0.00038,-0.00031,-0.00252,-0.00480,-0.00700,-0.00952,-0.01206,-0.01475,-0.01762,-0.02066,-0.02388,-0.00877,-0.00702,-0.00588,-0.01013,-0.02429,-0.01771,-0.01245,-0.01052,-0.00830,-0.00660,-0.00453,-0.00236,-0.00009,0.00000,0.00007,0.00013,0.00019]
var sole315 = [-0.00578,-0.00491,-0.00189,-0.01201,-0.00897,-0.00572,-0.00438,-0.00304,-0.00182,0.00102,0.00023,0.00035,0.00051,0.00072,0.00099,0.00134,0.00179,0.00024,-0.00272,-0.00116,-0.00078,0.00033,0.00019,-0.00040,-0.00188,-0.00323,-0.00445,-0.00555,-0.00651,-0.00735,-0.00808,-0.00409]
var halfStrideCorrections = [
    [0.00002,0.00052,-0.00000,-0.00483,-0.00018,-0.00000,0.00000,0.00000,0.00000,0.00006,0.00011,0.00014,0.00017,0.00017,0.00015,0.00010,0.00002,0.00034,-0.00000,-0.00483,-0.00018,-0.00000,0.00000,0.00000,-0.00000,0.00006,0.00011,0.00014,0.00017,0.00017,0.00015,0.00010],
    [-0.00112,-0.00165,0.00050,-0.00819,-0.00049,-0.00008,-0.00042,-0.00063,0.00000,0.00056,0.00106,0.00149,0.00186,0.00216,0.00736,-0.00009,0.00015,0.00014,-0.00347,-0.00166,0.00267,0.00196,0.00130,0.00062,-0.00000,-0.00227,0.00069,-0.00055,-0.00027,-0.00025,-0.00045,-0.00073],
    [0.00116,0.00091,-0.00253,0.01142,0.00473,0.00356,0.00239,0.00046,0.00000,-0.00075,-0.00228,-0.00363,-0.00238,-0.00110,0.00001,-0.00001,-0.00005,0.00019,-0.00020,-0.00987,-0.00387,-0.00308,-0.00215,-0.00111,0.00000,0.00117,0.00244,0.00382,0.00536,0.00707,0.02046,0.00402],
    [0.00924,-0.00084,0.00097,0.00909,0.00372,0.00319,0.00214,0.00108,-0.00000,-0.00101,-0.00000,-0.00000,0.00000,0.00001,0.00002,0.00005,0.00001,-0.00047,-0.00311,-0.00563,-0.00016,-0.00007,-0.00003,-0.00058,0.00000,0.00103,0.00212,0.00329,0.00457,0.00596,0.00750,0.00919],
    [-0.00002,-0.00004,-0.00004,-0.00002,-0.00001,-0.00000,-0.00000,-0.00000,-0.00000,-0.00006,-0.00011,-0.00014,-0.00016,-0.00016,-0.00013,-0.00007,-0.00002,-0.00004,-0.00004,-0.00002,-0.00001,-0.00000,-0.00000,-0.00000,0.00000,-0.00006,-0.00011,-0.00014,-0.00016,-0.00016,-0.00013,-0.00007],
    [0.00008,-0.00047,-0.00306,-0.00558,-0.00015,-0.00007,-0.00002,-0.00066,0.00000,0.00106,0.00219,0.00339,0.00468,0.00607,0.00759,0.00925,0.00924,-0.00090,0.00091,0.00891,0.00360,0.00309,0.00207,0.00104,0.00000,-0.00093,-0.00000,-0.00000,0.00000,0.00001,0.00003,0.00005],
    [-0.00005,0.00018,-0.00021,-0.01007,-0.00396,-0.00325,-0.00228,-0.00118,0.00000,0.00124,0.00257,0.00400,0.00557,0.00727,0.02071,0.00434,0.00115,0.00092,-0.00270,0.01083,0.00453,0.00337,0.00226,0.00085,0.00000,-0.00064,-0.00226,-0.00346,-0.00229,-0.00106,0.00002,-0.00001],
    [0.00015,0.00014,-0.00354,-0.00163,0.00271,0.00199,0.00132,0.00063,0.00000,-0.00228,0.00064,-0.00062,-0.00032,-0.00024,-0.00044,-0.00073,-0.00112,-0.00166,0.00050,-0.00816,-0.00043,-0.00008,-0.00033,-0.00063,-0.00000,0.00055,0.00104,0.00147,0.00183,0.00213,0.00711,-0.00008],
]
var extendedStrideCorrections = [
    [-0.00493,-0.00010,0.00000,0.00336,0.00044,0.00000,0.00000,-0.00000,0.00000,-0.00003,-0.00005,-0.00007,-0.00008,-0.00008,-0.00007,-0.00166,-0.00462,0.00000,0.00000,0.00336,0.00044,0.00000,0.00000,-0.00000,-0.00000,-0.00003,-0.00005,-0.00007,-0.00008,-0.00008,-0.00007,-0.00166],
    [-0.00869,-0.00661,-0.00043,0.00029,-0.00008,0.00007,0.00002,0.00029,0.00000,-0.00025,-0.00046,-0.00063,-0.00076,-0.00084,-0.00088,-0.00337,-0.00016,0.00006,-0.00006,-0.00095,-0.00086,-0.00089,-0.00060,-0.00028,-0.00000,0.00141,-0.00009,0.00005,0.00013,0.00026,0.00047,0.00063],
    [-0.01429,-0.00037,0.00324,0.00027,-0.00115,-0.00164,-0.00109,0.00000,0.00000,0.00050,0.00112,-0.00001,-0.00001,0.00000,0.00003,0.00011,-0.00044,-0.00075,0.00021,0.00193,0.00001,-0.00002,0.00097,0.00050,0.00000,-0.00053,-0.00112,-0.00179,-0.00258,-0.00350,-0.00459,-0.01437],
    [-0.01352,-0.00328,0.00417,0.00104,-0.00133,-0.00126,-0.00084,-0.00042,-0.00000,-0.00000,-0.00001,-0.00003,-0.00006,-0.00014,-0.00023,-0.00030,-0.00021,0.00010,0.00026,0.00015,0.00011,0.00004,0.00001,-0.00000,0.00000,-0.00041,-0.00086,-0.00133,-0.00188,-0.00250,-0.00324,-0.00410],
    [0.00002,0.00003,0.00003,0.00002,0.00001,0.00000,0.00000,-0.00001,-0.00000,0.00003,0.00005,0.00006,0.00007,0.00007,0.00006,0.00003,0.00002,0.00003,0.00003,0.00002,0.00001,0.00000,0.00000,-0.00001,0.00000,0.00003,0.00005,0.00006,0.00007,0.00007,0.00006,0.00003],
    [-0.00032,0.00005,0.00024,0.00014,0.00009,0.00004,0.00002,0.00000,0.00000,-0.00042,-0.00087,-0.00133,-0.00186,-0.00248,-0.00323,-0.00410,-0.01352,-0.00324,0.00423,0.00102,-0.00144,-0.00131,-0.00084,-0.00042,0.00000,0.00000,0.00000,-0.00003,-0.00006,-0.00015,-0.00024,-0.00043],
    [-0.00044,-0.00073,0.00022,0.00178,0.00001,-0.00002,0.00103,0.00053,0.00000,-0.00056,-0.00118,-0.00188,-0.00267,-0.00359,-0.00467,-0.01443,-0.01429,-0.00038,0.00332,0.00037,-0.00172,-0.00155,-0.00103,0.00000,0.00000,0.00052,0.00106,-0.00001,-0.00001,0.00000,0.00003,0.00011],
    [-0.00017,0.00006,-0.00007,-0.00102,-0.00087,-0.00090,-0.00061,-0.00029,0.00000,0.00135,-0.00000,0.00005,0.00013,0.00026,0.00046,0.00063,-0.00839,-0.00634,-0.00043,0.00028,-0.00008,0.00006,0.00002,0.00028,-0.00000,-0.00025,-0.00045,-0.00062,-0.00074,-0.00082,-0.00086,-0.00330],
]
var halfBackDiagonalCorrections = [
    [0.00029,0.00019,-0.00296,0.00001,-0.00197,-0.00144,-0.00096,-0.00046,-0.00000,0.00092,0.00177,0.00163,0.00194,0.00113,0.00001,0.00002,0.00002,0.00002,0.00003,0.00244,0.00194,0.00145,0.00097,0.00049,-0.00000,-0.00049,-0.00097,-0.00145,-0.00194,-0.00242,-0.00379,0.00024],
    [0.00095,0.00075,-0.00496,0.00001,-0.00321,-0.00228,-0.00150,-0.00063,0.00002,0.00136,0.00262,0.00274,0.00177,0.00062,-0.00069,-0.00075,-0.00077,-0.00010,0.00025,0.00403,0.00331,0.00221,0.00154,0.00082,-0.00008,-0.00074,-0.00153,-0.00231,-0.00308,-0.00386,-0.00686,0.00105],
    [-0.00080,-0.00010,0.00024,0.00406,0.00334,0.00226,0.00158,0.00084,-0.00005,-0.00075,-0.00155,-0.00234,-0.00312,-0.00389,-0.00690,0.00105,0.00095,0.00075,-0.00493,0.00002,-0.00318,-0.00226,-0.00149,-0.00072,0.00002,0.00134,0.00263,0.00261,0.00167,0.00055,-0.00071,-0.00078],
    [0.00002,0.00002,0.00003,0.00247,0.00197,0.00147,0.00098,0.00049,0.00000,-0.00049,-0.00099,-0.00148,-0.00196,-0.00245,-0.00382,0.00024,0.00029,0.00019,-0.00294,0.00001,-0.00195,-0.00143,-0.00095,-0.00047,0.00000,0.00090,0.00174,0.00145,0.00192,0.00109,0.00001,0.00002],
]
var extendedBackDiagonalCorrections = [
    [-0.01173,-0.00478,-0.00798,-0.00648,0.00021,-0.00363,-0.00220,0.00008,-0.00000,0.00164,0.00098,-0.00004,0.00012,0.00021,0.00029,-0.00138,-0.00216,-0.00170,-0.00141,-0.00086,-0.00032,-0.00021,0.00105,0.00116,-0.00000,-0.00098,-0.00197,-0.00279,-0.00363,-0.00442,-0.00523,-0.00979],
    [-0.01612,-0.00885,-0.01118,-0.00913,-0.00209,-0.00420,-0.00289,-0.00037,0.00041,0.00240,0.00108,-0.00011,-0.00021,-0.00043,-0.00023,-0.00266,-0.00405,-0.00197,-0.00150,-0.00079,-0.00016,-0.00023,0.00116,0.00230,0.00028,-0.00116,-0.00272,-0.00424,-0.00573,-0.00724,-0.00878,-0.01505],
    [-0.00412,-0.00200,-0.00153,-0.00079,-0.00016,-0.00021,0.00124,0.00234,0.00033,-0.00103,-0.00248,-0.00393,-0.00537,-0.00693,-0.00850,-0.01492,-0.01612,-0.00871,-0.01161,-0.00958,-0.00137,-0.00521,-0.00325,0.00017,0.00038,0.00223,0.00104,-0.00011,-0.00023,-0.00045,-0.00037,-0.00270],
    [-0.00222,-0.00175,-0.00145,-0.00087,-0.00032,-0.00021,0.00111,0.00118,0.00000,-0.00093,-0.00185,-0.00264,-0.00343,-0.00424,-0.00507,-0.00971,-0.01172,-0.00439,-0.00796,-0.00644,0.00018,-0.00371,-0.00237,-0.00001,0.00000,0.00152,0.00094,-0.00003,0.00012,0.00022,0.00031,-0.00140],
]
var residual105 = [0.00166,0.00064,0.00017,-0.00017,0,0.00022,-0.00010,-0.00030,0.00294,0.00168,0.00044,0.00005,0,-0.00041,-0.00112,-0.00197]
var residual120 = [0.00037,-0.00058,-0.00078,-0.00087,-0.00039,-0.00008,-0.00023,-0.00009,0.00363,0.00163,0.00034,-0.00022,-0.00036,-0.00108,-0.00187,-0.00266]
var residual135 = [-0.00094,-0.00040,-0.00029,-0.00014,0,0.00001,0.00005,0.00020,0.00046,0.00006,0.00003,0.00001,0,-0.00010,-0.00028,-0.00054]
var residual150 = [0.00032,0.00126,0.00086,0.00124,0.00089,0.00045,0.00027,0.00017,0.00013,0.00001,0.00001,0.00004,0.00006,0.00008,0.00010,0.00008,0.00040,0.00322,0.00222,0.00108,0.00042,0.00018,0.00007,0.00003,0.00014,0.00008,-0.00004,-0.00025,-0.00057,-0.00101,-0.00161,-0.00237]
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
            // Every count begins on a clearly presented heel. Roll through
            // heel, arch, ball and toe, then lift the heel for the transfer.
            // This mesh's visible toe extends along local +Z, opposite the
            // generated toe bone. Negative pitch raises that visible toe.
            if (t < 0.30) return mix(-32, 0, smootherStep(t / 0.30))
            // Stay completely flat until the opposite heel arrives. The
            // release belongs to the next recovery half-cycle so both shoes
            // exchange heel/arch/ball/toe support at the same time.
            return 0
        }
        // Begin from a completely flat old shoe as the opposite heel lands,
        // then release heel-to-toe while that arriving shoe rolls down.
        if (t < 0.30) return mix(0, 20, smootherStep(t / 0.30))
        if (t < 0.50) return mix(20, 0, smootherStep((t - 0.30) / 0.20))
        if (t < 0.76) return 0
        return mix(0, -32, smootherStep((t - 0.76) / 0.24))
    }
    if (mode === "march.backward") {
        // Backward marching keeps the platform close to the turf.  The ankle
        // stays slightly plantar-flexed instead of replaying heel-first
        // forward articulation in reverse.
        // Keep one calm forefoot platform instead of oscillating the ankle
        // through several pitch targets, which made backward legs chatter.
        if (stance) return 8
        return 8 - 2 * Math.sin(Math.PI * t)
    }
    return 0
}

function toePitch(mode, cycle) {
    // The generated toe bone points opposite the visible shoe. Keep it neutral
    // and articulate the actual heel/arch/ball/toe sequence at the ankle.
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
        if (mode === "march.forward") {
            // Controlled clearance peaks before the legs pass. At and after
            // the crossing point the shoe returns close to the turf so the
            // advancing leg can lengthen into the straight-leg silhouette.
            lift = t < 0.30
                    ? 0.018 * smootherStep(t / 0.30)
                    : t < 0.55
                      ? mix(0.018, 0.0015, smootherStep((t - 0.30) / 0.25))
                      : t < 0.72
                        ? mix(0.0015, 0, smootherStep((t - 0.55) / 0.17)) : 0
        } else {
            var arc = Math.pow(Math.sin(Math.PI * t), 1.35)
            lift = arc * (mode === "march.backward" ? 0.003 : 0.018)
        }
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
    // Let the support leg determine the pelvis adjustment. In particular, a
    // high-toe heel strike raises the ankle: allowing the pelvis to follow by
    // a few centimetres keeps that landing knee straight. The spine applies
    // the opposite lift, so head/chest height remains quiet. The recovering
    // leg may flex and must not force the newly landed support knee to buckle.
    var support = leftTarget.stance ? leftTarget : rightTarget
    var moving = leftTarget.stance ? rightTarget : leftTarget
    var supportAdjustment = clamp(availableVertical(support) - standingHipHeight, -0.06, 0.05)
    var landingAdjustment = clamp(availableVertical(moving) - standingHipHeight, -0.06, 0.05)
    // At the count boundary the old support shoe has just become the recovery
    // shoe, but it must remain completely flat while the opposite heel lands.
    // Transfer ownership to the new support as its sole rolls down. Keep that
    // support in charge through the rest of the count so it cannot float and
    // snap back to the turf while the next heel approaches.
    if (moving.progress < 0.30)
        return mix(landingAdjustment, supportAdjustment,
                   smootherStep(moving.progress / 0.30))
    return supportAdjustment
}

function bodyPose(mode, gaitPhase, strideMeters, motionWeight) {
    if (!isLocomotion(mode) || motionWeight <= 0.0001)
        return { pelvisX: 0, pelvisY: attentionPelvisDropMeters,
                 ikPelvisY: attentionPelvisDropMeters, pelvisYaw: 0, pelvisRoll: 0,
                 spineYaw: 0, spineRoll: 0, spinePitch: 1.6,
                 spineLift: -attentionPelvisDropMeters * 0.95,
                 headPitch: attentionHeadPitchDegrees, breath: 0 }
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
            ? slideDirection * (slidePelvisFromFacingDegrees
                                + Math.sin(rhythm) * 1.0) * motionWeight
            : Math.sin(rhythm) * 1.4 * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.38 * motionWeight
    return {
        pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
        pelvisY: pelvisDrop,
        ikPelvisY: pelvisDrop,
        pelvisYaw: pelvisYaw,
        pelvisRoll: pelvisRoll,
        spineYaw: -pelvisYaw * (slideDirection !== 0 ? 1.0 : 0.78),
        spineRoll: -pelvisRoll * 0.92,
        spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
        spineLift: -pelvisDrop * 0.95,
        headPitch: attentionHeadPitchDegrees
                   - Math.sin(rhythm + 0.18) * 0.12 * motionWeight,
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
    // Exclude ground calibration from the reach solve, then apply it afterward
    // to shoe and pelvis together. This grounds the lower body rigidly while
    // preventing the calibration from feeding back into hip/knee articulation.
    var soleCorrection = directionalSoleOffset(relativeDegrees, gaitPhase, strideMeters) * motionWeight
    left.lift -= soleCorrection
    right.lift -= soleCorrection
    var pelvisDrop = requiredPelvisDrop(left, right) * motionWeight + soleCorrection
    var rhythm = gaitPhase * Math.PI * 2
    var slideStrength = weights.right - weights.left
    var longitudinalWeight = weights.forward + weights.backward
    var pelvisYaw = (slideStrength * (slidePelvisFromFacingDegrees
                                     + Math.sin(rhythm) * 1.0)
                     + longitudinalWeight * Math.sin(rhythm) * 1.4) * motionWeight
    var pelvisRoll = -Math.cos(rhythm) * 0.38 * motionWeight
    return { pelvisX: Math.cos(rhythm) * 0.0065 * motionWeight,
             pelvisY: pelvisDrop, ikPelvisY: pelvisDrop,
             pelvisYaw: pelvisYaw, pelvisRoll: pelvisRoll,
             spineYaw: -pelvisYaw * mix(0.78, 1.0,
                                         smootherStep(Math.abs(slideStrength) * 2)),
             spineRoll: -pelvisRoll * 0.92,
             spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
             spineLift: -pelvisDrop * 0.95,
             headPitch: attentionHeadPitchDegrees
                        - Math.sin(rhythm + 0.18) * 0.12 * motionWeight,
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
        var crossingWeight = mode === "march.forward" && !target.stance
                ? 1 - smootherStep(Math.abs(target.progress - 0.5) / 0.20) : 0
        var inward = (leftSide ? 15.9 : -15.9) * crossingWeight
        return { hipX: hip, hipZ: inward, kneeX: knee, kneeZ: 0,
                 footX: target.footPitch - hip - knee, footY: 0, footZ: -inward * 0.8,
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
    var inward = leftSide ? 4.7 : -4.7
    return { hipX: 0, hipZ: inward, kneeX: -0.8, kneeZ: 0,
             footX: 0.8,
             footY: leftSide ? -attentionToeOutDegrees : attentionToeOutDegrees,
             footZ: -inward,
             toeX: 0, planted: true }
}

// The source rig has hand bones but no articulated fingers. Raise the hands to
// face height while the upper arms and forearms make the broad triangular
// "coat-hanger" set. The right hand sits slightly forward and wraps the left;
// the left remains closest to the face. Clavicles stay relaxed in QML.
function handSetPose(leftSide) {
    return { upperX: leftSide ? 79 : 71,
             upperY: leftSide ? -34 : 30,
             upperZ: leftSide ? 94 : -84,
             forearmX: leftSide ? 0 : -8,
             forearmY: leftSide ? 66 : -44,
             forearmZ: leftSide ? 117 : -112,
             handX: leftSide ? -12 : 18,
             handY: leftSide ? -18 : 22,
             handZ: leftSide ? -10 : 16 }
}

function applyClosingPose(pose, gaitPhase, leftSide, amount, closingLeftSide) {
    var blend = smootherStep(amount)
    if (blend <= 0) return pose
    var attention = attentionLegPose(leftSide)
    // Lock the closing gesture to the phrase's final foot. Re-evaluating the
    // recovery leg from a changing gait phase switched feet halfway through
    // the close and was the main source of end-of-phrase leg spasms.
    var recovering = leftSide === closingLeftSide
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
             pelvisY: mix(pose.pelvisY, attentionPelvisDropMeters, blend),
             ikPelvisY: mix(pose.ikPelvisY, attentionPelvisDropMeters, blend),
             pelvisYaw: mix(pose.pelvisYaw, 0, blend),
             pelvisRoll: mix(pose.pelvisRoll, 0, blend),
             spineYaw: mix(pose.spineYaw, 0, blend),
             spineRoll: mix(pose.spineRoll, 0, blend),
             spinePitch: mix(pose.spinePitch, 1.6, blend),
             spineLift: mix(pose.spineLift, -attentionPelvisDropMeters * 0.95, blend),
             headPitch: mix(pose.headPitch, attentionHeadPitchDegrees, blend),
             breath: pose.breath }
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
    var crossingWeight = !target.stance
            ? (1 - smootherStep(Math.abs(target.progress - 0.5) / 0.20)) * weights.forward : 0
    var inward = (leftSide ? 15.9 : -15.9) * crossingWeight
    var moving = { hipX: hip, hipZ: ik.hipZ + inward,
                   kneeX: knee, kneeZ: 0,
                   footX: target.footPitch - hip - knee,
                   footY: 0,
                   footZ: -ik.hipZ - inward * 0.8 - bodyPoseValue.pelvisRoll,
                   toeX: target.toePitch, planted: target.stance }
    var setBlend = 1 - smootherStep(motionWeight)
    if (setBlend <= 0.0001)
        return moving
    var attention = attentionLegPose(leftSide)
    return { hipX: mix(moving.hipX, attention.hipX, setBlend),
             hipZ: mix(moving.hipZ, attention.hipZ, setBlend),
             kneeX: mix(moving.kneeX, attention.kneeX, setBlend),
             kneeZ: mix(moving.kneeZ, attention.kneeZ, setBlend),
             footX: mix(moving.footX, attention.footX, setBlend),
             footY: mix(moving.footY, attention.footY, setBlend),
             footZ: mix(moving.footZ, attention.footZ, setBlend),
             toeX: mix(moving.toeX, attention.toeX, setBlend),
             planted: setBlend > 0.98 ? true : moving.planted }
}

function armPitch(legPoseValue) {
    return clamp(-legPoseValue.hipX * 0.075, -2.4, 2.4)
}
