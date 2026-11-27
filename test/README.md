# Test clips

The clips are generated with `test/reencode_videos.sh` and the scripts in `util/` (see [DEVELOPERS.md](../DEVELOPERS.md#testing)); the clips in `seek/` come from `util/generate-seek-test-videos.sh`, except for the one below.

`test-h264-paff.264` is `Sharp_MP_Field_1_B.jvt` from the ITU-T H.264.1 conformance bitstreams, as included in the [FFmpeg FATE suite](https://fate-suite.ffmpeg.org/h264-conformance/Sharp_MP_Field_1_B.jvt) (MD5 `44941da36b87c95ba0799d26abb70a70`). It is H.264 Main profile, 720x480, with 30 field pictures (15 field pairs).

`test-libx265-legacy-reference.ldjson` is the output of the legacy parser [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser) for `test-libx265.mp4` (run with its Docker image), converted to the keys of videoparser-ng: one line per frame with `frame_idx` (`FrameIdx` minus 1), `frame_type`, `is_idr`, `size`, `current_poc`, `mv_coded_count`, the QP fields (`Av_QP`, `StdDev_QP`, `min_QP`, `max_QP`, `Av_QPBB`, `StdDev_QPBB`) and the motion fields (`Av_Motion`, `StdDev_Motion`, `Av_MotionX`, `StdDev_MotionX`, `Av_MotionY`, `StdDev_MotionY`, `Av_MotionDif`, `StdDev_MotionDif`). The legacy-mode build must give the same values. The clip has a CRA picture at frame 250.
