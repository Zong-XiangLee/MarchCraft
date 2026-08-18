#!/usr/bin/env python3
"""Render a dependency-free SVG QA contact sheet from the conditioned skin."""

from __future__ import annotations

import argparse
import html
import math
import pathlib
import struct

from build_human_performer import JOINTS, read_accessor, read_glb

SOLE_TABLES = (
(-0.00118,-0.00103,-0.00039,-0.00007,0.00001,0.00001,0.00001,0.00000,-0.00000,-0.00026,-0.00049,-0.00070,-0.00085,-0.00069,0.00044,0.00168,-0.00130,-0.00118,-0.00047,-0.00010,-0.00001,-0.00001,-0.00001,-0.00002,-0.00003,-0.00028,-0.00052,-0.00072,-0.00087,-0.00067,0.00049,0.00177),
(0.00148,0.00088,0.00077,0.00074,0.00060,0.00039,0.00023,-0.00098,-0.00257,-0.00401,-0.00532,-0.00649,-0.00754,-0.00857,-0.00874,-0.00493,-0.00143,-0.00207,-0.01241,-0.01001,-0.00764,-0.00621,-0.00485,-0.00344,-0.00215,-0.00096,0.00015,0.00034,0.00050,-0.00001,0.00099,0.00208),
(-0.00069,-0.01232,-0.02541,-0.02238,-0.01962,-0.01699,-0.01435,-0.01166,-0.00889,-0.00604,-0.00306,-0.00003,0.00010,0.00080,0.00201,0.00319,0.00136,0.00014,-0.00043,-0.00040,-0.00028,-0.00030,-0.00287,-0.00556,-0.00822,-0.01101,-0.01397,-0.01716,-0.02062,-0.02519,-0.02148,-0.01254),
(-0.00161,-0.00599,-0.01604,-0.01439,-0.01218,-0.00986,-0.00751,-0.00511,-0.00265,-0.00012,0.00009,0.00015,0.00021,0.00025,0.00028,0.00030,-0.00002,-0.00032,-0.00048,0.00023,0.00005,0.00000,-0.00003,-0.00004,-0.00218,-0.00466,-0.00727,-0.01003,-0.01294,-0.01603,-0.01928,-0.01443),
(-0.00068,-0.00063,-0.00052,-0.00020,-0.00012,-0.00008,-0.00003,0.00001,0.00003,0.00004,0.00004,-0.00008,-0.00018,-0.00027,-0.00039,0.00001,-0.00061,-0.00057,-0.00047,-0.00016,-0.00008,-0.00004,0.00000,0.00004,0.00007,0.00009,0.00009,-0.00004,-0.00013,-0.00023,-0.00034,0.00006),
(0.00007,-0.00027,-0.00047,0.00021,0.00003,-0.00002,-0.00005,-0.00007,-0.00247,-0.00510,-0.00784,-0.01069,-0.01363,-0.01667,-0.01981,-0.01485,-0.00172,-0.00580,-0.01562,-0.01376,-0.01150,-0.00921,-0.00694,-0.00466,-0.00235,0.00001,0.00011,0.00018,0.00023,0.00027,0.00031,0.00034),
(0.00152,0.00024,-0.00038,-0.00040,-0.00030,-0.00031,-0.00314,-0.00612,-0.00911,-0.01216,-0.01534,-0.01864,-0.02209,-0.02656,-0.02299,-0.01343,-0.00087,-0.01185,-0.02441,-0.02104,-0.01815,-0.01553,-0.01302,-0.01054,-0.00805,-0.00550,-0.00282,-0.00004,0.00009,0.00080,0.00205,0.00328),
(-0.00126,-0.00193,-0.01252,-0.01024,-0.00794,-0.00652,-0.00515,-0.00372,-0.00239,-0.00116,-0.00000,0.00037,0.00052,-0.00001,0.00097,0.00201,0.00128,0.00070,0.00066,0.00069,0.00056,0.00036,0.00020,-0.00077,-0.00232,-0.00374,-0.00503,-0.00621,-0.00727,-0.00832,-0.00852,-0.00477))
ZERO_RESIDUAL = (0.0,) * 16
HALF_STRIDE_CORRECTIONS = (
    (0.00043,0,0,0.00017,0.00057,0,0,0.00016),
    (-0.00080,-0.00019,0,0.00204,-0.00074,0.00286,0,-0.00055),
    (-0.00083,0.00612,0,-0.00304,-0.00109,-0.00248,0,0.00644),
    (0.00157,0.00498,0,0,-0.00038,-0.00008,0,0.00516),
    (-0.00002,-0.00001,0.00000,0.00019,-0.00003,-0.00001,0.00000,0.00019),
    (-0.00043,-0.00008,0,0.00539,0.00158,0.00475,0,0),
    (-0.00124,-0.00267,0,0.00688,-0.00086,0.00566,0,-0.00283),
    (-0.00070,0.00291,0,-0.00068,-0.00058,-0.00018,0,0.00199))
