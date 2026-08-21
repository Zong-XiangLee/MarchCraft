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
var sole0 = [0.00806,0.00797,0.00470,-0.00213,-0.00126,0.00002,0.00001,-0.00000,-0.00002,-0.00028,-0.00053,-0.00074,-0.00086,-0.00071,-0.00070,0.00664,0.00806,0.00797,0.00470,-0.00213,-0.00126,0.00002,0.00001,-0.00000,-0.00002,-0.00028,-0.00053,-0.00074,-0.00086,-0.00071,-0.00070,0.00664]
var sole45 = [0.01321,0.01256,0.00828,0.00261,-0.00063,0.00015,0.00008,-0.00055,-0.00202,-0.00340,-0.00469,-0.00589,-0.00672,-0.00568,-0.00314,0.00616,0.00313,0.00793,0.00170,0.00241,0.00077,-0.00543,-0.00407,-0.00283,-0.00170,-0.00066,0.00008,0.00012,0.00019,0.00028,0.00270,0.01175]
var sole90 = [0.00135,0.01224,0.00391,-0.00224,-0.00279,-0.00609,-0.01104,-0.00931,-0.00714,-0.00487,-0.00251,-0.00023,-0.00029,-0.00035,-0.00047,0.01121,0.01327,0.01203,0.00788,-0.00091,-0.00150,-0.00018,-0.00220,-0.00438,-0.00667,-0.00910,-0.00306,-0.00347,-0.01341,-0.01460,-0.00736,0.00506]
var sole135 = [0.00678,0.00608,0.00216,-0.00198,-0.00352,-0.00223,-0.00050,0.00128,0.00311,0.00491,0.00474,0.00458,0.00443,0.00428,0.00407,0.00379,0.00463,0.00461,0.00459,0.00457,0.00460,0.00467,0.00479,0.00493,0.00340,0.00137,-0.00077,-0.00304,-0.00504,-0.00416,-0.00015,0.00412]
var sole180 = [0.00509,0.00510,0.00510,0.00511,0.00511,0.00511,0.00511,0.00510,0.00509,0.00506,0.00502,0.00496,0.00493,0.00497,0.00471,0.00443,0.00509,0.00510,0.00510,0.00511,0.00511,0.00511,0.00511,0.00510,0.00509,0.00506,0.00502,0.00496,0.00493,0.00497,0.00471,0.00443]
var sole225 = [0.00463,0.00459,0.00457,0.00455,0.00458,0.00466,0.00478,0.00493,0.00328,0.00118,-0.00102,-0.00333,-0.00533,-0.00436,-0.00021,0.00410,0.00678,0.00610,0.00225,-0.00177,-0.00322,-0.00193,-0.00024,0.00147,0.00323,0.00492,0.00475,0.00459,0.00444,0.00429,0.00407,0.00379]
var sole270 = [0.01327,0.01205,0.00790,-0.00087,-0.00150,-0.00018,-0.00233,-0.00466,-0.00709,-0.00965,-0.00316,-0.00394,-0.01410,-0.01507,-0.00794,0.00475,0.00135,0.01255,0.00406,-0.00227,-0.00279,-0.00610,-0.01074,-0.00876,-0.00672,-0.00460,-0.00238,-0.00023,-0.00028,-0.00035,-0.00047,0.01119]
var sole315 = [0.00313,0.00793,0.00167,0.00238,0.00071,-0.00559,-0.00422,-0.00297,-0.00181,-0.00075,0.00008,0.00012,0.00019,0.00028,0.00273,0.01176,0.01321,0.01255,0.00826,0.00266,-0.00063,0.00015,0.00008,-0.00045,-0.00190,-0.00326,-0.00454,-0.00573,-0.00658,-0.00558,-0.00311,0.00616]
var halfStrideCorrections = [
    [0.00248,0.00250,0.00292,0.00110,-0.00003,-0.00002,-0.00001,-0.00001,0.00000,0.00006,0.00012,0.00017,0.00016,-0.00000,0.00074,0.00254,0.00248,0.00250,0.00292,0.00110,-0.00003,-0.00002,-0.00001,-0.00001,0.00000,0.00006,0.00012,0.00017,0.00016,-0.00000,0.00074,0.00254],
    [0.00016,-0.00017,-0.00058,-0.00306,-0.00052,-0.00004,-0.00041,-0.00064,0.00000,0.00060,0.00115,0.00168,0.00205,0.00165,0.00067,0.00263,0.00719,0.00205,0.00477,-0.00294,-0.00207,0.00203,0.00129,0.00062,0.00000,-0.00057,-0.00088,-0.00052,-0.00005,-0.00011,0.00123,0.00017],
    [0.01699,0.00283,0.00146,0.00008,0.00005,0.00029,0.00202,0.00121,0.00000,-0.00124,-0.00253,-0.00367,-0.00233,-0.00081,0.00176,-0.00045,-0.00103,-0.00038,0.00016,0.00015,-0.00067,-0.00312,-0.00216,-0.00111,0.00000,0.00117,-0.00588,-0.00043,0.00853,0.00566,0.00787,0.00920],
    [-0.00068,-0.00002,0.00177,0.00350,0.00399,0.00317,0.00214,0.00108,0.00000,-0.00101,-0.00001,-0.00001,-0.00000,0.00003,0.00015,0.00016,-0.00035,-0.00030,-0.00024,-0.00015,-0.00008,-0.00004,-0.00002,-0.00058,-0.00000,0.00103,0.00211,0.00328,0.00434,0.00413,0.00250,0.00063],
    [-0.00000,0.00000,0.00001,0.00001,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00006,-0.00011,-0.00014,-0.00019,-0.00028,-0.00017,-0.00002,-0.00000,0.00000,0.00001,0.00001,0.00000,0.00000,0.00000,0.00000,0.00000,-0.00006,-0.00011,-0.00014,-0.00019,-0.00028,-0.00017,-0.00002],
    [-0.00035,-0.00029,-0.00023,-0.00014,-0.00008,-0.00004,-0.00002,-0.00066,0.00000,0.00106,0.00219,0.00338,0.00445,0.00421,0.00253,0.00065,-0.00068,-0.00004,0.00171,0.00340,0.00387,0.00306,0.00206,0.00104,0.00000,-0.00093,-0.00001,-0.00001,-0.00000,0.00004,0.00016,0.00016],
    [-0.00103,-0.00039,0.00014,0.00012,-0.00076,-0.00329,-0.00230,-0.00118,0.00000,0.00124,-0.00561,-0.00009,0.00888,0.00582,0.00833,0.00950,0.01699,0.00253,0.00143,0.00010,0.00004,0.00029,0.00224,0.00113,0.00000,-0.00117,-0.00240,-0.00350,-0.00224,-0.00079,0.00172,-0.00044],
    [0.00719,0.00205,0.00479,-0.00291,-0.00203,0.00206,0.00131,0.00063,0.00000,-0.00058,-0.00096,-0.00059,-0.00005,-0.00011,0.00122,0.00016,0.00016,-0.00016,-0.00055,-0.00307,-0.00052,-0.00004,-0.00032,-0.00063,0.00000,0.00059,0.00114,0.00165,0.00202,0.00162,0.00066,0.00263],
]
var extendedStrideCorrections = [
    [-0.02193,-0.00484,-0.00114,-0.00011,0.00001,0.00001,0.00001,0.00000,0.00000,-0.00003,-0.00005,-0.00008,-0.00007,0.00005,0.00004,-0.01187,-0.02193,-0.00484,-0.00114,-0.00011,0.00001,0.00001,0.00001,0.00000,0.00000,-0.00003,-0.00005,-0.00008,-0.00007,0.00005,0.00004,-0.01187],
    [-0.00961,-0.00011,0.00001,0.00198,0.00189,0.00003,0.00001,0.00029,0.00000,-0.00027,-0.00052,-0.00075,-0.00092,-0.00066,-0.00835,-0.02486,-0.03142,-0.01859,-0.00172,-0.00001,-0.00742,-0.00094,-0.00059,-0.00028,0.00000,0.00026,0.00000,0.00002,0.00006,0.00013,-0.00212,-0.00072],
    [-0.03216,-0.02619,-0.00342,-0.00024,-0.00002,-0.00003,-0.00012,0.00082,0.00000,0.00056,0.00114,0.00001,0.00000,-0.00003,-0.00012,-0.00013,-0.00294,0.00025,-0.00001,0.00004,0.00005,0.00002,0.00098,0.00050,0.00000,-0.00044,0.00137,-0.00208,-0.00564,-0.00573,-0.02012,-0.02676],
    [-0.02062,-0.00933,-0.00024,-0.00119,-0.00153,-0.00123,-0.00083,-0.00042,0.00000,-0.00000,-0.00001,-0.00003,-0.00006,-0.00018,-0.00039,-0.00030,-0.00009,0.00005,0.00017,0.00009,0.00004,0.00001,0.00000,-0.00000,-0.00000,-0.00041,-0.00086,-0.00132,-0.00176,-0.00155,-0.00073,-0.00927],
    [0.00000,-0.00000,-0.00001,-0.00001,-0.00000,-0.00000,-0.00000,-0.00001,0.00000,0.00003,0.00005,0.00007,0.00010,0.00010,0.00012,0.00001,0.00000,-0.00000,-0.00001,-0.00001,-0.00000,-0.00000,-0.00000,-0.00001,0.00000,0.00003,0.00005,0.00007,0.00010,0.00010,0.00012,0.00001],
    [-0.00019,-0.00002,0.00014,0.00007,0.00002,0.00001,0.00001,0.00000,0.00000,-0.00042,-0.00086,-0.00132,-0.00174,-0.00152,-0.00068,-0.00943,-0.02062,-0.00913,-0.00019,-0.00121,-0.00163,-0.00128,-0.00083,-0.00042,0.00000,0.00000,0.00000,-0.00002,-0.00006,-0.00019,-0.00040,-0.00034],
    [-0.00294,0.00029,0.00000,0.00006,0.00005,0.00002,0.00104,0.00053,0.00000,0.00034,0.00135,-0.00223,-0.00574,-0.00604,-0.02020,-0.02681,-0.03216,-0.02613,-0.00295,-0.00027,-0.00002,-0.00003,-0.00042,0.00027,0.00000,0.00053,0.00108,0.00001,0.00000,-0.00003,-0.00012,-0.00013],
    [-0.03142,-0.01865,-0.00172,-0.00007,-0.00757,-0.00096,-0.00060,-0.00028,0.00000,0.00026,0.00000,0.00002,0.00006,0.00013,-0.00216,-0.00071,-0.00961,-0.00011,0.00001,0.00201,0.00190,0.00003,0.00001,0.00028,0.00000,-0.00026,-0.00051,-0.00074,-0.00090,-0.00065,-0.00827,-0.02480],
]
var halfBackDiagonalCorrections = [
    [-0.01161,-0.00919,-0.00395,-0.00159,-0.00344,0.00032,0.00471,0.00497,0.00521,0.00544,0.00569,0.00594,0.00616,0.00494,0.00231,-0.00732,-0.00844,-0.00783,-0.00414,0.00473,0.00620,0.00594,0.00562,0.00533,0.00505,0.00477,0.00417,-0.00255,-0.00284,0.00334,-0.00065,-0.00937],
    [-0.01071,-0.00860,-0.00428,-0.00259,-0.00438,-0.00060,0.00380,0.00437,0.00491,0.00545,0.00598,0.00660,0.00609,0.00455,0.00211,-0.00678,-0.00795,-0.00737,-0.00392,0.00436,0.00582,0.00670,0.00601,0.00537,0.00478,0.00419,0.00330,-0.00327,-0.00383,0.00194,-0.00133,-0.00896],
    [-0.00795,-0.00738,-0.00394,0.00434,0.00589,0.00672,0.00602,0.00538,0.00478,0.00418,0.00265,-0.00367,-0.00400,0.00190,-0.00133,-0.00896,-0.01071,-0.00860,-0.00430,-0.00230,-0.00392,-0.00010,0.00380,0.00436,0.00490,0.00543,0.00597,0.00659,0.00602,0.00454,0.00215,-0.00677],
    [-0.00844,-0.00784,-0.00415,0.00472,0.00622,0.00595,0.00563,0.00533,0.00505,0.00476,0.00347,-0.00297,-0.00301,0.00331,-0.00064,-0.00937,-0.01161,-0.00919,-0.00397,-0.00127,-0.00294,0.00086,0.00470,0.00496,0.00520,0.00543,0.00567,0.00593,0.00615,0.00492,0.00235,-0.00731],
]
var extendedBackDiagonalCorrections = [
    [-0.00587,-0.00977,-0.01452,-0.01045,-0.01310,-0.00679,0.00207,0.00296,0.00521,0.00603,0.00569,0.00443,0.00440,0.00426,0.00402,-0.00858,-0.00821,-0.00965,-0.00470,0.00446,0.00553,0.00444,0.00579,0.00603,0.00505,0.00417,-0.00764,-0.00785,0.00221,0.00648,0.01418,-0.00156],
    [-0.01255,-0.01525,-0.01833,-0.01263,-0.01443,-0.00787,0.00111,0.00259,0.00530,0.00683,0.00553,0.00427,0.00415,0.00395,0.00386,-0.00907,-0.00971,-0.01051,-0.00533,0.00355,0.00490,0.00415,0.00562,0.00687,0.00514,0.00367,-0.00798,-0.00898,-0.00037,0.00310,0.00805,-0.00769],
    [-0.00976,-0.01059,-0.00539,0.00349,0.00489,0.00414,0.00567,0.00690,0.00516,0.00305,-0.00829,-0.00886,-0.00004,0.00335,0.00827,-0.00759,-0.01253,-0.01545,-0.01874,-0.01232,-0.01423,-0.00758,0.00140,0.00292,0.00526,0.00681,0.00551,0.00430,0.00417,0.00397,0.00388,-0.00907],
    [-0.00826,-0.00977,-0.00481,0.00435,0.00550,0.00442,0.00584,0.00604,0.00505,0.00342,-0.00812,-0.00788,0.00239,0.00691,0.01437,-0.00148,-0.00586,-0.00968,-0.01440,-0.00979,-0.01226,-0.00604,0.00263,0.00345,0.00520,0.00602,0.00565,0.00446,0.00441,0.00428,0.00403,-0.00854],
]
var residual105 = [0.00166,0.00064,0.00017,-0.00017,0,0.00022,-0.00010,-0.00030,0.00294,0.00168,0.00044,0.00005,0,-0.00041,-0.00112,-0.00197]
var residual120 = [0.00037,-0.00058,-0.00078,-0.00087,-0.00039,-0.00008,-0.00023,-0.00009,0.00363,0.00163,0.00034,-0.00022,-0.00036,-0.00108,-0.00187,-0.00266]
var residual135 = [-0.00094,-0.00040,-0.00029,-0.00014,0,0.00001,0.00005,0.00020,0.00046,0.00006,0.00003,0.00001,0,-0.00010,-0.00028,-0.00054]
var residual150 = [0.00253,0.00149,0.00077,0.00083,0.00077,0.00043,0.00025,0.00016,0.00012,-0.00001,0.00000,0.00003,-0.00001,-0.00016,-0.00013,0.00183,0.00270,0.00219,0.00152,0.00082,0.00037,0.00017,0.00006,0.00002,0.00013,0.00007,-0.00005,-0.00026,-0.00058,-0.00092,-0.00135,0.00146]
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

