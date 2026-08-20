#!/usr/bin/env python3
"""Recalculate cardinal sole-height tables for the current procedural gait."""

from __future__ import annotations

import argparse
import pathlib
import re

import render_human_performer_preview as preview
from build_human_performer import read_accessor, read_glb


def js_rows(rows) -> str:
    return "[\n" + "\n".join(
        "    [" + ",".join(f"{value:.5f}" for value in row) + "]," for row in rows
    ) + "\n]"


def python_rows(rows) -> str:
    return "(\n" + "\n".join(
        "    (" + ", ".join(f"{value:.5f}" for value in row) + ")," for row in rows
    ) + "\n)"


def write_tables(js_path: pathlib.Path, preview_path: pathlib.Path,
                 sole_tables, correction_tables) -> None:
    js = js_path.read_text(encoding="utf-8")
    for angle, row in zip((0, 45, 90, 135, 180, 225, 270, 315), sole_tables):
        js = re.sub(rf"var sole{angle} = \[[^\n]*\]",
                    f"var sole{angle} = [" + ",".join(f"{value:.5f}" for value in row) + "]", js)
    for label, rows in correction_tables.items():
        js = re.sub(rf"var {label} = \[.*?\n\]", f"var {label} = {js_rows(rows)}",
                    js, flags=re.DOTALL)
    js_path.write_text(js, encoding="utf-8")

    preview = preview_path.read_text(encoding="utf-8")
    python_names = {
        "halfStrideCorrections": "HALF_STRIDE_CORRECTIONS",
        "extendedStrideCorrections": "EXTENDED_STRIDE_CORRECTIONS",
        "halfBackDiagonalCorrections": "HALF_BACK_DIAGONAL_CORRECTIONS",
        "extendedBackDiagonalCorrections": "EXTENDED_BACK_DIAGONAL_CORRECTIONS",
    }
    preview = re.sub(r"SOLE_TABLES = \(.*?(?=\nZERO_RESIDUAL)",
                     f"SOLE_TABLES = {python_rows(sole_tables)}", preview, flags=re.DOTALL)
    next_names = {
        "halfStrideCorrections": "EXTENDED_STRIDE_CORRECTIONS",
        "extendedStrideCorrections": "HALF_BACK_DIAGONAL_CORRECTIONS",
        "halfBackDiagonalCorrections": "EXTENDED_BACK_DIAGONAL_CORRECTIONS",
        "extendedBackDiagonalCorrections": "EXTENDED_RESIDUALS",
    }
    for label, rows in correction_tables.items():
        preview = re.sub(rf"{python_names[label]} = \(.*?(?=\n{next_names[label]})",
                         f"{python_names[label]} = {python_rows(rows)}",
                         preview, flags=re.DOTALL)
    preview_path.write_text(preview, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--write-js", type=pathlib.Path)
    parser.add_argument("--write-preview", type=pathlib.Path)
    arguments = parser.parse_args()
    source = arguments.source
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
    for _ in range(1):
        preview.SOLE_TABLES = tuple(tuple(table) for table in sole_tables)
        for table_index, angle in enumerate(angles):
            for sample in range(32):
                phase = sample / 32.0
                vertices = preview.skinned_vertices(
                    positions, joints, weights,
                    preview.pose_matrices(f"direction.{angle}", phase, 0.5715))
                sole_tables[table_index][sample] -= min(vertex[1] for vertex in vertices)
    preview.SOLE_TABLES = tuple(tuple(table) for table in sole_tables)
    print("soleTables = [")
    for table in sole_tables:
        print("  [" + ",".join(f"{value:.5f}" for value in table) + "],")
    print("]")

    def calibrate(stride: float, attribute: str, target_angles=angles):
        corrections = [[0.0] * 32 for _ in target_angles]
        for _ in range(1):
            setattr(preview, attribute, tuple(tuple(table) for table in corrections))
            for table_index, angle in enumerate(target_angles):
                for sample in range(32):
                    phase = sample / 32.0
                    vertices = preview.skinned_vertices(
                        positions, joints, weights,
                        preview.pose_matrices(f"direction.{angle}", phase, stride))
                    corrections[table_index][sample] -= min(vertex[1] for vertex in vertices)
        setattr(preview, attribute, tuple(tuple(table) for table in corrections))
        return corrections

    correction_tables = dict((
        ("halfStrideCorrections", calibrate(0.28575, "HALF_STRIDE_CORRECTIONS")),
        ("extendedStrideCorrections", calibrate(0.70, "EXTENDED_STRIDE_CORRECTIONS")),
        ("halfBackDiagonalCorrections", calibrate(
            0.28575, "HALF_BACK_DIAGONAL_CORRECTIONS", (105, 120, 240, 255))),
        ("extendedBackDiagonalCorrections", calibrate(
            0.70, "EXTENDED_BACK_DIAGONAL_CORRECTIONS", (105, 120, 240, 255))),
    ))
    for label, corrections in correction_tables.items():
        print(f"{label} = [")
        for table in corrections:
            print("  [" + ",".join(f"{value:.5f}" for value in table) + "],")
        print("]")
    if arguments.write_js and arguments.write_preview:
        write_tables(arguments.write_js, arguments.write_preview,
                     sole_tables, correction_tables)
        print(f"Updated {arguments.write_js} and {arguments.write_preview}")


if __name__ == "__main__":
    main()
