# Metrics

This document describes the various metrics extracted by the videoparser-ng tool.

## Codec Availability Matrix

The following table shows which metrics are available for each supported codec.

| Metric                    | H.264 | HEVC  |  VP9  |  AV1  | MPEG-2 |
| ------------------------- | :---: | :---: | :---: | :---: | :----: |
| **QP Metrics**            |       |       |       |       |        |
| `qp_avg`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |   ✅    |
| `qp_stdev`                |   ✅   |   ✅   |  ✅¹   |   ✅   |   ✅    |
| `qp_min`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |   ✅    |
| `qp_max`                  |   ✅   |   ✅   |  ✅¹   |   ✅   |   ✅    |
| `qp_init`                 |   ✅   |   ✅   |  ✅¹   |   ✅   |   ✅    |
| `qp_bb_avg`               |   ⏳   |   ⏳   |   ⏳   |   ⏳   |   ⏳    |
| `qp_bb_stdev`             |   ⏳   |   ⏳   |   ⏳   |   ⏳   |   ⏳    |
| `qp_coded_avg`            |   ✅   |   ✅   |  ✅³   |  ✅³   |   —    |
| `qp_coded_stdev`          |   ✅   |   ✅   |  ✅³   |  ✅³   |   —    |
| `qp_coded_min`            |   ✅   |   ✅   |  ✅³   |  ✅³   |   —    |
| `qp_coded_max`            |   ✅   |   ✅   |  ✅³   |  ✅³   |   —    |
| **Motion Vector Metrics** |       |       |       |       |        |
| `motion_avg`              |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_stdev`            |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_x_avg`            |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_y_avg`            |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_x_stdev`          |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_y_stdev`          |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_diff_avg`         |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `motion_diff_stdev`       |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| **Bit Count Metrics**     |       |       |       |       |        |
| `motion_bit_count`        |   ✅   |   ✅   |   ✅   |   ✅   |   ✅²   |
| `coefs_bit_count`         |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| **Block Count Metrics**   |       |       |       |       |        |
| `mb_mv_count`             |   ✅   |   ✅   |   ✅   |   ✅   |   ✅²   |
| `mv_coded_count`          |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `blocks_total`            |   ✅   |   ✅   |   ✅   |   ✅   |   —    |
| `blocks_skipped`          |   ✅   |   ✅   |   ✅   |   ✅   |   —    |
| `blocks_coded`            |   ✅   |   ✅   |   ✅   |   ✅   |   —    |
| `is_all_skip`             |   ✅   |   ✅   |   ✅   |   ✅   |   —    |
| **POC Metrics**           |       |       |       |       |        |
| `current_poc`             |   ✅   |   ✅   |   —   |   —   |   —    |
| `poc_diff`                |   ✅   |   ✅   |   —   |   —   |   —    |
| **Frame Metadata**        |       |       |       |       |        |
| `frame_type`              |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `frame_idx`               |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `is_idr`                  |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `size`                    |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `pts`                     |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| `dts`                     |   ✅   |   ✅   |   ✅   |   ✅   |   ✅    |
| **Interlacing**           |       |       |       |       |        |
| `interlaced`              |   ✅   |  ✅⁴   |   —   |   —   |   ✅    |
| `top_field_first`         |   ✅   |  ✅⁴   |   —   |   —   |   ✅    |
| `repeated_fields`         |   ✅   |   —   |   —   |   —   |   ✅    |

Legend:

- ✅ = Fully supported
- ⏳ = Work in progress
- — = Not applicable (codec does not support this concept)

Notes:

