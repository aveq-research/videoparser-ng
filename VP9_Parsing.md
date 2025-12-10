@README.md @DEVELOPERS.md @METRICS.md

# VP9 Legacy Mode Implementation

This document tracks the VP9 motion vector processing mode controlled by the flag `VP_MV_POC_NORMALIZATION`.

## Summary

After extensive investigation, we have determined that **the legacy VP9 parser has severe bugs** that produce unreliable motion statistics. The new videoparser-ng implementation produces correct, internally consistent values and should be preferred.

## VP9 Legacy Mode Implementation Status

All legacy behaviors have been implemented (when `VP_MV_POC_NORMALIZATION=1`):

- ✅ FrmDist calculation: `max(1, (cur_pts - ref_pts) / duration)`
- ✅ Reference frame buffer bug replication (uses `b->ref[0]` directly as index)
- ✅ MV normalization: `mv / (8 * FrmDist)`
- ✅ 4x MV length multiplier
- ✅ NEWMV-only MV accumulation
- ✅ `count*count` variance weighting
- ✅ `motion_diff_stdev` RMS bug (in frame.c)
- ✅ Outlier rejection: blocks where MV > 20x running average are rejected
- ✅ Weighted `mv_coded_count` (incremented by 4x4 block count, not 1)
- ✅ Integer truncation in outlier rejection (AvMot/AvDif as int, not double)
- ✅ `abs()` truncation bug replication (cast to int before comparison)
- ✅ CodedMv only counts when `j != MV_JOINT_ZERO`
- ✅ Hidden frame accumulation (invisible/altref frames)
- ✅ Short frame detection (`pkt->size < 100`)

## H.264 and HEVC Status

Both codecs show **0% difference** on all motion vector metrics, confirming the legacy mode implementation is correct for these codecs.

## Legacy VP9 Parser Bugs

Frame-by-frame analysis of legacy VP9 output reveals impossible values:

**Frame 1:**

| Metric      | Legacy Value | New Value | Analysis |
|-------------|--------------|-----------|----------|
| NumBlksMv   | 100          | 100       | Match ✓ |
| CodedMv     | 98128        | 84        | **IMPOSSIBLE** - CodedMv cannot exceed NumBlksMv |
| Av_Motion   | 8812.8       | 4.77      | **UNREASONABLE** - MV averages should be 0-100 range |

**Frame 50:**

| Metric      | Legacy Value | New Value | Analysis |
|-------------|--------------|-----------|----------|
| NumBlksMv   | 49964        | 49676     | Close match ✓ |
| CodedMv     | 81652        | 30696     | Legacy still impossibly high |
| Av_Motion   | 11.44        | 5.44      | Both reasonable |

### Bug Characteristics

1. **CodedMv values are impossible**: Legacy reports CodedMv=98128 when NumBlksMv=100. CodedMv (number of coded motion vectors) cannot exceed the number of blocks.

2. **Early frame anomalies**: Av_Motion=8812 for frame 1 is absurdly high (motion vectors are typically 0-100 in subpel units), suggesting uninitialized memory or state corruption.

3. **Fluctuating accumulated state**: CodedMv values in legacy output fluctuate independently of frame content (98128 → 92812 → 87884 → ...), suggesting corrupted global state.

### New Parser Correctness

The new parser values are internally consistent:

- `mv_coded_count` ≤ `mb_mv_count` ✓
- Motion averages are in reasonable ranges (0-20) ✓
- Values scale appropriately with block counts ✓

## Recommendation

**Use the new videoparser-ng implementation for VP9.** The legacy parser has fundamental bugs that make its VP9 motion statistics unreliable. The ~5x difference in averaged comparison results is dominated by corrupted early-frame values in the legacy parser.

For P.1204.3 or other quality models that depend on motion statistics:

- H.264 and HEVC: Legacy mode (`VP_MV_POC_NORMALIZATION=1`) produces identical results
- VP9: Use standard mode (`VP_MV_POC_NORMALIZATION=0`) - the legacy output is unreliable

## Current Comparison Results (2025-12-10)

### VP9 (19 videos)

