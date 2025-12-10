# Developer Guide

Contents:

- [General Structure](#general-structure)
- [Modifications Made](#modifications-made)
  - [QP Information](#qp-information)
  - [Motion Vector Information](#motion-vector-information)
  - [AV1 / libaom Specific Changes](#av1--libaom-specific-changes)
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

H.264, HEVC, and VP9 support an optional compile-time flag `VP_MV_POC_NORMALIZATION` that, when set to `1`, enables POC-based motion vector normalization and "legacy" mode. This replicates the behavior of the legacy `bitstream_mode3_videoparser` for compatibility testing. By default, raw motion vector values are used.

For H.264 and HEVC, this weighs motion vectors by their temporal distance to reference frames, using the formula `1.0 / (2.0 * |temporal_distance| / poc_diff)`. For HEVC, we also determine the coding type (Intra, Skip, Inter) and change the normalization based on that.

For VP9, the legacy mode applies:

- Normalization by frame distance: `mv / (8 * FrmDist)` where `FrmDist = max(1, (current_PTS - ref_PTS) / frame_duration)`
- A 4x multiplier to MV lengths: `MV_Length = 4.0 * sqrt(mvX² + mvY²)`
- Only accumulates statistics for NEWMV mode blocks (explicitly coded motion vectors)
- NEARESTMV and NEARMV modes only increment the block count but don't contribute to MV statistics
- Outlier rejection: Blocks where `(mvX + mvY) > 20 * AvMot` or `(mvdX + mvdY) > 20 * AvDif` are rejected
- Weighted `mv_coded_count`: In legacy mode, `mv_coded_count` is incremented by the block's 4x4 count (not just 1), and only for non-outlier NEWMV blocks

**VP9 Reference Frame Buffer Bug Replication**: The legacy parser had a bug in how it accessed reference frame PTS values. The correct way to get the reference frame is `s->s.refs[s->s.h.refidx[b->ref[0]]]` (mapping reference type through refidx), but the legacy code used `s->s.refs[b->ref[0]]` directly. This bug is replicated for compatibility. In practice, this only affects behavior when refidx mapping is non-identity (which is uncommon).

**VP9 Outlier Rejection**: The legacy parser (`VideoStatVP9.c`, `ProcessMV` function) rejects motion vectors that exceed 20x the running average. Specifically:

```c
if( FrmStat->S.CodedMv )
    AvMot = FrmStat->S.MV_Length / FrmStat->S.CodedMv ;
if( FrmStat->S.CodedMv )
    AvDif = FrmStat->S.MV_dLength / FrmStat->S.CodedMv ;

if( !((abs(mvX) + abs(mvY) > 20 * AvMot) || (abs(mvdX) + abs(mvdY) > 20 * AvDif)) )
{
    // Only accumulate if not an outlier
}
```

This is now replicated in `mv_statistics_vp9()` when legacy mode is enabled. If `CodedMv` is 0 (first block), the running averages default to 1.0.

**Comparison Status**: With legacy mode enabled (including outlier rejection), the VP9 implementation should closely match the legacy parser. Any remaining differences may be due to:

1. X/Y asymmetry: Minor precision differences in MV component extraction
2. Frame duration field: Legacy uses `pkt_duration` while we use `duration`

To enable POC normalization, rebuild ffmpeg with:

```bash
VP_EXTRA_CFLAGS="-DVP_MV_POC_NORMALIZATION=1" util/build-ffmpeg.sh --clean
util/build-cmake.sh
```

When `VP_MV_POC_NORMALIZATION=1`, we intentionally replicate a bug from the legacy parser for exact compatibility. The legacy code (`VideoStatCommon.c` line 195) computes `motion_diff_stdev` using:

```c
StdDev_MotionDif = sqrt(0.00001 + MV_DifSumSQR / NumDifs - SQR(MV_DifSum / NumDifs))
```

However, `MV_DifSum` is never accumulated in the legacy code (only `MV_dLength` is), so `MV_DifSum` is always 0. This means the legacy formula effectively becomes:

```c
StdDev_MotionDif = sqrt(0.00001 + MV_DifSumSQR / NumDifs)  // Missing mean subtraction
```

This computes the root mean square (RMS) rather than the true standard deviation. The correct formula would be `sqrt(E[X²] - E[X]²)`, but legacy computes `sqrt(E[X²])`.

We replicate this bug in `libavutil/frame.c` when legacy mode is enabled. See the comment "LEGACY BUG REPLICATION" in the source code for details.

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