// A true slide is a narrow band around perpendicular travel. Once the path
// has a clear rearward component, use backward forefoot technique even when
// most of the displacement is lateral. The 15-degree blend prevents a curved
// path from switching ankle/knee families in a single frame.
function backwardTechniqueWeight(relativeDegrees) {
    var rearAngle = Math.abs(normalizeDegrees(relativeDegrees))
    // A square 90-degree slide still uses the forward heel roll. Blend into
    // backward platform technique immediately behind the performer so rear
    // side-diagonals cannot inherit the forward heel strike.
    return smootherStep((rearAngle - 90.0) / 15.0)
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
            // The conditioned visible shoe extends along local +Z, opposite
            // the helper toe bone. Negative pitch raises the visible toe.
            if (t < 0.30) return mix(-32, 0, smootherStep(t / 0.30))
            // Stay completely flat until the opposite heel arrives. The
            // release belongs to the next recovery half-cycle so both shoes
            // exchange heel/arch/ball/toe support at the same time.
            return 0
        }
        // Begin from a completely flat old shoe as the opposite heel lands,
        // then release heel-to-toe while that arriving shoe rolls down.
        if (t < 0.20) return mix(0, 16, smootherStep(t / 0.20))
        if (t < 0.50) return mix(16, 0, smootherStep((t - 0.20) / 0.30))
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
            lift = t < 0.20
                    ? 0.022 * smootherStep(t / 0.20)
                    : t < 0.55
                      ? mix(0.022, 0.003, smootherStep((t - 0.20) / 0.35))
                      : t < 0.72
                        ? mix(0.003, 0, smootherStep((t - 0.55) / 0.17)) : 0
        } else {
            var arc = Math.pow(Math.sin(Math.PI * t), 1.35)
            lift = arc * (mode === "march.backward" ? 0.003 : 0.018)
        }
    }
    var pitch = footPitch(mode, cycle)
    // Raising the ankle by the rotated sole's lowest extent keeps heel strike
    // and toe-off on the turf instead of rotating the shoe through it.
    // The conditioned low-poly shoe already places its heel below the ankle
    // pivot, so forward heel contact needs no additional whole-foot lift.
    var toeExtent = mode === "march.backward" ? 0.08 : 0.0
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
    var backwardAmount = backwardTechniqueWeight(relativeDegrees)
    var lift = mix(forwardTechnique.lift
                   - soleLiftForPitch(forwardTechnique.footPitch, 0.07),
                   backwardTechnique.lift
                   - soleLiftForPitch(backwardTechnique.footPitch, 0.08),
                   backwardAmount)
    var pitch = mix(forwardTechnique.footPitch, backwardTechnique.footPitch,
                    backwardAmount)
    var toe = mix(forwardTechnique.toePitch, backwardTechnique.toePitch,
                  backwardAmount)
    lift += soleLiftForPitch(pitch, mix(0.07, 0.08, backwardAmount))
            + directionalSoleOffset(relativeDegrees, gaitPhase, strideMeters) * motionWeight
    return { stance: stance, progress: t,
             x: Math.sin(radians) * offset,
             z: -Math.cos(radians) * offset,
             lift: lift, footPitch: pitch, toePitch: toe }
}