EXTENDED_STRIDE_CORRECTIONS = (
    (0.00040,0,0,-0.00008,0.00054,0,0,-0.00008),
    (0.00110,0.00016,0,-0.00085,0.00064,-0.00131,0,0.00010),
    (-0.00151,-0.00289,0,-0.00004,0.00269,0.00009,0,-0.00307),
    (-0.00458,-0.00206,0,-0.00005,0.00050,0.00004,0,-0.00215),
    (0.00002,0.00001,0.00000,0.00008,0.00002,0.00000,0.00000,0.00008),
    (0.00040,0.00003,0,-0.00219,-0.00459,-0.00211,0,-0.00005),
    (0.00273,0.00011,0,-0.00327,-0.00157,-0.00267,0,-0.00004),
    (0.00069,-0.00133,0,0.00010,0.00134,0.00015,0,-0.00082))
HALF_BACK_DIAGONAL_CORRECTIONS = (
    (0.00054,-0.00202,0,0.00206,0.00002,0.00205,0,-0.00179),
    (0.00115,-0.00331,-0.00025,0.00283,0.00004,0.00234,-0.00022,-0.00149),
    (0.00004,0.00253,-0.00023,-0.00048,0.00115,-0.00330,-0.00025,0.00264),
    (0.00002,0.00208,0,-0.00074,0.00054,-0.00200,0,0.00203))
EXTENDED_BACK_DIAGONAL_CORRECTIONS = (
    (-0.00065,-0.00518,0,0.00012,-0.00666,-0.00041,0,-0.00395),
    (0.00168,-0.00702,0.00014,0.00028,-0.00717,-0.00031,0.00014,-0.00656),
    (-0.00740,-0.00032,0.00015,-0.00623,0.00173,-0.00767,0.00011,0.00028),
    (-0.00690,-0.00042,0,-0.00382,-0.00062,-0.00523,0,0.00012))
EXTENDED_RESIDUALS = (ZERO_RESIDUAL,
(0.00166,0.00064,0.00017,-0.00017,0,0.00022,-0.00010,-0.00030,0.00294,0.00168,0.00044,0.00005,0,-0.00041,-0.00112,-0.00197),
(0.00037,-0.00058,-0.00078,-0.00087,-0.00039,-0.00008,-0.00023,-0.00009,0.00363,0.00163,0.00034,-0.00022,-0.00036,-0.00108,-0.00187,-0.00266),
(-0.00094,-0.00040,-0.00029,-0.00014,0,0.00001,0.00005,0.00020,0.00046,0.00006,0.00003,0.00001,0,-0.00010,-0.00028,-0.00054),
(-0.00223,-0.00088,-0.00026,-0.00003,0.00001,0.00002,0.00039,0.00102,0.00039,-0.00015,-0.00005,0.00003,0.00001,-0.00021,-0.00042,-0.00088),
(-0.00117,-0.00043,-0.00014,-0.00002,0,0,0.00035,0.00082,0.00062,0.00020,0.00004,0.00001,0,-0.00009,-0.00019,-0.00052),
ZERO_RESIDUAL,
(0.00063,0.00020,0.00004,0.00001,0,-0.00014,-0.00019,-0.00051,-0.00116,-0.00043,-0.00014,-0.00002,0,0,0.00036,0.00081),
(0.00053,-0.00013,-0.00002,0.00003,0.00001,-0.00021,-0.00043,-0.00088,-0.00223,-0.00088,-0.00027,-0.00004,0.00001,0.00002,0.00039,0.00102),
(0.00056,0.00008,0.00004,0,0,-0.00013,-0.00035,-0.00060,-0.00094,-0.00041,-0.00013,-0.00010,0,0,0.00005,0.00021),
(0.00368,0.00162,0.00033,-0.00022,-0.00038,-0.00139,-0.00233,-0.00303,0.00035,-0.00008,-0.00019,-0.00047,-0.00036,-0.00011,-0.00023,-0.00010),
(0.00299,0.00170,0.00044,0.00005,0,-0.00057,-0.00138,-0.00219,0.00165,0.00067,0.00019,0.00002,0,0.00020,-0.00010,-0.00032),
ZERO_RESIDUAL)


