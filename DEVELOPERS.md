# Developer Guide

## General Structure

This program patches ffmpeg to add support for extracting additional bitstream properties, or codec-related information, from the bitstream. The program is structured as follows:

- VideoParser is an API that provides a simple interface for extracting bitstream properties from a video file. It is implemented in `VideoParser.cpp` and `VideoParser.h`, using ffmpeg
- VideoParserCli is a command-line interface for VideoParser. It is implemented in `main.cpp`.
- ffmpeg is patched in a separate branch, and it uses the `SharedFrameInfo` struct to store the bitstream properties. This is part of the `AVFrame` struct.

## Modifications Made

This explains the high level changes made to ffmpeg to support the extraction of bitstream properties.

### QP Information

To obtain the QP information, we modify:

- H.264: `h264_mb.c`, to extract the QP information from the `H264SliceContext` struct, in the function `ff_h264_hl_decode_mb`
