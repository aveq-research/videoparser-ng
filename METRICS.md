# Metrics

This document describes the various metrics extracted by the videoparser-ng tool.

## Codec Availability Matrix

The following table shows which metrics are available for each supported codec.

| Metric                    | H.264 | HEVC  |  VP9  |  AV1  |
| ------------------------- | :---: | :---: | :---: | :---: |
| **QP Metrics**            |       |       |       |       |
| `qp_avg`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |
| `qp_stdev`                |   ✅   |   ✅   |  ✅¹   |   ✅   |
| `qp_min`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |
| `qp_max`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |
| `qp_init`                 |   ✅   |   ✅   |  ✅¹   |   ✅   |
| `qp_bb_avg`               |   ⏳   |   ⏳   |   ⏳   |   ⏳   |
| `qp_bb_stdev`             |   ⏳   |   ⏳   |   ⏳   |   ⏳   |
| **Motion Vector Metrics** |       |       |       |       |
| `motion_avg`              |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_stdev`            |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_x_avg`            |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_y_avg`            |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_x_stdev`          |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_y_stdev`          |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_diff_avg`         |   ✅   |   ✅   |   ✅   |   ✅   |
| `motion_diff_stdev`       |   ✅   |   ✅   |   ✅   |   ✅   |
| **Bit Count Metrics**     |       |       |       |       |
| `motion_bit_count`        |   ✅   |   ✅   |   ✅   |   ✅   |
| `coefs_bit_count`         |   ✅   |   ✅   |   ✅   |   ✅   |
| **Block Count Metrics**   |       |       |       |       |
| `mb_mv_count`             |   ✅   |   ✅   |   ✅   |   ✅   |
| `mv_coded_count`          |   ✅   |   ✅   |   ✅   |   ✅   |
| **POC Metrics**           |       |       |       |       |
| `current_poc`             |   ✅   |   ✅   |   —   |   —   |
| `poc_diff`                |   ✅   |   ✅   |   —   |   —   |
| **Frame Metadata**        |       |       |       |       |
| `frame_type`              |   ✅   |   ✅   |   ✅   |   ✅   |
| `frame_idx`               |   ✅   |   ✅   |   ✅   |   ✅   |
| `is_idr`                  |   ✅   |   ✅   |   ✅   |   ✅   |
| `size`                    |   ✅   |   ✅   |   ✅   |   ✅   |
| `pts`                     |   ✅   |   ✅   |   ✅   |   ✅   |
| `dts`                     |   ✅   |   ✅   |   ✅   |   ✅   |

Legend:

- ✅ = Fully supported
- ⏳ = Work in progress
- — = Not applicable (codec does not support this concept)

Notes:

1. VP9 QP metrics are not yet implemented for segmented streams.

## Metric Definitions

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

**Work in progress for all codecs.**

#### qp_bb_stdev

Standard deviation of QP excluding black border regions. Unit: QP index (dimensionless).

**Work in progress for all codecs.**

### Motion Vector Metrics

Motion vector metrics are in codec-native sub-pel units:

- **H.264/HEVC**: Quarter-pel (1/4 pixel). Divide by 4 for full-pel values.
- **VP9/AV1**: Eighth-pel (1/8 pixel). Divide by 8 for full-pel values.

#### motion_avg

Average motion vector length per frame, computed as the mean of `sqrt(mv_x² + mv_y²)` over all motion vectors.

- **H.264**: Extracted in `h264_mb.c` via `mv_statistics_264`. Motion vectors are collected from both L0 (forward) and L1 (backward) reference lists. For bi-directional blocks, values from both directions are averaged. By default, raw motion vector values are used. A compile-time flag `VP_MV_POC_NORMALIZATION` can be set to `1` to enable POC-based normalization, which weighs motion vectors by temporal distance to their reference frames.
- **HEVC**: Extracted in `hevcdec.c` via `mv_statistics_hevc`. Motion vectors are collected from L0 and L1 prediction lists. For bi-predictive blocks, values from both directions are averaged. Raw MV values without POC normalization.
- **VP9**: Extracted in `vp9mvs.c` via `mv_statistics_vp9`. Motion vectors are collected after prediction in `ff_vp9_fill_mv`. For compound (bi-predictive) mode, values from both references are averaged. Raw MV values.
- **AV1**: Extracted in `libaomdec.c` via `videoparser_av1_extract_mv_stats` using libaom's inspection API. Motion vectors are collected from all inter blocks (mode >= NEARESTMV). For compound prediction, values from L0 and L1 references are averaged. Raw MV values in 1/8 pel units.