def identity():
    return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def multiply(a, b):
    return [[sum(a[row][k] * b[k][column] for k in range(4)) for column in range(4)] for row in range(4)]


def translation(x, y, z):
    matrix = identity()
    matrix[0][3], matrix[1][3], matrix[2][3] = x, y, z
    return matrix


def rotation(x_degrees=0.0, y_degrees=0.0, z_degrees=0.0):
    x, y, z = (math.radians(value) for value in (x_degrees, y_degrees, z_degrees))
    rx = [[1,0,0,0], [0,math.cos(x),-math.sin(x),0], [0,math.sin(x),math.cos(x),0], [0,0,0,1]]
    ry = [[math.cos(y),0,math.sin(y),0], [0,1,0,0], [-math.sin(y),0,math.cos(y),0], [0,0,0,1]]
    rz = [[math.cos(z),-math.sin(z),0,0], [math.sin(z),math.cos(z),0,0], [0,0,1,0], [0,0,0,1]]
    return multiply(multiply(rz, ry), rx)


def transform(matrix, point):
    x, y, z = point
    return tuple(sum(matrix[row][k] * (x, y, z, 1.0)[k] for k in range(4)) for row in range(3))


def clamp(value, low, high):
    return max(low, min(high, value))


def mix(a, b, amount):
    return a + (b - a) * amount


def smoother_step(value):
    value = clamp(value, 0.0, 1.0)
    return value ** 3 * (value * (value * 6.0 - 15.0) + 10.0)


def leg_cycle(phase, left):
    return (phase + (0.5 if left else 0.0)) % 1.0


def foot_pitch(mode, cycle):
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    if mode == "march.forward":
        if stance:
            if t < 0.24:
                return mix(20.0, 0.0, smoother_step(t / 0.24))
            return 0.0
        if t < 0.24:
            return mix(0.0, 2.0, smoother_step(t / 0.24))
        if t < 0.70:
            return mix(2.0, 8.0, smoother_step((t - 0.24) / 0.46))
        return mix(8.0, 20.0, smoother_step((t - 0.70) / 0.30))
    if mode == "march.backward":
        if stance:
            return (mix(-5.0, -2.0, smoother_step(t / 0.24)) if t < 0.24
                    else mix(-2.0, -4.0, smoother_step((t - 0.24) / 0.76)))
        return (mix(-4.0, -1.0, smoother_step(t / 0.55)) if t < 0.55
                else mix(-1.0, -5.0, smoother_step((t - 0.55) / 0.45)))
    return 0.0


def toe_pitch(mode, cycle):
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    if mode == "march.backward" and stance:
        return 2.0 + 2.0 * smoother_step(t)
    return 0.0


def sole_lift(pitch, toe_extent):
    radians = math.radians(pitch)
    return max(0.0, math.sin(radians) * 0.17, -math.sin(radians) * toe_extent)


def leg_target(mode, phase, left, stride):
    cycle = leg_cycle(phase, left)
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    travel = t if stance else smoother_step(t)
    direction = 1.0 if mode in ("march.backward", "slide.right") else -1.0
    along = direction * stride * ((0.5 - travel) if stance else (travel - 0.5))
    lift = 0.0
    if not stance:
        arc = math.sin(math.pi * t) ** 1.35
        lift = arc * (0.005 if mode == "march.backward" else 0.026 if mode == "march.forward" else 0.018)
    pitch = foot_pitch(mode, cycle)
    toe_extent = 0.08 if mode == "march.backward" else 0.07
    lift += sole_lift(pitch, toe_extent)
    sagittal = mode in ("march.forward", "march.backward")
    return {"stance": stance, "t": t, "x": 0.0 if sagittal else along,
            "z": along if sagittal else 0.0, "lift": lift,
            "foot": pitch, "toe": toe_pitch(mode, cycle)}


