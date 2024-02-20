# VideoParser – The Next Generation

A command-line and API-based video bitstream parser, using ffmpeg and other third party libraries.

- [Overview](#overview)
- [Requirements](#requirements)
  - [Installation under macOS](#installation-under-macos)
  - [Installation under Ubuntu](#installation-under-ubuntu)
- [Build](#build)
  - [Rebuilding ffmpeg](#rebuilding-ffmpeg)
- [Usage](#usage)
- [Available Metrics](#available-metrics)
- [Developers](#developers)
- [License](#license)

## Overview

This project supplies two main components:

- A command-line tool for parsing video bitstreams (`video-parser`)
- A C++ API for parsing video bitstreams, `libvideoparser`, which is used by the CLI program

Internally, ffmpeg is used, and statically linked into the project. This means that the project is self-contained and does not require any external dependencies once built. This way, it can be easily distributed.

## Requirements

- Ubuntu 22.04 or higher, or macOS 12.0 or higher
- CMake 3.15 or higher
- A C++17 compiler (GCC 9.3 or higher, Clang 10.0 or higher)

You also need:

- `pkg-config`
- `ninja-build`

### Installation under macOS

Xcode is required to compile software on your Mac. Install Xcode by ​downloading it from the website or using the Mac App Store.

After installing Xcode, install the Command Line Tools from Preferences > Downloads > Components. You can also install the tools via your shell:

```bash
xcode-select --install
```

Then, using [Homebrew](https://brew.sh), install the required packages:

```bash
brew install \
  cmake ninja pkg-config \
  automake git libtool sdl shtool texi2html wget nasm
```

### Installation under Ubuntu

```bash
sudo apt install \
  autoconf \
  automake \
  build-essential \
  cmake \
  git-core \
  libsdl2-dev \
  libtool \
  libunistring-dev \
  meson \
  ninja-build \
  pkg-config \
  texinfo \
  wget \
  yasm \
  zlib1g-dev \
  libbz2-dev
```

## Build

First clone all submodules:

```bash
git submodule update --init --recursive
```

Then build ffmpeg initially:

```bash
util/build-ffmpeg.sh
```

Then run the build script for the project:

```bash
util/build-cmake.sh
```

This will create the library: `build/VideoParser/libvideoparser.a`

You can also run the CLI:

```
build/VideoParserCli/video-parser
```

### Rebuilding ffmpeg

If you get a warning like:

```
WARNING: libavcodec/allcodecs.c newer than config_components.h, rerun configure
```

Then run:

```bash
util/build-ffmpeg.sh --reconfigure
```

This will rebuild ffmpeg after which you can run the build script for the project again.

## Usage

To run the CLI, run:

```bash
build/VideoParserCli/video-parser <path-to-video-file>
```

Add the option `-h` for detailed usage.

The tool will print a set of line-delimited JSON records to STDOUT, either for per-sequence statistics, or per-frame statistics. These are denoted with the `type` field:

```json
{"dts":0.0,"frame_idx":0,"frame_type":1,"is_idr":true,"pts":0.0,"size":19261,"type":"frame_info"}
{"type":"sequence_info","video_bit_depth":8,"video_bitrate":26.4,"video_codec":"h264","video_codec_level":52,"video_codec_profile":100,"video_duration":10.0,"video_frame_count":2,"video_framerate":60.0,"video_height":2160,"video_pix_fmt":"yuv420p","video_width":3840}
```

The tool will also print various logs to STDERR.

## Available Metrics

The following metadata/metrics are available:

- Per-sequence (type: `sequence_info`):
  - Video codec
  - Video codec profile
  - Video codec level
  - Video width
  - Video height
  - Video pixel format
  - Video bit depth
  - Video frame rate
  - Video duration
  - Video frame count
  - Video bitrate
- Per-frame data (type: `frame_info`):
  - Frame type
  - Frame size
  - Frame PTS
  - Frame DTS
  - Frame index
  - Frame is IDR

Specific per-frame metrics are being implemented at the moment.

## Developers

See [DEVELOPERS.md](DEVELOPERS.md).

## License

Copyright (c) 2023 AVEQ GmbH.
Copyright (c) 2023 videoparser-ng contributors.

This project is licensed under the GNU GNU LESSER GENERAL PUBLIC License v2.1 - see the LICENSE.md file for details.

Also, see the LICENSE.md file for details on third-party licenses.