#### motion_stdev

Standard deviation of motion vector lengths within a frame.

This is computed from `mv_sum_sqr` accumulated during motion vector extraction.

#### motion_x_avg

Average of the absolute X (horizontal) components of motion vectors.

This is accumulated separately from Y components using `fabs()` to ensure positive contributions.

#### motion_y_avg

Average of the absolute Y (vertical) components of motion vectors.

Same accumulation method as `motion_x_avg`, but for vertical motion.

#### motion_x_stdev

Standard deviation of the X components of motion vectors.

Computed from `mv_x_sum_sqr`.

#### motion_y_stdev

Standard deviation of the Y components of motion vectors.

Computed from `mv_y_sum_sqr`.

#### motion_diff_avg

Average motion vector prediction residual length. Represents the difference between the actual motion vector and its predicted value (MVD).

- **H.264**: Extracted from `motion_diff_L0` and `motion_diff_L1` arrays. Values stored as unsigned 8-bit integers; values > 127 are interpreted as negative (two's complement).
- **HEVC**: Extracted from `MvField.mvd` which contains the coded motion vector difference.
- **VP9**: Extracted from coded MVD components for NEWMV mode only. For NEARESTMV/NEARMV modes (which use predicted MVs without coded residuals), MVD is zero.
- **AV1**: Extracted via modified libaom decoder. MVD is captured during `assign_mv()` in `decodemv.c` by computing the difference between final MV and reference MV for NEWMV modes. Stored in `mbmi->mvd[]` and exposed through the inspection API. For compound modes with one NEWMV reference, only that reference's MVD is non-zero.

#### motion_diff_stdev

Standard deviation of motion vector prediction residuals.

Computed from `mv_diff_sum_sqr`.

- **AV1**: Note: MVD values from the inspection API are accumulated in `libaomdec.c`.

### Bit Count Metrics

#### motion_bit_count

Number of bits used for coding motion information in the frame. Unit: bits.

- **H.264**: Accumulated during CAVLC/CABAC decoding.
- **HEVC**: Accumulated during CABAC decoding in `hevcdec.c`.
- **VP9**: Accumulated during entropy decoding in `vp9mvs.c` for NEWMV mode blocks.
- **AV1**: Accumulated in modified libaom decoder using `aom_reader_tell_frac()` before and after `assign_mv()` calls in `read_inter_block_mode_info()` (`decodemv.c`). Stored in `pbi->motion_bits` and exposed through `insp_frame_data.motion_bits`. Note: Bit counts are in fractional bits (1/8th precision) during accumulation and converted to whole bits in `ifd_inspect()`.

#### coefs_bit_count

Number of bits used for coding transform coefficients in the frame. Unit: bits.

- **H.264**: Accumulated during CAVLC/CABAC decoding.
- **HEVC**: Accumulated during CABAC decoding.
- **VP9**: Accumulated during frame decoding.
- **AV1**: Accumulated in modified libaom decoder using `aom_reader_tell_frac()` before and after coefficient reading calls in `decode_reconstruct_tx()` and intra block decoding loops (`decodeframe.c`). Stored in `td->coef_bits` (per thread) and exposed through `insp_frame_data.coef_bits`. Note: Bit counts are in fractional bits (1/8th precision) during accumulation and converted to whole bits in `ifd_inspect()`.

### Block Count Metrics

#### mb_mv_count

Number of macroblocks/coding units/blocks with motion vectors. Unit: count.

- **H.264**: Incremented for each macroblock partition that uses inter prediction.
- **HEVC**: Incremented for each prediction unit (PU) with inter prediction.
- **VP9**: Incremented for each block with non-zero motion (excludes ZEROMV mode). Note: VP9 uses variable block sizes (4x4 to 64x64), so count depends on encoder block size decisions.
- **AV1**: Incremented for each MI (mode info) block with inter prediction (mode >= NEARESTMV). Note: AV1 uses variable block sizes (4x4 to 128x128), so count is at MI resolution (4x4 units).

#### mv_coded_count

Number of explicitly coded motion vectors (excluding predicted/derived MVs). Unit: count.

- **H.264**: Counts MVs that are entropy-coded in the bitstream.
- **HEVC**: Counts MVs that are entropy-coded in the bitstream.
- **VP9**: Counts NEWMV mode blocks where motion delta is explicitly coded. NEARESTMV/NEARMV modes use predicted MVs and are not counted.
- **AV1**: Counts NEWMV mode blocks (including compound variants: NEW_NEWMV, NEAREST_NEWMV, NEW_NEARESTMV, NEAR_NEWMV, NEW_NEARMV). NEARESTMV/NEARMV/GLOBALMV modes use predicted MVs and are not counted.

### POC Metrics

POC (Picture Order Count) is a frame ordering mechanism used in H.264 and HEVC to track the display order of frames independently from decode order.

#### current_poc

The Picture Order Count of the current frame. Unit: count (dimensionless).

- **H.264**: Extracted from the decoded picture's POC value in `h264_slice.c`. POC values are computed according to the H.264 spec (types 0, 1, or 2) and can wrap at 65536. Values > 32768 are adjusted to be negative for consistency.
- **HEVC**: Extracted from `s->poc` in `hevcdec.c` during `hevc_frame_start()`. POC is computed per the HEVC spec from `pic_order_cnt_lsb` in the slice header.
- **VP9**: Not applicable. VP9 does not use the POC concept. Always returns 0.
- **AV1**: Not applicable. AV1 does not use the POC concept. Always returns 0.

#### poc_diff

The minimum POC difference between consecutive frames. This represents the POC increment per frame and is useful for temporal normalization of motion vectors. Unit: count (dimensionless).

- **H.264**: Calculated by tracking POC changes between frames. Uses PTS information when available for more accurate calculation (handles cases where frames are decoded out of order). Typical values: 1 or 2.
- **HEVC**: Same calculation method as H.264. Tracked in `hevc_frame_start()`. Typical values: 1 or 2.
- **VP9**: Not applicable. Always returns 0.
- **AV1**: Not applicable. Always returns 0.

**Note:** For the first frame, `poc_diff` is `-1` (sentinel value indicating "not yet calculated"). Starting from the second frame, the actual POC difference is computed and reported. Most encoders use a `poc_diff` of 1 or 2.

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

- **All codecs**: Extracted from packet size directly in ffmpeg.

#### pts

Presentation timestamp. Unit: seconds.

- **All codecs**: Converted from ffmpeg's `pts` using stream time base.

#### dts

Decoding timestamp. Unit: seconds.

- **All codecs**: Converted from ffmpeg's `dts` using stream time base.

## Differences with Legacy Implementation

This section documents (intentional) differences between videoparser-ng and the legacy [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser) implementation. These changes were made to improve correctness, cross-codec consistency, and simplicity.

### Difference Analysis

A detailed analysis of the differences was performed with the help of the `util/reencode_videos.sh` script, which generates a test set of encoded videos.
As test videos, the [`a2_2k` set from the AOM CTC was used](https://media.xiph.org/video/aomctc/test_set/a2_2k/). All videos were encoded with H.264, HEVC, VP9, and AV1 at default encoding settings, using `libx264`, `libx265`, `libvpx-vp9`, and `rav1e` encoders from FFmpeg.

The `util/compare_parsers.py` script was then used to compare the output of videoparser-ng against the legacy videoparser for all test videos and codecs (except AV1, because it has no legacy implementation). The results were analyzed to identify systematic differences, using v0.3.0 of the parser.

Here's the summary – note the (intentional) differences in motion vector metrics:

#### All Codecs (60 videos)

| Metric            | Legacy Mean | New Mean | Diff Mean | Rel Diff (%) |
| ----------------- | ----------- | -------- | --------- | ------------ |
| coefs_bit_count   | 0           | 287541   | 287541    | N/A          |
| current_poc       | 3.70577     | 3.70577  | 0         | +0.0000      |
| dts               | 1.76728     | 1.76728  | 0         | +0.0000      |
| frame_idx         | 64.5        | 64.5     | 0         | +0.0000      |
| frame_type        | 1.93731     | 1.93731  | 0         | +0.0000      |
| is_idr            | 0.062692    | 0.062692 | 0         | +0.0000      |
| mb_mv_count       | 53095.9     | 56914.3  | 3818.45   | +18.4815     |
| motion_avg        | 59.3029     | 66.2023  | 6.8994    | +106.3121    |
| motion_bit_count  | 0           | 32502.3  | 32502.3   | N/A          |
| motion_diff_avg   | 11.8565     | 13.3622  | 1.50562   | +61.2840     |
| motion_diff_stdev | 82.7285     | 34.617   | -48.1115  | +15.7609     |
| motion_stdev      | 151.392     | 61.8602  | -85.4229  | +35.5615     |
| motion_x_avg      | 21.929      | 48.918   | 26.989    | +361.8505    |
| motion_x_stdev    | 41.4919     | 51.2327  | 11.0114   | +66.1315     |
| motion_y_avg      | 11.2515     | 30.8161  | 19.5647   | +359.1044    |
| motion_y_stdev    | 28.3646     | 38.7961  | 11.0156   | +70.2423     |
| mv_coded_count    | 23458.3     | 11115.3  | -12343    | -29.3364     |
| poc_diff          | 1.22603     | 0.678462 | -0.547564 | -44.6430     |
| pts               | 1.76728     | 1.76728  | 0         | +0.0000      |
| qp_avg            | 56.6819     | 56.6131  | -0.068836 | -0.1222      |
| qp_bb_avg         | 56.6826     | 56.6131  | -0.069485 | -0.1245      |
| qp_bb_stdev       | 1.73938     | 1.75385  | 0.014475  | +3.6207      |
| qp_init           | 59.9001     | 57.6337  | -2.26641  | -6.6261      |
| qp_max            | 63.5731     | 63.5731  | 0         | +0.0000      |
| qp_min            | 54.0451     | 54.0451  | 0         | +0.0000      |
| qp_stdev          | 1.73937     | 1.75385  | 0.014483  | +3.6206      |
| size              | 48851.7     | 48851.7  | 0         | +0.0000      |

#### Codec: h264 (22 videos)

| Metric            | Legacy Mean | New Mean | Diff Mean | Rel Diff (%) |
| ----------------- | ----------- | -------- | --------- | ------------ |
| coefs_bit_count   | 0           | 43261    | 43261     | N/A          |
| current_poc       | 5.42308     | 5.42308  | 0         | +0.0000      |
| dts               | 1.7696      | 1.7696   | 0         | +0.0000      |
| frame_idx         | 64.5        | 64.5     | 0         | +0.0000      |
| frame_type        | 1.91538     | 1.91538  | 0         | +0.0000      |
| is_idr            | 0.084615    | 0.084615 | 0         | +0.0000      |
| mb_mv_count       | 70000.8     | 70000.8  | 0         | +0.0000      |
| motion_avg        | 25.1819     | 50.3638  | 25.1819   | +100.0000    |
| motion_bit_count  | 0           | 43609    | 43609     | N/A          |
| motion_diff_avg   | 4.59748     | 9.19496  | 4.59748   | +100.0000    |
| motion_diff_stdev | 8.63572     | 14.3427  | 5.70701   | +75.8013     |
| motion_stdev      | 24.9904     | 49.9809  | 24.9904   | +100.0000    |
| motion_x_avg      | 19.2419     | 38.4838  | 19.2419   | +100.0000    |
| motion_x_stdev    | 22.8662     | 45.7323  | 22.8662   | +100.0000    |
| motion_y_avg      | 10.8976     | 21.7953  | 10.8976   | +100.0000    |
| motion_y_stdev    | 13.4801     | 26.9602  | 13.4801   | +99.9999     |
| mv_coded_count    | 6238.22     | 6238.22  | 0         | +0.0000      |
| poc_diff          | 1.76923     | 1        | -0.769231 | -43.4783     |
| pts               | 1.7696      | 1.7696   | 0         | +0.0000      |
| qp_avg            | 28.6783     | 28.6783  | 0         | +0.0000      |
| qp_bb_avg         | 28.6783     | 28.6783  | -1.1e-05  | -0.0000      |
| qp_bb_stdev       | 1.93188     | 1.9319   | 2.1e-05   | +0.0011      |
| qp_init           | 33.7552     | 30.5615  | -3.19371  | -9.7213      |
| qp_max            | 40.8217     | 40.8217  | 0         | +0.0000      |
| qp_min            | 27.2476     | 27.2476  | 0         | +0.0000      |
| qp_stdev          | 1.9319      | 1.9319   | -0        | -0.0000      |
| size              | 49785.2     | 49785.2  | 0         | +0.0000      |

#### Codec: hevc (19 videos)

| Metric            | Legacy Mean | New Mean | Diff Mean | Rel Diff (%) |
| ----------------- | ----------- | -------- | --------- | ------------ |
| coefs_bit_count   | 0           | 151677   | 151677    | N/A          |
| current_poc       | 5.42308     | 5.42308  | 0         | +0.0000      |
| dts               | 1.76595     | 1.76595  | 0         | +0.0000      |
| frame_idx         | 64.5        | 64.5     | 0         | +0.0000      |
| frame_type        | 1.91538     | 1.91538  | 0         | +0.0000      |
| is_idr            | 0.084615    | 0.084615 | 0         | +0.0000      |
| mb_mv_count       | 39225.8     | 91495    | 52269.2   | +142.8419    |
| motion_avg        | 25.2005     | 43.7143  | 18.5138   | +51.9497     |
| motion_bit_count  | 0           | 13420.3  | 13420.3   | N/A          |
| motion_diff_avg   | 10.1426     | 15.6337  | 5.49102   | +46.1900     |
| motion_diff_stdev | 27.9233     | 41.3103  | 13.387    | +34.4893     |
| motion_stdev      | 27.4369     | 45.4117  | 17.9748   | +59.0297     |
| motion_x_avg      | 18.3599     | 32.1805  | 13.8206   | +52.5993     |
| motion_x_stdev    | 22.7405     | 37.3236  | 14.5832   | +58.3041     |
| motion_y_avg      | 11.5723     | 19.8128  | 8.24053   | +52.0909     |
| motion_y_stdev    | 17.3031     | 28.7829  | 11.4798   | +57.9902     |
| mv_coded_count    | 24907.9     | 24907.9  | 0         | +0.0000      |
| poc_diff          | 1.82308     | 0.984615 | -0.838462 | -45.9916     |
| pts               | 1.76595     | 1.76595  | 0         | +0.0000      |
| qp_avg            | 31.9785     | 31.7612  | -0.217378 | -0.3860      |
| qp_bb_avg         | 31.9806     | 31.7612  | -0.219413 | -0.3932      |
| qp_bb_stdev       | 3.25586     | 3.30155  | 0.045688  | +7.8119      |
| qp_init           | 36.2628     | 32.8036  | -3.45911  | -9.6683      |
| qp_max            | 39.6794     | 39.6794  | 0         | +0.0000      |
| qp_min            | 25.3085     | 25.3085  | 0         | +0.0000      |
| qp_stdev          | 3.25581     | 3.30155  | 0.045737  | +7.8129      |
| size              | 26132.6     | 26132.6  | 0         | +0.0000      |

#### Codec: vp9 (19 videos)

| Metric            | Legacy Mean | New Mean | Diff Mean | Rel Diff (%) |
| ----------------- | ----------- | -------- | --------- | ------------ |
| coefs_bit_count   | 0           | 706254   | 706254    | N/A          |
| current_poc       | 0           | 0        | 0         | N/A          |
| dts               | 1.76595     | 1.76595  | 0         | +0.0000      |
| frame_idx         | 64.5        | 64.5     | 0         | +0.0000      |
| frame_type        | 1.98462     | 1.98462  | 0         | +0.0000      |
| is_idr            | 0.015385    | 0.015385 | 0         | +0.0000      |
| mb_mv_count       | 47391.8     | 7180.91  | -40210.9  | -84.4794     |
| motion_avg        | 132.914     | 107.03   | -25.8842  | +167.9833    |
| motion_bit_count  | 0           | 38723.9  | 38723.9   | N/A          |
| motion_diff_avg   | 21.9757     | 15.9159  | -6.05983  | +31.5489     |
| motion_diff_stdev | 223.325     | 51.3991  | -171.926  | -72.4881     |
| motion_stdev      | 421.708     | 92.0636  | -329.515  | -67.9687     |
| motion_x_avg      | 28.6094     | 77.7372  | 49.1278   | +974.2970    |
| motion_x_stdev    | 81.81       | 71.5106  | -7.24807  | +32.9989     |
| motion_y_avg      | 11.3404     | 52.2647  | 40.9243   | +966.1333    |
| motion_y_stdev    | 56.6607     | 62.514   | 7.51332   | +46.8045     |
| mv_coded_count    | 41947.9     | 2969.93  | -38978    | -92.6414     |
| poc_diff          | 0           | 0        | 0         | N/A          |
| pts               | 1.76595     | 1.76595  | 0         | +0.0000      |
| qp_avg            | 113.811     | 113.811  | 0         | +0.0000      |
| qp_bb_avg         | 113.811     | 113.811  | 0         | +0.0000      |
| qp_bb_stdev       | 0           | 0        | 0         | N/A          |
| qp_init           | 113.811     | 113.811  | 0         | +0.0000      |
| qp_max            | 113.811     | 113.811  | 0         | +0.0000      |
| qp_min            | 113.811     | 113.811  | 0         | +0.0000      |
| qp_stdev          | 0           | 0        | 0         | N/A          |
| size              | 70489.9     | 70489.9  | 0         | +0.0000      |

### All Codecs: POC-based Motion Vector Normalization

The legacy implementation normalized motion vectors by temporal distance (POC difference) to reference frames:

```c
// Legacy: MV values divided by temporal distance
mvX /= (8 * FrmDist);
NormFwd = 1.0 / (2.0 * fabs((CurrPOC - RefPOC) / POC_DIF));
```

videoparser-ng uses raw motion vector values without normalization. This provides:

- Simpler, more predictable output
- Values that directly correspond to what's in the bitstream
- Independence from GOP structure and reference frame selection

If temporal normalization is needed, it can be applied as a post-processing step.

### VP9: Motion Statistics for All Inter Modes

The legacy VP9 implementation only accumulated motion vector statistics for `NEWMV` mode blocks (explicitly coded motion deltas):

```c
// Legacy VP9: Only NEWMV counted
if (b->mode[idx] == NEWMV) {
    S->MV_Length += MV_LengthXY * count;
    // ...
}
```

This was inconsistent with H.264/HEVC, which counted **all** inter blocks regardless of prediction mode.

videoparser-ng counts all inter modes (`NEARESTMV`, `NEARMV`, `NEWMV`) except `ZEROMV`, making VP9 behavior consistent with other codecs:

| Codec | Legacy MV Stats  | videoparser-ng MV Stats            |
| ----- | ---------------- | ---------------------------------- |
| H.264 | All inter blocks | All inter blocks                   |
| HEVC  | All inter blocks | All inter blocks                   |
| VP9   | `NEWMV` only     | All inter blocks (except `ZEROMV`) |

### VP9: Motion Bit Count

The legacy implementation did not track `motion_bit_count` for VP9 (always returned 0).

videoparser-ng correctly tracks motion bits during VP9 entropy decoding.

### VP9: Removed Weighted Variance Accumulation

The legacy VP9 implementation used `scount = count * count` when accumulating squared values for variance:

```c
// Legacy: Inflated variance calculation
scount = count * count;
S->MV_SumSQR += SQR(MV_LengthXY) * scount;
```

videoparser-ng uses standard variance accumulation without the extra count multiplier, providing mathematically correct standard deviation values.

### All Codecs: Removed Arbitrary Scaling Factors

The legacy implementation applied various scaling factors (e.g., `4.0 *` multiplier on MV lengths) that were likely historical artifacts.

videoparser-ng outputs unscaled values in native codec units (1/4 pel for H.264/HEVC, 1/8 pel for VP9/AV1).
