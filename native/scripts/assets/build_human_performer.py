#!/usr/bin/env python3
"""Build MarchCraft's canonical performer from extracted Blender rig data.

The checked-in Blender source remains the attribution-bearing artist asset.
``extract_lowpolyboy_mesh.py`` preserves its authored deformation weights and
converts its axes/scale into a deterministic JSON interchange file. This script
packages that data into the compact GLB contract consumed by Qt Quick 3D and
authors the semantic drill clips used by validators and authoring tools.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import struct
from dataclasses import dataclass


JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963


def read_glb(path: pathlib.Path) -> tuple[dict, bytes]:
    raw = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", raw, 0)
    if magic != 0x46546C67 or version != 2 or length != len(raw):
        raise ValueError(f"{path} is not a valid glTF 2.0 binary")
    document = None
    binary = b""
    offset = 12
    while offset < length:
        chunk_length, chunk_type = struct.unpack_from("<II", raw, offset)
        offset += 8
        chunk = raw[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == JSON_CHUNK:
            document = json.loads(chunk.rstrip(b" \0"))
        elif chunk_type == BIN_CHUNK:
            binary = chunk
    if document is None:
        raise ValueError(f"{path} has no JSON chunk")
    return document, binary


COMPONENT_FORMAT = {5121: "B", 5123: "H", 5125: "I", 5126: "f"}
TYPE_SIZE = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def read_accessor(document: dict, binary: bytes, index: int) -> list[tuple]:
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    component_type = accessor["componentType"]
    count = TYPE_SIZE[accessor["type"]]
    fmt = "<" + COMPONENT_FORMAT[component_type] * count
    packed_size = struct.calcsize(fmt)
    stride = view.get("byteStride", packed_size)
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [struct.unpack_from(fmt, binary, base + row * stride) for row in range(accessor["count"])]


class BufferBuilder:
    def __init__(self) -> None:
        self.data = bytearray()
        self.views: list[dict] = []
        self.accessors: list[dict] = []

    def align(self, boundary: int = 4) -> None:
        while len(self.data) % boundary:
            self.data.append(0)

    def add_accessor(
        self,
        rows: list[tuple] | list[float],
        component_type: int,
        value_type: str,
        *,
        target: int | None = None,
        include_bounds: bool = False,
    ) -> int:
        self.align()
        offset = len(self.data)
        width = TYPE_SIZE[value_type]
        fmt = "<" + COMPONENT_FORMAT[component_type] * width
        normalized_rows: list[tuple] = []
        for row in rows:
            values = (row,) if width == 1 and not isinstance(row, tuple) else tuple(row)
            normalized_rows.append(values)
            self.data.extend(struct.pack(fmt, *values))
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(self.data) - offset}
        if target is not None:
            view["target"] = target
        view_index = len(self.views)
        self.views.append(view)
        accessor = {
            "bufferView": view_index,
            "componentType": component_type,
            "count": len(normalized_rows),
            "type": value_type,
        }
        if include_bounds and normalized_rows:
            accessor["min"] = [min(row[column] for row in normalized_rows) for column in range(width)]
            accessor["max"] = [max(row[column] for row in normalized_rows) for column in range(width)]
        accessor_index = len(self.accessors)
        self.accessors.append(accessor)
        return accessor_index


@dataclass(frozen=True)
class JointDefinition:
    name: str
    parent: int | None
    global_position: tuple[float, float, float]


JOINTS = [
    JointDefinition("root", None, (0.0, 0.0, 0.0)),
    JointDefinition("pelvis", 0, (0.0, 0.91, 0.0)),
    JointDefinition("spine.lower", 1, (0.0, 1.08, 0.0)),
    JointDefinition("spine.upper", 2, (0.0, 1.31, 0.0)),
    JointDefinition("neck", 3, (0.0, 1.49, 0.0)),
    JointDefinition("head", 4, (0.0, 1.61, -0.01)),
    JointDefinition("thigh.left", 1, (-0.105, 0.88, 0.0)),
    JointDefinition("shin.left", 6, (-0.105, 0.49, 0.0)),
    JointDefinition("foot.left", 7, (-0.105, 0.075, 0.0)),
    JointDefinition("toe.left", 8, (-0.105, 0.035, -0.12)),
    JointDefinition("thigh.right", 1, (0.105, 0.88, 0.0)),
    JointDefinition("shin.right", 10, (0.105, 0.49, 0.0)),
    JointDefinition("foot.right", 11, (0.105, 0.075, 0.0)),
    JointDefinition("toe.right", 12, (0.105, 0.035, -0.12)),
    JointDefinition("clavicle.left", 3, (-0.15, 1.40, 0.0)),
    JointDefinition("upper_arm.left", 14, (-0.26, 1.37, 0.0)),
    JointDefinition("forearm.left", 15, (-0.50, 1.18, 0.0)),
    JointDefinition("hand.left", 16, (-0.73, 1.04, 0.0)),
    JointDefinition("clavicle.right", 3, (0.15, 1.40, 0.0)),
    JointDefinition("upper_arm.right", 18, (0.26, 1.37, 0.0)),
    JointDefinition("forearm.right", 19, (0.50, 1.18, 0.0)),
    JointDefinition("hand.right", 20, (0.73, 1.04, 0.0)),
]

J = {joint.name: index for index, joint in enumerate(JOINTS)}


def subtract(a, b):
    return tuple(a[index] - b[index] for index in range(3))


def add(a, b):
    return tuple(a[index] + b[index] for index in range(3))


def multiply_vector(vector, amount):
    return tuple(component * amount for component in vector)


def dot(a, b):
    return sum(a[index] * b[index] for index in range(3))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def unit(vector):
    length = math.sqrt(dot(vector, vector)) or 1.0
    return multiply_vector(vector, 1.0 / length)


def rotate_between(vector, source_direction, target_direction):
    source = unit(source_direction)
    target = unit(target_direction)
    cosine = max(-1.0, min(1.0, dot(source, target)))
    if cosine > 0.999999:
        return vector
    if cosine < -0.999999:
        axis = unit(cross(source, (1.0, 0.0, 0.0)
                          if abs(source[0]) < 0.9 else (0.0, 1.0, 0.0)))
        return add(multiply_vector(axis, 2.0 * dot(axis, vector)),
                   multiply_vector(vector, -1.0))
    axis = cross(source, target)
    sine = math.sqrt(dot(axis, axis))
    axis = multiply_vector(axis, 1.0 / sine)
    return add(add(multiply_vector(vector, cosine),
                   multiply_vector(cross(axis, vector), sine)),
               multiply_vector(axis, dot(axis, vector) * (1.0 - cosine)))


def target_segment(name):
    child_by_name = {
        "pelvis": "spine.lower", "spine.lower": "spine.upper",
        "spine.upper": "neck", "neck": "head",
        "thigh.left": "shin.left", "shin.left": "foot.left",
        "thigh.right": "shin.right", "shin.right": "foot.right",
        "clavicle.left": "upper_arm.left", "upper_arm.left": "forearm.left",
        "forearm.left": "hand.left", "clavicle.right": "upper_arm.right",
        "upper_arm.right": "forearm.right", "forearm.right": "hand.right",
    }
    if name in ("foot.left", "foot.right"):
        x = JOINTS[J[name]].global_position[0]
        inward = 0.0159 if name.endswith("left") else -0.0159
        return (x, 0.040, -0.14), (x + inward, 0.035, 0.10)
    if name in ("toe.left", "toe.right"):
        x = JOINTS[J[name]].global_position[0]
        inward = 0.0159 if name.endswith("left") else -0.0159
        return (x + inward, 0.035, 0.10), (x + inward * 1.55, 0.025, 0.25)
    if name == "head":
        # Preserve the source crown volume while landing its highest vertex at
        # the canonical 1.75 m height instead of extending above the guide.
        return JOINTS[J[name]].global_position, (0.0, 1.7338, -0.01)
    if name in ("hand.left", "hand.right"):
        head = JOINTS[J[name]].global_position
        parent = JOINTS[JOINTS[J[name]].parent].global_position
        return head, add(head, multiply_vector(unit(subtract(head, parent)), 0.075))
    child_name = child_by_name.get(name)
    if child_name:
        return JOINTS[J[name]].global_position, JOINTS[J[child_name]].global_position
    return None


def retarget_rows(rows, source_joints):
    segments = {}
    for name, source in source_joints.items():
        target = target_segment(name)
        if target:
            segments[name] = (tuple(source["head"]), tuple(source["tail"]), target[0], target[1])

    output_positions = []
    output_normals = []
    for row in rows:
        accumulated_position = (0.0, 0.0, 0.0)
        accumulated_normal = (0.0, 0.0, 0.0)
        total = 0.0
        for name, weight in row["weights"].items():
            segment = segments.get(name)
            if not segment or weight <= 0.0:
                continue
            source_head, source_tail, target_head, target_tail = segment
            source_direction = subtract(source_tail, source_head)
            target_direction = subtract(target_tail, target_head)
            relative = subtract(tuple(row["position"]), source_head)
            rotated = rotate_between(relative, source_direction, target_direction)
            target_axis = unit(target_direction)
            axial = dot(rotated, target_axis)
            length_ratio = (math.sqrt(dot(target_direction, target_direction))
                            / max(0.000001, math.sqrt(dot(source_direction, source_direction))))
            perpendicular = subtract(rotated, multiply_vector(target_axis, axial))
            moved = add(target_head, add(perpendicular,
                                         multiply_vector(target_axis, axial * length_ratio)))
            moved_normal = rotate_between(tuple(row["normal"]), source_direction, target_direction)
            accumulated_position = add(accumulated_position, multiply_vector(moved, weight))
            accumulated_normal = add(accumulated_normal, multiply_vector(moved_normal, weight))
            total += weight
        if total < 0.000001:
            output_positions.append(tuple(row["position"]))
            output_normals.append(tuple(row["normal"]))
        else:
            output_positions.append(multiply_vector(accumulated_position, 1.0 / total))
            output_normals.append(unit(accumulated_normal))
    def sole_profile(z):
        points = ((-0.11, 0.029), (0.0, 0.010), (0.095, 0.0),
                  (0.18, 0.003), (0.30, 0.003))
        if z <= points[0][0]:
            return points[0][1]
        for index in range(1, len(points)):
            if z <= points[index][0]:
                first, second = points[index - 1], points[index]
                amount = (z - first[0]) / (second[0] - first[0])
                return first[1] + (second[1] - first[1]) * amount
        return points[-1][1]

    flattened_positions = []
    for position, row in zip(output_positions, rows):
        foot_weight = sum(weight for name, weight in row["weights"].items()
                          if name.startswith("foot.") or name.startswith("toe."))
        if foot_weight > 0.55:
            position = (position[0], position[1] - sole_profile(position[2]), position[2])
        flattened_positions.append(position)
    output_positions = flattened_positions
    minimum_y = min(position[1] for position in output_positions)
    output_positions = [(x, y - minimum_y, z) for x, y, z in output_positions]
    return output_positions, output_normals


def normalized_pair(first: int, second: int, amount: float) -> tuple[tuple[int, int, int, int], tuple[float, float, float, float]]:
    amount = max(0.0, min(1.0, amount))
    return (first, second, 0, 0), (1.0 - amount, amount, 0.0, 0.0)


def segment_weights(position: tuple[float, float, float]) -> tuple[tuple[int, int, int, int], tuple[float, float, float, float]]:
    x, y, _ = position
    absolute_x = abs(x)

    # Arms are in a relaxed A-pose.  Blend along the visible joint chain while
    # keeping shoulder/chest vertices on the torso rather than the arm.
    if (y > 0.97 and absolute_x > 0.235) or (y > 0.80 and absolute_x > 0.52):
        side = "left" if x < 0.0 else "right"
        if absolute_x < 0.30:
            return normalized_pair(J[f"clavicle.{side}"], J[f"upper_arm.{side}"], (absolute_x - 0.235) / 0.065)
        if absolute_x < 0.52:
            return normalized_pair(J[f"upper_arm.{side}"], J[f"forearm.{side}"], (absolute_x - 0.30) / 0.22)
        if absolute_x < 0.74:
            return normalized_pair(J[f"forearm.{side}"], J[f"hand.{side}"], (absolute_x - 0.52) / 0.22)
        return (J[f"hand.{side}"], 0, 0, 0), (1.0, 0.0, 0.0, 0.0)

    if y < 0.96:
        side = "left" if x < 0.0 else "right"
        if y > 0.82 and absolute_x < 0.07:
            return normalized_pair(J["pelvis"], J[f"thigh.{side}"], (0.96 - y) / 0.14)
        if y > 0.49:
            return normalized_pair(J[f"thigh.{side}"], J[f"shin.{side}"], (0.88 - y) / 0.39)
        if y > 0.085:
            return normalized_pair(J[f"shin.{side}"], J[f"foot.{side}"], (0.49 - y) / 0.405)
        return normalized_pair(J[f"foot.{side}"], J[f"toe.{side}"], min(1.0, max(0.0, -position[2] / 0.18)))

    if y < 1.10:
        return normalized_pair(J["pelvis"], J["spine.lower"], (y - 0.96) / 0.14)
    if y < 1.32:
        return normalized_pair(J["spine.lower"], J["spine.upper"], (y - 1.10) / 0.22)
    if y < 1.49:
        return normalized_pair(J["spine.upper"], J["neck"], (y - 1.32) / 0.17)
    if y < 1.58:
        return normalized_pair(J["neck"], J["head"], (y - 1.49) / 0.09)
    return (J["head"], 0, 0, 0), (1.0, 0.0, 0.0, 0.0)


def quaternion(axis: str, degrees: float) -> tuple[float, float, float, float]:
    angle = math.radians(degrees) * 0.5
    sine = math.sin(angle)
    components = {"x": (sine, 0.0, 0.0), "y": (0.0, sine, 0.0), "z": (0.0, 0.0, sine)}[axis]
    return components[0], components[1], components[2], math.cos(angle)


def quaternion_euler(x_degrees: float, y_degrees: float, z_degrees: float) -> tuple[float, float, float, float]:
    def product(a, b):
        ax, ay, az, aw = a
        bx, by, bz, bw = b
        return (aw*bx + ax*bw + ay*bz - az*by,
                aw*by - ax*bz + ay*bw + az*bx,
                aw*bz + ax*by - ay*bx + az*bw,
                aw*bw - ax*bx - ay*by - az*bz)
    return product(product(quaternion("z", z_degrees), quaternion("y", y_degrees)), quaternion("x", x_degrees))


def animation_channels(
    builder: BufferBuilder,
    name: str,
    rotations: dict[str, tuple[str, list[float]]],
    times_accessor: int,
) -> dict:
    samplers = []
    channels = []
    for joint_name, (axis, degrees) in rotations.items():
        output = builder.add_accessor([
            quaternion_euler(*value) if axis == "xyz" else quaternion(axis, value)
            for value in degrees
        ], 5126, "VEC4")
        sampler = len(samplers)
        samplers.append({"input": times_accessor, "interpolation": "LINEAR", "output": output})
        channels.append({"sampler": sampler, "target": {"node": J[joint_name], "path": "rotation"}})
    return {"name": name, "samplers": samplers, "channels": channels}


def gait_rotations(axis: str, amplitude: float, *, backward: bool = False) -> dict[str, tuple[str, list[float]]]:
    sign = -1.0 if backward else 1.0
    left = [sign * amplitude, 0.0, -sign * amplitude, 0.0, sign * amplitude]
    right = [-value for value in left]
    rotations: dict[str, tuple[str, list[float]]] = {
        "thigh.left": (axis, left),
        "thigh.right": (axis, right),
    }
    if axis == "x":
        bend = max(4.0, amplitude * 0.30)
        rotations.update(
            {
                "shin.left": ("x", [0.0, -bend, -3.0, -bend * 0.65, 0.0]),
                "shin.right": ("x", [-3.0, -bend * 0.65, 0.0, -bend, -3.0]),
                "foot.left": ("x", [14.0 if not backward else 2.0, 4.0, -4.0, 3.0, 14.0 if not backward else 2.0]),
                "foot.right": ("x", [-4.0, 3.0, 14.0 if not backward else 2.0, 4.0, -4.0]),
                "upper_arm.left": ("xyz", [(-value * 0.28, 0.0, 50.0) for value in left]),
                "upper_arm.right": ("xyz", [(-value * 0.28, 0.0, -50.0) for value in right]),
                "forearm.left": ("x", [-5.0, -7.0, -5.0, -3.0, -5.0]),
                "forearm.right": ("x", [-5.0, -3.0, -5.0, -7.0, -5.0]),
            }
        )
    else:
        # Slides keep the torso facing the authored direction while the legs
        # reach laterally and the hips absorb a small, controlled counter-twist.
        rotations.update(
            {
                "shin.left": ("z", [0.0, -amplitude * 0.12, 0.0, amplitude * 0.08, 0.0]),
                "shin.right": ("z", [0.0, amplitude * 0.08, 0.0, -amplitude * 0.12, 0.0]),
                "pelvis": ("y", [-4.0, -2.0, 4.0, 2.0, -4.0]),
                "spine.upper": ("y", [3.0, 1.0, -3.0, -1.0, 3.0]),
                "upper_arm.left": ("xyz", [(0.0, 0.0, 50.0)] * 5),
                "upper_arm.right": ("xyz", [(0.0, 0.0, -50.0)] * 5),
            }
        )
    return rotations


def build_animations(builder: BufferBuilder) -> list[dict]:
    times = builder.add_accessor([(0.0,), (0.25,), (0.5,), (0.75,), (1.0,)], 5126, "SCALAR", include_bounds=True)
    animations: list[dict] = []
    still = {
        "pelvis": ("y", [0.0] * 5),
        "upper_arm.left": ("xyz", [(0.0, 0.0, 50.0)] * 5),
        "upper_arm.right": ("xyz", [(0.0, 0.0, -50.0)] * 5),
    }
    animations.append(animation_channels(builder, "idle", still, times))

    for label, amplitude in (("half", 12.0), ("standard", 24.0), ("extended", 32.0)):
        animations.append(animation_channels(builder, f"march.forward.{label}", gait_rotations("x", amplitude), times))
        animations.append(animation_channels(builder, f"march.backward.{label}", gait_rotations("x", amplitude * 0.78, backward=True), times))
        slide = gait_rotations("z", amplitude * 0.78)
        animations.append(animation_channels(builder, f"slide.left.{label}", slide, times))
        mirrored = {joint: (axis, [-value for value in values] if axis == "z" else values) for joint, (axis, values) in slide.items()}
        animations.append(animation_channels(builder, f"slide.right.{label}", mirrored, times))

    mark_time = {
        "thigh.left": ("x", [4.0, 0.0, -4.0, 0.0, 4.0]),
        "thigh.right": ("x", [-4.0, 0.0, 4.0, 0.0, -4.0]),
        "shin.left": ("x", [-10.0, -3.0, 0.0, -3.0, -10.0]),
        "shin.right": ("x", [0.0, -3.0, -10.0, -3.0, 0.0]),
        "foot.left": ("x", [-5.0, 0.0, 0.0, 0.0, -5.0]),
        "foot.right": ("x", [0.0, 0.0, -5.0, 0.0, 0.0]),
        "upper_arm.left": ("xyz", [(0.0, 0.0, 50.0)] * 5),
        "upper_arm.right": ("xyz", [(0.0, 0.0, -50.0)] * 5),
    }
    animations.append(animation_channels(builder, "mark_time", mark_time, times))
    direction_change = {
        "pelvis": ("y", [0.0, 8.0, 0.0, -8.0, 0.0]),
        "foot.left": ("y", [0.0, 18.0, 35.0, 18.0, 0.0]),
        "foot.right": ("y", [0.0, -18.0, -35.0, -18.0, 0.0]),
        "upper_arm.left": ("xyz", [(0.0, 0.0, 50.0)] * 5),
        "upper_arm.right": ("xyz", [(0.0, 0.0, -50.0)] * 5),
    }
    animations.append(animation_channels(builder, "direction_change", direction_change, times))
    horn_up = {
        "upper_arm.left": ("xyz", [(-52.0, 0.0, 32.0)] * 5),
        "upper_arm.right": ("xyz", [(-52.0, 0.0, -32.0)] * 5),
        "forearm.left": ("x", [-72.0] * 5),
        "forearm.right": ("x", [-72.0] * 5),
    }
    animations.append(animation_channels(builder, "horn.up", horn_up, times))
    animations.append(animation_channels(builder, "horn.down", still, times))
    return animations


def simplify_geometry(positions, normals, texcoords, joints, weights, indices, grid_size):
    clusters = {}
    remap = []
    for index, position in enumerate(positions):
        key = (round(position[0] / grid_size), round(position[1] / grid_size),
               round(position[2] / grid_size), joints[index][0])
        cluster = clusters.get(key)
        if cluster is None:
            cluster = {"members": [], "index": len(clusters)}
            clusters[key] = cluster
        cluster["members"].append(index)
        remap.append(cluster["index"])

    simplified_positions = []
    simplified_normals = []
    simplified_texcoords = []
    simplified_joints = []
    simplified_weights = []
    for cluster in clusters.values():
        members = cluster["members"]
        count = len(members)
        position = tuple(sum(positions[index][axis] for index in members) / count for axis in range(3))
        normal = tuple(sum(normals[index][axis] for index in members) / count for axis in range(3))
        length = math.sqrt(sum(value * value for value in normal)) or 1.0
        normal = tuple(value / length for value in normal)
        texcoord = tuple(sum(texcoords[index][axis] for index in members) / count for axis in range(2))
        influence_totals = {}
        for index in members:
            for joint, weight in zip(joints[index], weights[index]):
                if weight > 0.0:
                    influence_totals[joint] = influence_totals.get(joint, 0.0) + weight
        influences = sorted(influence_totals.items(), key=lambda item: item[1], reverse=True)[:4]
        total = sum(weight for _, weight in influences) or 1.0
        joint_row = tuple([joint for joint, _ in influences] + [0] * (4 - len(influences)))
        weight_row = tuple([weight / total for _, weight in influences] + [0.0] * (4 - len(influences)))
        simplified_positions.append(position)
        simplified_normals.append(normal)
        simplified_texcoords.append(texcoord)
        simplified_joints.append(joint_row)
        simplified_weights.append(weight_row)

    minimum_y = min(position[1] for position in simplified_positions)
    maximum_y = max(position[1] for position in simplified_positions)
    vertical_scale = 1.75 / (maximum_y - minimum_y)
    simplified_positions = [(position[0], (position[1] - minimum_y) * vertical_scale, position[2])
                            for position in simplified_positions]

    simplified_indices = []
    seen = set()
    for triangle in range(0, len(indices), 3):
        mapped = tuple(remap[indices[triangle + offset]] for offset in range(3))
        if len(set(mapped)) < 3:
            continue
        canonical = tuple(sorted(mapped))
        if canonical in seen:
            continue
        seen.add(canonical)
        simplified_indices.extend(mapped)
    return (simplified_positions, simplified_normals, simplified_texcoords,
            simplified_joints, simplified_weights, simplified_indices)


def build(source: pathlib.Path, destination: pathlib.Path, grid_size: float = 0.0) -> dict:
    source_data = json.loads(source.read_text(encoding="utf-8"))
    if source_data.get("format") != "marchcraft.weighted-mesh.v1":
        raise ValueError(f"{source} is not a MarchCraft weighted-mesh interchange file")
    rows = source_data["vertices"]
    positions, normals = retarget_rows(rows, source_data["sourceJoints"])
    texcoords = [tuple(row["texcoord"]) for row in rows]
    indices = list(source_data["indices"])
    source_height = float(source_data["sourceHeight"])
    scale = float(source_data["scale"])

    joints = []
    weights = []
    for row in rows:
        influences = sorted(
            ((J[name], float(weight)) for name, weight in row["weights"].items()),
            key=lambda item: item[1], reverse=True)[:4]
        total = sum(weight for _, weight in influences) or 1.0
        joints.append(tuple([joint for joint, _ in influences] + [0] * (4 - len(influences))))
        weights.append(tuple([weight / total for _, weight in influences]
                             + [0.0] * (4 - len(influences))))

    if grid_size > 0.0:
        positions, normals, texcoords, joints, weights, indices = simplify_geometry(
            positions, normals, texcoords, joints, weights, indices, grid_size)

    builder = BufferBuilder()
    position_accessor = builder.add_accessor(positions, 5126, "VEC3", target=ARRAY_BUFFER, include_bounds=True)
    normal_accessor = builder.add_accessor(normals, 5126, "VEC3", target=ARRAY_BUFFER)
    texcoord_accessor = builder.add_accessor(texcoords, 5126, "VEC2", target=ARRAY_BUFFER)
    joint_accessor = builder.add_accessor(joints, 5123, "VEC4", target=ARRAY_BUFFER)
    weight_accessor = builder.add_accessor(weights, 5126, "VEC4", target=ARRAY_BUFFER)
    index_accessor = builder.add_accessor(indices, 5125, "SCALAR", target=ELEMENT_ARRAY_BUFFER)

    inverse_bind_matrices = []
    for joint in JOINTS:
        x, y, z = joint.global_position
        inverse_bind_matrices.append((1.0, 0.0, 0.0, 0.0,
                                      0.0, 1.0, 0.0, 0.0,
                                      0.0, 0.0, 1.0, 0.0,
                                      -x, -y, -z, 1.0))
    inverse_bind_accessor = builder.add_accessor(inverse_bind_matrices, 5126, "MAT4")

    nodes = []
    for index, joint in enumerate(JOINTS):
        if joint.parent is None:
            translation = joint.global_position
        else:
            parent = JOINTS[joint.parent].global_position
            translation = tuple(joint.global_position[axis] - parent[axis] for axis in range(3))
        node = {"name": joint.name, "translation": list(translation)}
        children = [child for child, candidate in enumerate(JOINTS) if candidate.parent == index]
        if children:
            node["children"] = children
        nodes.append(node)
    mesh_node = len(nodes)
    nodes.append({"name": "HumanPerformerMesh", "mesh": 0, "skin": 0})

    animations = build_animations(builder)
    document = {
        "asset": {
            "version": "2.0",
            "generator": "MarchCraft human performer conditioner v2",
            "copyright": "Rigged Male Human by aaravanimates; used with creator permission",
            "extras": {
                "author": "aaravanimates",
                "license": "Creator-Permission",
                "source": "https://free3d.com/3d-model/rigged-male-human-442626.html",
                "canonicalHeightMeters": 1.75,
                "upAxis": "+Y",
                "forwardAxis": "-Z",
                "groundPlaneY": 0.0,
            },
        },
        "scene": 0,
        "scenes": [{"name": "HumanPerformer", "nodes": [0, mesh_node]}],
        "nodes": nodes,
        "meshes": [{
            "name": "HumanPerformer",
            "primitives": [{
                "attributes": {
                    "POSITION": position_accessor,
                    "NORMAL": normal_accessor,
                    "TEXCOORD_0": texcoord_accessor,
                    "JOINTS_0": joint_accessor,
                    "WEIGHTS_0": weight_accessor,
                },
                "indices": index_accessor,
                "material": 0,
                "mode": 4,
            }],
        }],
        "skins": [{
            "name": "marchcraft.canonical.v1",
            "inverseBindMatrices": inverse_bind_accessor,
            "skeleton": 0,
            "joints": list(range(len(JOINTS))),
        }],
        "materials": [{
            "name": "skin.medium",
            "doubleSided": False,
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.50, 0.29, 0.20, 1.0],
                "metallicFactor": 0.0,
                "roughnessFactor": 0.72,
            },
        }],
        "animations": animations,
        "buffers": [{"byteLength": len(builder.data)}],
        "bufferViews": builder.views,
        "accessors": builder.accessors,
    }

    destination.parent.mkdir(parents=True, exist_ok=True)
    json_bytes = json.dumps(document, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "
    builder.align()
    binary_bytes = bytes(builder.data)
    total_length = 12 + 8 + len(json_bytes) + 8 + len(binary_bytes)
    output = bytearray(struct.pack("<III", 0x46546C67, 2, total_length))
    output.extend(struct.pack("<II", len(json_bytes), JSON_CHUNK))
    output.extend(json_bytes)
    output.extend(struct.pack("<II", len(binary_bytes), BIN_CHUNK))
    output.extend(binary_bytes)
    destination.write_bytes(output)
    return {
        "sourceHeight": source_height,
        "scale": scale,
        "vertices": len(positions),
        "triangles": len(indices) // 3,
        "joints": len(JOINTS),
        "animations": [animation["name"] for animation in animations],
        "outputBytes": len(output),
        "gridSize": grid_size,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    arguments = parser.parse_args()
    reports = [build(arguments.source, arguments.destination)]
    reports.append(build(arguments.source,
                         arguments.destination.with_name(arguments.destination.stem + "_lod1.glb"), 0.012))
    reports.append(build(arguments.source,
                         arguments.destination.with_name(arguments.destination.stem + "_lod2.glb"), 0.032))
    print(json.dumps(reports, indent=2))


if __name__ == "__main__":
    main()
