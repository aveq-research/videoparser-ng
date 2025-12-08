# **VideoParser Reimplementation**

## *Requirement Specification*

# **Background — The “old” VideoParser**

There is an existing piece of software that parses video bitstreams from H.264, H.265 and VP9-encoded files and calculates statistics on the bitstream properties, both on a per-frame and per-sequence level — it’s called “VideoParser”. These statistics can later be used to, for example, infer the quality of a coded video stream with (ML or deep learning) algorithms.

The latest version of the software is available [here](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser/tree/d191af68e2752a83402b004d2264e416bea6736b) (note the specific commit just to avoid misunderstandings).

The VideoParser part provides an API. It is a wrapper around ffmpeg that opens and reads container files — and from there reads the (first/best) video stream. libavcodec is used to decode/parse the bitstreams, and in some places, original ffmpeg code has been modified to write out data into new structs that are, for example,  included in AVFrame, or use externally declared functions that take the extracted data.

The TestMain part is again a wrapper around the VideoParser that implements a CLI to open and read statistics from a file on-disk.

# **Goal**

We want to provide, for the community, a research tool that helps video quality engineers understand properties of the video bitstreams they are working with. In particular, the features extracted from the parser can be used for predicting the visual quality of a stream.

The existing code has been written a while ago and has the following issues:

* It is based on an old ffmpeg version
* It does not compile properly under all targets that ffmpeg supports; mostly it targets Windows and the VideoParser seems to use Windows-specific code. We cannot get it to compile under Docker either.
* It is not documented properly, both in code and externally
  * In particular, the changes wrt. ffmpeg are not clean and only marked with inline (“P.L.” or “A.S.”) comments, so a raw diff will not be 100% helpful and includes unnecessary whitespace changes.
* It is not really performant; one would expect much faster parsing/decoding of bitstreams
* It is using somewhat idiomatic style and formatting that makes it hard to parse
* It creates aggregated per-sequence statistics which is not strictly required and usually done from the outside after getting per-frame information

Therefore, **a new version should be written** that fixes the above issues.

This is the **videoparser-ng**.

# **High-Level Requirements**

The new version must fulfill the following requirements:

* The ffmpeg changes must be rebased on top of the latest stable ffmpeg version at the time of rewriting it
  * These changes must be available in the form of a Git branch, in one commit set — i.e. one clean patch that can be later rebased onto newer versions of ffmpeg without major efforts required
  * Not all statistics must be calculated, a limited set of features is needed
  * The patching of ffmpeg should be improved to avoid as many modifications as possible
* The VideoParser part must be rewritten in a clean manner
  * No platform-specific code\!
  * It must only extract the minimum necessary frame statistics (see section below)
  * The code must be structured to easily support adding new codecs at a later point in time (AV1, VVC, …)
* The TestMain part must be rewritten with the following changes:
  * It must be renamed to VideoParserCli
  * It must write out the statistics per-frame to stdout, or into a JSON file that is flushed on each frame being read out. The format must be one JSON-formatted line per frame to allow per-frame parsing from the outside.
  * A parameter for the number of frames to be read must be introduced, if unset, all frames must be parsed.
  * The per-sequence statistics must be written at the end of the sequence, or when the user requests exit earlier via a set number of frames.
* The code must be fast\!
  * Avoid copying all (per-sequence) structs from frame to frame, we only need per-frame stats, no later aggregation
  * No thread locking if possible, e.g. as done in VP9, instead aggregate per thread — use FFmpeg side data as needed
  * We can keep a reference to a global data struct if needed but we should not do that from ffmpeg
* All code must compile under all major architectures (amd64, arm64, armv7l)
  * It *should* compile under all architectures that ffmpeg supports
  * Prio-2: It must be possible to statically compile the VideoParser and VideoParserCli, including its ffmpeg dependencies, for easier distribution at a later point in time, in line with the ffmpeg LGPL license, so that the user only needs the statically compiled VideoParser API if they want to use it in their own program, or one executable CLI that “does everything”
* A test harness must be introduced
  * It must test:
    * Compilation
    * The VideoParser API itself
    * The VideoParserCli call
  * It must use at least one file for H.264, H.265 and VP9