1. VP9 QP metrics are not yet implemented for segmented streams.
2. For MPEG-2, `mb_mv_count` counts 16x16 macroblocks, and `motion_bit_count` includes concealment motion vectors in intra macroblocks.
3. For VP9, the coded QP is the QP of each coded block's segment. For AV1, it is the quantizer index of each coded 4x4 block, with delta-q and segments applied. Without segments or delta-q, it equals the frame QP. The existing `qp_*` fields still report the frame QP.
4. For HEVC, FFmpeg returns each field as its own frame. See [Interlacing](#interlacing).

For MPEG-2, the block counts are 0, `is_all_skip` is false, and the coded QP fields are empty.

## Metric Definitions

This section documents the per-frame metrics extracted by the video parser. For implementation details, see [DEVELOPERS.md](DEVELOPERS.md).

### QP Metrics

QP (Quantization Parameter) values are codec-specific indices that control quantization strength. Higher values mean more compression/lower quality. Typical ranges: H.264/HEVC: 0–51, VP9: 0–255, AV1: 0–255, MPEG-2: 1–112 (the `quantiser_scale` value, not the 5-bit code; for MPEG-1: 1–31).

| Metric | Description |
| ------ | ----------- |
| `qp_avg` | Average QP of all coding units in the frame. |
| `qp_stdev` | Standard deviation of QP values within a frame. |
| `qp_min` | Minimum QP value encountered in the frame. |
| `qp_max` | Maximum QP value encountered in the frame. |
| `qp_init` | Initial QP value from slice or frame header. |
| `qp_bb_avg` | Average QP excluding black border regions (letterbox areas). ⏳ Work in progress. |
| `qp_bb_stdev` | Standard deviation of QP excluding black border regions. ⏳ Work in progress. |
| `qp_coded_avg` | Average QP of coded blocks (see `blocks_coded`). `null` if the frame has no coded blocks. |
| `qp_coded_stdev` | Standard deviation of the QP of coded blocks. `null` if the frame has no coded blocks. |
| `qp_coded_min` | Minimum QP of coded blocks. 0 if the frame has no coded blocks. |
| `qp_coded_max` | Maximum QP of coded blocks. 0 if the frame has no coded blocks. |

The QP of skipped blocks is not used by the decoder: H.264 and HEVC carry the slice QP or the predicted QP over to them, and VP9 and AV1 use the frame QP for all blocks. An encoder can code a static picture with skipped frames at any QP, for example 51. The `qp_coded_*` fields leave out these blocks. Like `qp_avg`, they count each block once, in the codec's own unit (H.264: macroblock, HEVC: coding unit).

### Motion Vector Metrics

Motion vector metrics are in codec-native sub-pel units:

- **H.264/HEVC**: Quarter-pel (1/4 pixel). Divide by 4 for full-pel values.
- **VP9/AV1**: Eighth-pel (1/8 pixel). Divide by 8 for full-pel values.
- **MPEG-2**: Half-pel (1/2 pixel). Divide by 2 for full-pel values. Vertical components of field motion vectors in frame pictures are converted to frame lines.

| Metric | Description |
| ------ | ----------- |
| `motion_avg` | Average motion vector length per frame, computed as the mean of `sqrt(mv_x² + mv_y²)` over all motion vectors. For bi-directional/compound blocks, values from both reference directions are averaged. |
| `motion_stdev` | Standard deviation of motion vector lengths within a frame. |
| `motion_x_avg` | Average of the absolute X (horizontal) components of motion vectors. |
| `motion_y_avg` | Average of the absolute Y (vertical) components of motion vectors. |
| `motion_x_stdev` | Standard deviation of the X components of motion vectors. |
| `motion_y_stdev` | Standard deviation of the Y components of motion vectors. |
| `motion_diff_avg` | Average motion vector prediction residual (MVD) length. Represents the difference between the actual motion vector and its predicted value. |
| `motion_diff_stdev` | Standard deviation of motion vector prediction residuals. |

### Bit Count Metrics

| Metric | Description |
| ------ | ----------- |
| `motion_bit_count` | Number of bits used for coding motion information in the frame. |
| `coefs_bit_count` | Number of bits used for coding transform coefficients in the frame. |

### Block Count Metrics

| Metric | Description |
| ------ | ----------- |
| `mb_mv_count` | Number of macroblocks/coding units/blocks with motion vectors. For variable block size codecs (VP9, AV1), the count depends on encoder block size decisions. |
| `mv_coded_count` | Number of explicitly coded motion vectors (excluding predicted/derived MVs). Only counts blocks where motion delta is explicitly coded in the bitstream. |
| `blocks_total` | Area of all decoded blocks in the frame, in units of 4x4 luma samples. 0 if the codec does not report block counts. |
| `blocks_skipped` | Area of skipped blocks, in units of 4x4 luma samples. A skipped block has no residual and no coded motion vector difference: it is a copy of its motion-compensated prediction. |
| `blocks_coded` | Area of intra blocks and blocks with coded residual, in units of 4x4 luma samples. The QP applies only to these blocks. Inter blocks without residual but with a coded motion vector difference are neither skipped nor coded. |
| `is_all_skip` | Whether this is a non-I frame in which all blocks are skipped (`blocks_skipped == blocks_total`). Such a frame quantizes nothing; its QP fields describe no coded data and should be left out of QP statistics. |

The codecs define skipped and coded blocks as follows:

- H.264: per macroblock (16 units). Skipped: P_Skip and B_Skip, and in B slices also B_Direct_16x16 and B_8x8 with four direct sub-macroblocks if they have no residual (cbp 0). Coded: intra or cbp not 0.
- HEVC: per coding unit ((size / 4)² units). Skipped: `cu_skip_flag` set. Coded: intra (including PCM), or at least one `cbf` set in the transform tree.
- VP9: per block, clipped to the picture. Skipped: inter block without non-zero coefficients and without a non-zero coded motion vector difference (NEWMV). The VP9 skip flag alone only means "no residual". Coded: intra or with non-zero coefficients.
- AV1: per 4x4 mode-info unit, clipped to the picture. Skipped: inter block with `skip_mode`, or with `skip_txfm` and no NEWMV mode. Coded: intra (including intra block copy) or without `skip_txfm`.
- VP9 and AV1 frames that show an earlier decoded frame (`show_existing_frame`) have no block data. Their whole area counts as skipped, so they are all-skip unless they are I-frames. Such a frame may show a hidden reference frame (alt-ref) that was not displayed before.

### POC Metrics

POC (Picture Order Count) is a frame ordering mechanism used in H.264 and HEVC to track the display order of frames independently from decode order. VP9, AV1 and MPEG-2 do not use POC.

| Metric | Description |
| ------ | ----------- |
| `current_poc` | The Picture Order Count of the current frame. Returns 0 for VP9/AV1/MPEG-2. |
| `poc_diff` | The minimum POC difference between consecutive frames. Useful for temporal normalization. Returns 0 for VP9/AV1/MPEG-2. For the first frame, returns `-1` (sentinel value). Most encoders use a value of 1 or 2. |

### Frame Metadata

| Metric | Description |
| ------ | ----------- |
| `frame_type` | Type of the frame: 1 = I (Intra), 2 = P (Predicted), 3 = B (Bi-directional). |
| `frame_idx` | Zero-based index of the frame in decode order. |
| `is_idr` | Whether the frame is an IDR (Instantaneous Decoder Refresh) or keyframe. |
| `size` | Frame size in bytes. |
| `pts` | Presentation timestamp in seconds. |
| `dts` | Decoding timestamp in seconds. |

### Interlacing

The interlacing fields come from the flags that FFmpeg's decoder sets on each decoded frame (`AV_FRAME_FLAG_INTERLACED`, `AV_FRAME_FLAG_TOP_FIELD_FIRST` and `repeat_pict`), the same values that `ffprobe -show_frames` prints as `interlaced_frame`, `top_field_first` and `repeat_pict`.

| Metric | Description |
| ------ | ----------- |
| `interlaced` | Whether the frame is interlaced: its two fields show different times. |
| `top_field_first` | Whether the top field is displayed first. Only meaningful for interlaced frames and frames with repeated fields. |
| `repeated_fields` | Number of fields to display again after the two fields of the frame: 1 if the first field is repeated (soft telecine, for example 3:2 pulldown), 2 for frame doubling, 4 for frame tripling, otherwise 0. The frame is displayed for `2 + repeated_fields` field periods. |

The sequence info and the summary have these fields:

| Metric | Description |
| ------ | ----------- |
| `video_interlaced` | Whether the first decoded frame is interlaced. |
| `video_field_order` | Field order of the first decoded frame: `progressive`, `tt` (top field first) or `bb` (bottom field first), as in ffprobe's `field_order`. `unknown` if no frame could be decoded when opening the input. |
| `interlaced_frames` | Number of interlaced frames (in the summary). |
| `repeated_fields` | Sum of `repeated_fields` over all frames (in the summary). |

The parser decodes the first frame when it opens the input, so that the sequence info, which comes before the frames, has `video_interlaced` and `video_field_order`. The field order of the container or of FFmpeg's parsers (ffprobe's stream `field_order`) is not used, because it is often wrong. For example, it is `tb` for TFF H.264 that FFmpeg wrote to Matroska, `tt` for BFF H.264 in MPEG-TS, and `progressive` for interlaced HEVC in MPEG-TS. A stream can mix interlaced and progressive frames, so use `interlaced_frames` or the frame fields if the first frame may not be typical.

