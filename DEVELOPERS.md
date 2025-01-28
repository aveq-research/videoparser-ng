# Developer Guide

Contents:

- [General Structure](#general-structure)
- [Modifications Made](#modifications-made)
  - [QP Information](#qp-information)
  - [Open Bugs and TODO](#open-bugs-and-todo)
- [Testing](#testing)
- [Maintenance](#maintenance)
  - [Fetching new FFmpeg commits](#fetching-new-ffmpeg-commits)

## General Structure

This program patches ffmpeg to add support for extracting additional bitstream properties, or codec-related information, from the bitstream. The program is structured as follows:

- `VideoParser` is an API that provides a simple interface for extracting bitstream properties from a video file. It is implemented as a basic frame-by-frame reader that itself is using ffmpeg standard API calls to read the video.
- VideoParserCli is a command-line interface for VideoParser. It is implemented by using VideoParser API calls to extract the bitstream properties, and then printing them to the console in JSON format.
- ffmpeg is cloned and has a separate branch checked out.

To pass extra information from the ffmpeg part to the VideoParser part, we use the `SharedFrameInfo` struct to store the bitstream properties like QP values, motion vectors, etc. The definition is in `VideoParser/include/shared.h`, and it is included in ffmpeg as well. The shared info is part of the `AVFrame` struct, and is extracted from there using a helper function `av_frame_get_shared_frame_info`.

## Modifications Made

This explains the high level changes made to ffmpeg to support the extraction of bitstream properties.

### QP Information

To obtain the QP information, we modify:

- H.264: `h264_mb.c`, to extract the QP information from the `H264SliceContext` struct, in the function `ff_h264_hl_decode_mb`
- HEVC: `hevcdec.c`, to extract the QP information from the `HEVCLocalContext` struct, in the function `hls_coding_unit`
- VP9: `vp9.c`, to extract the QP information from the `VP9SharedContext` struct, in the function `vp9_decode_frame`
- AV1: `libaomdec.c`, to extract the QP information from the `AV1DecodeContext` struct, in the function `aom_decode`

### Open Bugs and TODO

- [ ] Fix the QP information for H.265: sometimes they are missing completely
- [ ] Add other modifications here!

## Testing

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
python3 -m pytest test/test.py
```

Also you can run the CLI tests:

```bash
python3 -m pytest test/test-cli.py
```

## Maintenance

### Fetching new FFmpeg commits

Occasionally you want to rebase your local FFmpeg commits on top of the latest upstream FFmpeg commits. This is done by running the dedicated script.

Run the script:

```bash
util/rebase-ffmpeg.sh
```

The rebase may not be clean, so check the output of the script and resolve any conflicts.
