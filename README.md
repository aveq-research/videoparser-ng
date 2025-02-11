# VideoParser – The Next Generation

A command-line and API-based video bitstream parser, using ffmpeg and other third party libraries.

- [Overview](#overview)
- [Requirements](#requirements)
  - [Installation under macOS](#installation-under-macos)
  - [Installation under Ubuntu](#installation-under-ubuntu)
- [Build](#build)
  - [Rebuilding ffmpeg](#rebuilding-ffmpeg)
- [Building with Docker](#building-with-docker)
- [Usage](#usage)
- [Output](#output)
- [Available Metrics](#available-metrics)
- [Developer Guide](#developer-guide)
- [Acknowledgements](#acknowledgements)
- [Contributing](#contributing)
- [License](#license)

## Overview

This project supplies two main components:

- A command-line tool for parsing video bitstreams (`video-parser`)
- A C++ API for parsing video bitstreams, `libvideoparser`, which is used by the CLI program

The project is aimed at providing input for bitstream-based video (quality) assessment.

Internally, ffmpeg is used, and linked into the project. Currently, the project is built with ffmpeg `959b799c8d7` (January 2025) based on the `master` branch. We strive to keep the project up to date with the latest ffmpeg version.

There are some design docs about this project [available here](https://docs.google.com/document/d/1pnQGDWRjSfff4TyTNWcdeUmCMdCz8crsGo6Ws3rR6W0/edit?tab=t.0). It explains the rationale for the creation of the project, and what we would like to add as features.

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
  automake git libtool sdl shtool texi2html wget nasm \
  aom
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
  libbz2-dev \
  libaom-dev
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

## Building with Docker

To build the project with Docker, run:

```bash
docker build -t videoparser-ng .
```

This will build the project and create a Docker image named `videoparser-ng`.

## Usage

To run the CLI, run:

```bash
build/VideoParserCli/video-parser <path-to-video-file>
```

Or, for Docker, you must mount the video file into the container, e.g. to run it on `test/test_video_h264.mkv`, do:

```bash
docker run --rm -it -v $(pwd)/test/test_video_h264.mkv:/video.mkv videoparser-ng /video.mkv
```

Add the option `-h` for detailed usage.

## Output

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
  - Other, specific per-frame metrics are being implemented at the moment.

## Developer Guide

We have a more detailed guide for testing and the specific features implemented. See [DEVELOPERS.md](DEVELOPERS.md).

## Acknowledgements

If you use this project in your research, please reference this repository.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Copyright (c) AVEQ GmbH.
Copyright (c) videoparser-ng contributors.

This project is licensed under the GNU GNU LESSER GENERAL PUBLIC License v2.1 - see the LICENSE.txt file for details.

Also, see the LICENSE.txt file for details on third-party licenses.