* The formatting must be harmonized:
  * Only use snake\_case, no PascalCase for struct members etc.
  * For the API, use clang-format with a default setting to avoid bikeshedding on formatting questions
  * Do not change any of ffmpeg’s internal formatting

# **Steps**

1. Check out new ffmpeg version ✅
2. Build a new framework for the video parser ✅
   1. Write a new VideoParser API ✅
   2. Write a new VideoParserCli program ✅
3. Build a new struct into ffmpeg for extracting data ✅
4. Apply the changes from libavcodec ✅
5. Add unit test framework ✅
6. Add the actual statistics ⏳ → this is the major work
7. Build a static release
8. Auto-Generate the documentation/API docs
9. Include third party dependencies like libdav1d if needed

# **Existing and Required Statistics for New Version**

The statistics must be available for the following codecs:

* H.264
* H.265/HEVC
* VP9
* AV1 (at a later stage)

In the following we will describe what to do with the existing statistics.

## Per Sequence Statistics (SeqData)

These SEQ\_DATA items will be mostly kept as-is, just renamed in case we later want to also add audio properties.

This can possibly be inferred all directly from ffmpeg at the beginning of the sequence using standard libav calls. Possibly, the bitrate, framerate and profile/level are not signaled all the time – in which case bitrate/framerate shall be inferred from the file size and duration/number of frames.

| Name | Type | Description | Action | New Name | New Type | Done in new implementation |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| ChromaFormat | int | 0=Graylevel 1=4:2:0 2=4:2:2 3:4:4:4 4:RGB | Replace with ffmpeg’s AVPixelFormat string directly | video\_pix\_fmt | std::string | ✅ |
| BitDepth | int | in bit 8...12 | Implicitly contained in pixel format, maybe introduce a new field? | video\_bit\_depth | uint8 | ✅ |
| Profile | int | HEVC: 1=Main 2=Main10 3=StilPicture 4=R-EXT\<br\>H.264: 88=Baseline 77=Main 88=Extended 100=High 110:High10 122=High422 .... and many more\<br\>VP9: 0:8bit 4:2:0 1:8bit 4:2:2/4:4:4 2:10-12bit 4:2:0 3:10-12bit 4:2:2/4:4:4 | Keep as is | video\_codec\_profile | uint8 | ✅ |
| Level | int | for instance: 120 \-\> level 1.20 VP9 has no levels(?) | Keep as is | video\_codec\_level | uint8 | ✅ |
| Bitrate | int |  | Convert to double, and infer from the filesize if not directly given in SPS/PPS | video\_bitrate | double | ✅ |
| FramesPerSec | double |  | Rename | video\_framerate | double | ✅ |
| BitsPertPel | double |  | Remove |  |  | ✅ |
| Resolution | int | 0:CIF 1:SD 2:HD 3:UHD | Remove |  |  | ✅ |
| ArithmCoding | int | Has arithmetic coding? (always true for H.265 and VP9) | Remove |  |  | ✅ |
| Codec | int | 1:H.264 2:HEVC 3:VP9 | Rename, replace with string values h264, hevc, vp9 | video\_codec | std::string | ✅ |
| FrameWidth | int |  | Rename | video\_width | uint8 | ✅ |
| FrameHeight | int |  | Rename | video\_height | uint8 | ✅ |
|  |  | Duration of the stream | New, in seconds | video\_duration | double | ✅ |
|  |  |  | New, from below | video\_frame\_count | uint32\_t | ✅ |

## VideoStat (Previous Per-Sequence Statistics)

These were, in the old code, updated on a per-frame basis and gave per-sequence output values. We now only want them on a per-frame basis and no aggregation, so there is some duplication with the next section.

