#!/usr/bin/env python3
"""Validate grounded skin deformation across a complete two-count gait."""

from __future__ import annotations

import argparse
import json
import math
import pathlib

from build_human_performer import JOINTS, read_accessor, read_glb
from render_human_performer_preview import pose_matrices, skinned_vertices, transform


def validate(source: pathlib.Path, samples: int) -> dict:
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])

    report = {}
    violations = []
    attention_matrices = pose_matrices("idle", 0.0, 0.5715)
    attention_vertices = skinned_vertices(positions, joints, weights, attention_matrices)
    attention_ground = min(vertex[1] for vertex in attention_vertices)
    pelvis = transform(attention_matrices[1], JOINTS[1].global_position)
    head = transform(attention_matrices[5], JOINTS[5].global_position)
    left_ankle = transform(attention_matrices[8], JOINTS[8].global_position)
    left_toe = transform(attention_matrices[9], JOINTS[9].global_position)
    right_ankle = transform(attention_matrices[12], JOINTS[12].global_position)
    right_toe = transform(attention_matrices[13], JOINTS[13].global_position)
    left_hand = transform(attention_matrices[17], JOINTS[17].global_position)
    right_hand = transform(attention_matrices[21], JOINTS[21].global_position)
    left_elbow = transform(attention_matrices[16], JOINTS[16].global_position)
    right_elbow = transform(attention_matrices[20], JOINTS[20].global_position)
    heel_separation = math.dist((left_ankle[0], left_ankle[2]),
                                (right_ankle[0], right_ankle[2]))
    hand_separation = math.dist(left_hand, right_hand)
    left_forearm = tuple(left_hand[axis] - left_elbow[axis] for axis in range(3))
    right_forearm = tuple(right_hand[axis] - right_elbow[axis] for axis in range(3))
    forearm_angle = math.degrees(math.acos(max(-1.0, min(1.0,
        sum(left_forearm[axis] * right_forearm[axis] for axis in range(3))
        / (math.sqrt(sum(value * value for value in left_forearm))
           * math.sqrt(sum(value * value for value in right_forearm)))))))

    def foot_heading(ankle, toe):
        return math.degrees(math.atan2(toe[0] - ankle[0], -(toe[2] - ankle[2])))

    toe_angle = abs(foot_heading(left_ankle, left_toe)
                    - foot_heading(right_ankle, right_toe))
    head_forward = transform(attention_matrices[5],
                             (JOINTS[5].global_position[0],
                              JOINTS[5].global_position[1],
                              JOINTS[5].global_position[2] - 1.0))
    look_vector = tuple(head_forward[axis] - head[axis] for axis in range(3))
    head_pitch = math.degrees(math.asin(look_vector[1]
                              / math.sqrt(sum(value * value for value in look_vector))))
    if abs(attention_ground) > 0.002:
        violations.append(f"attention feet miss turf by {attention_ground:.4f} m")
    if heel_separation > 0.005:
        violations.append(f"attention heels are {heel_separation:.4f} m apart")
    if abs(toe_angle - 90.0) > 1.0:
        violations.append(f"attention toe angle is {toe_angle:.2f} degrees")
    if abs(head[0] - pelvis[0]) > 0.005:
        violations.append("attention torso is not vertically stacked")
    if not 9.0 <= head_pitch <= 12.0:
        violations.append(f"attention head pitch is {head_pitch:.2f} degrees")
    if hand_separation > 0.05 or min(left_hand[1], right_hand[1]) < 1.35:
        violations.append(f"set hands miss the face-height grip ({hand_separation:.4f} m)")
    if not 82.0 <= forearm_angle <= 108.0:
        violations.append(f"set forearms form a {forearm_angle:.2f}-degree angle")
    if right_hand[2] >= left_hand[2] - 0.015:
        violations.append("set right hand is not visibly forward of the left")
    report["attention"] = {
        "minimumGroundContactMeters": round(attention_ground, 6),
        "heelSeparationMeters": round(heel_separation, 6),
        "toeAngleDegrees": round(toe_angle, 3),
        "headPitchDegrees": round(head_pitch, 3),
        "handSeparationMeters": round(hand_separation, 6),
        "handHeightMeters": round((left_hand[1] + right_hand[1]) * 0.5, 6),
        "forearmAngleDegrees": round(forearm_angle, 3),
    }

    def foot_rows(bones, toe):
        return [row for row, (position, joint_row, weight_row) in enumerate(
            zip(positions, joints, weights))
            if position[1] < 0.20
            and (position[2] < -0.04 if toe else position[2] > 0.10)
            and sum(weight_row[influence] for influence in range(4)
                    if int(joint_row[influence]) in bones) > 0.55]

    contact_checks = []
    for phase, bones, label in ((0.0, (12, 13), "right"), (0.5, (8, 9), "left")):
        contact_vertices = skinned_vertices(
            positions, joints, weights, pose_matrices("march.forward", phase, 0.5715))
        toe_height = min(contact_vertices[row][1] for row in foot_rows(bones, True))
        heel_height = min(contact_vertices[row][1] for row in foot_rows(bones, False))
        if heel_height > 0.01 or toe_height - heel_height < 0.08:
            violations.append(
                f"forward {label} contact is not a high-toe heel strike "
                f"(heel {heel_height:.4f} m, toe {toe_height:.4f} m)")
        contact_checks.append({"foot": label, "heelMeters": round(heel_height, 6),
                               "toeMeters": round(toe_height, 6)})

    flat_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.15, 0.5715))
    release_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.49, 0.5715))
    right_toe_rows = foot_rows((12, 13), True)
    right_heel_rows = foot_rows((12, 13), False)
    flat_toe = min(flat_vertices[row][1] for row in right_toe_rows)
    flat_heel = min(flat_vertices[row][1] for row in right_heel_rows)
    release_toe = min(release_vertices[row][1] for row in right_toe_rows)
    release_heel = min(release_vertices[row][1] for row in right_heel_rows)
    if abs(flat_toe - flat_heel) > 0.006:
        violations.append("forward shoe does not roll from heel to a flat platform")
    if release_heel - release_toe < 0.02:
        violations.append("forward support does not transfer through the forefoot")
    backward_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.backward", 0.0, 0.5715))
    backward_toe = min(backward_vertices[row][1] for row in right_toe_rows)
    backward_heel = min(backward_vertices[row][1] for row in right_heel_rows)
    if backward_heel - backward_toe < 0.015:
        violations.append("backward contact is not supported on the forefoot")
    report["footTechnique"] = {
        "heelStrikeContacts": contact_checks,
        "flatSupportToeMeters": round(flat_toe, 6),
        "flatSupportHeelMeters": round(flat_heel, 6),
        "releaseToeMeters": round(release_toe, 6),
        "releaseHeelMeters": round(release_heel, 6),
        "backwardToeMeters": round(backward_toe, 6),
        "backwardHeelMeters": round(backward_heel, 6),
    }
    core_cases = [(angle, 0.5715, samples) for angle in (0, 45, 90, 135, 180, -135, -90, -45)]
    stride_samples = min(samples, 16)
    stride_cases = [(angle, stride, stride_samples)
                    for stride in (0.28575, 0.70)
                    for angle in (0, 45, 90, 105, 120, 135, 150, 180, -120, -135)]
    for angle, stride, case_samples in core_cases + stride_cases:
        mode = f"direction.{angle}"
        label = f"{mode}@{stride:.5f}m"
        ground_contacts = []
        heights = []
        for sample in range(case_samples):
            phase = sample / case_samples
            vertices = skinned_vertices(positions, joints, weights,
                                        pose_matrices(mode, phase, stride))
            ground_contacts.append(min(vertex[1] for vertex in vertices))
            heights.append(max(vertex[1] for vertex in vertices))

        ankle_positions = []
        radians = math.radians(angle)
        for sample in range(case_samples + 1):
            phase = 0.5 * sample / case_samples
            matrices = pose_matrices(mode, phase, stride)
            ankle = transform(matrices[12], JOINTS[12].global_position)
            ankle_positions.append((ankle[0] + math.sin(radians) * 2.0 * stride * phase,
                                    ankle[2] - math.cos(radians) * 2.0 * stride * phase))
        ankle_origin = ankle_positions[0]
        ankle_drift = max(math.hypot(point[0] - ankle_origin[0], point[1] - ankle_origin[1])
                          for point in ankle_positions)

        lowest = min(ground_contacts)
        highest_contact = max(ground_contacts)
        height_variation = max(heights) - min(heights)
        if lowest < -0.002:
            violations.append(f"{label} penetrates turf by {-lowest:.4f} m")
        if highest_contact > 0.002:
            violations.append(f"{label} floats by {highest_contact:.4f} m")
        if height_variation > 0.015:
            violations.append(f"{label} upper body bobs {height_variation:.4f} m")
        if ankle_drift > 0.001:
            violations.append(f"{label} planted ankle skates {ankle_drift:.4f} m")
        report[label] = {
            "minimumGroundContactMeters": round(lowest, 6),
            "maximumGroundContactMeters": round(highest_contact, 6),
            "heightVariationMeters": round(height_variation, 6),
            "maximumPlantedAnkleDriftMeters": round(ankle_drift, 6),
        }
    assert not violations, "\n".join(violations)
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--samples", type=int, default=32)
    arguments = parser.parse_args()
    print(json.dumps(validate(arguments.source, arguments.samples), indent=2))


if __name__ == "__main__":
    main()