function pelvisTransferPose(leftTarget, rightTarget, pelvisYaw, spatial) {
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
    function effectivePoint(target, leftSide) {
        if (!spatial)
            return { x: target.x, z: target.z }
        var yaw = pelvisYaw * Math.PI / 180
    var hipBind = leftSide ? -0.105 : 0.105
        return { x: target.x + hipBind - Math.cos(yaw) * hipBind,
                 z: target.z + Math.sin(yaw) * hipBind }
    }
    var supportPoint = effectivePoint(support, support === leftTarget)
    var movingPoint = effectivePoint(moving, moving === leftTarget)
    var supportAdjustment = clamp(availableVertical(support) - standingHipHeight, -0.06, 0.05)
    // At the count boundary the old support shoe has just become the recovery
    // shoe, but it must remain completely flat while the opposite heel lands.
    // Transfer ownership to the new support as its sole rolls down. Keep that
    // support in charge through the rest of the count so it cannot float and
    // snap back to the turf while the next heel approaches.
    if (moving.progress >= 0.30 && moving.progress <= 0.70)
        return { x: 0, y: supportAdjustment, z: 0, weight: 0 }

    // While the old platform and arriving heel are both grounded, place the
    // hip at the upper intersection of two equal leg-reach spheres. This is
    // the weight-transfer position that lets both knees remain long instead
    // of collapsing the arriving thigh at the count boundary.
    var deltaX = supportPoint.x - movingPoint.x
    var deltaZ = supportPoint.z - movingPoint.z
    var horizontalDistance = Math.sqrt(deltaX * deltaX + deltaZ * deltaZ)
    var ankleHeightDelta = support.lift - moving.lift
    var ankleDistance = Math.sqrt(horizontalDistance * horizontalDistance
                                  + ankleHeightDelta * ankleHeightDelta)
    if (horizontalDistance < 0.0001 || ankleDistance >= reach * 2)
        return { x: 0, y: supportAdjustment, z: 0, weight: 0 }
    var sphereHeight = Math.sqrt(Math.max(0, reach * reach
                                         - ankleDistance * ankleDistance * 0.25))
    var distanceAlongGround = horizontalDistance * 0.5
            - ankleHeightDelta / ankleDistance * sphereHeight
    var hipHeight = (moving.lift + support.lift) * 0.5
            + horizontalDistance / ankleDistance * sphereHeight
    var transferWeight = moving.progress < 0.30
            ? 1 - smootherStep(moving.progress / 0.30)
            : smootherStep((moving.progress - 0.70) / 0.30)
    var intersectionX = movingPoint.x + deltaX / horizontalDistance * distanceAlongGround
    var intersectionZ = movingPoint.z + deltaZ / horizontalDistance * distanceAlongGround
    return { x: intersectionX * transferWeight,
             y: mix(supportAdjustment,
                    clamp(hipHeight - standingHipHeight, -0.06, 0.05),
                    transferWeight),
             z: intersectionZ * transferWeight,
             weight: transferWeight }
}

