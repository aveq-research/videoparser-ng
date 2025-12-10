#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "numpy>=1.24.0",
#     "tqdm>=4.64.0",
# ]
# ///
"""
Compare legacy videoparser (.bz2) and new videoparser-ng (.ldjson) outputs.

Analyzes commonly implemented metrics across both parsers and outputs
statistical descriptions (min, max, mean, median, stdev, etc.) per video.

Usage:
    uv run test/compare_parsers.py /path/to/data/folder
    uv run test/compare_parsers.py /path/to/data/folder --output results.json
"""

import argparse
import bz2
import json
import sys
from pathlib import Path
from typing import Any, Callable, Optional, TypedDict

import numpy as np
from tqdm import tqdm


class SingleResultsDict(TypedDict):
    video_name: str
    codec: str
    legacy_frame_count: int
    new_frame_count: int
    sequence_info: Optional[dict[str, Any]]
    metrics: dict[str, Any]


class ResultsDict(TypedDict):
    source_folder: str
    video_count: int
    videos: list[SingleResultsDict]
    aggregate_metrics: dict[str, Any]  # todo: refine type


# Metric mappings: (legacy_name, new_name, optional_transform_for_legacy)
# Transforms convert legacy values to match new format units
# Only include metrics that are implemented in both parsers
METRIC_MAPPINGS: list[tuple[str, str, Callable[[float], float] | None]] = [
    # QP metrics (same units)
    ("Av_QP", "qp_avg", None),
    ("StdDev_QP", "qp_stdev", None),
    ("Av_QPBB", "qp_bb_avg", None),
    ("StdDev_QPBB", "qp_bb_stdev", None),
    ("min_QP", "qp_min", None),
    ("max_QP", "qp_max", None),
    ("InitialQP", "qp_init", None),
    # Motion metrics (same units - subpel)
    ("Av_Motion", "motion_avg", None),
    ("StdDev_Motion", "motion_stdev", None),
    ("Av_MotionX", "motion_x_avg", None),
    ("StdDev_MotionX", "motion_x_stdev", None),
    ("Av_MotionY", "motion_y_avg", None),
    ("StdDev_MotionY", "motion_y_stdev", None),
    ("Av_MotionDif", "motion_diff_avg", None),
    ("StdDev_MotionDif", "motion_diff_stdev", None),
    # Bit count metrics (same units - bits)
    ("BitCntMotion", "motion_bit_count", None),
    ("BitCntCoefs", "coefs_bit_count", None),
    # Block count metrics (same units - count)
    ("NumBlksMv", "mb_mv_count", None),
    ("CodedMv", "mv_coded_count", None),
    # Frame metadata
    ("FrameSize", "size", None),
    ("FrameType", "frame_type", None),
    # FrameIdx: legacy is 1-based, new is 0-based
    ("FrameIdx", "frame_idx", lambda x: x - 1),
    ("IsIDR", "is_idr", None),
    # PTS/DTS: legacy is milliseconds, new is seconds
    ("PTS", "pts", lambda x: x / 1000.0),
    ("DTS", "dts", lambda x: x / 1000.0),
    # POC metrics (H.264/HEVC only)
    ("CurrPOC", "current_poc", None),
    ("POC_DIF", "poc_diff", None),
]


def load_legacy_data(filepath: Path) -> list[dict[str, Any]]:
    """Load legacy videoparser .json.bz2 file."""
    with bz2.open(filepath, "rt", encoding="utf-8") as f:
        return json.load(f)


def load_new_data(filepath: Path) -> tuple[dict[str, Any] | None, list[dict[str, Any]]]:
    """Load new videoparser-ng .ldjson file.

    Returns:
        Tuple of (sequence_info dict or None, list of frame_info dicts)
    """
    sequence_info = None
    frames = []

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            record = json.loads(line)
            if record.get("type") == "sequence_info":
                sequence_info = record
            elif record.get("type") == "frame_info":
                frames.append(record)

    return sequence_info, frames