`video_framerate` does not change for interlaced video: it is the frame rate, which is half the field rate (25 for 1080i with 50 fields per second). For soft telecine, it is the display frame rate. For example, MPEG-2 with 23.976 coded frames per second and 3:2 pulldown has a `video_framerate` of 29.97. The coded frame rate is then `frame_count / video_duration`, and the display frame rate is `(2 * frame_count + repeated_fields) / 2 / video_duration`.

The decoders set the flags as follows:

- MPEG-2: `interlaced` if `progressive_sequence` and `progressive_frame` are both 0. `top_field_first` from the picture coding extension. A pair of field pictures is returned as one frame, with `top_field_first` set if the first field is the top field. `repeated_fields` is 1 for `repeat_first_field` in a progressive frame of an interlaced sequence. In a progressive sequence (`progressive_sequence` 1), `repeat_first_field` means frame doubling (2), or with `top_field_first` frame tripling (4). MPEG-1 is always progressive.
- H.264: with a picture timing SEI and `pic_struct_present_flag`, `interlaced` for the field `pic_struct` values, and for top-bottom or bottom-top (3, 4) if the picture is a field pair or an MBAFF frame. A set `ct_type` overrides this. `pic_struct` 5 and 6 (top-bottom-top, bottom-top-bottom) give `repeated_fields` 1, frame doubling 2, and frame tripling 4. Without the SEI, `interlaced` if the picture is a field pair (PAFF) or an MBAFF frame. `top_field_first` comes from the field picture order counts, otherwise from `pic_struct`, otherwise it is set for interlaced frames.
- HEVC: HEVC codes interlaced video as separate field pictures, and FFmpeg returns each field as its own frame (of half height, at the field rate). `interlaced` is set if the `pic_struct` of the picture timing SEI is a field (1, 2, 9–12), and `top_field_first` for top fields. Without this SEI, frames are progressive. `repeated_fields` is always 0.
- VP9 and AV1 have no interlaced coding; the fields are always false or 0.