function bodyPose(mode, gaitPhase, strideMeters, motionWeight) {
    if (!isLocomotion(mode) || motionWeight <= 0.0001)
        return { pelvisX: 0, pelvisY: attentionPelvisDropMeters,
                 pelvisZ: 0, ikPelvisY: attentionPelvisDropMeters,
                 pelvisYaw: 0, pelvisRoll: 0,
                 spineYaw: 0, spineRoll: 0, spinePitch: 1.6,
                 spineLift: -attentionPelvisDropMeters * 0.95,
                 spineX: 0, spineZ: 0,
                 headPitch: attentionHeadPitchDegrees, breath: 0 }
    var left = targetForLeg(mode, gaitPhase, true, strideMeters, motionWeight)
    var right = targetForLeg(mode, gaitPhase, false, strideMeters, motionWeight)
    var rhythm = gaitPhase * Math.PI * 2
    var slideDirection = mode === "slide.right" ? 1 : mode === "slide.left" ? -1 : 0
    var transfer = pelvisTransferPose(left, right, 0, false)
    var pelvisDrop = transfer.y * motionWeight
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
    var pelvisYawRadians = pelvisYaw * Math.PI / 180
    return {
        pelvisX: transfer.x * motionWeight
                 + Math.cos(rhythm) * 0.0065 * (1 - transfer.weight) * motionWeight,
        pelvisY: pelvisDrop,
        pelvisZ: transfer.z * motionWeight,
        ikPelvisY: pelvisDrop,
        pelvisYaw: pelvisYaw,
        pelvisRoll: pelvisRoll,
        spineYaw: -pelvisYaw * (slideDirection !== 0 ? 1.0 : 0.78),
        spineRoll: -pelvisRoll * 0.92,
        spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
        spineLift: -pelvisDrop * 0.95,
        spineX: (-Math.cos(pelvisYawRadians) * transfer.x
                 + Math.sin(pelvisYawRadians) * transfer.z) * motionWeight,
        spineZ: (-Math.sin(pelvisYawRadians) * transfer.x
                 - Math.cos(pelvisYawRadians) * transfer.z) * motionWeight,
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
    var rhythm = gaitPhase * Math.PI * 2
    var slideStrength = weights.right - weights.left
    var longitudinalWeight = weights.forward + weights.backward
    var pelvisYaw = (slideStrength * (slidePelvisFromFacingDegrees
                                     + Math.sin(rhythm) * 1.0)
                     + longitudinalWeight * Math.sin(rhythm) * 1.4) * motionWeight
    var transfer = pelvisTransferPose(left, right, pelvisYaw, true)
    var pelvisDrop = transfer.y * motionWeight + soleCorrection
    var pelvisRoll = -Math.cos(rhythm) * 0.38 * motionWeight
    var pelvisYawRadians = pelvisYaw * Math.PI / 180
    return { pelvisX: transfer.x * motionWeight
                      + Math.cos(rhythm) * 0.0065
                        * (1 - transfer.weight) * motionWeight,
             pelvisY: pelvisDrop, ikPelvisY: pelvisDrop,
             pelvisZ: transfer.z * motionWeight,
             pelvisYaw: pelvisYaw, pelvisRoll: pelvisRoll,
             spineYaw: -pelvisYaw * mix(0.78, 1.0,
                                         smootherStep(Math.abs(slideStrength) * 2)),
             spineRoll: -pelvisRoll * 0.92,
             spinePitch: Math.sin(rhythm + 0.18) * 0.22 * motionWeight,
             spineLift: -pelvisDrop * 0.95,
             spineX: (-Math.cos(pelvisYawRadians) * transfer.x
                      + Math.sin(pelvisYawRadians) * transfer.z) * motionWeight,
             spineZ: (-Math.sin(pelvisYawRadians) * transfer.x
                      - Math.cos(pelvisYawRadians) * transfer.z) * motionWeight,
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

function conditionedRecoveryVertical(target, horizontalSquared, vertical,
                                     backwardAmount) {
    if (target.stance || target.progress >= 0.5)
        return vertical
    // Keep the airborne leg away from the unstable, perfectly-straight IK
    // limit. Shorten its reach with a smooth zero-at-both-ends envelope so the
    // actual two-bone solve stays continuous instead of correcting rotations
    // after IK (which would move a planted ankle).
    var envelope = Math.pow(Math.sin(Math.PI * target.progress / 0.5), 2)
    var baseFlex = mix(1.4, 0.7, backwardAmount)
    var peakFlex = mix(10, 8, backwardAmount)
    var kneeFlex = mix(baseFlex, peakFlex, envelope) * Math.PI / 180
    var desiredDistanceSquared = thighLength * thighLength + shinLength * shinLength
            + 2 * thighLength * shinLength * Math.cos(kneeFlex)
    if (vertical * vertical + horizontalSquared <= desiredDistanceSquared
            || desiredDistanceSquared <= horizontalSquared)
        return vertical
    return Math.sqrt(desiredDistanceSquared - horizontalSquared)
}

function legPose(mode, gaitPhase, leftSide, strideMeters, motionWeight,
                 pelvisDrop, pelvisZ) {
    var target = targetForLeg(mode, gaitPhase, leftSide, strideMeters, motionWeight)
    if (mode === "march.forward" || mode === "march.backward") {
        var backwardAmount = mode === "march.backward" ? 1 : 0
        var targetZ = target.z - (pelvisZ || 0)
        var vertical = standingHipHeight + pelvisDrop - target.lift
        vertical = conditionedRecoveryVertical(target, targetZ * targetZ,
                                                vertical, backwardAmount)
        var ik = sagittalIk(targetZ,
                            standingHipHeight + pelvisDrop - vertical,
                            pelvisDrop)
        var knee = ik.knee
        var hip = ik.hip
        var crossingWeight = !target.stance
                ? 1 - smootherStep(Math.abs(target.progress - 0.5) / 0.45) : 0
        var inward = (leftSide ? 16.2 : -16.2) * crossingWeight
        return { hipX: hip, hipZ: inward, kneeX: knee, kneeZ: 0,
                 footX: target.footPitch - hip - knee, footY: 0, footZ: -inward,
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
    // Reference-derived horn carriage: the upper arms project almost directly
    // forward on a level plane, then the forearms rise and converge at roughly
    // 90 degrees. The right hand overlaps slightly above/in front of the left.
    return { upperX: 111.7,
             upperY: leftSide ? -56.8 : 56.8,
             upperZ: leftSide ? -63.0 : 63.0,
             forearmX: leftSide ? 72.0 : 68.0,
             forearmY: leftSide ? -57.0 : 55.0,
             forearmZ: leftSide ? -43.8 : 42.1,
             handX: leftSide ? -8 : 12,
             handY: leftSide ? -10 : 14,
             handZ: leftSide ? -6 : 10 }
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
             pelvisZ: mix(pose.pelvisZ || 0, 0, blend),
             ikPelvisY: mix(pose.ikPelvisY, attentionPelvisDropMeters, blend),
             pelvisYaw: mix(pose.pelvisYaw, 0, blend),
             pelvisRoll: mix(pose.pelvisRoll, 0, blend),
             spineYaw: mix(pose.spineYaw, 0, blend),
             spineRoll: mix(pose.spineRoll, 0, blend),
             spinePitch: mix(pose.spinePitch, 1.6, blend),
             spineLift: mix(pose.spineLift, -attentionPelvisDropMeters * 0.95, blend),
             spineX: mix(pose.spineX || 0, 0, blend),
             spineZ: mix(pose.spineZ || 0, 0, blend),
             headPitch: mix(pose.headPitch, attentionHeadPitchDegrees, blend),
             breath: pose.breath }
}

function directionalLegPose(relativeDegrees, gaitPhase, leftSide, strideMeters,
                             motionWeight, bodyPoseValue) {
    if (motionWeight <= 0.0001)
        return legPose("idle", gaitPhase, leftSide, strideMeters, 0, 0)
    var target = targetForDirection(relativeDegrees, gaitPhase, leftSide,
                                    strideMeters, motionWeight)
    var weights = directionalWeights(relativeDegrees)
    var hipBindX = leftSide ? -0.105 : 0.105
    var crossingWeight = !target.stance
            ? 1 - smootherStep(Math.abs(target.progress - 0.5) / 0.45) : 0
    // Move the recovering shoe toward the support shoe in body/world space
    // before solving the leg. Unlike an extra hip roll, this preserves the
    // same crossing plane through slides and rear diagonals as the pelvis turns.
    var lateralWeight = weights.left + weights.right
    var crossingDistance = 0.233 + 0.007 * lateralWeight
    var crossingX = (leftSide ? crossingDistance : -crossingDistance) * crossingWeight
    var slideStrength = weights.right - weights.left
    var crossingForward = (leftSide ? -1 : 1) * slideStrength
            * 0.026 * crossingWeight
    var facingYaw = bodyPoseValue.pelvisYaw * Math.PI / 180
    var worldX = hipBindX + target.x + crossingX
            + Math.sin(facingYaw) * crossingForward - bodyPoseValue.pelvisX
    var worldY = 0.075 + target.lift - (0.91 + bodyPoseValue.pelvisY)
    var worldZ = target.z + Math.cos(facingYaw) * crossingForward
            - (bodyPoseValue.pelvisZ || 0)
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
    var backwardAmount = backwardTechniqueWeight(relativeDegrees)
    vertical = conditionedRecoveryVertical(target, horizontalSquared, vertical,
                                           backwardAmount)
    var ik = spatialIkVector(localX, localZ, vertical)
    var knee = ik.kneeX
    var hip = ik.hipX
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