def required_pelvis_drop(targets):
    reach = 0.39 + 0.415 - 0.001

    def available(target):
        horizontal = math.hypot(target["x"], target["z"])
        return math.sqrt(max(0.0, reach * reach - horizontal * horizontal)) + target["lift"]

    support = next(target for target in targets if target["stance"])
    moving = next(target for target in targets if not target["stance"])
    support_drop = min(0.0, available(support) - 0.805)
    moving_drop = min(0.0, available(moving) - 0.805)
    edge = min(moving["t"], 1.0 - moving["t"])
    double_support = 1.0 - smoother_step(edge / 0.12)
    return mix(support_drop, min(support_drop, moving_drop), double_support)


def body_pose(mode, phase, stride):
    if mode == "idle":
        return {"pelvis_x": 0.0, "pelvis_y": 0.0, "ik_pelvis_y": 0.0, "pelvis_yaw": 0.0,
                "pelvis_roll": 0.0, "spine_yaw": 0.0, "spine_roll": 0.0,
                "spine_pitch": 0.0, "spine_lift": 0.0, "head_pitch": 0.0}
    targets = [leg_target(mode, phase, left, stride) for left in (True, False)]
    support = next(target for target in targets if target["stance"])
    drop = required_pelvis_drop(targets)
    rhythm = phase * math.pi * 2.0
    slide = 1.0 if mode == "slide.right" else -1.0 if mode == "slide.left" else 0.0
    if slide:
        support_is_left = targets[0]["stance"]
        normalized_reach = support["x"] / (stride * 0.5) if stride > 0.0001 else 0.0
        if slide > 0:
            sole_factor = ((0.012 + 0.010 * normalized_reach) if support_is_left
                           else (0.026 + 0.009 * normalized_reach))
        else:
            sole_factor = ((0.025 - 0.010 * normalized_reach) if support_is_left
                           else (0.010 - 0.010 * normalized_reach))
        drop += abs(support["x"]) * max(0.0, sole_factor)
        if slide < 0.0:
            drop += 0.00002 * clamp(stride / 0.70, 0.0, 1.0)
    yaw = slide * (8.0 + math.sin(rhythm) * 1.5) if slide else math.sin(rhythm) * 1.8
    roll = -math.cos(rhythm) * 0.55
    return {"pelvis_x": math.cos(rhythm) * 0.0065, "pelvis_y": drop,
            "ik_pelvis_y": drop,
            "pelvis_yaw": yaw, "pelvis_roll": roll,
            "spine_yaw": -yaw * (0.92 if slide else 0.72), "spine_roll": -roll * 0.88,
            "spine_pitch": math.sin(rhythm + 0.18) * 0.28, "spine_lift": -drop * 0.95,
            "head_pitch": -math.sin(rhythm + 0.18) * 0.16}


def directional_weights(angle_degrees):
    radians = math.radians(((angle_degrees + 180.0) % 360.0) - 180.0)
    longitudinal, lateral = abs(math.cos(radians)), abs(math.sin(radians))
    quadrant_degrees = math.degrees(math.atan2(lateral, longitudinal))
    lateral_weight = smoother_step((quadrant_degrees - 22.5) / 45.0)
    longitudinal_weight = 1.0 - lateral_weight
    return [longitudinal_weight if math.cos(radians) >= 0.0 else 0.0,
            longitudinal_weight if math.cos(radians) < 0.0 else 0.0,
            lateral_weight if math.sin(radians) < 0.0 else 0.0,
            lateral_weight if math.sin(radians) >= 0.0 else 0.0]


