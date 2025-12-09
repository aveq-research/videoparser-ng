# Developer Guide

Contents:

- [General Structure](#general-structure)
- [Modifications Made](#modifications-made)
  - [QP Information](#qp-information)
  - [Motion Vector Information](#motion-vector-information)
  - [AV1 / libaom Specific Changes](#av1--libaom-specific-changes)
- [Metrics](#metrics)
  - [QP Metrics](#qp-metrics)
  - [Motion Vector Metrics](#motion-vector-metrics)
  - [Bit Count Metrics](#bit-count-metrics)
  - [Block Count Metrics](#block-count-metrics)
  - [POC Metrics](#poc-metrics)
  - [Frame Metadata](#frame-metadata)
- [Differences with Legacy Implementation](#differences-with-legacy-implementation)
  - [All Codecs: POC-based Motion Vector Normalization](#all-codecs-poc-based-motion-vector-normalization)
  - [VP9: Motion Statistics for All Inter Modes](#vp9-motion-statistics-for-all-inter-modes)
  - [VP9: Motion Bit Count](#vp9-motion-bit-count)
  - [VP9: Removed Weighted Variance Accumulation](#vp9-removed-weighted-variance-accumulation)
  - [All Codecs: Removed Arbitrary Scaling Factors](#all-codecs-removed-arbitrary-scaling-factors)
- [Testing](#testing)
  - [Feature Testing](#feature-testing)
  - [Regenerating Test Reference Files](#regenerating-test-reference-files)
  - [Legacy Testing](#legacy-testing)
  - [CLI Testing](#cli-testing)
- [Debugging](#debugging)
- [Maintenance](#maintenance)
  - [Generating Docs](#generating-docs)
  - [Fetching new FFmpeg commits](#fetching-new-ffmpeg-commits)
  - [Fetching new libaom commits](#fetching-new-libaom-commits)
- [Black Border Implementation Notes](#black-border-implementation-notes)
  - [Algorithm Details](#algorithm-details)
  - [1. BlackLine Array Population (Per-Codec)](#1-blackline-array-population-per-codec)

## General Structure

This program patches ffmpeg to add support for extracting additional bitstream properties, or codec-related information, from the bitstream. The program is structured as follows:

- `VideoParser` is an API that provides a simple interface for extracting bitstream properties from a video file. It is implemented as a basic frame-by-frame reader that itself is using ffmpeg standard API calls to read the video.
- VideoParserCli is a command-line interface for VideoParser. It is implemented by using VideoParser API calls to extract the bitstream properties, and then printing them to the console in JSON format.
- ffmpeg is cloned and has a separate branch checked out.
- `libaom` is also cloned and has a separate branch checked out, with modifications to support the extraction of bitstream properties.

To pass extra information from the ffmpeg part to the VideoParser part, we use the `SharedFrameInfo` struct to store the bitstream properties like QP values, motion vectors, etc. The definition is in `VideoParser/include/shared.h`, and it is included in ffmpeg as well, via an extra side data type `AV_FRAME_DATA_VIDEOPARSER_INFO`.

It is extracted from there using a helper function `videoparser_get_final_shared_frame_info`. This is implemented in `ffmpeg/libavutil/frame.c` as an additional method. It performs some extra calculations on the data, like average QP, standard deviation, etc.

To update the data, we have helper functions like `videoparser_shared_frame_info_update_qp`.

## Modifications Made

This explains the high level changes made to ffmpeg to support the extraction of bitstream properties.

All modifications are marked with `// videoparser:` comments for easy identification.

We have modified `decode.c` to extract the frame index (method `ff_decode_receive_frame`).

### QP Information

To obtain the QP information, we modify:

- H.264: `h264_mb.c`, to extract the QP information from the `H264SliceContext` struct, in the function `ff_h264_hl_decode_mb`
- HEVC: `hevcdec.c`, to extract the QP information from the `HEVCLocalContext` struct, in the function `hls_coding_unit`
- VP9: `vp9.c`, to extract the QP information from the `VP9SharedContext` struct, in the function `vp9_decode_frame`
- AV1: `libaomdec.c`, to extract the QP information from the `AV1DecodeContext` struct, in the function `aom_decode`

### Motion Vector Information

To obtain motion vector information, we modify:

- H.264: `h264_mb.c`, via `mv_statistics_264` function
- HEVC: `hevcdec.c`, via `mv_statistics_hevc` function
- VP9: `vp9mvs.c`, via `mv_statistics_vp9` function
- AV1: `libaomdec.c`, via `videoparser_av1_extract_mv_stats` function using libaom's inspection API

### AV1 / libaom Specific Changes

For AV1 support, we use a vendored copy of libaom built with `CONFIG_INSPECTION=1` to enable the inspection API. The inspection API provides access to internal decoder state including per-block mode info and motion vectors.

Key modifications:

- `util/build-libaom.sh`: Configured with `-DCONFIG_INSPECTION=1` to enable inspection API
- `external/libaom/av1/av1_dx_iface.c`: Modified `decode_one()` to propagate the inspection callback to the decoder instance (the stock code only did this in `decoder_inspect()` which FFmpeg doesn't use)
- `external/ffmpeg/libavcodec/libaomdec.c`: Added inspection callback setup and MV/MVD/bit count extraction function

#### MVD and Bit Count Extraction

To extract motion vector differences (MVD) and bit counts for AV1, additional modifications were made to libaom:

- `external/libaom/av1/common/blockd.h`: Added `mvd[2]` field to `MB_MODE_INFO` struct under `CONFIG_INSPECTION` to store motion vector differences during decoding
- `external/libaom/av1/decoder/decoder.h`: Added `motion_bits` and `coef_bits` accumulators to `AV1Decoder` and `ThreadData` structs
- `external/libaom/av1/decoder/inspection.h`: Added `mvd[2]` to `insp_mi_data` and `motion_bits`/`coef_bits` to `insp_frame_data`
- `external/libaom/av1/decoder/inspection.c`: Added copying of MVD and bit counts in `ifd_inspect()`
- `external/libaom/av1/decoder/decodemv.c`: Store MVD in `mbmi->mvd[]` for all NEWMV modes in `assign_mv()`, and track motion bits using `aom_reader_tell_frac()` around MV decoding
- `external/libaom/av1/decoder/decodeframe.c`: Track coefficient bits around intra/inter coefficient decoding using `aom_reader_tell_frac()`, reset counters at frame start in `av1_decode_frame_headers_and_setup()`

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

## Testing

The test scripts use [uv](https://docs.astral.sh/uv/) inline script metadata (PEP 723) for dependency management. This means you can run them directly without installing dependencies manually – `uv` will handle it automatically.

### Feature Testing

The main test suite validates parser output against reference `.ldjson` files for all supported codecs (H.264, H.265, VP9, AV1):

```bash
# Run with uv
uv run test/test.py

# Or make executable and run directly
chmod +x test/test.py
./test/test.py

# Run specific codec test
uv run test/test.py -k "libx264"
```

The test compares all frames in each test video against the expected output in the corresponding `.ldjson` file. Any differences are reported with a readable table showing expected vs actual values.

### Regenerating Test Reference Files

If you intentionally change parser output (e.g., fixing a bug or adding a feature), regenerate the reference files:

```bash
# Generate reference output for all test videos
for video in test/test-lib*.mp4; do
    base=$(basename "$video" .mp4)
    build/VideoParserCli/video-parser "$video" > "test/${base}.ldjson"
done
```

### Legacy Testing

The legacy test suite compares against output from the original [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser). This is useful for verifying backwards compatibility or understanding intentional differences.

First, obtain the legacy test data:

```bash
wget https://storage.googleapis.com/aveq-storage/data/videoparser-ng/test/test.zip -O test/legacy/test.zip
unzip test/legacy/test.zip -d test/legacy
```

Then run the legacy tests:

```bash
uv run test/legacy/test.py
```

Note: Some tests may fail due to intentional differences documented in [Differences with Legacy Implementation](#differences-with-legacy-implementation).

### CLI Testing

CLI tests validate command-line interface behavior:

```bash
uv run test/test-cli.py
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

### Generating Docs

API documentation can be generated using Doxygen:

```bash
util/generate-docs.sh
```

This requires `doxygen` to be installed. For better diagrams, also install `graphviz`:

```bash
# macOS
brew install doxygen graphviz

# Ubuntu
sudo apt-get install doxygen graphviz
```

After generation, open `docs/html/index.html` in your browser. On macOS, you can use:

```bash
util/generate-docs.sh --open
```

### Fetching new FFmpeg commits

Occasionally you want to rebase your local FFmpeg commits on top of the latest upstream FFmpeg commits. This is done by running the dedicated script.

Run the script:

```bash
util/rebase-ffmpeg.sh
```

The rebase may not be clean, so check the output of the script and resolve any conflicts.

### Fetching new libaom commits

Similarly, you can rebase your local libaom commits on top of the latest upstream libaom commits.

Run the script:

```bash
util/rebase-libaom.sh
```

The rebase may not be clean, so check the output of the script and resolve any conflicts.

## Black Border Implementation Notes

This section describes the black border detection algorithm and its possible integration into the QP statistics calculation.

`Av_QPBB` is a computed statistic that represents the average Quantization Parameter (QP) of video content  excluding black letterbox/pillarbox borders. This is important for video quality assessment because black
borders typically have uniform, easily-encodable content with artificially low or high QP values that would skew the "true" content QP measurement.

The legacy code developer's key observation was that black border regions in widescreen/letterboxed videos contain mostly zero-coefficient INTRA blocks. Since pure black areas have no texture or motion, encoders typically:

1. Use INTRA prediction (DC or planar modes)
2. Generate nearly all-zero residual coefficients after transform

By counting rows that have a high proportion of such "empty" blocks, the algorithm can estimate where borders
exist.

### Algorithm Details

### 1. BlackLine Array Population (Per-Codec)

During decoding, a BlackLine[] array is populated where each entry counts zero-coefficient blocks in that row.

H.264 (VideoStat264.c:160-185)

```c++
// Only on I-frames
for( j = 0, NonZeroCoefs=0 ; j<16 ; NonZeroCoefs += NZC_Table[sl->mb_xy][j++] ) ;

if( (MbType & MB_TYPE_INTRA4x4) || (MbType & MB_TYPE_INTRA16x16) )
{
  if( !Cbp_Table[sl->mb_xy] && (NonZeroCoefs == 0) )
    Ctx->BlackLine[sl->mb_y]++ ;  // Increment count for this row
}
```

Uses non_zero_count and cbp_table from H.264 decoder to identify INTRA macroblocks with no coded coefficients.

H.265/HEVC (VideoStatHEVC.c:345-364)

```c++
// Only on I-frames
for( tuy = 0 ; tuy < sps->min_tb_height ; tuy++ )
  for( tux = 0 ; tux < sps->min_tb_width ; tux++ )
    if( cbf_luma[tuy*sps->min_tb_width + tux] == 0 )
      BlackLine[tuy]++ ;
```

Uses the cbf_luma (Coded Block Flag for luma) array to identify transform units with no coded coefficients.

VP9 (VideoStatVP9.c:376-381)

```c++
// Count 4x4 columns with zero-tx
if (sum == 0)
  for( j = y; j < y + step ; j++ )
    s->BlackLine[2 * row + j] += step ;
```

Sums transform coefficients; if sum is zero, increments the BlackLine counter.

#### 2. Black Border Detection (VideoStatCommon.c:12-31)

```c++
int BlackborderDetect( int* BlackLine, int rows, int threshold, int logBlkSize )
{
  int BlackLines = 0, i;

  // Step 1: Symmetry enforcement - combine top and bottom
  for( i = 0 ; i < rows>>1 ; i++ )
    BlackLine[i] = BlackLine[rows-i-1] =
      ((BlackLine[i] + BlackLine[rows-i-1]) >= (threshold << 1)) ? 1 : 0 ;

  // Step 2: Count consecutive "black" rows from top
  for( i = 0 ; i < rows ; i++ )
  {
    if( BlackLine[i] != 0 )
      BlackLines++ ;
    else
      break ;
  }

  // Step 3: Sanity check - reject if > 50% of frame
  if( BlackLines >= (rows >> 1) )
    BlackLines = 0 ;

  // Step 4: Convert block rows to pixels
  return( BlackLines << logBlkSize ) ;
}
```

Key Algorithm Steps:

1. Symmetry Enforcement: Assumes letterboxing is symmetric (top = bottom). Combines counts from row i and row rows-i-1. If combined count exceeds 2 × threshold, marks row as "black" (1), else not black (0).
2. Consecutive Count: Counts consecutive rows from the top that are marked as black. Stops at first non-black row.
3. Sanity Check: If detected black lines exceed 50% of frame height, rejects detection (returns 0). This prevents false positives from mostly-dark content.
4. Pixel Conversion: Multiplies block count by block size (1 << logBlkSize) to get pixel height.

#### 3. Threshold Values (Codec-Specific)

| Codec | Threshold           | Block Size                  | Rationale                                  |
| ----- | ------------------- | --------------------------- | ------------------------------------------ |
| H.264 | 0.8 × mb_width      | 16×16 (logBlkSize=4)        | 80% of macroblocks in row must be zero     |
| H.265 | 0.72 × min_tb_width | Variable (log2_min_tb_size) | 72% of transform blocks must be zero       |
| VP9   | 1.2 × cols          | 4×4 (logBlkSize=2)          | 120% accounts for 8×8 grid with sub-blocks |

TODO: Why were these specific thresholds chosen? Possibly empirical tuning.

#### 4. QP Statistics with Border Exclusion

Determining if Block is in Border (VideoStat264.c:308)

```c++
NoBorder = ((sl->mb_y << 4) >= FrmStat->BlackBorder) &&
            ((sl->mb_y << 4) < (h->height - FrmStat->BlackBorder));

```

Accumulating QP (VideoStatCommon.c:38-56)

```c++
void QPStatistics( VIDEO_STAT* FrmStat, int CurrQP, int CurrType, int NoBorder )
{
  // Always accumulate (with border)
  S->QpSum += CurrQP ;
  S->QpSumSQ += SQR( CurrQP ) ;
  S->QpCnt++ ;

  // Only accumulate for non-border blocks (BB = "Black Border" excluded)
  if( NoBorder )
  {
    S->QpSumBB += CurrQP ;
    S->QpSumSQBB += SQR( CurrQP ) ;
    S->QpCntBB++ ;
  }
}
```

#### 5. Final Computation (VideoStatCommon.c:165-171)

```c++
FrmStat->Av_QPBB     = S->QpSumBB / (double)S->QpCntBB ;
FrmStat->StdDev_QPBB = sqrt( S->QpSumSQBB / (double)S->QpCntBB -
                              SQR( S->QpSumBB / (double)S->QpCntBB ) ) ;
```

Mathematical Formulas:

- Av_QPBB = Σ(QP for blocks outside black border) / Count
- StdDev_QPBB = √(Var) = √(E[QP²] - E[QP]²)

#### FFmpeg Modifications

Modified Files:

| File                                 | Purpose                                        | Marker |
| ------------------------------------ | ---------------------------------------------- | ------ |
| ffmpeg/libavutil/internal.h:357      | Declares extern int CurrBlackBorder            | P.L.   |
| ffmpeg/libavcodec/h264_slice.c:50-51 | Function declarations                          | P.L.   |
| ffmpeg/libavcodec/h264_slice.c:2417  | Calls InitFrameStatistics264() at slice start  | P.L.   |
| ffmpeg/libavcodec/h264_slice.c:2591  | Calls BlackborderDetect() at end of I-frame    | P.L.   |
| ffmpeg/libavcodec/hevc.c:2385        | Calls BlackBorderEstimationHEVC() at frame end | P.L.   |
| ffmpeg/libavcodec/hevc.h:1078        | Function declaration                           | P.L.   |

Key Integration Points:

1. H.264 (h264_slice.c:2590-2591): At the finish: label after slice decoding:
2. HEVC (hevc.c:2385): After CTB (Coding Tree Block) processing:
3. VP9 (VideoStatVP9.c:476): At frame statistics finalization:

#### Design Decisions

1. Only I-frames trigger recalculation: Black border detection only happens on I-frames (or keyframes for VP9). P/B frames inherit the previous CurrBlackBorder value. This is because:

   - I-frames have complete INTRA prediction making zero-coefficient detection reliable
   - Scene changes (where borders might change) typically occur at I-frames
   - Reduces computational overhead

2. Global CurrBlackBorder variable: Persists across frames to maintain border detection between I-frames.
3. Symmetric assumption: The algorithm enforces symmetry between top and bottom borders, which is typical for letterboxed content but would fail for asymmetric borders (rare in practice).
4. Spatial complexity usage (VideoStatCommon.c:175): `FrmStat->SpatialComplexety[0] = BpF *exp( 0.115524* FrmStat->Av_QPBB );`
