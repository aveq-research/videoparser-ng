#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pytest"]
# ///

import json
import math
import os
import subprocess

import pytest

HERE = os.path.dirname(os.path.realpath(__file__))

FIXTURES = [
    pytest.param(
        {
            "expected_features": "test-libx264.ldjson",
            "video": "test-libx264.mp4",
        },
        id="libx264-h264",
    ),
    pytest.param(
        {
            "expected_features": "test-libx265.ldjson",
            "video": "test-libx265.mp4",
        },
        id="libx265-h265",
    ),
    pytest.param(
        {
            "expected_features": "test-libvpx-vp9.ldjson",
            "video": "test-libvpx-vp9.mp4",
        },
        id="libvpx-vp9",
    ),
    pytest.param(
        {
            "expected_features": "test-libaom-av1.ldjson",
            "video": "test-libaom-av1.mp4",
        },
        id="libaom-av1",
    ),
]

# Keys to compare for frame_info entries
FRAME_INFO_KEYS = [
    "coefs_bit_count",
    "current_poc",
    "dts",
    "frame_idx",
    "frame_type",
    "is_idr",
    "mb_mv_count",
    "motion_avg",
    "motion_bit_count",
    "motion_diff_avg",
    "motion_diff_stdev",
    "motion_stdev",
    "motion_x_avg",
    "motion_x_stdev",
    "motion_y_avg",
    "motion_y_stdev",
    "mv_coded_count",
    "poc_diff",
    "pts",
    "qp_avg",
    "qp_bb_avg",
    "qp_bb_stdev",
    "qp_init",
    "qp_max",
    "qp_min",
    "qp_stdev",
    "size",
    "type",
]

# Keys to compare for sequence_info entries
SEQUENCE_INFO_KEYS = [
    "type",
    "video_bit_depth",
    "video_bitrate",
    "video_codec",
    "video_codec_level",
    "video_codec_profile",
    "video_duration",
    "video_frame_count",
    "video_framerate",
    "video_height",
    "video_pix_fmt",
    "video_width",
]

# Tolerance for floating point comparisons
FLOAT_TOLERANCE = 1e-9


def format_value_errors(
    video_name: str, value_errors: list[dict], keys_with_errors: set[str]
) -> str:
    """Format value errors into a readable table grouped by key."""
    lines = [
        f"[{video_name}] Value mismatches for {len(keys_with_errors)} key(s): {sorted(keys_with_errors)}",
        "",
    ]

    # Group errors by key for better readability
    errors_by_key: dict[str, list[dict]] = {}
    for error in value_errors:
        key = error["key"]
        if key not in errors_by_key:
            errors_by_key[key] = []
        errors_by_key[key].append(error)

    for key in sorted(errors_by_key.keys()):
        key_errors = errors_by_key[key]
        lines.append(f"  {key}:")
        lines.append(f"    {'Frame':<8} {'Expected':<20} {'Actual':<20} {'Diff'}")
        lines.append(f"    {'-' * 8} {'-' * 20} {'-' * 20} {'-' * 15}")

        for err in key_errors:
            expected = err["expected"]
            actual = err["actual"]

            # Calculate difference for numeric values
            diff_str = ""
            if isinstance(expected, (int, float)) and isinstance(actual, (int, float)):
                diff = actual - expected
                if isinstance(expected, float) or isinstance(actual, float):
                    diff_str = f"{diff:+.6f}"
                else:
                    diff_str = f"{diff:+d}"

            lines.append(
                f"    {err['frame_index']:<8} {str(expected):<20} {str(actual):<20} {diff_str}"
            )
        lines.append("")

    return "\n".join(lines)


def values_equal(expected, actual) -> bool:
    """Compare two values, with tolerance for floating point numbers."""
    if isinstance(expected, float) and isinstance(actual, float):
        if math.isnan(expected) and math.isnan(actual):
            return True
        if math.isinf(expected) and math.isinf(actual):
            return expected > 0 == actual > 0  # same sign infinity
        return abs(expected - actual) < FLOAT_TOLERANCE
    return expected == actual


def call_parser(video_file: str) -> list[dict]:
    """Call the video parser on the given video file and return the parsed data."""

    output = subprocess.check_output(
        [
            "../build/VideoParserCli/video-parser",
            video_file,
        ],
        cwd=HERE,
    )

    parsed_data: list[dict] = []
    for line in output.decode("utf-8").splitlines():
        json_line = json.loads(line)
        parsed_data.append(json_line)

    return parsed_data


def load_expected_data(features_file: str) -> list[dict]:
    """Load expected data from the ldjson file."""
    expected_data: list[dict] = []
    with open(os.path.join(HERE, features_file), "r") as f:
        for line in f:
            data = json.loads(line)
            expected_data.append(data)
    return expected_data


class TestVideoParser:
    @pytest.mark.parametrize("fixture", FIXTURES)
    def test_parser_output(self, fixture: dict[str, str]):
        video_path = os.path.join(HERE, fixture["video"])

        # Call the parser and get the parsed data
        parsed_data: list[dict] = call_parser(video_path)

        # Read the expected features file
        expected_data: list[dict] = load_expected_data(fixture["expected_features"])

        video_name = fixture["video"]

        # Verify we have the expected number of entries
        assert len(parsed_data) == len(expected_data), (
            f"[{video_name}] Expected {len(expected_data)} entries, got {len(parsed_data)}"
        )

        value_errors: list[dict] = []
        keys_without_errors: set[str] = set()

        for idx, (expected_entry, parsed_entry) in enumerate(
            zip(expected_data, parsed_data)
        ):
            # Determine which keys to compare based on entry type
            if expected_entry.get("type") == "sequence_info":
                keys_to_compare = SEQUENCE_INFO_KEYS
                frame_label = "seq"
            else:
                keys_to_compare = FRAME_INFO_KEYS
                frame_label = str(idx - 1)  # -1 because first entry is sequence_info

            # Compare each key
            for key in keys_to_compare:
                if key not in expected_entry:
                    continue

                expected_value = expected_entry[key]
                parsed_value = parsed_entry.get(key)

                if parsed_value is None:
                    value_errors.append(
                        {
                            "frame_index": frame_label,
                            "key": key,
                            "expected": expected_value,
                            "actual": "MISSING",
                        }
                    )
                elif not values_equal(expected_value, parsed_value):
                    value_errors.append(
                        {
                            "frame_index": frame_label,
                            "key": key,
                            "expected": expected_value,
                            "actual": parsed_value,
                        }
                    )
                else:
                    keys_without_errors.add(key)

        print(f"\n[{video_name}] Keys without errors:")
        print(sorted(keys_without_errors))

        if value_errors:
            keys_with_errors = set(error["key"] for error in value_errors)
            error_msg = format_value_errors(video_name, value_errors, keys_with_errors)
            pytest.fail(error_msg)


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main([__file__, "-v"] + sys.argv[1:]))