| Name | Type | Description | Action | Prio(1 \= must have, 2 \= nice to have, 3 \= ignore for now) | Internal? | New Name | New impl (H.264) | New impl (H.265) | New Impl (VP9) | New Impl (AV1) |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| S | FRAME\_SUMS | For internal use only | Remove, or rather: restructure to keep counters per-frame (e.g. QP counts) |  | Yes |  |  |  |  |  |
| Seq | SEQ\_DATA | Sequence base properties (see above) | Remove? Not sure why we need this reference duplicated. It’s copied for every frame\! |  | Yes |  |  |  |  |  |
| IsHidden | int | Only valid for VP9, internal use | Rename, move to internal struct |  | Yes | is\_hidden |  |  |  |  |
| IsShrtFrame | int | Only valid for VP9, internal use (Note: unclear what specifically happened here, check with VP9 developer) | Rename, move to internal struct |  | Yes | is\_short\_frame |  |  |  |  |
| AnalyzedMBs | double | Number of Block structures in frame (H.264: Macroblocks H.265: Coding Blocks with min\_cb\_Size VP9: Blocks) | Rename |  |  | mb\_analyzed\_count |  |  |  |  |
| SKIP\_mb\_ratio | double | Number of Skipped Blocks / AnalyzedMBs | Rename |  |  | mb\_skip\_ratio |  |  |  |  |
| Av\_QP | double | Average QP of the whole frame | Rename | 1 |  | qp\_avg | ✅ | ⏳ (some issue with frame ordering) | ⏳ not yet for segmented streams | ✅ |
| StdDev\_QP | double | Standard deviation of Av\_QP | Rename | 2 |  | qp\_stdev | ✅ | ✅ | ⏳ not yet for segmented streams | ✅ |
| Av\_QPBB | double | Average QP without the black regions (wide screen movie) | Rename | 3 |  | qp\_bb\_avg | ⏳ | ⏳ (some issue with frame ordering) | (prio-2) |  |
| StdDev\_QPBB | double | Standard deviation of Av\_QPBB | Rename | 3 |  | qp\_bb\_stdev | ⏳ | ⏳ | (prio-2) |  |
| min\_QP | double | Minimum QP value encountered in this frame | Rename | 1 |  | qp\_min | ✅ | ✅ | ⏳ not yet for segmented streams | ✅ |
| max\_QP | double | Maximum QP value encountered in this frame | Rename | 1 |  | qp\_max | ✅ | ✅ | ⏳ not yet for segmented streams | ✅ |
| InitialQP | double | QP Value the frame is starting with (to be found in the slice- or frame-header) | Rename | 2 |  | qp\_init | ✅ | ✅ | ⏳ not yet for segmented streams | ✅ |
| MbQPs | double\[7\] | Average QP dependent on MB-Types (SKIP=0, L0=1, L1=2, Bi=3, INTRA(directional)=5, INTRA(plane)=6) | Rename | 2 |  | qp\_avg\_per\_mb |  |  |  |  |
| Av\_Motion | double | Average length (sqrt(xx \+ yy)) of the vectors in the motion field | Rename | 1 |  | motion\_avg | ✅ | ✅ | ✅ |  |
| StdDev\_Motion | double | Standard Deviation of Av\_Motion | Rename | 1 |  | motion\_stdev | ✅ | ✅ | ✅ |  |
| Av\_MotionX | double | Average of abs(MotX) | Rename | 1 |  | motion\_x\_avg | ✅ | ✅ | ✅ |  |
| Av\_MotionY | double | Average of abs(MotY) | Rename | 1 |  | motion\_y\_avg | ✅ | ✅ | ✅ |  |
| StdDev\_MotionX | double | Standard deviation of Av\_MotionX | Rename | 1 |  | motion\_x\_stdev | ✅ | ✅ | ✅ |  |
| StdDev\_MotionY | double | Standard deviation of Av\_MotionY | Rename | 1 |  | motion\_y\_stdev | ✅ | ✅ | ✅ |  |
| Av\_MotionDif | double | Difference of the motion with its prediction | Rename | 1 |  | motion\_diff\_avg | ✅ | ✅ | ✅ |  |
| StdDev\_MotionDif | double | Standard deviation of Av\_MotionDif | Rename | 1 |  | motion\_diff\_stdev | ✅ | ✅ | ✅ |  |
| Av\_Coefs | double\[4\]\[32\]\[32\] | Represents the 2-dim arrays of the average size of the transform coefficients \[NoTrans,-,4x4, 8x8, 16x16, 32x32\]\[coef-y\]\[codef-x\] | Rename | 2 |  | coefs\_avg |  |  |  |  |
| StdDev\_Coefs | double\[4\]\[32\]\[32\] | Standard deviation of Av\_Coefs | Rename | 2 |  | coefs\_stdev |  |  |  |  |
| HF\_LF\_ratio | double\[3\]\[4\]\[3\] | The Ratio between high-frequency and low-frequency components in Av\_Coef arrays | Rename | 2 |  | coefs\_hf\_lf\_ratio |  |  |  |  |
| MbTypes | double\[7\] | Count for MB Types: 0=Skipped, 1=Forward (L0), 2=Backward (L1), 3=Bidirect, 4=Direct, 5=Intra(directional), 6=Intra(planar) | Rename | 2 |  | mb\_types |  |  |  |  |
| BlkShapes | double\[10\] | Count for Shapes: 8=64x64    7=64x32,32x64  6=32x32  5=16x32,32x16   4=16x16  3=16x8,8x16  2=8x8 1=8x4,4x8  0=4x4    (H.264: only 0,1,2,3,4 )  \[9\] \= Block-Size Measure  | Rename | 2 |  | mb\_shapes |  |  |  |  |
| TrShapes | double\[8\] | Count for Transform Sizes: 6=64x64, 5=32x32, 4=16x16, 3=8x8, 2=4x4, 0=NO\_TRANSFORM (on 4x4 basis) | Rename | 2 |  | tr\_shapes |  |  |  |  |
| FarFWDRef | double\[2\] | 0: BLocks, which use the nearest reference frame for FORWARD-prediction    1: Block uses Refs. more far away | Rename | 2 |  | mb\_far\_fwd\_ref |  |  |  |  |
| PredFrm\_Intra | double | Amount of I-blk area in a predicted frame | Rename | 3 |  | pred\_frame\_intra\_area |  |  |  |  |
| FrameSize | int | Number of actual bytes for this frame | Rename | 1 |  | size | ✅ | ✅ | ✅ | ✅ |
| FrameType | int | Type of this frame (1=I, 2=P, 3=B) | Rename | 1 |  | frame\_type | ✅ | ✅ | ✅ | ✅ |
| IsIDR | int | 0: any frame, 1: IDR-Intra frame | Rename, but check meaning | 1 |  | is\_idr | ✅ | ✅ | ✅ | ✅ |
| FrameIdx | int | Frame index starting with 1 | Rename, start with 0 (Note: not set from ffmpeg, but after reading the stats) | 1 |  | frame\_idx | ✅ | ✅ | ✅ | ✅ |
| FirstFrame | int | Only used for Sequence Statistics | Remove | 3 |  |  |  |  |  |  |
| NumFrames | int | Only used for Sequence Statistics | Remove, part of the sequence stats | 3 |  |  |  |  |  |  |
| BlackBorder | int | Number of black scanlines at the top and bottom of the frame | Rename | 3 |  | black\_border\_lines |  |  |  |  |
| DTS | int64\_t | The frame's Decoding Timestamp (if transport stream) | Rename | 1 |  | dts | ✅ | ✅ | ✅ | ✅ |
| PTS | int64\_t | The frame's Presentation Timestamp (if transport stream) | Rename | 1 |  | pts | ✅ | ✅ | ✅ | ✅ |
| CurrPOC | int |  | Rename | 3 |  | current\_poc | ⏳ |  |  |  |
| POC\_DIF | int |  | Rename, clarify meaning | 3 |  | poc\_diff | ⏳ |  |  |  |
| FrameCnt | double |  | Remove, part of the sequence stats | 3 |  |  |  |  |  |  |
| SpatialComplexety | double\[3\] |  | Remove, not needed | 3 |  |  |  |  |  |  |
| TemporalComplexety | double\[3\] |  | Remove, not needed | 3 |  |  |  |  |  |  |
| TI\_Mot | double\[2\] |  | Remove, not needed | 3 |  |  |  |  |  |  |
| TI\_910 | double |  | Remove, not needed | 3 |  |  |  |  |  |  |
| SI\_910 | double |  | Remove, not needed | 3 |  |  |  |  |  |  |
| Blockiness | double\[3\]\[6\] |  | Remove, not needed | 3 |  |  |  |  |  |  |
| BitCntMotion | double | The number of bits used for coding motion | Rename | 2 |  | motion\_bit\_count | ✅ | ✅ | ✅ | Needs vendoring libaom |
| BitCntCoefs | double | The number of bits used for coding coeffs | Rename | 2 |  | coefs\_bit\_count | ✅ | ✅ | ✅ | Needs vendoring libaom |
| NumBlksSkip | int | Number of skip macroblocks | Rename | 2 |  | mb\_skip\_count |  |  |  |  |
| NumBlksMv | int | Number of macroblocks with MVs | Rename | 2 |  | mb\_mv\_count | ✅ | ✅ | ✅ |  |
| NumBlksMerge | int | Number of merge macroblocks | Rename | 2 |  | mb\_merge\_count |  |  |  |  |
| NumBlksIntra | int | Number of intra MBs | Rename | 2 |  | mb\_intra\_count |  |  |  |  |
| CodedMv | int | Number of coded MVs | Rename | 2 |  | mv\_coded\_count | ✅ | ✅ | ✅ |  |