def directional_sole_offset(angle_degrees, phase, stride):
    angle = angle_degrees % 360.0
    quadrant = int(math.floor(angle / 90.0)) % 4
    longitudinal = SOLE_TABLES[0] if quadrant in (0, 3) else SOLE_TABLES[4]
    side = SOLE_TABLES[2] if quadrant < 2 else SOLE_TABLES[6]
    diagonal = SOLE_TABLES[(quadrant * 2 + 1) % 8]

    def sampled(table):
        position = (phase % 1.0) * len(table)
        index = int(math.floor(position)) % len(table)
        return mix(table[index], table[(index + 1) % len(table)], position - math.floor(position))

    weights = directional_weights(angle_degrees)
    lateral_weight = weights[2] + weights[3]
    base = (mix(sampled(longitudinal), sampled(diagonal), lateral_weight * 2.0)
            if lateral_weight <= 0.5
            else mix(sampled(diagonal), sampled(side), (lateral_weight - 0.5) * 2.0))
    def correction_from(tables):
        longitudinal_correction = tables[0] if quadrant in (0, 3) else tables[4]
        side_correction = tables[2] if quadrant < 2 else tables[6]
        diagonal_correction = tables[(quadrant * 2 + 1) % 8]
        return (mix(sampled(longitudinal_correction), sampled(diagonal_correction), lateral_weight * 2.0)
                if lateral_weight <= 0.5
                else mix(sampled(diagonal_correction), sampled(side_correction),
                         (lateral_weight - 0.5) * 2.0))
    def back_diagonal_correction(tables):
        zero = (0.0,) * 8
        if 90.0 <= angle <= 135.0:
            if angle <= 105.0:
                first, second, amount = zero, tables[0], (angle - 90.0) / 15.0
            elif angle <= 120.0:
                first, second, amount = tables[0], tables[1], (angle - 105.0) / 15.0
            else:
                first, second, amount = tables[1], zero, (angle - 120.0) / 15.0
        elif 225.0 <= angle <= 270.0:
            if angle <= 240.0:
                first, second, amount = zero, tables[2], (angle - 225.0) / 15.0
            elif angle <= 255.0:
                first, second, amount = tables[2], tables[3], (angle - 240.0) / 15.0
            else:
                first, second, amount = tables[3], zero, (angle - 255.0) / 15.0
        else:
            return 0.0
        return mix(sampled(first), sampled(second), amount)
    if stride < 0.5715:
        base += (correction_from(HALF_STRIDE_CORRECTIONS)
                 + back_diagonal_correction(HALF_BACK_DIAGONAL_CORRECTIONS)) * clamp((0.5715 - stride) / 0.28575, 0.0, 1.0)
    elif stride > 0.5715:
        base += (correction_from(EXTENDED_STRIDE_CORRECTIONS)
                 + back_diagonal_correction(EXTENDED_BACK_DIAGONAL_CORRECTIONS)) * clamp((stride - 0.5715) / 0.1285, 0.0, 1.0)
    extended_weight = clamp((stride - 0.5715) / 0.1285, 0.0, 1.0)
    if extended_weight <= 0.0 or angle < 90.0 or angle > 270.0:
        return base
    residual_position = (angle - 90.0) / 15.0
    residual_index = min(11, int(math.floor(residual_position)))
    residual = mix(sampled(EXTENDED_RESIDUALS[residual_index]),
                   sampled(EXTENDED_RESIDUALS[residual_index + 1]),
                   residual_position - residual_index)
    return base + residual * extended_weight


def directional_target(angle_degrees, phase, left, stride):
    cycle = leg_cycle(phase, left)
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    travel = t if stance else smoother_step(t)
    offset = stride * ((0.5 - travel) if stance else (travel - 0.5))
    radians = math.radians(((angle_degrees + 180.0) % 360.0) - 180.0)
    weights = directional_weights(angle_degrees)
    forward = leg_target("march.forward", phase, left, stride)
    backward = leg_target("march.backward", phase, left, stride)
    backward_amount = weights[1]
    pitch = mix(forward["foot"], backward["foot"], backward_amount)
    lift = mix(forward["lift"] - sole_lift(forward["foot"], 0.07),
               backward["lift"] - sole_lift(backward["foot"], 0.08),
               backward_amount)
    lift += sole_lift(pitch, mix(0.07, 0.08, weights[1])) + directional_sole_offset(angle_degrees, phase, stride)
    return {"stance": stance, "t": t,
            "x": math.sin(radians) * offset, "z": -math.cos(radians) * offset,
            "lift": lift, "foot": pitch,
            "toe": mix(forward["toe"], backward["toe"], backward_amount)}


