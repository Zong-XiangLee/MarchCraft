#!/usr/bin/env python3
"""Validate the runtime human GLB contract without third-party dependencies."""

from __future__ import annotations

import json
import pathlib
import struct
import sys


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


def load_document(path: pathlib.Path) -> dict:
    raw = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", raw, 0)
    assert magic == 0x46546C67 and version == 2 and length == len(raw)
    offset = 12
    while offset < length:
        chunk_length, chunk_type = struct.unpack_from("<II", raw, offset)
        offset += 8
        chunk = raw[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == 0x4E4F534A:
            return json.loads(chunk.rstrip(b" \0"))
    raise AssertionError("missing JSON chunk")


def main() -> None:
    path = pathlib.Path(sys.argv[1])
    document = load_document(path)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    assert {"POSITION", "NORMAL", "COLOR_0", "JOINTS_0", "WEIGHTS_0"} <= set(attributes)
    position = document["accessors"][attributes["POSITION"]]
    color = document["accessors"][attributes["COLOR_0"]]
    assert color["count"] == position["count"] and color["type"] == "VEC4"
    assert abs(position["min"][1]) <= 0.002, position["min"]
    assert abs(position["max"][1] - 1.75) <= 0.002, position["max"]
    assert 1000 <= position["count"] <= 18057
    assert len(document["skins"][0]["joints"]) >= 20
    animation_names = {animation["name"] for animation in document["animations"]}
    assert REQUIRED_ANIMATIONS <= animation_names, REQUIRED_ANIMATIONS - animation_names
    extras = document["asset"]["extras"]
    assert extras["license"] == "Creator-Permission"
    assert extras["upAxis"] == "+Y" and extras["forwardAxis"] == "-Z"
    assert document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] == 0.0
    print(f"validated {path}: {position['count']} vertices, {len(document['animations'])} clips")


if __name__ == "__main__":
    main()
