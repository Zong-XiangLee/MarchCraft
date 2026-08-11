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


def pose_matrices(mode: str, phase: float, stride: float = 1.0):
    angle = phase * math.pi * 2.0
    amplitude = min(32.0, max(7.0, stride * 24.0))
    rotations = {15: (0.0, 0.0, 50.0), 19: (0.0, 0.0, -50.0)}
    for left, thigh, shin, foot, arm in (
        (True, 6, 7, 8, 15), (False, 10, 11, 12, 19)
    ):
        wave = math.cos(angle) * (1 if left else -1)
        if mode in ("march.forward", "march.backward"):
            direction = -0.78 if mode == "march.backward" else 1.0
            hip = wave * amplitude * direction
            knee = -max(0.0, -wave) * max(4.0, amplitude * 0.30)
            foot_angle = (max(0.0, wave) * (2.0 if mode == "march.backward" else 14.0)
                          - max(0.0, -wave) * (2.0 if mode == "march.backward" else 4.0))
            rotations[thigh] = (hip, 0.0, 0.0)
            rotations[shin] = (knee, 0.0, 0.0)
            rotations[foot] = (foot_angle, 0.0, 0.0)
            rotations[arm] = (-hip * 0.28, 0.0, 50.0 if left else -50.0)
        elif mode in ("slide.left", "slide.right"):
            direction = -1.0 if mode == "slide.left" else 1.0
            cadence = -wave if mode == "slide.right" else wave
            rotations[thigh] = (0.0, 0.0, cadence * amplitude * 0.78 * direction)

    globals_ = []
    skins = []
    for index, joint in enumerate(JOINTS):
        if joint.parent is None:
            local_position = joint.global_position
        else:
            parent_position = JOINTS[joint.parent].global_position
            local_position = tuple(joint.global_position[axis] - parent_position[axis] for axis in range(3))
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
    width, height, ground = 1200, 720, 630
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
        ("march.forward", 0.08, 250, (217, 163, 127)),
        ("idle", 0.0, 600, (169, 103, 75)),
        ("slide.right", 0.08, 950, (112, 65, 47)),
    ]
    for mode, phase, center_x, base in poses:
        vertices = skinned_vertices(positions, joints, weights, pose_matrices(mode, phase))
        for x, y, z in sorted(vertices, key=lambda point: point[2], reverse=True):
            px, py = int(center_x + x * 285.0), int(ground - y * 285.0)
            shade = max(0.58, min(1.08, 0.86 - z * 0.9))
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
