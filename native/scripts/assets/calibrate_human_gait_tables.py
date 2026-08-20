#!/usr/bin/env python3
"""Recalculate cardinal sole-height tables for the current procedural gait."""

from __future__ import annotations

import pathlib
import sys

import render_human_performer_preview as preview
from build_human_performer import read_accessor, read_glb


def main() -> None:
    source = pathlib.Path(sys.argv[1])
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])
    # Ground contact can only come from the lower-leg and foot portion of the
    # source mesh. Restrict calibration to that subset; the validator still
    # deforms and checks the complete performer mesh afterward.
    ground_rows = [(position, joint_row, weight_row)
                   for position, joint_row, weight_row in zip(positions, joints, weights)
                   if position[1] < 0.30]
    positions = [row[0] for row in ground_rows]
    joints = [row[1] for row in ground_rows]
    weights = [row[2] for row in ground_rows]

    angles = (0, 45, 90, 135, 180, 225, 270, 315)
    sole_tables = [[0.0] * 32 for _ in angles]
    for _ in range(3):
        preview.SOLE_TABLES = tuple(tuple(table) for table in sole_tables)
        for table_index, angle in enumerate(angles):
            for sample in range(32):
                phase = sample / 32.0
                vertices = preview.skinned_vertices(
                    positions, joints, weights,
                    preview.pose_matrices(f"direction.{angle}", phase, 0.5715))
                sole_tables[table_index][sample] -= min(vertex[1] for vertex in vertices)
    print("soleTables = [")
    for table in sole_tables:
        print("  [" + ",".join(f"{value:.5f}" for value in table) + "],")
    print("]")

    def calibrate(stride: float, attribute: str, target_angles=angles):
        corrections = [[0.0] * 32 for _ in target_angles]
        for _ in range(3):
            setattr(preview, attribute, tuple(tuple(table) for table in corrections))
            for table_index, angle in enumerate(target_angles):
                for sample in range(32):
                    phase = sample / 32.0
                    vertices = preview.skinned_vertices(
                        positions, joints, weights,
                        preview.pose_matrices(f"direction.{angle}", phase, stride))
                    corrections[table_index][sample] -= min(vertex[1] for vertex in vertices)
        return corrections

    for label, corrections in (
        ("halfStrideCorrections", calibrate(0.28575, "HALF_STRIDE_CORRECTIONS")),
        ("extendedStrideCorrections", calibrate(0.70, "EXTENDED_STRIDE_CORRECTIONS")),
        ("halfBackDiagonalCorrections", calibrate(
            0.28575, "HALF_BACK_DIAGONAL_CORRECTIONS", (105, 120, 240, 255))),
        ("extendedBackDiagonalCorrections", calibrate(
            0.70, "EXTENDED_BACK_DIAGONAL_CORRECTIONS", (105, 120, 240, 255))),
    ):
        print(f"{label} = [")
        for table in corrections:
            print("  [" + ",".join(f"{value:.5f}" for value in table) + "],")
        print("]")


if __name__ == "__main__":
    main()
