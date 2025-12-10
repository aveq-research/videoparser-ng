#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "numpy>=1.24.0",
#     "tqdm>=4.64.0",
#     "tabulate>=0.9.0",
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
import multiprocessing
import sys
from pathlib import Path
from typing import Any, Callable, Optional, TypedDict

import numpy as np
from tabulate import tabulate
from tqdm.contrib.concurrent import process_map


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


def _analyze_video_pair_wrapper(args: tuple[str, Path, Path]) -> SingleResultsDict:
    """Wrapper for analyze_video_pair to work with process_map."""
    video_name, legacy_path, new_path = args
    return analyze_video_pair(video_name, legacy_path, new_path)


def compute_aggregate_stats(
    videos: list[SingleResultsDict], codec_filter: str | None = None
) -> dict[str, Any]:
    """Compute aggregate statistics across videos, optionally filtered by codec."""
    filtered_videos = videos
    if codec_filter:
        filtered_videos = [v for v in videos if v["codec"] == codec_filter]

    if not filtered_videos:
        return {}

    aggregate_stats: dict[str, dict[str, Any]] = {}
    for video in filtered_videos:
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
    result: dict[str, Any] = {}
    for metric_name, data in aggregate_stats.items():
        result[metric_name] = {
            "legacy_name": data["legacy_name"],
            "new_name": data["new_name"],
            "legacy_across_videos": compute_stats(data["legacy_means"]),
            "new_across_videos": compute_stats(data["new_means"]),
        }
        if data["difference_means"]:
            result[metric_name]["difference_across_videos"] = compute_stats(
                data["difference_means"]
            )
        if data["rel_diff_means"]:
            result[metric_name]["relative_difference_percent_across_videos"] = (
                compute_stats(data["rel_diff_means"])
            )

    return result


def format_markdown_table(
    aggregate_metrics: dict[str, Any], title: str, video_count: int
) -> str:
    """Format aggregate metrics as a markdown table."""
    lines = [f"## {title} ({video_count} videos)", ""]

    # Build table data
    table_data = []
    headers = ["Metric", "Legacy Mean", "New Mean", "Diff Mean", "Rel Diff (%)"]

    for metric_name in sorted(aggregate_metrics.keys()):
        data = aggregate_metrics[metric_name]
        legacy_mean = data["legacy_across_videos"]["mean"]
        new_mean = data["new_across_videos"]["mean"]
        diff_mean = data.get("difference_across_videos", {}).get("mean")
        rel_diff_mean = data.get("relative_difference_percent_across_videos", {}).get(
            "mean"
        )

        row = [
            metric_name,
            f"{legacy_mean:.6f}" if legacy_mean is not None else "N/A",
            f"{new_mean:.6f}" if new_mean is not None else "N/A",
            f"{diff_mean:+.6f}" if diff_mean is not None else "N/A",
            f"{rel_diff_mean:+.4f}" if rel_diff_mean is not None else "N/A",
        ]
        table_data.append(row)

    lines.append(tabulate(table_data, headers=headers, tablefmt="github"))
    lines.append("")
    return "\n".join(lines)


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
    parser.add_argument(
        "--workers",
        "-w",
        type=int,
        default=None,
        help="Number of parallel workers (default: CPU count)",
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

    num_workers = args.workers or multiprocessing.cpu_count()
    print(
        f"Found {len(pairs)} video pairs to analyze using {num_workers} workers",
        file=sys.stderr,
    )

    # Prepare arguments for parallel processing
    work_items = [
        (video_name, pairs[video_name]["legacy"], pairs[video_name]["new"])
        for video_name in sorted(pairs.keys())
    ]

    # Analyze pairs in parallel using process_map
    video_results = process_map(
        _analyze_video_pair_wrapper,
        work_items,
        max_workers=num_workers,
        desc="Analyzing videos",
    )

    # Build results dict
    results: ResultsDict = {
        "source_folder": str(folder),
        "video_count": len(pairs),
        "videos": list(video_results),
        "aggregate_metrics": {},
    }

    # Compute aggregate statistics across all videos
    results["aggregate_metrics"] = compute_aggregate_stats(results["videos"])

    # Compute per-codec aggregate statistics
    codecs = sorted(set(v["codec"] for v in results["videos"]))
    codec_aggregates: dict[str, dict[str, Any]] = {}
    for codec in codecs:
        codec_aggregates[codec] = compute_aggregate_stats(results["videos"], codec)

    # Write JSON output
    output_path = args.output or (folder / "comparison_results.json")
    json_output = {
        **results,
        "per_codec_aggregate_metrics": codec_aggregates,
    }
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(json_output, f, indent=2)

    print(f"Results written to: {output_path}", file=sys.stderr)

    # Print markdown summary to stdout
    print("\n# Parser Comparison Results\n")
    print(f"**Source folder:** `{folder}`\n")

    # Overall aggregate table
    print(
        format_markdown_table(
            results["aggregate_metrics"], "All Codecs", len(results["videos"])
        )
    )

    # Per-codec tables
    for codec in codecs:
        codec_videos = [v for v in results["videos"] if v["codec"] == codec]
        if codec_aggregates[codec]:
            print(
                format_markdown_table(
                    codec_aggregates[codec], f"Codec: {codec}", len(codec_videos)
                )
            )


if __name__ == "__main__":
    main()