## Per-Frame Sums

This is a struct that was used to hold per-frame information, and where later, derived statistics were calculated. For example this struct would count the number of macroblocks of a certain type. It can be kept and merged with the previous one if needed to improve computational efficiency.

| Name | Type | Description | Exists in FrameStat? | Action | New Name | Done in implementation? |
| :---- | :---- | :---- | :---- | :---- | :---- | :---- |
| QpSum | int | Sum of QP values | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_sum | ✅ |
| QpSumSQ | int | Sum of squared QP values | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_sum\_sqr | ✅ |
| QpCnt | int | Count of QP values | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_cnt | ✅ |
| QpSumBB | int | Sum of QP values without black border | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_sum\_bb | ✅ |
| QpSumSQBB | int | Sum of squared QP values without black border | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_sum\_sqr\_bb | ✅ |
| QpCntBB | int | Count of QP values without black border | Only used for QP Avg/Stdev | Keep temporarily if needed | qp\_cnt\_bb | ✅ |
| NumBlksMv | int | Number of Blocks with Motion Vectors (MVs) |  | Remove duplicate use |  |  |
| NumMvs | int | Number of Motion Vectors (Not used) |  | Remove |  |  |
| NumMerges | int | Number of Merges (Not used) |  | Remove |  |  |
| MbCnt | int | Number of Macroblocks (MBs) |  | Rename | mb\_count |  |
| BlkCnt4x4 | int | Number of 4x4 Blocks (Not used) |  | Remove |  |  |
| CodedMv | int | Coded Motion Vectors | Yes | Remove duplicate use |  |  |
| NumBlksSkip | int | Number of Skipped Blocks | Yes | Remove duplicate use |  |  |
| NumBlksIntra | int | Number of Intra-coded Blocks | Yes | Remove duplicate use |  |  |
| BitCntMotion | double | Bit count for motion | Yes | Remove duplicate use |  |  |
| BitCntCoefs | double | Bit count for coefficients | Yes | Remove duplicate use |  |  |
| MV\_Length | double | Motion Vector (MV) length, overall |  | Rename | mv\_length | ⏳ |
| MV\_dLength | double | Difference in MV length |  | Rename | mv\_length\_diff |  |
| MV\_SumSQR | double | Sum of squared MV lengths |  | Rename | mv\_sum\_sqr | ⏳ |
| MV\_LengthX | double | MV length in the X direction |  | Rename | mv\_x\_length | ⏳ |
| MV\_LengthY | double | MV length in the Y direction |  | Rename | mv\_y\_length | ⏳ |
| MV\_XSQR | double | Sum of squared MV lengths in the X direction |  | Rename | mv\_x\_sum\_sqr | ⏳ |
| MV\_YSQR | double | Sum of squared MV lengths in the Y direction |  | Rename | mv\_y\_sum\_sqr | ⏳ |
| MV\_DifSum | double | Sum of MV differences (Not used) |  | Remove |  |  |
| MV\_DifSumSQR | double | Sum of squared MV differences (Not used) |  | Remove |  |  |
| PU\_Stat | int32\_t\[7\]\[6\] | PU (Prediction Unit) statistics (HEVC only) |  | Rename | pu\_stats |  |
| TreeStat | int32\_t | Tree statistics (Not used) |  | Remove |  |  |
| AverageCoefs | double\[5\]\[1024\] | Average coefficients for different sizes and types of transforms | Yes | Remove duplicate use |  |  |
| AverageCoefsSQR | double\[5\]\[1024\] | Squared average coefficients for different sizes and types of transforms |  | Rename | coefs\_avg\_sqr |  |
| AverageCoefsBlkCnt | uint32\_t\[5\]\[1024\] | Block count for average coefficients, different sizes and types of transforms \[0\]:No Transf.   \[2\]: 4x4    \[3\]: 8x8    \[4\]: 16x16    \[5\]: 32\*32 |  | Rename | coefs\_avg\_mb\_count |  |
| NormalizedField | float\*\[2\] | Normalized field pointers (array of two float pointers) |  | Remove |  |  |
| FrameDistance | int | Number of frames between a "hidden frame" and a "golden frame" (for counting) |  | Rename | frame\_distance |  |

