#!/usr/bin/env python3
"""Extract the authorized low-poly character mesh and source-rig weights.

Run this script through Blender. It emits a deterministic JSON interchange file
consumed by build_human_performer.py; the JSON is a build intermediate and is
not committed.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


GROUP_TO_CANONICAL = {
    "hips": "pelvis",
    "spine": "spine.lower",
    "chest": "spine.upper",
    "neck": "neck",
    "head": "head",
    "shoulder.L": "clavicle.left",
    "upper_arm.L": "upper_arm.left",
    "forearm.L": "forearm.left",
    "hand.L": "hand.left",
    "shoulder.R": "clavicle.right",
    "upper_arm.R": "upper_arm.right",
    "forearm.R": "forearm.right",
    "hand.R": "hand.right",
    "thigh.L": "thigh.left",
    "shin.L": "shin.left",
    "foot.L": "foot.left",
    "toe.L": "toe.left",
    "thigh.R": "thigh.right",
    "shin.R": "shin.right",
    "foot.R": "foot.right",
    "toe.R": "toe.right",
}


def canonical_group(group_name):
    mapped = GROUP_TO_CANONICAL.get(group_name)
    if mapped:
        return mapped
    if group_name.startswith(("palm.", "f_", "thumb.")):
        return "hand.left" if group_name.endswith(".L") else "hand.right"
    return None


def transformed(vector):
    # Proper rotation: Blender +Z up / -Y forward -> MarchCraft +Y up / -Z forward.
    return Vector((-vector.x, vector.z, vector.y))


def normalized(values):
    length = math.sqrt(sum(component * component for component in values)) or 1.0
    return [component / length for component in values]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])

    mesh_objects = [bpy.data.objects.get("body"), bpy.data.objects.get("Sphere"),
                    bpy.data.objects.get("Sphere.001")]
    mesh_objects = [obj for obj in mesh_objects if obj and obj.type == "MESH"]
    if not mesh_objects or mesh_objects[0].name != "body":
        raise RuntimeError("Expected the low-poly character body mesh")

    rows = []
    indices = []
    for obj in mesh_objects:
        mesh = obj.data
        mesh.calc_loop_triangles()
        normal_matrix = obj.matrix_world.to_3x3().inverted().transposed()
        groups_by_index = {group.index: group.name for group in obj.vertex_groups}

        for triangle in mesh.loop_triangles:
            for loop_index in triangle.loops:
                loop = mesh.loops[loop_index]
                vertex = mesh.vertices[loop.vertex_index]
                position = transformed(obj.matrix_world @ vertex.co)
                normal = transformed(normal_matrix @ vertex.normal)
                influences = {}
                for membership in vertex.groups:
                    target = canonical_group(groups_by_index.get(membership.group, ""))
                    if target:
                        influences[target] = influences.get(target, 0.0) + membership.weight
                if not influences:
                    influences["head" if obj.name.startswith("Sphere") else "root"] = 1.0
                rows.append({
                    "position": list(position),
                    "normal": normalized(normal),
                    "texcoord": [0.0, 0.0],
                    "weights": influences,
                })
                indices.append(len(rows) - 1)

    minimum_y = min(row["position"][1] for row in rows)
    maximum_y = max(row["position"][1] for row in rows)
    source_height = maximum_y - minimum_y
    scale = 1.75 / source_height
    for row in rows:
        row["position"] = [
            row["position"][0] * scale,
            (row["position"][1] - minimum_y) * scale,
            row["position"][2] * scale,
        ]

    armature = bpy.data.objects.get("metarig")
    if not armature or armature.type != "ARMATURE":
        raise RuntimeError("Expected the low-poly character metarig")
    source_joints = {}
    for source_name, target_name in GROUP_TO_CANONICAL.items():
        bone = armature.data.bones.get(source_name)
        if not bone:
            continue
        head = transformed(armature.matrix_world @ bone.head_local)
        tail = transformed(armature.matrix_world @ bone.tail_local)
        source_joints[target_name] = {
            "head": [head.x * scale, (head.y - minimum_y) * scale, head.z * scale],
            "tail": [tail.x * scale, (tail.y - minimum_y) * scale, tail.z * scale],
        }

    report = {
        "format": "marchcraft.weighted-mesh.v1",
        "source": bpy.data.filepath,
        "sourceHeight": source_height,
        "scale": scale,
        "sourceJoints": source_joints,
        "vertices": rows,
        "indices": indices,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, separators=(",", ":")), encoding="utf-8")
    print(json.dumps({
        "output": str(args.output),
        "vertices": len(rows),
        "triangles": len(indices) // 3,
        "sourceHeight": source_height,
        "scale": scale,
    }, indent=2))


if __name__ == "__main__":
    main()