def directional_body_pose(angle_degrees, phase, stride):
    targets = [directional_target(angle_degrees, phase, left, stride) for left in (True, False)]
    weights = directional_weights(angle_degrees)
    drop = required_pelvis_drop(targets)
    rhythm = phase * math.pi * 2.0
    slide_strength = weights[3] - weights[2]
    longitudinal_weight = weights[0] + weights[1]
    yaw = slide_strength * (68.0 + math.sin(rhythm) * 2.0) + longitudinal_weight * math.sin(rhythm) * 1.4
    roll = -math.cos(rhythm) * 0.38
    return {"pelvis_x": math.cos(rhythm) * 0.0065, "pelvis_y": drop,
            "ik_pelvis_y": drop, "pelvis_yaw": yaw, "pelvis_roll": roll,
            "spine_yaw": -yaw * mix(0.78, 0.985,
                                      smoother_step(abs(slide_strength) * 2.0)),
            "spine_roll": -roll * 0.92,
            "spine_pitch": math.sin(rhythm + 0.18) * 0.22,
            "spine_lift": -drop * 0.95,
            "head_pitch": -math.sin(rhythm + 0.18) * 0.16}


def sagittal_ik(target_z, lift, pelvis_drop):
    thigh, shin = 0.39, 0.415
    vertical = 0.805 + pelvis_drop - lift
    distance = clamp(math.hypot(vertical, target_z), 0.08, thigh + shin - 0.001)
    target_angle = math.atan2(-target_z, vertical)
    hip_offset = math.acos(clamp((thigh * thigh + distance * distance - shin * shin)
                                 / (2.0 * thigh * distance), -1.0, 1.0))
    interior = math.acos(clamp((thigh * thigh + shin * shin - distance * distance)
                               / (2.0 * thigh * shin), -1.0, 1.0))
    return math.degrees(target_angle + hip_offset), math.degrees(interior - math.pi)


def spatial_ik_vector(target_x, target_z, vertical):
    thigh, shin = 0.39, 0.415
    plane_vertical = math.hypot(vertical, target_x)
    distance = clamp(math.hypot(plane_vertical, target_z), 0.08, thigh + shin - 0.001)
    target_angle = math.atan2(-target_z, plane_vertical)
    hip_offset = math.acos(clamp((thigh * thigh + distance * distance - shin * shin)
                                 / (2.0 * thigh * distance), -1.0, 1.0))
    interior = math.acos(clamp((thigh * thigh + shin * shin - distance * distance)
                               / (2.0 * thigh * shin), -1.0, 1.0))
    return (math.degrees(target_angle + hip_offset), math.degrees(math.atan2(target_x, vertical)),
            math.degrees(interior - math.pi))


def leg_pose(mode, phase, left, stride, pelvis_drop):
    target = leg_target(mode, phase, left, stride)
    if mode in ("march.forward", "march.backward"):
        hip, knee = sagittal_ik(target["z"], target["lift"], pelvis_drop)
        minimum_flex = -0.7 if mode == "march.backward" else -1.4
        clamped_knee = min(knee, minimum_flex)
        hip -= (clamped_knee - knee) * 0.5
        knee = clamped_knee
        return hip, 0.0, knee, 0.0, target["foot"] - hip - knee, 0.0, target["toe"]
    if mode in ("slide.left", "slide.right"):
        roll = math.degrees(math.asin(clamp(target["x"] / 0.805, -0.62, 0.62)))
        bend = -2.0 if target["stance"] else -7.0 * math.sin(math.pi * target["t"])
        return -bend * 0.38, roll, bend, -roll * 0.08, -bend * 0.62, -roll * 0.92, 0.0
    return (0.0,) * 7


def directional_leg_pose(angle_degrees, phase, left, stride, body):
    target = directional_target(angle_degrees, phase, left, stride)
    hip_bind_x = -0.105 if left else 0.105
    world_x = hip_bind_x + target["x"] - body["pelvis_x"]
    world_y = 0.075 + target["lift"] - (0.91 + body["pelvis_y"])
    world_z = target["z"]
    roll, yaw = math.radians(body["pelvis_roll"]), math.radians(body["pelvis_yaw"])
    roll_x = math.cos(roll) * world_x + math.sin(roll) * world_y
    roll_y = -math.sin(roll) * world_x + math.cos(roll) * world_y
    local_x = math.cos(yaw) * roll_x - math.sin(yaw) * world_z - hip_bind_x
    local_z = math.sin(yaw) * roll_x + math.cos(yaw) * world_z
    local_y = roll_y + 0.03
    vertical = -local_y
    reach = 0.39 + 0.415 - 0.001
    horizontal_squared = local_x * local_x + local_z * local_z
    if vertical * vertical + horizontal_squared > reach * reach:
        vertical = math.sqrt(max(0.0, reach * reach - horizontal_squared))
    hip_x, hip_z, knee_x = spatial_ik_vector(local_x, local_z, vertical)
    minimum_flex = mix(-1.4, -0.7, directional_weights(angle_degrees)[1])
    clamped_knee = min(knee_x, minimum_flex)
    hip_x -= (clamped_knee - knee_x) * 0.5
    knee_x = clamped_knee
    return (hip_x, hip_z, knee_x, 0.0, target["foot"] - hip_x - knee_x,
            -hip_z - body["pelvis_roll"], target["toe"])


