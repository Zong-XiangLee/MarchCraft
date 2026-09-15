#!/usr/bin/env python3
"""Validate the runtime human GLB contract without third-party dependencies."""

from __future__ import annotations

import math
import pathlib
import sys

from build_human_performer import read_accessor, read_glb


REQUIRED_ANIMATIONS = {
    "idle",
    "march.forward.half",
    "march.forward.standard",
    "march.forward.extended",
    "march.backward.half",
    "march.backward.standard",
    "march.backward.extended",
    "slide.left.standard",
    "slide.right.standard",
    "mark_time",
    "direction_change",
    "horn.up",
    "horn.down",
}


def main() -> None:
    path = pathlib.Path(sys.argv[1])
    document, binary = read_glb(path)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    assert {"POSITION", "NORMAL", "COLOR_0", "JOINTS_0", "WEIGHTS_0"} <= set(attributes)
    position = document["accessors"][attributes["POSITION"]]
    color = document["accessors"][attributes["COLOR_0"]]
    assert color["count"] == position["count"] and color["type"] == "VEC4"
    assert abs(position["min"][1]) <= 0.002, position["min"]
    assert abs(position["max"][1] - 1.75) <= 0.002, position["max"]
    assert 0 < position["count"] <= 18057
    indices = read_accessor(document, binary, primitive["indices"])
    assert len(indices) % 3 == 0 and 1000 <= len(indices) // 3 <= 12000
    assert all(0 <= int(row[0]) < position["count"] for row in indices)
    assert len(document["skins"][0]["joints"]) >= 20
    joint_count = len(document["skins"][0]["joints"])
    for row in read_accessor(document, binary, attributes["POSITION"]):
        assert all(math.isfinite(value) for value in row)
    joint_rows = read_accessor(document, binary, attributes["JOINTS_0"])
    weight_rows = read_accessor(document, binary, attributes["WEIGHTS_0"])
    assert len(joint_rows) == len(weight_rows) == position["count"]
    for joint_row, weight_row in zip(joint_rows, weight_rows):
        assert all(0 <= joint < joint_count for joint in joint_row)
        assert all(math.isfinite(weight) and 0 <= weight <= 1 for weight in weight_row)
        assert abs(sum(weight_row) - 1) < 0.0001
    animation_names = {animation["name"] for animation in document["animations"]}
    assert REQUIRED_ANIMATIONS <= animation_names, REQUIRED_ANIMATIONS - animation_names
    extras = document["asset"]["extras"]
    assert extras["license"] == "Creator-Permission"
    assert extras["upAxis"] == "+Y" and extras["forwardAxis"] == "-Z"
    assert document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] == 0.0
    print(f"validated {path}: {position['count']} vertices, {len(document['animations'])} clips")


if __name__ == "__main__":
    main()
