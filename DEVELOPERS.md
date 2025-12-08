# Developer Guide

Contents:

- [General Structure](#general-structure)
- [Modifications Made](#modifications-made)
  - [QP Information](#qp-information)
- [Metrics](#metrics)
  - [QP Metrics](#qp-metrics)
  - [Motion Vector Metrics](#motion-vector-metrics)
  - [Bit Count Metrics](#bit-count-metrics)
  - [Block Count Metrics](#block-count-metrics)
  - [Frame Metadata](#frame-metadata)
- [Testing](#testing)
- [Debugging](#debugging)
- [Maintenance](#maintenance)
  - [Fetching new FFmpeg commits](#fetching-new-ffmpeg-commits)

## General Structure

This program patches ffmpeg to add support for extracting additional bitstream properties, or codec-related information, from the bitstream. The program is structured as follows:

- `VideoParser` is an API that provides a simple interface for extracting bitstream properties from a video file. It is implemented as a basic frame-by-frame reader that itself is using ffmpeg standard API calls to read the video.
- VideoParserCli is a command-line interface for VideoParser. It is implemented by using VideoParser API calls to extract the bitstream properties, and then printing them to the console in JSON format.
- ffmpeg is cloned and has a separate branch checked out.

To pass extra information from the ffmpeg part to the VideoParser part, we use the `SharedFrameInfo` struct to store the bitstream properties like QP values, motion vectors, etc. The definition is in `VideoParser/include/shared.h`, and it is included in ffmpeg as well, via an extra side data type `AV_FRAME_DATA_VIDEOPARSER_INFO`.

It is extracted from there using a helper function `videoparser_get_final_shared_frame_info`. This is implemented in `ffmpeg/libavutil/frame.c` as an additional method. It performs some extra calculations on the data, like average QP, standard deviation, etc.

To update the data, we have helper functions like `videoparser_shared_frame_info_update_qp`.

## Modifications Made

This explains the high level changes made to ffmpeg to support the extraction of bitstream properties.

We have modified `decode.c` to extract the frame index (method `ff_decode_receive_frame`).

### QP Information

To obtain the QP information, we modify:

- H.264: `h264_mb.c`, to extract the QP information from the `H264SliceContext` struct, in the function `ff_h264_hl_decode_mb`
- HEVC: `hevcdec.c`, to extract the QP information from the `HEVCLocalContext` struct, in the function `hls_coding_unit`
- VP9: `vp9.c`, to extract the QP information from the `VP9SharedContext` struct, in the function `vp9_decode_frame`
- AV1: `libaomdec.c`, to extract the QP information from the `AV1DecodeContext` struct, in the function `aom_decode`

## Metrics

This section documents the per-frame metrics extracted by the video parser.

### QP Metrics

QP (Quantization Parameter) values are codec-specific indices that control quantization strength. Higher values mean more compression/lower quality. Typical ranges: H.264/HEVC: 0–51, VP9: 0–255, AV1: 0–255.

#### qp_avg

Average QP of all coding units in the frame. Unit: QP index (dimensionless).

- **H.264**: Extracted per macroblock in `h264_mb.c`. Accumulated via `qp_sum` and `qp_cnt`. Range: 0–51.
- **HEVC**: Extracted per coding unit in `hevcdec.c`. Range: 0–51.
- **VP9**: Extracted per frame in `vp9.c`. Range: 0–255. Note: not yet implemented for segmented streams.
- **AV1**: Extracted via libaom in `libaomdec.c`. Range: 0–255.

#### qp_stdev

Standard deviation of QP values within a frame. Unit: QP index (dimensionless).

- **H.264**: Computed from `qp_sum_sqr` using variance formula.
- **HEVC**: Same method as H.264.
- **VP9**: Not yet implemented for segmented streams.
- **AV1**: Computed via libaom.

#### qp_min

Minimum QP value encountered in the frame. Unit: QP index (dimensionless).

- **H.264**: Updated during macroblock processing.
- **HEVC**: Updated during coding unit processing.
- **VP9**: Not yet implemented for segmented streams.
- **AV1**: Extracted via libaom.

#### qp_max

Maximum QP value encountered in the frame. Unit: QP index (dimensionless).

- **H.264**: Updated during macroblock processing.
- **HEVC**: Updated during coding unit processing.
- **VP9**: Not yet implemented for segmented streams.
- **AV1**: Extracted via libaom.

#### qp_init

Initial QP value from slice or frame header. Unit: QP index (dimensionless).

- **H.264**: Extracted from slice header.
- **HEVC**: Extracted from slice header.
- **VP9**: Not yet implemented for segmented streams.
- **AV1**: Extracted via libaom.

#### qp_bb_avg

Average QP excluding black border regions (letterbox areas). Unit: QP index (dimensionless).

- **H.264**: Work in progress.
- **HEVC**: Work in progress.

#### qp_bb_stdev

Standard deviation of QP excluding black border regions. Unit: QP index (dimensionless).

- **H.264**: Work in progress.
- **HEVC**: Work in progress.

### Motion Vector Metrics

Motion vector metrics are in codec-native sub-pel units:

- **H.264/HEVC**: Quarter-pel (1/4 pixel). Divide by 4 for full-pel values.
- **VP9**: Eighth-pel (1/8 pixel). Divide by 8 for full-pel values.

#### motion_avg

Average motion vector length per frame, computed as the mean of `sqrt(mv_x² + mv_y²)` over all motion vectors.

- **H.264**: Extracted in `h264_mb.c` via `mv_statistics_264`. Motion vectors are collected from both L0 (forward) and L1 (backward) reference lists. For bi-directional blocks, values from both directions are averaged. By default, raw motion vector values are used. A compile-time flag `VP_MV_POC_NORMALIZATION` can be set to `1` to enable POC-based normalization, which weighs motion vectors by temporal distance to their reference frames.
- **HEVC**: Extracted in `hevcdec.c` via `mv_statistics_hevc`. Motion vectors are collected from L0 and L1 prediction lists. For bi-predictive blocks, values from both directions are averaged. Raw MV values without POC normalization.
- **VP9**: Extracted in `vp9mvs.c` via `mv_statistics_vp9`. Motion vectors are collected after prediction in `ff_vp9_fill_mv`. For compound (bi-predictive) mode, values from both references are averaged. Raw MV values.

#### motion_stdev

Standard deviation of motion vector lengths within a frame.

- **H.264**: Computed from `mv_sum_sqr` accumulated during motion vector extraction.
- **HEVC**: Same method as H.264.
- **VP9**: Same method as H.264.

#### motion_x_avg

Average of the absolute X (horizontal) components of motion vectors.

- **H.264**: Accumulated separately from Y components using `fabs()` to ensure positive contributions.
- **HEVC**: Same accumulation method as H.264.
- **VP9**: Same accumulation method as H.264.

#### motion_y_avg

Average of the absolute Y (vertical) components of motion vectors.

- **H.264**: Same accumulation method as `motion_x_avg`, but for vertical motion.
- **HEVC**: Same accumulation method as H.264.
- **VP9**: Same accumulation method as H.264.

#### motion_x_stdev

Standard deviation of the X components of motion vectors.

- **H.264**: Computed from `mv_x_sum_sqr`.
- **HEVC**: Same method as H.264.
- **VP9**: Same method as H.264.

#### motion_y_stdev

Standard deviation of the Y components of motion vectors.

- **H.264**: Computed from `mv_y_sum_sqr`.
- **HEVC**: Same method as H.264.
- **VP9**: Same method as H.264.

#### motion_diff_avg

Average motion vector prediction residual length. Represents the difference between the actual motion vector and its predicted value (MVD).

- **H.264**: Extracted from `motion_diff_L0` and `motion_diff_L1` arrays. Values stored as unsigned 8-bit integers; values > 127 are interpreted as negative (two's complement).
- **HEVC**: Extracted from `MvField.mvd` which contains the coded motion vector difference.
- **VP9**: Extracted from coded MVD components for NEWMV mode only. For NEARESTMV/NEARMV modes (which use predicted MVs without coded residuals), MVD is zero.

#### motion_diff_stdev

Standard deviation of motion vector prediction residuals.

- **H.264**: Computed from `mv_diff_sum_sqr`.
- **HEVC**: Same method as H.264.
- **VP9**: Same method as H.264.

### Bit Count Metrics

#### motion_bit_count

Number of bits used for coding motion information in the frame. Unit: bits.

- **H.264**: Accumulated during CAVLC/CABAC decoding.
- **HEVC**: Accumulated during CABAC decoding in `hevcdec.c`.
- **VP9**: Accumulated during entropy decoding in `vp9mvs.c` for NEWMV mode blocks.

#### coefs_bit_count

Number of bits used for coding transform coefficients in the frame. Unit: bits.

- **H.264**: Accumulated during CAVLC/CABAC decoding.
- **HEVC**: Accumulated during CABAC decoding.
- **VP9**: Accumulated during frame decoding.

### Block Count Metrics

#### mb_mv_count

Number of macroblocks/coding units/blocks with motion vectors. Unit: count.

- **H.264**: Incremented for each macroblock partition that uses inter prediction.
- **HEVC**: Incremented for each prediction unit (PU) with inter prediction.
- **VP9**: Incremented for each block with non-zero motion (excludes ZEROMV mode). Note: VP9 uses variable block sizes (4x4 to 64x64), so count depends on encoder block size decisions.

#### mv_coded_count

Number of explicitly coded motion vectors (excluding predicted/derived MVs). Unit: count.

- **H.264**: Counts MVs that are entropy-coded in the bitstream.
- **HEVC**: Counts MVs that are entropy-coded in the bitstream.
- **VP9**: Counts NEWMV mode blocks where motion delta is explicitly coded. NEARESTMV/NEARMV modes use predicted MVs and are not counted.

### Frame Metadata

#### frame_type

Type of the frame: 1 = I (Intra), 2 = P (Predicted), 3 = B (Bi-directional). Unit: enum.

- **All codecs**: Extracted from frame header via ffmpeg's `pict_type`.

#### frame_idx

Zero-based index of the frame in decode order. Unit: count.

- **All codecs**: Tracked by the parser during decoding.

#### is_idr

Whether the frame is an IDR (Instantaneous Decoder Refresh) frame. Unit: boolean.

- **H.264**: True for NAL unit type 5.
- **HEVC**: True for IDR_W_RADL or IDR_N_LP NAL units.
- **VP9**: True for keyframes.
- **AV1**: True for keyframes.

#### size

Frame size. Unit: bytes.

- **All codecs**: Extracted from packet size.

#### pts

Presentation timestamp. Unit: seconds.

- **All codecs**: Converted from ffmpeg's `pts` using stream time base.

#### dts

Decoding timestamp. Unit: seconds.

- **All codecs**: Converted from ffmpeg's `dts` using stream time base.

## Testing

For testing you need `pytest`. For easy installation, just use `uv` (https://docs.astral.sh/uv/), which is a faster package manager for Python.

Generate the test videos:

```bash
util/generate-test-videos.sh
```

Then, obtain the test features in ldjson format:

```bash
wget https://storage.googleapis.com/aveq-storage/data/videoparser-ng/test/test.zip -O test/test.zip
unzip test/test.zip -d test
```

Now you can run the Python-based tests:

```bash
uvx pytest test/test.py
```

Also you can run the CLI tests:

```bash
uvx pytest test/test-cli.py
```

## Debugging

We have successfully used the following VS Code `launch.json` configuration to debug the CLI – it requires the `CMake Tools` extension:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "(lldb) Launch",
      "type": "cppdbg",
      "request": "launch",
      // Resolved by CMake Tools:
      "program": "${command:cmake.launchTargetPath}",
      "args": [
        // "${workspaceFolder}/test/test_video_h264.mkv",
        "${workspaceFolder}/test/test_video_h265.mkv",
      ],
      // comment out the below if you want to set your own breakpoints!
      "stopAtEntry": true,
      "cwd": "${workspaceFolder}/build",
      "environment": [
        {
          // add the directory where our target was built to the PATHs
          // it gets resolved by CMake Tools:
          "name": "PATH",
          "value": "${env:PATH}:${command:cmake.getLaunchTargetDirectory}"
        }
      ],
      "MIMode": "lldb"
    }
  ]
}
```

Replace the `"${workspaceFolder}/test/test_video_h265.mkv"` with the path to the video you want to debug.

## Maintenance

### Fetching new FFmpeg commits

Occasionally you want to rebase your local FFmpeg commits on top of the latest upstream FFmpeg commits. This is done by running the dedicated script.

Run the script:

```bash
util/rebase-ffmpeg.sh
```

The rebase may not be clean, so check the output of the script and resolve any conflicts.