def pose_matrices(mode: str, phase: float, stride: float = 0.5715):
    direction_angle = (float(mode.split(".", 1)[1]) if mode.startswith("direction.")
                       else 90.0 if mode == "slide.right"
                       else -90.0 if mode == "slide.left" else None)
    body = (directional_body_pose(direction_angle, phase, stride)
            if direction_angle is not None else body_pose(mode, phase, stride))
    rotations = {
        1: (0.0, body["pelvis_yaw"], body["pelvis_roll"]),
        2: (body["spine_pitch"], body["spine_yaw"] * 0.42, body["spine_roll"] * 0.45),
        3: (-body["spine_pitch"] * 0.62, body["spine_yaw"] * 0.58, body["spine_roll"] * 0.55),
        5: (body["head_pitch"], 0.0, 0.0),
        15: (0.0, 0.0, 50.0), 19: (0.0, 0.0, -50.0),
    }
    translations = {1: (body["pelvis_x"], body["pelvis_y"], 0.0),
                    2: (0.0, body["spine_lift"], 0.0)}
    for left, thigh, shin, foot, toe, arm in (
        (True, 6, 7, 8, 9, 15), (False, 10, 11, 12, 13, 19)
    ):
        if direction_angle is not None:
            pose = directional_leg_pose(direction_angle, phase, left, stride, body)
        else:
            pose = leg_pose(mode, phase, left, stride, body["pelvis_y"])
        hip_x, hip_z, knee_x, knee_z, foot_x, foot_z, toe_x = pose
        rotations[thigh] = (hip_x, 0.0, hip_z)
        rotations[shin] = (knee_x, 0.0, knee_z)
        rotations[foot] = (foot_x, 0.0, foot_z)
        rotations[toe] = (toe_x, 0.0, 0.0)
        rotations[arm] = (clamp(-hip_x * 0.075, -2.4, 2.4), 0.0, 50.0 if left else -50.0)

    globals_ = []
    skins = []
    for index, joint in enumerate(JOINTS):
        if joint.parent is None:
            local_position = joint.global_position
        else:
            parent_position = JOINTS[joint.parent].global_position
            local_position = tuple(joint.global_position[axis] - parent_position[axis] for axis in range(3))
        offset = translations.get(index, (0.0, 0.0, 0.0))
        local_position = tuple(local_position[axis] + offset[axis] for axis in range(3))
        local = multiply(translation(*local_position), rotation(*rotations.get(index, (0.0, 0.0, 0.0))))
        global_matrix = local if joint.parent is None else multiply(globals_[joint.parent], local)
        globals_.append(global_matrix)
        skins.append(multiply(global_matrix, translation(*(-value for value in joint.global_position))))
    return skins


def skinned_vertices(positions, joints, weights, matrices):
    output = []
    for position, joint_row, weight_row in zip(positions, joints, weights):
        result = [0.0, 0.0, 0.0]
        for joint, weight in zip(joint_row, weight_row):
            if weight <= 0.0:
                continue
            moved = transform(matrices[int(joint)], position)
            for axis in range(3):
                result[axis] += moved[axis] * weight
        output.append(tuple(result))
    return output


def color(base, brightness):
    return "#" + "".join(f"{max(0, min(255, int(channel * brightness))):02x}" for channel in base)


