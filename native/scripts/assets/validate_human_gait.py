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

    # The conditioned mesh's visible shoe points along local +Z even though
    # the generated toe helper bone points toward -Z. Validate anatomy from the
    # weighted shoe vertices so a reversed foot bone cannot produce a false pass.
    def foot_zone_rows(bones, minimum_z, maximum_z):
        return [row for row, (position, joint_row, weight_row) in enumerate(
            zip(positions, joints, weights))
            if position[1] < 0.25
            and minimum_z <= position[2] < maximum_z
            and sum(weight_row[influence] for influence in range(4)
                    if int(joint_row[influence]) in bones) > 0.55]

    def foot_rows(bones, toe):
        return foot_zone_rows(bones, 0.10, math.inf) if toe \
            else foot_zone_rows(bones, -math.inf, -0.04)

    def center(vertices, rows):
        return tuple(sum(vertices[row][axis] for row in rows) / len(rows)
                     for axis in range(3))

    def interval_gap(vertices, left_rows, right_rows):
        left_min = min(vertices[row][0] for row in left_rows)
        left_max = max(vertices[row][0] for row in left_rows)
        right_min = min(vertices[row][0] for row in right_rows)
        right_max = max(vertices[row][0] for row in right_rows)
        return max(right_min - left_max, left_min - right_max, 0.0)

    left_heel_rows = foot_rows((8, 9), False)
    left_toe_rows = foot_rows((8, 9), True)
    right_heel_rows = foot_rows((12, 13), False)
    right_toe_rows = foot_rows((12, 13), True)
    left_heel_center = center(attention_vertices, left_heel_rows)
    left_toe_center = center(attention_vertices, left_toe_rows)
    right_heel_center = center(attention_vertices, right_heel_rows)
    right_toe_center = center(attention_vertices, right_toe_rows)
    pelvis = transform(attention_matrices[1], JOINTS[1].global_position)
    head = transform(attention_matrices[5], JOINTS[5].global_position)
    left_hand = transform(attention_matrices[17], JOINTS[17].global_position)
    right_hand = transform(attention_matrices[21], JOINTS[21].global_position)
    left_elbow = transform(attention_matrices[16], JOINTS[16].global_position)
    right_elbow = transform(attention_matrices[20], JOINTS[20].global_position)
    heel_separation = interval_gap(attention_vertices, left_heel_rows, right_heel_rows)
    hand_separation = math.dist(left_hand, right_hand)
    left_forearm = tuple(left_hand[axis] - left_elbow[axis] for axis in range(3))
    right_forearm = tuple(right_hand[axis] - right_elbow[axis] for axis in range(3))
    forearm_angle = math.degrees(math.acos(max(-1.0, min(1.0,
        sum(left_forearm[axis] * right_forearm[axis] for axis in range(3))
        / (math.sqrt(sum(value * value for value in left_forearm))
           * math.sqrt(sum(value * value for value in right_forearm)))))))

    def foot_heading(heel, toe):
        return math.degrees(math.atan2(toe[0] - heel[0], toe[2] - heel[2]))

    toe_angle = abs(foot_heading(left_heel_center, left_toe_center)
                    - foot_heading(right_heel_center, right_toe_center))
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

    # Check the synchronized left step-off described by the technique: the
    # arriving shoe rolls heel-to-toe while the old right shoe clears in the
    # opposite order. Regions use the visible shoe's actual +Z toe anatomy.
    zones = (("heel", -math.inf, -0.04), ("arch", -0.04, 0.06),
             ("ball", 0.06, 0.13), ("toe", 0.13, math.inf))

    def zone_heights(vertices, bones):
        return {name: min(vertices[row][1]
                          for row in foot_zone_rows(bones, minimum_z, maximum_z))
                for name, minimum_z, maximum_z in zones}

    left_contact_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.50, 0.5715))
    left_roll_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.575, 0.5715))
    left_flat_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.65, 0.5715))
    left_contact_zones = zone_heights(left_contact_vertices, (8, 9))
    right_flat_zones = zone_heights(left_contact_vertices, (12, 13))
    left_roll_zones = zone_heights(left_roll_vertices, (8, 9))
    left_flat_zones = zone_heights(left_flat_vertices, (8, 9))
    right_release_zones = zone_heights(left_roll_vertices, (12, 13))
    if not (left_contact_zones["heel"] + 0.015 < left_contact_zones["arch"]
            < left_contact_zones["ball"] < left_contact_zones["toe"]):
        violations.append("left step-off does not begin on the visible heel")
    if (max(right_flat_zones.values()) > 0.006
            or max(right_flat_zones.values()) - min(right_flat_zones.values()) > 0.006):
        violations.append("right shoe is not completely flat when the left heel lands")
    if not (left_roll_zones["heel"] <= 0.005
            and left_roll_zones["arch"] > left_roll_zones["heel"] + 0.006
            and left_roll_zones["toe"] > left_roll_zones["arch"] + 0.025):
        violations.append("left shoe does not progressively roll heel-to-toe")
    if max(left_flat_zones.values()) - min(left_flat_zones.values()) > 0.006:
        violations.append("left shoe does not finish its roll on a flat platform")
    if not (right_release_zones["heel"] > right_release_zones["arch"]
            > right_release_zones["ball"] > right_release_zones["toe"] >= 0.008):
        violations.append("right shoe remains on its toe after left-foot weight transfer")

    flat_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.15, 0.5715))
    pretransfer_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.49, 0.5715))
    flat_toe = min(flat_vertices[row][1] for row in right_toe_rows)
    flat_heel = min(flat_vertices[row][1] for row in right_heel_rows)
    pretransfer_toe = min(pretransfer_vertices[row][1] for row in right_toe_rows)
    pretransfer_heel = min(pretransfer_vertices[row][1] for row in right_heel_rows)
    if abs(flat_toe - flat_heel) > 0.006:
        violations.append("forward shoe does not roll from heel to a flat platform")
    if (max(pretransfer_heel, pretransfer_toe) > 0.006
            or abs(pretransfer_heel - pretransfer_toe) > 0.006):
        violations.append("old support shoe lifts before the opposite heel arrives")
    backward_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.backward", 0.0, 0.5715))
    backward_toe = min(backward_vertices[row][1] for row in right_toe_rows)
    backward_heel = min(backward_vertices[row][1] for row in right_heel_rows)
    if backward_heel - backward_toe < 0.015:
        violations.append("backward contact is not supported on the forefoot")

    passing_vertices = skinned_vertices(
        positions, joints, weights, pose_matrices("march.forward", 0.75, 0.5715))
    passing_gap = interval_gap(passing_vertices,
                               left_heel_rows + left_toe_rows,
                               right_heel_rows + right_toe_rows)
    passing_left_heel = center(passing_vertices, left_heel_rows)
    passing_left_toe = center(passing_vertices, left_toe_rows)
    passing_right_heel = center(passing_vertices, right_heel_rows)
    passing_right_toe = center(passing_vertices, right_toe_rows)
    passing_left_heading = foot_heading(passing_left_heel, passing_left_toe)
    passing_right_heading = foot_heading(passing_right_heel, passing_right_toe)
    passing_heading_delta = abs((passing_left_heading - passing_right_heading + 180) % 360 - 180)
    passing_z_delta = abs((passing_left_heel[2] + passing_left_toe[2]) * 0.5
                          - (passing_right_heel[2] + passing_right_toe[2]) * 0.5)
    if passing_gap > 0.005:
        violations.append(f"passing shoes are {passing_gap:.4f} m apart instead of touching")
    if passing_heading_delta > 2.0:
        violations.append(f"passing shoes differ by {passing_heading_delta:.2f} degrees")
    if passing_z_delta > 0.03:
        violations.append(f"passing shoes miss the crossing plane by {passing_z_delta:.4f} m")
    report["footTechnique"] = {
        "heelStrikeContacts": contact_checks,
        "leftStepOffContactMeters": {key: round(value, 6)
                                      for key, value in left_contact_zones.items()},
        "rightAtLeftHeelContactMeters": {key: round(value, 6)
                                          for key, value in right_flat_zones.items()},
        "leftStepOffRollMeters": {key: round(value, 6)
                                   for key, value in left_roll_zones.items()},
        "rightReleaseMeters": {key: round(value, 6)
                                for key, value in right_release_zones.items()},
        "flatSupportToeMeters": round(flat_toe, 6),
        "flatSupportHeelMeters": round(flat_heel, 6),
        "preTransferToeMeters": round(pretransfer_toe, 6),
        "preTransferHeelMeters": round(pretransfer_heel, 6),
        "backwardToeMeters": round(backward_toe, 6),
        "backwardHeelMeters": round(backward_heel, 6),
        "passingShoeGapMeters": round(passing_gap, 6),
        "passingHeadingDeltaDegrees": round(passing_heading_delta, 3),
        "passingLongitudinalDeltaMeters": round(passing_z_delta, 6),
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
