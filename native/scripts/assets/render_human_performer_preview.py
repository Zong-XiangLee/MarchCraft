#!/usr/bin/env python3
"""Render a dependency-free SVG QA contact sheet from the conditioned skin."""

from __future__ import annotations

import argparse
import html
import math
import pathlib
import struct

from build_human_performer import JOINTS, read_accessor, read_glb


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
            if t < 0.22:
                return mix(15.0, 0.0, smoother_step(t / 0.22))
            if t > 0.76:
                return mix(0.0, -10.0, smoother_step((t - 0.76) / 0.24))
            return 0.0
        if t < 0.28:
            return mix(-10.0, 1.5, smoother_step(t / 0.28))
        if t < 0.72:
            return mix(1.5, 7.0, smoother_step((t - 0.28) / 0.44))
        return mix(7.0, 15.0, smoother_step((t - 0.72) / 0.28))
    if mode == "march.backward":
        if stance:
            return (mix(-7.0, -2.0, smoother_step(t / 0.28)) if t < 0.28
                    else mix(-2.0, -6.0, smoother_step((t - 0.28) / 0.72)))
        return (mix(-6.0, 0.5, smoother_step(t / 0.55)) if t < 0.55
                else mix(0.5, -7.0, smoother_step((t - 0.55) / 0.45)))
    return 0.0


def toe_pitch(mode, cycle):
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    if mode == "march.forward" and stance and t > 0.72:
        return 10.0 * smoother_step((t - 0.72) / 0.28)
    if mode == "march.backward" and stance:
        return 3.0 + 3.0 * smoother_step(t)
    return 0.0


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
        lift = arc * (0.020 if mode == "march.backward" else 0.026 if mode == "march.forward" else 0.018)
    pitch = foot_pitch(mode, cycle)
    pitch_radians = math.radians(pitch)
    toe_extent = 0.08 if mode == "march.backward" else 0.07
    lift += max(0.0, math.sin(pitch_radians) * 0.17, -math.sin(pitch_radians) * toe_extent)
    sagittal = mode in ("march.forward", "march.backward")
    return {"stance": stance, "t": t, "x": 0.0 if sagittal else along,
            "z": along if sagittal else 0.0, "lift": lift,
            "foot": pitch, "toe": toe_pitch(mode, cycle)}


def body_pose(mode, phase, stride):
    if mode == "idle":
        return {"pelvis_x": 0.0, "pelvis_y": 0.0, "pelvis_yaw": 0.0,
                "pelvis_roll": 0.0, "spine_yaw": 0.0, "spine_roll": 0.0,
                "spine_pitch": 0.0, "spine_lift": 0.0, "head_pitch": 0.0}
    targets = [leg_target(mode, phase, left, stride) for left in (True, False)]
    reach = 0.39 + 0.415 - 0.001
    support = next(target for target in targets if target["stance"])
    vertical = math.sqrt(max(0.0, reach * reach - support["x"] ** 2 - support["z"] ** 2))
    drop = min(0.0, vertical + support["lift"] - 0.805)
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
    yaw = slide * (8.0 + math.sin(rhythm) * 1.5) if slide else math.sin(rhythm) * 1.8
    roll = -math.cos(rhythm) * 0.55
    return {"pelvis_x": math.cos(rhythm) * 0.0065, "pelvis_y": drop,
            "pelvis_yaw": yaw, "pelvis_roll": roll,
            "spine_yaw": -yaw * (0.92 if slide else 0.72), "spine_roll": -roll * 0.88,
            "spine_pitch": math.sin(rhythm + 0.18) * 0.28, "spine_lift": -drop * 0.88,
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


def leg_pose(mode, phase, left, stride, pelvis_drop):
    target = leg_target(mode, phase, left, stride)
    if mode in ("march.forward", "march.backward"):
        hip, knee = sagittal_ik(target["z"], target["lift"], pelvis_drop)
        return hip, 0.0, knee, 0.0, target["foot"] - hip - knee, 0.0, target["toe"]
    if mode in ("slide.left", "slide.right"):
        roll = math.degrees(math.asin(clamp(target["x"] / 0.805, -0.62, 0.62)))
        bend = -2.0 if target["stance"] else -7.0 * math.sin(math.pi * target["t"])
        return -bend * 0.38, roll, bend, -roll * 0.08, -bend * 0.62, -roll * 0.92, 0.0
    return (0.0,) * 7


def pose_matrices(mode: str, phase: float, stride: float = 0.5715):
    body = body_pose(mode, phase, stride)
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
        hip_x, hip_z, knee_x, knee_z, foot_x, foot_z, toe_x = leg_pose(
            mode, phase, left, stride, body["pelvis_y"])
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
        ("march.forward", 0.25, 540, (198, 137, 101), math.radians(78)),
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