def render_bmp(source: pathlib.Path, destination: pathlib.Path):
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])
    width, height, ground = 1800, 720, 630
    pixels = bytearray(width * height * 3)
    for y in range(height):
        base = (40, 91, 61) if y >= ground else (135, 170, 189)
        for x in range(width):
            offset = ((height - 1 - y) * width + x) * 3
            pixels[offset:offset+3] = bytes((base[2], base[1], base[0]))
    for x in range(width):
        offset = ((height - 1 - ground) * width + x) * 3
        pixels[offset:offset+3] = bytes((223, 233, 220))
    poses = [
        ("march.forward", 0.50, 180, (217, 163, 127), math.radians(78)),
        ("direction.45", 0.25, 540, (198, 137, 101), math.radians(45)),
        ("idle", 0.0, 900, (169, 103, 75), 0.0),
        ("march.backward", 0.50, 1260, (137, 80, 58), math.radians(78)),
        ("slide.right", 0.08, 1620, (112, 65, 47), 0.0),
    ]
    for mode, phase, center_x, base, view_angle in poses:
        vertices = skinned_vertices(positions, joints, weights, pose_matrices(mode, phase))
        cosine, sine = math.cos(view_angle), math.sin(view_angle)
        viewed = [(x * cosine + z * sine, y, -x * sine + z * cosine) for x, y, z in vertices]
        for x, y, depth in sorted(viewed, key=lambda point: point[2], reverse=True):
            px, py = int(center_x + x * 285.0), int(ground - y * 285.0)
            shade = max(0.58, min(1.08, 0.86 - depth * 0.9))
            rgb = tuple(max(0, min(255, int(channel * shade))) for channel in base)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = px + dx, py + dy
                    if 0 <= xx < width and 0 <= yy < height:
                        offset = ((height - 1 - yy) * width + xx) * 3
                        pixels[offset:offset+3] = bytes((rgb[2], rgb[1], rgb[0]))
    row_bytes = width * 3
    padding = (4 - row_bytes % 4) % 4
    pixel_bytes = b"".join(pixels[row*row_bytes:(row+1)*row_bytes] + b"\0" * padding for row in range(height))
    file_size = 54 + len(pixel_bytes)
    header = (b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
              + struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0,
                            len(pixel_bytes), 2835, 2835, 0, 0))
    destination.write_bytes(header + pixel_bytes)
    print(destination)


def render(source: pathlib.Path, destination: pathlib.Path):
    if destination.suffix.lower() == ".bmp":
        render_bmp(source, destination)
        return
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])
    indices = [row[0] for row in read_accessor(document, binary, primitive["indices"])]

    width, height, ground = 1200, 720, 630
    poses = [
        ("Forward 8-to-5", "march.forward", 0.08, 250, (217, 163, 127)),
        ("Attention", "idle", 0.0, 600, (169, 103, 75)),
        ("Right slide", "slide.right", 0.08, 950, (112, 65, 47)),
    ]
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
           '<rect width="100%" height="100%" fill="#87aabd"/>',
           f'<rect y="{ground}" width="100%" height="{height-ground}" fill="#285b3d"/>',
           f'<line y1="{ground}" y2="{ground}" x2="{width}" stroke="#dce9df" stroke-width="3"/>']

    for label, mode, phase, center_x, base in poses:
        vertices = skinned_vertices(positions, joints, weights, pose_matrices(mode, phase))
        projected = [(center_x + x * 285.0, ground - y * 285.0, z) for x, y, z in vertices]
        triangles = []
        for offset in range(0, len(indices), 3):
            a, b, c = (vertices[indices[offset + item]] for item in range(3))
            ab = (b[0]-a[0], b[1]-a[1], b[2]-a[2])
            ac = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
            normal = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
            if normal[2] >= 0.0:  # front view looks from -Z toward +Z
                continue
            length = math.sqrt(sum(value*value for value in normal)) or 1.0
            diffuse = max(0.0, (-normal[0]*0.25 + normal[1]*0.55 - normal[2]*0.79) / length)
            shade = 0.48 + diffuse * 0.52
            points = [projected[indices[offset + item]] for item in range(3)]
            triangles.append((sum(point[2] for point in points) / 3.0, points, shade))
        triangles.sort(reverse=True, key=lambda item: item[0])
        for _, points, shade in triangles:
            coordinates = " ".join(f"{point[0]:.1f},{point[1]:.1f}" for point in points)
            svg.append(f'<polygon points="{coordinates}" fill="{color(base, shade)}"/>')
        svg.append(f'<text x="{center_x}" y="690" fill="#f7fbff" font-family="sans-serif" font-size="22" text-anchor="middle">{html.escape(label)}</text>')
    svg.append("</svg>")
    destination.write_text("\n".join(svg), encoding="utf-8")
    print(destination)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    arguments = parser.parse_args()
    render(arguments.source, arguments.destination)


if __name__ == "__main__":
    main()