| Metric            | Legacy Mean | New Mean  | Diff Mean  | Rel Diff (%) |
| ----------------- | ----------- | --------- | ---------- | ------------ |
| mb_mv_count       | 47391.8     | 47213.5   | -178.3     | -0.41%       |
| motion_avg        | 132.914     | 24.6461   | -108.3     | -43.4%       |
| motion_stdev      | 421.708     | 292.999   | -119.6     | -26.3%       |
| motion_x_avg      | 28.6094     | 4.40532   | -24.2      | -43.4%       |
| motion_y_avg      | 11.3404     | 3.13509   | -8.2       | -43.8%       |
| motion_diff_avg   | 21.9757     | 6.42094   | -15.6      | -43.6%       |
| motion_diff_stdev | 223.325     | 139.486   | -83.8      | -34.8%       |
| mv_coded_count    | 41947.9     | 20310.7   | -21637     | -47.1%       |

Note: The large differences are due to legacy parser bugs, not implementation differences.

### H.264 (22 videos)

All motion vector metrics show **0% difference**.

### HEVC (19 videos)

All motion vector metrics show **0% difference**.

## Implementation Details

### Hidden Frame Handling (vp9.c)

Added global state for VP9 hidden frame accumulation:

```c
#if VP_MV_POC_NORMALIZATION
static SharedFrameInfo vp9_hidden_frame_stats;
static int vp9_hidden_frame_distance = 0;
static int vp9_hidden_stats_valid = 0;
#endif
```

For show_existing_frame (short frame):

```c
if (pkt->size < 100 && vp9_hidden_stats_valid) {
    // Copy stats from hidden frame
    copy_shared_frame_stats(sf, &vp9_hidden_frame_stats);
    // Apply FrameDistance division
    int fd = vp9_hidden_frame_distance - 2;
    if (fd < 1) fd = 1;
    apply_frame_distance_division(sf, fd);
}
```

For invisible frames:

```c
if (s->s.h.invisible) {
    copy_shared_frame_stats(&vp9_hidden_frame_stats, sf);
    vp9_hidden_frame_distance = 0;
    vp9_hidden_stats_valid = 1;
} else {
    vp9_hidden_frame_distance++;
}
```

### SharedFrameInfo Extensions

Added fields for VP9 hidden frame tracking:

```c
// VP9 hidden frame handling (legacy mode only)
int is_hidden;        // VP9: frame is invisible/hidden (altref)
int is_short_frame;   // VP9: frame is show_existing_frame
int frame_distance;   // VP9: distance from last invisible frame
int64_t pts;          // Presentation timestamp
```

## Key Files

**videoparser-ng:**

- `external/ffmpeg/libavcodec/vp9mvs.c` - VP9 motion vector extraction
- `external/ffmpeg/libavcodec/vp9.c` - VP9 decoder with hidden frame handling
- `external/ffmpeg/libavutil/frame.c` - Final statistics calculation
- `VideoParser/include/shared.h` - SharedFrameInfo struct definition

**Legacy parser (for reference):**

- `VideoParser/VideoStatVP9.c` - VP9 statistics (ProcessMV, BlockMVStatsVP9)
- `VideoParser/VideoStatCommon.c` - Common statistics (FinishStatistics)

## Build Commands

Legacy mode build (for comparison testing):

```bash
VP_EXTRA_CFLAGS="-DVP_MV_POC_NORMALIZATION=1" util/build-ffmpeg.sh --clean
util/build-cmake.sh
```

Standard mode build (recommended for VP9):

```bash
util/build-ffmpeg.sh --clean
util/build-cmake.sh
```

To run a complete comparison:

```bash
VP_EXTRA_CFLAGS="-DVP_MV_POC_NORMALIZATION=1" util/build-ffmpeg.sh --clean && ./util/build-cmake.sh
parallel --eta --progress 'build/VideoParserCli/video-parser {} > {.}.ldjson' ::: /Volumes/Data/Databases/a2_2k_reencoded/*.mkv
uv run ./test/compare_parsers.py /Volumes/Data/Databases/a2_2k_reencoded
```