# **Bitstream Model Features**

These features are used by the P.1204.3 model, and are either direct usages or aggregations of underlying features.

The goal is not to compute these features in the video parser, but only later.

| Feature | Description | Used underlying feature (new names) | Aggregation | Can already be implemented? |
| :---- | :---- | :---- | :---- | :---- |
| Bitrate | Avg. bitrate over entire file | video\_bitrate | Once per sequence | ✅ |
| Framerate | Avg. framerate over entire file | video\_framerate | Once per sequence | ✅ |
| Resolution | Number of pixels | video\_width, video\_height | Once per sequence | ✅ |
| Codec | either h264, hevc, vp9 | video\_codec | Once per sequence | ✅ |
| BitDepth | Bit depth | video\_bit\_depth | Once per sequence | ✅ |
| QPValuesStatsPerGop | Mean, median, std, skew, kurtosis, iqr, quantile (0-10) of underlying features | qp\_avgqp\_bb\_avgqp\_maxqp\_min | Per GOP | ⏳  (without BB, VP9 only for non-segmented streams) |
| QPstatspersecond | Some aggregation of QP (the feature code is quite convoluted, simplify) | qp\_bb\_avg | Second | ⏳  (without BB, VP9 only for non-segmented streams) |
| FramesizeStatsPerGop | Mean, median, std, skew, kurtosis, iqr, quantile (0-10) of underlying feature | frame\_size | GOP | ✅ |
| AvMotionStatsPerGop | Mean, median, std, skew, kurtosis, iqr, quantile (0-10) of underlying feature | motion\_avg, motion\_diff\_avg, motion\_x\_avg, motion\_y\_avg, motion\_stdev, motion\_diff\_stdev, motion\_x\_stdev, motion\_y\_stdev | GOP | ⏳ |

