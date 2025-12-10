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

This section documents intentional differences between videoparser-ng and the legacy [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser) implementation. These changes were made to improve correctness, cross-codec consistency, and simplicity.

### Difference Analysis

A detailed analysis of the differences was performed with the help of the `util/reencode_videos.sh` script, which generates a test set of encoded videos.
As test videos, the [`a2_2k` set from the AOM CTC was used](https://media.xiph.org/video/aomctc/test_set/a2_2k/). All videos were encoded with H.264, HEVC, VP9, and AV1 at default encoding settings, using `libx264`, `libx265`, `libvpx-vp9`, and `rav1e` encoders from FFmpeg.

The `util/compare_parsers.py` script was then used to compare the output of videoparser-ng against the legacy videoparser for all test videos and codecs (except AV1, because it has no legacy implementation). The results were analyzed to identify systematic differences.

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