def compute_stats(values: list[float | int]) -> dict[str, float | int | None]:
    """Compute descriptive statistics for a list of values."""
    if not values:
        return {
            "count": 0,
            "min": None,
            "max": None,
            "mean": None,
            "median": None,
            "stdev": None,
            "sum": None,
        }

    arr = np.array(values, dtype=np.float64)
    return {
        "count": len(values),
        "min": float(np.min(arr)),
        "max": float(np.max(arr)),
        "mean": float(np.mean(arr)),
        "median": float(np.median(arr)),
        "stdev": float(np.std(arr)) if len(arr) > 1 else 0.0,
        "sum": float(np.sum(arr)),
    }


def extract_metric_values(
    frames: list[dict[str, Any]],
    metric_name: str,
    transform: Callable[[float], float] | None = None,
) -> list[float | int]:
    """Extract values for a given metric from frame data."""
    values = []
    for frame in frames:
        val = frame.get(metric_name)
        if val is not None and not (isinstance(val, float) and np.isnan(val)):
            # Handle boolean conversion for is_idr
            if isinstance(val, bool):
                val = int(val)
            # Apply transform if provided
            if transform is not None:
                val = transform(val)
            values.append(val)
    return values


def find_video_pairs(folder: Path) -> dict[str, dict[str, Path]]:
    """Find matching legacy/new file pairs in folder.

    Returns dict keyed by video basename with 'legacy' and 'new' paths.
    Only includes videos that have both formats (skips AV1 which has no legacy).
    """
    pairs = {}

    # Find all .ldjson files (new format)
    for new_file in folder.glob("*.ldjson"):
        # Extract video name: e.g., "Aerial3200_1920x1080_5994_10bit_420-h264"
        video_name = new_file.stem  # removes .ldjson

        # Look for matching legacy file
        legacy_file = folder / f"{video_name}-videoparser.json.bz2"

        if legacy_file.exists():
            pairs[video_name] = {"new": new_file, "legacy": legacy_file}
        # else: no legacy (e.g., AV1), skip

    return pairs


