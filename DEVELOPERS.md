# Developer Guide

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