# **FFmpeg Git Diff Conversion Notes**

* Cleaned up code vs. original changes:
  * [https://github.com/slhck/ffmpeg-videoparser-comparison](https://github.com/slhck/ffmpeg-videoparser-comparison)
    * [https://github.com/slhck/ffmpeg-videoparser-comparison/tree/modified\_code\_clean](https://github.com/slhck/ffmpeg-videoparser-comparison/tree/modified_code_clean) → cleaned code
  * Check libavcodec/h264\_cavlc.c changes in detail, possibly we missed something due to the massive whitespace changes
* VP9:
  * Got some ideas from Ronald S. Bultje on video-dev slack:
    * “mvs should be somewhat straightforward, yes. there's multiple ways: you can do what you did here and have it in a codec-specific way”
    * “you can also try using AV\_FRAME\_DATA\_MOTION\_VECTORS”
    * “the codec-agnostic version of that is called AV\_FRAME\_DATA\_VIDEO\_ENC\_PARAMS”
    * (see vf\_qp video filter)
* AV1
  * Changes from PL within libaom: [https://avt10.rz.tu-ilmenau.de/git/avt-pnats2avhd/videoparser\_v2\_dev/-/blob/main/changes-aom?ref\_type=heads](https://avt10.rz.tu-ilmenau.de/git/avt-pnats2avhd/videoparser_v2_dev/-/blob/main/changes-aom?ref_type=heads)
  * Most information is already exported (like seqhdr, frmhdr)

