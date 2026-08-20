.pragma library

// Deterministic two-count marching biomechanics. Distances are canonical
// metres and angles are degrees, so technique is independent of body height.
var thighLength = 0.39
var shinLength = 0.415
var standingHipHeight = 0.805
var attentionToeOutDegrees = 22.5
var attentionHeadPitchDegrees = 10.0
var attentionPelvisDropMeters = -0.00691
var slidePelvisFromFacingDegrees = 60.0
var sole0 = [-0.00241,-0.00184,-0.00076,-0.00027,0.00001,0.00001,0.00001,0.00000,-0.00000,-0.00026,-0.00049,-0.00070,-0.00085,-0.00094,-0.00125,-0.00121,-0.00245,-0.00204,-0.00088,-0.00031,-0.00001,-0.00001,-0.00001,-0.00002,-0.00003,-0.00028,-0.00052,-0.00072,-0.00087,-0.00096,-0.00128,-0.00137]
var sole45 = [-0.00081,-0.00038,0.00027,0.00053,0.00054,0.00036,0.00021,-0.00048,-0.00198,-0.00335,-0.00458,-0.00568,-0.00664,-0.00746,-0.00855,-0.00187,-0.01201,-0.00738,-0.01183,-0.00937,-0.00690,-0.00556,-0.00424,-0.00292,-0.00172,-0.00061,0.00019,0.00032,0.00048,0.00069,0.00075,0.00035]
var sole90 = [-0.02636,-0.01829,-0.01995,-0.01736,-0.01510,-0.01313,-0.01114,-0.00910,-0.00698,-0.00473,-0.00246,-0.00009,-0.00000,0.00007,-0.00094,-0.00034,-0.00088,-0.00074,-0.00071,-0.00056,-0.00029,-0.00029,-0.00239,-0.00455,-0.00658,-0.00900,-0.01144,-0.01408,-0.01696,-0.02007,-0.02367,-0.02015]
var sole135 = [-0.01791,-0.01091,-0.01343,-0.01206,-0.01019,-0.00820,-0.00620,-0.00415,-0.00206,0.00000,0.00007,0.00013,0.00017,0.00021,0.00035,-0.00027,-0.00005,-0.00019,-0.00032,0.00023,0.00005,-0.00000,-0.00003,-0.00005,-0.00175,-0.00395,-0.00627,-0.00871,-0.01129,-0.01401,-0.01677,-0.01229]
var sole180 = [-0.00068,-0.00063,-0.00052,-0.00020,-0.00012,-0.00008,-0.00003,0.00001,0.00003,0.00004,0.00004,-0.00008,-0.00018,-0.00027,-0.00039,0.00001,-0.00061,-0.00057,-0.00047,-0.00016,-0.00008,-0.00004,0.00000,0.00004,0.00007,0.00009,0.00009,-0.00004,-0.00013,-0.00023,-0.00034,0.00006]
var sole225 = [0.00004,-0.00012,-0.00029,0.00021,0.00003,-0.00003,-0.00006,-0.00007,-0.00189,-0.00417,-0.00655,-0.00904,-0.01164,-0.01434,-0.01703,-0.01235,-0.01802,-0.01087,-0.01325,-0.01175,-0.00985,-0.00788,-0.00591,-0.00393,-0.00191,0.00003,0.00010,0.00015,0.00020,0.00024,0.00038,-0.00021]
var sole270 = [-0.00065,-0.00056,-0.00061,-0.00053,-0.00030,-0.00031,-0.00252,-0.00480,-0.00700,-0.00952,-0.01206,-0.01475,-0.01762,-0.02066,-0.02411,-0.02026,-0.02660,-0.01820,-0.01955,-0.01675,-0.01442,-0.01245,-0.01052,-0.00859,-0.00660,-0.00453,-0.00236,-0.00009,0.00000,0.00007,-0.00097,-0.00021]
var sole315 = [-0.01223,-0.00713,-0.01182,-0.00947,-0.00706,-0.00572,-0.00438,-0.00304,-0.00182,-0.00068,0.00022,0.00035,0.00051,0.00072,0.00079,0.00052,-0.00058,-0.00060,0.00013,0.00047,0.00051,0.00033,0.00019,-0.00040,-0.00188,-0.00323,-0.00445,-0.00555,-0.00651,-0.00735,-0.00849,-0.00197]
var halfStrideCorrections = [
    [-0.00001,0.00000,-0.00000,-0.00000,-0.00000,-0.00000,0.00000,0.00000,0.00000,0.00006,0.00011,0.00014,0.00017,0.00017,0.00015,0.00011,-0.00001,0.00000,-0.00000,-0.00000,-0.00000,-0.00000,0.00000,0.00000,-0.00000,0.00006,0.00011,0.00014,0.00017,0.00017,0.00015,0.00042],
    [-0.00090,-0.00086,-0.00045,-0.00032,-0.00017,-0.00008,-0.00042,-0.00063,0.00000,0.00056,0.00106,0.00149,0.00186,0.00216,0.00235,0.00140,0.01093,0.00438,0.01057,0.00386,0.00270,0.00196,0.00130,0.00062,0.00000,-0.00057,-0.00088,-0.00055,-0.00027,-0.00025,-0.00049,-0.00052],
    [0.02468,0.01713,0.01969,0.00592,0.00472,0.00356,0.00239,0.00121,-0.00000,-0.00075,-0.00228,-0.00363,-0.00238,-0.00110,-0.00001,-0.00092,-0.00145,-0.00129,-0.00070,-0.00161,-0.00198,-0.00308,-0.00215,-0.00111,-0.00000,0.00117,0.00244,0.00382,0.00536,0.00707,0.00891,0.01993],
    [0.00887,0.00434,0.00642,0.00535,0.00427,0.00320,0.00215,0.00108,-0.00000,-0.00101,0.00000,0.00000,0.00000,0.00000,-0.00001,-0.00057,-0.00094,-0.00074,-0.00047,-0.00017,-0.00008,-0.00004,-0.00001,-0.00058,-0.00000,0.00103,0.00212,0.00328,0.00455,0.00592,0.00741,0.00494],
    [-0.00003,-0.00004,-0.00002,-0.00002,-0.00001,-0.00001,-0.00000,-0.00000,0.00000,-0.00000,-0.00000,0.00011,0.00018,-0.00016,-0.00013,-0.00013,-0.00003,-0.00004,-0.00002,-0.00002,-0.00001,-0.00001,-0.00000,-0.00000,0.00000,-0.00000,-0.00000,0.00011,0.00018,-0.00016,-0.00013,-0.00013],
    [-0.00093,-0.00072,-0.00046,-0.00017,-0.00008,-0.00004,-0.00001,-0.00066,-0.00000,0.00106,0.00219,0.00339,0.00467,0.00604,0.00751,0.00500,0.00887,0.00426,0.00632,0.00523,0.00416,0.00310,0.00207,0.00104,-0.00000,-0.00093,0.00000,0.00000,0.00000,0.00000,-0.00001,-0.00070],
    [-0.00148,-0.00129,-0.00069,-0.00165,-0.00208,-0.00325,-0.00228,-0.00118,0.00000,0.00124,0.00257,0.00400,0.00557,0.00727,0.00907,0.02009,0.02513,0.01682,0.01942,0.00570,0.00450,0.00337,0.00226,0.00114,-0.00000,-0.00064,-0.00226,-0.00346,-0.00229,-0.00106,-0.00001,-0.00091],
    [0.01094,0.00417,0.01068,0.00390,0.00273,0.00199,0.00132,0.00063,-0.00000,-0.00058,-0.00097,-0.00062,-0.00032,-0.00024,-0.00049,-0.00083,-0.00098,-0.00085,-0.00044,-0.00031,-0.00017,-0.00008,-0.00033,-0.00063,-0.00000,0.00055,0.00104,0.00147,0.00183,0.00213,0.00232,0.00135]
]
var extendedStrideCorrections = [
    [0.00001,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00000,0.00000,-0.00003,-0.00005,-0.00007,-0.00008,-0.00008,-0.00007,-0.00001,-0.00016,0.00000,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00000,-0.00000,-0.00003,-0.00005,-0.00007,-0.00008,-0.00008,-0.00007,-0.00001],
    [0.00084,0.00050,0.00035,0.00024,0.00013,0.00007,0.00002,0.00029,0.00000,-0.00025,-0.00046,-0.00063,-0.00076,-0.00084,-0.00084,0.00216,-0.00101,-0.00394,-0.00260,-0.00195,-0.00122,-0.00089,-0.00060,-0.00028,0.00000,0.00026,0.00001,0.00005,0.00013,0.00026,0.00051,0.00106],
    [-0.00506,-0.00113,-0.00346,-0.00282,-0.00221,-0.00164,-0.00109,-0.00055,-0.00000,0.00050,0.00112,-0.00001,-0.00001,0.00000,-0.00009,0.00157,0.00182,0.00143,0.00097,0.00059,0.00017,-0.00002,0.00097,0.00050,-0.00000,-0.00053,-0.00112,-0.00179,-0.00258,-0.00350,-0.00465,-0.00298],
    [-0.00371,-0.00009,-0.00275,-0.00225,-0.00173,-0.00127,-0.00084,-0.00042,-0.00000,-0.00000,-0.00001,-0.00003,-0.00005,-0.00012,-0.00017,0.00048,0.00055,0.00052,0.00038,0.00011,0.00004,0.00001,0.00000,-0.00000,-0.00000,-0.00041,-0.00086,-0.00133,-0.00186,-0.00244,-0.00312,-0.00103],
    [0.00002,0.00003,0.00002,0.00001,0.00001,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00001,0.00006,0.00007,0.00007,0.00006,-0.00001,0.00002,0.00003,0.00002,0.00001,0.00001,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00001,0.00006,0.00007,0.00007,0.00006,-0.00001],
    [0.00045,0.00045,0.00035,0.00010,0.00003,0.00001,0.00001,0.00000,-0.00000,-0.00042,-0.00087,-0.00132,-0.00185,-0.00243,-0.00311,-0.00103,-0.00372,-0.00006,-0.00269,-0.00227,-0.00184,-0.00132,-0.00085,-0.00042,-0.00000,0.00000,-0.00000,-0.00003,-0.00005,-0.00012,-0.00018,0.00044],
    [0.00180,0.00141,0.00096,0.00058,0.00019,-0.00002,0.00103,0.00053,0.00000,-0.00056,-0.00118,-0.00188,-0.00267,-0.00359,-0.00472,-0.00302,-0.00507,-0.00106,-0.00336,-0.00270,-0.00210,-0.00155,-0.00103,-0.00051,-0.00000,0.00052,0.00106,-0.00001,-0.00001,0.00000,-0.00009,0.00156],
    [-0.00101,-0.00402,-0.00261,-0.00196,-0.00124,-0.00090,-0.00061,-0.00029,-0.00000,0.00026,0.00001,0.00005,0.00013,0.00026,0.00051,0.00106,0.00083,0.00049,0.00034,0.00023,0.00013,0.00006,0.00002,0.00028,-0.00000,-0.00025,-0.00045,-0.00062,-0.00074,-0.00082,-0.00082,0.00217]
]
var halfBackDiagonalCorrections = [
    [0.00014,0.00024,0.00042,-0.00243,-0.00194,-0.00144,-0.00096,-0.00048,-0.00000,0.00092,0.00177,0.00163,0.00194,0.00113,0.00000,-0.00025,-0.00035,-0.00028,-0.00011,0.00101,0.00194,0.00145,0.00097,0.00049,0.00000,-0.00049,-0.00097,-0.00145,-0.00194,-0.00242,-0.00293,0.00026],
    [0.00121,0.00108,0.00227,-0.00376,-0.00325,-0.00248,-0.00172,-0.00098,-0.00026,0.00136,0.00283,0.00295,0.00225,0.00107,0.00046,-0.00045,-0.00047,-0.00047,-0.00022,0.00090,0.00189,0.00254,0.00168,0.00075,-0.00012,-0.00105,-0.00186,-0.00267,-0.00346,-0.00426,-0.00458,0.00057],
    [-0.00063,-0.00048,-0.00023,0.00093,0.00197,0.00257,0.00170,0.00074,-0.00012,-0.00106,-0.00188,-0.00269,-0.00349,-0.00429,-0.00461,0.00057,0.00081,0.00107,0.00074,-0.00375,-0.00323,-0.00246,-0.00171,-0.00098,-0.00021,0.00133,0.00278,0.00277,0.00216,0.00103,0.00045,-0.00050],
    [-0.00038,-0.00028,-0.00012,0.00104,0.00197,0.00147,0.00098,0.00049,0.00000,-0.00049,-0.00099,-0.00148,-0.00196,-0.00245,-0.00295,0.00001,0.00000,0.00023,0.00612,-0.00241,-0.00192,-0.00143,-0.00095,-0.00047,-0.00000,0.00090,0.00174,0.00145,0.00192,0.00109,0.00000,-0.00025]
]
var extendedBackDiagonalCorrections = [
    [-0.01332,-0.01082,-0.00860,-0.00678,-0.00515,-0.00363,-0.00220,-0.00109,-0.00000,0.00164,0.00098,-0.00004,0.00012,0.00021,0.00032,-0.00217,-0.00449,-0.00359,-0.00232,-0.00138,-0.00046,-0.00021,0.00105,0.00116,0.00000,-0.00098,-0.00197,-0.00279,-0.00363,-0.00442,-0.00516,-0.00817],
    [-0.01805,-0.01469,-0.01173,-0.00919,-0.00716,-0.00508,-0.00309,-0.00146,0.00013,0.00221,0.00118,0.00019,0.00027,0.00020,0.00007,-0.00408,-0.00677,-0.00515,-0.00304,-0.00177,-0.00034,0.00001,0.00125,0.00222,0.00024,-0.00147,-0.00305,-0.00458,-0.00611,-0.00765,-0.00880,-0.01228],
    [-0.00682,-0.00518,-0.00305,-0.00179,-0.00036,0.00001,0.00131,0.00223,0.00026,-0.00133,-0.00279,-0.00428,-0.00575,-0.00733,-0.00855,-0.01218,-0.01800,-0.01493,-0.01223,-0.00973,-0.00774,-0.00555,-0.00347,-0.00166,0.00015,0.00204,0.00117,0.00021,0.00027,0.00021,0.00008,-0.00406],
    [-0.00454,-0.00364,-0.00237,-0.00141,-0.00048,-0.00021,0.00111,0.00118,0.00000,-0.00093,-0.00185,-0.00264,-0.00343,-0.00424,-0.00501,-0.00810,-0.01329,-0.01082,-0.00861,-0.00678,-0.00513,-0.00371,-0.00237,-0.00117,-0.00000,0.00152,0.00094,-0.00003,0.00012,0.00022,0.00034,-0.00215]
]
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
            // onto the turf. It remains whole-footed through support, then the
            // heel rolls up only during the final transfer into the next count.
            if (t < 0.24) return mix(20, 0, smootherStep(t / 0.24))
            if (t < 0.84) return 0
            return mix(0, 20, smootherStep((t - 0.84) / 0.16))
        }
        // Roll off the rear platform before the feet cross, return the ankle
        // to neutral through the forward swing, then present the heel only as
        // double support begins. Delaying heel presentation keeps the forward
        // leg visually straight instead of carrying a pedestrian bent knee.
        if (t < 0.22) return mix(20, 4, smootherStep(t / 0.22))
        if (t < 0.46) return mix(4, 0, smootherStep((t - 0.22) / 0.24))
        if (t < 0.86) return 0
        return mix(0, 20, smootherStep((t - 0.86) / 0.14))
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
            lift = arc * (mode === "march.backward" ? 0.005 : 0.018)
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
    var pelvisDrop = requiredPelvisDrop(left, right) * motionWeight
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
    var inward = leftSide ? 7.5 : -7.5
    return { hipX: 0, hipZ: inward, kneeX: -0.8, kneeZ: 0,
             footX: 0.8,
             footY: leftSide ? attentionToeOutDegrees : -attentionToeOutDegrees,
             footZ: -inward,
             toeX: 0, planted: true }
}

// The source rig has hand bones but no articulated fingers. This keeps the
// elbows relaxed at the sides, bends both forearms to roughly 90 degrees, and
// overlaps the hands at center-front. Whole-hand rotations suggest a right
// fist covered by the left hand without lifting either shoulder.
function handSetPose(leftSide) {
    return { upperX: -2.0, upperY: 0,
             upperZ: leftSide ? 50 : -50,
             forearmX: 34, forearmY: 0,
             forearmZ: leftSide ? 98 : -98,
             handX: leftSide ? -8 : 8,
             handY: leftSide ? -18 : 18,
             handZ: leftSide ? -12 : 12 }
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
    var moving = { hipX: hip, hipZ: ik.hipZ,
                   kneeX: knee, kneeZ: 0,
                   footX: target.footPitch - hip - knee,
                   footY: 0,
                   footZ: -ik.hipZ - bodyPoseValue.pelvisRoll,
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