## Differences with Legacy Implementation

This section documents (intentional) differences between videoparser-ng and the legacy [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser) implementation. These changes were made to improve correctness, cross-codec consistency, and simplicity.

The two "Difference Analysis" sections below summarize the differences found when comparing the two implementations on a common test set, both in new and legacy mode. Then, there are explanations regarding the most significant differences.

### Difference Analysis

A detailed analysis of the differences was performed with the help of the `util/reencode_videos.sh` script, which generates a test set of encoded videos.
As test videos, the [`a2_2k` set from the AOM CTC was used](https://media.xiph.org/video/aomctc/test_set/a2_2k/). All videos were encoded with H.264, HEVC, VP9, and AV1 at default encoding settings, using `libx264`, `libx265`, `libvpx-vp9`, and `rav1e` encoders from FFmpeg.

The `util/compare_parsers.py` script was then used to compare the output of videoparser-ng against the legacy videoparser for all test videos and codecs (except AV1, because it has no legacy implementation). The results were analyzed to identify systematic differences, using v0.3.0 of the parser.

Here's the summary – note the (intentional) differences in motion vector metrics:

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
| mb_mv_count       | 47391.8     | 47213.5  | -178.293  | -0.4063      |
| motion_avg        | 132.914     | 24.6461  | -108.268  | -43.4282     |
| motion_bit_count  | 0           | 38723.9  | 38723.9   | N/A          |
| motion_diff_avg   | 21.9757     | 6.42094  | -15.5548  | -43.6058     |
| motion_diff_stdev | 223.325     | 139.486  | -83.8389  | -34.7667     |
| motion_stdev      | 421.708     | 292.999  | -119.622  | -26.3397     |
| motion_x_avg      | 28.6094     | 4.40532  | -24.2041  | -43.3741     |
| motion_x_stdev    | 81.81       | 55.6268  | -23.6735  | -27.0273     |
| motion_y_avg      | 11.3404     | 3.13509  | -8.20535  | -43.7883     |
| motion_y_stdev    | 56.6607     | 40.4491  | -15.5727  | -27.1215     |
| mv_coded_count    | 41947.9     | 20310.7  | -21637.2  | -47.0781     |
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

### Difference Analysis (Legacy Mode)

Here are the results when compiling videoparser-ng with the `VP_MV_POC_NORMALIZATION` flag set to `1`, which enables POC-based motion vector normalization and other legacy behaviors to match the old parser.
<!--

VP_EXTRA_CFLAGS="-DVP_MV_POC_NORMALIZATION=1" util/build-ffmpeg.sh --clean && ./util/build-cmake.sh
parallel --eta --progress 'build/VideoParserCli/video-parser {} > {.}.ldjson' ::: /Volumes/Data/Databases/a2_2k_reencoded/*.mkv
uv run ./test/compare_parsers.py /Volumes/Data/Databases/a2_2k_reencoded
-->

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
| motion_avg        | 25.1819     | 25.1819  | 0         | +0.0000      |
| motion_bit_count  | 0           | 43609    | 43609     | N/A          |
| motion_diff_avg   | 4.59748     | 4.59748  | 0         | +0.0000      |
| motion_diff_stdev | 8.63572     | 8.63572  | 0         | +0.0000      |
| motion_stdev      | 24.9904     | 24.9904  | 0         | +0.0000      |
| motion_x_avg      | 19.2419     | 19.2419  | 0         | +0.0000      |
| motion_x_stdev    | 22.8662     | 22.8662  | 0         | +0.0000      |
| motion_y_avg      | 10.8976     | 10.8976  | 0         | +0.0000      |
| motion_y_stdev    | 13.4801     | 13.4801  | 0         | +0.0000      |
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
| mb_mv_count       | 39225.8     | 39225.8  | 0         | +0.0000      |
| motion_avg        | 25.2005     | 25.2005  | 0         | +0.0000      |
| motion_bit_count  | 0           | 13420.3  | 13420.3   | N/A          |
| motion_diff_avg   | 10.1426     | 10.1426  | 0         | +0.0000      |
| motion_diff_stdev | 27.9233     | 27.9233  | 0         | +0.0000      |
| motion_stdev      | 27.4369     | 27.4369  | 0         | +0.0000      |
| motion_x_avg      | 18.3599     | 18.3599  | 0         | +0.0000      |
| motion_x_stdev    | 22.7405     | 22.7405  | -0        | +0.0000      |
| motion_y_avg      | 11.5723     | 11.5723  | 0         | +0.0000      |
| motion_y_stdev    | 17.3031     | 17.3031  | 0         | -0.0000      |
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
| mb_mv_count       | 47391.8     | 47213.5  | -178.293  | -0.4063      |
| motion_avg        | 132.914     | 25.0518  | -107.862  | -41.1636     |
| motion_bit_count  | 0           | 38723.9  | 38723.9   | N/A          |
| motion_diff_avg   | 21.9757     | 6.55199  | -15.4237  | -41.4324     |
| motion_diff_stdev | 223.325     | 142.344  | -80.981   | -32.2643     |
| motion_stdev      | 421.708     | 298.596  | -113.729  | -23.9113     |
| motion_x_avg      | 28.6094     | 4.50133  | -24.1081  | -41.0552     |
| motion_x_stdev    | 81.81       | 56.9991  | -22.2286  | -24.5506     |
| motion_y_avg      | 11.3404     | 3.1537   | -8.18674  | -41.8347     |
| motion_y_stdev    | 56.6607     | 40.8945  | -15.1036  | -24.7284     |
| mv_coded_count    | 41947.9     | 20470.9  | -21477    | -46.3126     |
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

If temporal normalization is needed, it can be applied as a post-processing step, or via the `VP_MV_POC_NORMALIZATION` flag (see [DEVELOPERS.md](DEVELOPERS.md)).

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
