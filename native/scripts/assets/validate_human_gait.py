#!/usr/bin/env python3
"""Validate grounded skin deformation across a complete two-count gait."""

from __future__ import annotations

import argparse
import json
import pathlib

from build_human_performer import read_accessor, read_glb
from render_human_performer_preview import pose_matrices, skinned_vertices


def validate(source: pathlib.Path, samples: int) -> dict:
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])

    report = {}
    for mode in ("march.forward", "march.backward", "slide.left", "slide.right"):
        ground_contacts = []
        heights = []
        for sample in range(samples):
            phase = sample / samples
            vertices = skinned_vertices(positions, joints, weights,
                                        pose_matrices(mode, phase))
            ground_contacts.append(min(vertex[1] for vertex in vertices))
            heights.append(max(vertex[1] for vertex in vertices))
        lowest = min(ground_contacts)
        highest_contact = max(ground_contacts)
        height_variation = max(heights) - min(heights)
        assert lowest >= -0.002, f"{mode} penetrates turf by {-lowest:.4f} m"
        assert highest_contact <= 0.002, f"{mode} floats by {highest_contact:.4f} m"
        assert height_variation <= 0.015, f"{mode} upper body bobs {height_variation:.4f} m"
        report[mode] = {
            "minimumGroundContactMeters": round(lowest, 6),
            "maximumGroundContactMeters": round(highest_contact, 6),
            "heightVariationMeters": round(height_variation, 6),
        }
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--samples", type=int, default=32)
    arguments = parser.parse_args()
    print(json.dumps(validate(arguments.source, arguments.samples), indent=2))


if __name__ == "__main__":
    main()
