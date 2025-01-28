#!/usr/bin/env pytest

import json
import os
import subprocess
from typing import Dict, List

import pytest

HERE = os.path.dirname(os.path.realpath(__file__))

# test files and their expected codecs
TEST_FILES = [
    ("test-libaom-av1.mp4", "av1"),
    ("test-libvpx-vp9.mp4", "vp9"),
    ("test-libx264.mp4", "h264"),
    ("test-libx265.mp4", "hevc"),
]


def call_parser(video_file: str, num_frames: int = 2) -> tuple[List[Dict], Dict]:
    """Call the video parser on the given video file and return frame info and sequence info."""

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
    frame_info: List[Dict] = []
    sequence_info: Dict = {}

    for line in output.decode("utf-8").splitlines():
        json_line = json.loads(line)
        if json_line["type"] == "frame_info":
            frame_info.append(json_line)
        elif json_line["type"] == "sequence_info":
            sequence_info = json_line

    return frame_info, sequence_info


class TestCLI:
    @pytest.mark.parametrize("test_file,expected_codec", TEST_FILES)
    def test_parser_cli(self, test_file: str, expected_codec: str):
        # Call parser on test file
        frame_info, sequence_info = call_parser(os.path.join(HERE, test_file))

        # Basic validation of output structure
        assert len(frame_info) == 2, "Expected 2 frames of data"
        assert sequence_info, "Expected sequence info"

        # Validate sequence info
        assert sequence_info["video_codec"] == expected_codec
        assert sequence_info["video_width"] > 0
        assert sequence_info["video_height"] > 0
        assert sequence_info["video_framerate"] > 0
        assert sequence_info["video_duration"] > 0
        assert sequence_info["video_frame_count"] > 0

        # Validate frame info for both frames
        for frame in frame_info:
            # Check required fields exist and have valid values
            assert "frame_idx" in frame
            assert "pts" in frame
            assert "dts" in frame
            assert "size" in frame and frame["size"] > 0
            assert "frame_type" in frame
            assert "is_idr" in frame

            # QP values should exist and be reasonable
            assert "qp_avg" in frame
            assert "qp_min" in frame
            assert "qp_max" in frame
            assert frame["qp_min"] <= frame["qp_avg"] <= frame["qp_max"]

        # First frame should be a keyframe
        assert frame_info[0]["is_idr"] is True
        assert frame_info[0]["frame_idx"] == 0

        # Second frame should have frame_idx 1
        assert frame_info[1]["frame_idx"] == 1