def analyze_video_pair(
    video_name: str, legacy_path: Path, new_path: Path
) -> SingleResultsDict:
    """Analyze a single video pair and return comparison statistics."""
    # Load data
    legacy_frames = load_legacy_data(legacy_path)
    sequence_info, new_frames = load_new_data(new_path)

    # Extract codec from sequence_info or filename
    codec = "unknown"
    if sequence_info and "video_codec" in sequence_info:
        codec = sequence_info["video_codec"]
    else:
        # Try to extract from filename
        for c in ["h264", "hevc", "vp9", "av1"]:
            if f"-{c}" in video_name.lower():
                codec = c
                break

    result: SingleResultsDict = {
        "video_name": video_name,
        "codec": codec,
        "legacy_frame_count": len(legacy_frames),
        "new_frame_count": len(new_frames),
        "metrics": {},
        "sequence_info": None,
    }

    # Add sequence info if available
    if sequence_info:
        result["sequence_info"] = {
            k: v for k, v in sequence_info.items() if k != "type"
        }

    # Compare each metric
    for legacy_name, new_name, transform in METRIC_MAPPINGS:
        # Apply transform to legacy values to match new format units
        legacy_values = extract_metric_values(legacy_frames, legacy_name, transform)
        new_values = extract_metric_values(new_frames, new_name)

        # Only include metrics that have data in at least one parser
        if legacy_values or new_values:
            metric_result = {
                "legacy_name": legacy_name,
                "new_name": new_name,
                "legacy": compute_stats(legacy_values),
                "new": compute_stats(new_values),
            }

            # Note if a transform was applied
            if transform is not None:
                metric_result["legacy_transformed"] = True

            # Compute difference statistics if both have data
            if legacy_values and new_values and len(legacy_values) == len(new_values):
                differences = [
                    new_values[i] - legacy_values[i] for i in range(len(legacy_values))
                ]
                metric_result["difference"] = compute_stats(differences)

                # Also compute relative difference (percentage) for non-zero legacy values
                rel_diffs = []
                for i in range(len(legacy_values)):
                    if legacy_values[i] != 0:
                        rel_diffs.append(
                            (new_values[i] - legacy_values[i])
                            / abs(legacy_values[i])
                            * 100
                        )
                if rel_diffs:
                    metric_result["relative_difference_percent"] = compute_stats(
                        rel_diffs
                    )

            result["metrics"][new_name] = metric_result

    return result


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare legacy and new videoparser outputs"
    )
    parser.add_argument(
        "folder",
        type=Path,
        help="Folder containing .json.bz2 (legacy) and .ldjson (new) files",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="Output JSON file path (default: comparison_results.json in input folder)",
    )
    args = parser.parse_args()

    folder = args.folder.resolve()
    if not folder.is_dir():
        print(f"Error: {folder} is not a directory", file=sys.stderr)
        sys.exit(1)

    # Find video pairs
    pairs = find_video_pairs(folder)
    if not pairs:
        print(f"Error: No matching video pairs found in {folder}", file=sys.stderr)
        print(
            "Expected: <name>.ldjson and <name>-videoparser.json.bz2", file=sys.stderr
        )
        sys.exit(1)

    print(f"Found {len(pairs)} video pairs to analyze", file=sys.stderr)

    # Analyze each pair
    results: ResultsDict = {
        "source_folder": str(folder),
        "video_count": len(pairs),
        "videos": [],
        "aggregate_metrics": {},
    }

    for video_name in tqdm(
        sorted(pairs.keys()), desc="Analyzing videos", file=sys.stderr
    ):
        paths = pairs[video_name]
        tqdm.write(f"  Analyzing: {video_name}", file=sys.stderr)
        video_result = analyze_video_pair(video_name, paths["legacy"], paths["new"])
        results["videos"].append(video_result)

    # Compute aggregate statistics across all videos per metric
    aggregate_stats: dict[str, dict[str, Any]] = {}
    for video in results["videos"]:
        for metric_name, metric_data in video["metrics"].items():
            if metric_name not in aggregate_stats:
                aggregate_stats[metric_name] = {
                    "legacy_name": metric_data["legacy_name"],
                    "new_name": metric_data["new_name"],
                    "legacy_means": [],
                    "new_means": [],
                    "difference_means": [],
                    "rel_diff_means": [],
                }

            if metric_data["legacy"]["mean"] is not None:
                aggregate_stats[metric_name]["legacy_means"].append(
                    metric_data["legacy"]["mean"]
                )
            if metric_data["new"]["mean"] is not None:
                aggregate_stats[metric_name]["new_means"].append(
                    metric_data["new"]["mean"]
                )
            if (
                "difference" in metric_data
                and metric_data["difference"]["mean"] is not None
            ):
                aggregate_stats[metric_name]["difference_means"].append(
                    metric_data["difference"]["mean"]
                )
            if "relative_difference_percent" in metric_data:
                aggregate_stats[metric_name]["rel_diff_means"].append(
                    metric_data["relative_difference_percent"]["mean"]
                )

    # Compute final aggregate stats
    results["aggregate_metrics"] = {}
    for metric_name, data in aggregate_stats.items():
        results["aggregate_metrics"][metric_name] = {
            "legacy_name": data["legacy_name"],
            "new_name": data["new_name"],
            "legacy_across_videos": compute_stats(data["legacy_means"]),
            "new_across_videos": compute_stats(data["new_means"]),
        }
        if data["difference_means"]:
            results["aggregate_metrics"][metric_name]["difference_across_videos"] = (
                compute_stats(data["difference_means"])
            )
        if data["rel_diff_means"]:
            results["aggregate_metrics"][metric_name][
                "relative_difference_percent_across_videos"
            ] = compute_stats(data["rel_diff_means"])

    # Write output
    output_path = args.output or (folder / "comparison_results.json")
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)

    print(f"Results written to: {output_path}", file=sys.stderr)

    # Print summary to stdout
    print("\n=== Summary ===")
    print(f"Videos analyzed: {len(results['videos'])}")
    print(f"Codecs: {set(v['codec'] for v in results['videos'])}")
    print("\nAggregate metric differences (mean across all videos):")
    for metric_name, data in sorted(results["aggregate_metrics"].items()):
        if "difference_across_videos" in data:
            diff = data["difference_across_videos"]["mean"]
            rel = data.get("relative_difference_percent_across_videos", {}).get("mean")
            rel_str = f" ({rel:+.2f}%)" if rel is not None else ""
            print(f"  {metric_name}: {diff:+.6f}{rel_str}")


if __name__ == "__main__":
    main()
