#!/usr/bin/env pytest

import json
import os
import subprocess

import pytest

HERE = os.path.dirname(os.path.realpath(__file__))

FIXTURES = [
    {
        "original_features": "test_video_h264-features.ldjson",
        "video": "test_video_h264.mkv",
    },
    # TODO uncomment once implemented
    # {
    #     "original_features": "test_video_h265-features.ldjson",
    #     "video": "test_video_h265.mkv",
    # },
    # {
    #     "original_features": "test_video_vp9-features.ldjson",
    #     "video": "test_video_vp9.mkv",
    # },
]

# original feature keys in the ldjson file, with mapping to the new keys
#
FEATURE_KEY_MAPPING = {
    # Per-Frame Sums, not really needed
    "S": {
        # "QpSum": "qp_sum",
        # "QpSumSQ": "qp_sum_sqr",
        # "QpCnt": "qp_cnt",
        # "QpSumBB": "qp_sum_bb",
        # "QpSumSQBB": "qp_sum_sqr_bb",
        # "QpCntBB": "qp_cnt_bb",
        # "NumBlksMv": None,
        # "NumMvs": None,
        # "NumMerges": None,
        # "MbCnt": "mb_count",
        # "BlkCnt4x4": None,
        # "CodedMv": None,
        # "NumBlksSkip": None,
        # "NumBlksIntra": None,
        # "BitCntMotion": None,
        # "BitCntCoefs": None,
        # "MV_Length": "mv_length",
        # "MV_dLength": None,
        # "MV_SumSQR": "mv_sum_sqr",
        # "MV_LengthX": "mv_x_length",
        # "MV_LengthY": "mv_y_length",
        # "MV_XSQR": "mv_x_sum_sqr",
        # "MV_YSQR": "mv_y_sum_sqr",
        # "MV_DifSum": None,
        # "MV_DifSumSQR": None,
        # "PU_Stat": "pu_stats",
        # "TreeStat": None,
        # "AverageCoefs": None,
        # "AverageCoefsSQR": "coefs_avg_sqr",
        # "AverageCoefsBlkCnt": "coefs_avg_mb_count",
        # "NormalizedField": None,
        # "FrameDistance": "frame_distance",
    },
    # VideoStat (Previous Per-Sequence Statistics, ignored for now)
    "Seq": None,
    # TODO uncomment once implemented
    # "IsHidden": "is_hidden",
    # "IsShrtFrame": "is_short_frame",
    # "AnalyzedMBs": "mb_analyzed_count",
    # "SKIP_mb_ratio": "mb_skip_ratio",
    "Av_QP": "qp_avg",
    "StdDev_QP": "qp_stdev",
    "Av_QPBB": "qp_bb_avg",
    "StdDev_QPBB": "qp_bb_stdev",
    "min_QP": "qp_min",
    "max_QP": "qp_max",
    "InitialQP": "qp_init",
    # "MbQPs": "qp_avg_per_mb",
    "Av_Motion": "motion_avg",
    "StdDev_Motion": "motion_stdev",
    "Av_MotionX": "motion_x_avg",
    "Av_MotionY": "motion_y_avg",
    "StdDev_MotionX": "motion_x_stdev",
    "StdDev_MotionY": "motion_y_stdev",
    "Av_MotionDif": "motion_diff_avg",
    "StdDev_MotionDif": "motion_diff_stdev",
    # "Av_Coefs": "coefs_avg",
    # "StdDev_Coefs": "coefs_stdev",
    # "HF_LF_ratio": "coefs_hf_lf_ratio",
    # "MbTypes": "mb_types",
    # "BlkShapes": "mb_shapes",
    # "TrShapes": "tr_shapes",
    # "FarFWDRef": "mb_far_fwd_ref",
    # "PredFrm_Intra": "pred_frame_intra_area",
    # "FrameSize": "size",
    # "FrameType": "frame_type",
    "IsIDR": "is_idr",
    "FrameIdx": "frame_idx",
    # "FirstFrame": "is_first_frame",
    # "NumFrames": None,
    # "BlackBorder": "black_border_lines",
    "DTS": "dts",
    "PTS": "pts",
    "CurrPOC": "current_poc",
    "POC_DIF": "poc_diff",
    "BitCntMotion": "motion_bit_count",
    "BitCntCoefs": "coefs_bit_count",
    "NumBlksSkip": "mb_skip_count",
    "NumBlksMv": "mb_mv_count",
    "NumBlksMerge": "mb_merge_count",
    "NumBlksIntra": "mb_intra_count",
    "CodedMv": "mv_coded_count",
}

# how to map from old keys to new keys if the meaning changed
TRANSFORMATION_FUNCTIONS = {
    # we start with 0 based frame indices, but the old parser uses 1 based indices
    "FrameIdx": lambda x: int(x)
    - 1,
}


def call_parser(video_file, num_frames: int = 1) -> list[dict]:
    """Call the video parser on the given video file and return the parsed data."""

    # get stdout only
    output = subprocess.check_output(
        [
            "../build/VideoParserCli/video-parser",
            video_file,
            "-n",  # number of frames to parse
            str(num_frames),
        ],
        cwd=HERE,
    )

    # parse the ldjson output
    parsed_data: list[dict] = []
    for line in output.decode("utf-8").splitlines():
        json_line = json.loads(line)
        # we ignore per-sequence statistics for now
        if json_line["type"] == "frame_info":
            parsed_data.append(json_line)

    return parsed_data


class TestVideoParser:
    @pytest.mark.parametrize("fixture", FIXTURES)
    def test_parser_output(self, fixture):
        # Call the parser and get the parsed data
        parsed_data: list[dict] = call_parser(os.path.join(HERE, fixture["video"]))

        # Read the original features file
        original_features_file = fixture["original_features"]
        original_data: list[dict] = []
        with open(os.path.join(HERE, original_features_file), "r") as f:
            for line in f:
                original_data.append(json.loads(line))

        parsed_frames = len(parsed_data)

        # find any missing keys from the parsed data
        missing_keys = []
        # one entry per frame
        value_errors: list[dict] = []
        keys_without_errors = set()

        # Compare the parsed data with the original data using the FEATURE_KEY_MAPPING,
        # ignoring the S keys for now
        for key, mapped_key in FEATURE_KEY_MAPPING.items():
            if mapped_key is None or key == "S" or key == "Seq":
                continue

            if mapped_key not in parsed_data[0]:
                missing_keys.append(mapped_key)
                continue

            for frame_index in range(parsed_frames):
                original_value = original_data[frame_index][key]
                parsed_value = parsed_data[frame_index][mapped_key]

                # apply transformation function if needed
                if key in TRANSFORMATION_FUNCTIONS:
                    original_value = TRANSFORMATION_FUNCTIONS[key](original_value)

                if parsed_value != original_value:
                    value_errors.append(
                        {
                            "frame_index": frame_index,
                            "key": key,
                            "mapped_key": mapped_key,
                            "expected": original_value,
                            "actual": parsed_value,
                        }
                    )
                else:
                    keys_without_errors.add(mapped_key)

        assert len(missing_keys) == 0, f"Missing keys in parsed output: {missing_keys}"

        print("Keys without errors:")
        print(keys_without_errors)

        keys_with_errors = set([error["mapped_key"] for error in value_errors])
        assert (
            len(value_errors) == 0
        ), f"Value error with keys {keys_with_errors}!\n\n{json.dumps(value_errors, indent=2)}"
