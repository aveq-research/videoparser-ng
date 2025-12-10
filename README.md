# VideoParser – The Next Generation

A command-line and API-based video bitstream parser, using ffmpeg and other third party libraries.

- [Overview](#overview)
- [History and Goals](#history-and-goals)
- [Installation](#installation)
  - [Native Binaries](#native-binaries)
  - [Docker](#docker)
- [Usage](#usage)
- [Output](#output)
- [Available Metrics](#available-metrics)
  - [Sequence Info](#sequence-info)
  - [Frame Info](#frame-info)
- [API Integration](#api-integration)
- [Building Manually](#building-manually)
  - [Requirements](#requirements)
  - [Installation under macOS](#installation-under-macos)
  - [Installation under Ubuntu](#installation-under-ubuntu)
  - [Building](#building)
  - [Rebuilding ffmpeg](#rebuilding-ffmpeg)
  - [Building with Docker](#building-with-docker)
- [Developer Guide](#developer-guide)
- [Acknowledgements](#acknowledgements)
- [Contributing](#contributing)
- [License](#license)

## Overview

This project supplies two main components:

- A command-line tool for parsing video bitstreams (`video-parser`)
- A C++ API for parsing video bitstreams, `libvideoparser`, which is used by the CLI program

The project is aimed at providing input for bitstream-based video (quality) assessment.

Internally, ffmpeg is used, and linked into the project. Currently, the project is built with FFmpeg 8.1-dev (1407 commits from latest release).

<!-- git -C external/ffmpeg describe --tags HEAD~3 | sed 's/n\(.*\)-\([0-9]*\)-.*/FFmpeg \1 (\2 commits from latest release)/' -->

We strive to keep the project up to date with the latest ffmpeg version.
The actual ffmpeg changes are published in [this `ffmpeg` fork](https://github.com/aveq-research/ffmpeg/tree/videoparser) in the `videoparser` branch.

For AV1 support, libaom is also vendored as a submodule. The code, including its changes, is mirrored at [this `libaom` fork](https://github.com/aveq-research/libaom) in the `videoparser` branch.

## History and Goals

The project is using previous work from the [`bitstream_mode3_videoparser`](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_videoparser) project. This project was written from the ground up to be faster. How fast?

```
               ╔══════════════════════════════════════════════════╗
Legacy parser  ╢██████████████████████████████████████████████████╟ 21.161s
videoparser-ng ╢███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░╟  1.291s
               ╠══════════════════════════════════════════════════╣
```

Super fast – a 16⨉ speedup! This is on a 1920x1080 HEVC video file at 29.97 fps, parsing all frames and extracting all available metrics.
For 3840x2160 (4K) videos, the speedup is even higher, around 70x as measured on a M1 Mac.

The overall goal is to provide bitstream statistics to be later used for calculating video quality metrics such as ITU-T Rec. P.1204.3, which has a [reference implementation available](https://github.com/Telecommunication-Telemedia-Assessment/bitstream_mode3_p1204_3).

There are some design docs about this project [available here](https://docs.google.com/document/d/1pnQGDWRjSfff4TyTNWcdeUmCMdCz8crsGo6Ws3rR6W0/edit?tab=t.0). It explains the rationale for the creation of the project, the detailed statistics available in the `bitstream_mode3_videoparser` project have ported over, and what we would like to add as features on top of that.

## Installation

### Native Binaries

Go to [releases](https://github.com/aveq-research/videoparser-ng/releases) and pick the right archive for your platform. Extract it, then run the `video-parser` binary.

> [!NOTE]
> **macOS Security Limitations:** You might need to allow the binary to run. Try running it once from the Terminal, and you will be shown a security notice. Then, go to *System Preferences > Security & Privacy > General*, and allow the binary. After that, you can run it from the Terminal again. Another security popup will show, but this time you can allow it to run directly.

### Docker

We provide a pre-built Docker image on GitHub Container Registry. You can use it without building it yourself.

To pull the image, you need to have a valid GitHub token with access to the repository. If you don't have a token, [create one here](https://github.com/settings/tokens), and make sure it has `read:packages` scope enabled.

```bash
docker login ghcr.io
```

You will be prompted to enter your GitHub username, and as password, enter your personal access token. Once you have a token, you can pull the image, and rename it to `videoparser-ng` for easier use.

```bash
docker pull ghcr.io/aveq-research/videoparser-ng:latest
docker image tag ghcr.io/aveq-research/videoparser-ng:latest videoparser-ng
```

The `latest` tag will always point to the latest stable release. If you want to use a specific version, you can pull it by replacing `latest` with the version tag, e.g. `0.2.0`:

```bash
docker pull ghcr.io/aveq-research/videoparser-ng:0.2.0
```

The latest master branch build is also available under the `master` tag, but it is not guaranteed to be stable.

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

The tool will print a set of line-delimited JSON records to STDOUT, either for per-sequence statistics (`sequence_info`), or per-frame statistics (`frame_info`). These are denoted with the `type` field.

Here is an example, but formatted with `jq` to make it more readable:

```bash
build/VideoParserCli/video-parser test/test_video_h264.mkv -n 1 | jq
```

This would print:

```json
{
  "type": "sequence_info",
  "video_bit_depth": 8,
  "video_bitrate": 37.0,
  "video_codec": "h264",
  "video_codec_level": 13,
  "video_codec_profile": 100,
  "video_duration": 10.0,
  "video_frame_count": 300,
  "video_framerate": 30.0,
  "video_height": 240,
  "video_pix_fmt": "yuv420p",
  "video_width": 320
}
{
  "coefs_bit_count": 1328,
  "current_poc": 0,
  "dts": 0.0,
  "frame_idx": 0,
  "frame_type": 1,
  "is_idr": true,
  "mb_mv_count": 0,
  "motion_avg": 0.0,
  "motion_bit_count": 0,
  "motion_diff_avg": 0.0,
  "motion_diff_stdev": 0.0,
  "motion_stdev": 0.0,
  "motion_x_avg": 0.0,
  "motion_x_stdev": 0.0,
  "motion_y_avg": 0.0,
  "motion_y_stdev": 0.0,
  "mv_coded_count": 0,
  "poc_diff": -1,
  "pts": 0.0,
  "qp_avg": 16.196666666666665,
  "qp_bb_avg": 16.196666666666665,
  "qp_bb_stdev": 7.840790067900615,
  "qp_init": 10,
  "qp_max": 31,
  "qp_min": 10,
  "qp_stdev": 7.840790067900615,
  "size": 87,
  "type": "frame_info"
}
```

The tool will also print various logs to STDERR which you can redirect to a file if you want to save them, or ignore with `2>/dev/null`.

## Available Metrics

The following metadata/metrics are available:

### Sequence Info

| Metric                | Description                       | Unit    |
| --------------------- | --------------------------------- | ------- |
| `video_duration`      | Duration of the video             | seconds |
| `video_codec`         | Codec name (h264, hevc, vp9, av1) | —       |
| `video_bitrate`       | Average bitrate                   | kbps    |
| `video_framerate`     | Frame rate                        | fps     |
| `video_width`         | Frame width                       | pixels  |
| `video_height`        | Frame height                      | pixels  |
| `video_codec_profile` | Codec profile ID                  | —       |
| `video_codec_level`   | Codec level ID                    | —       |
| `video_bit_depth`     | Bit depth per sample              | bits    |
| `video_pix_fmt`       | Pixel format (e.g., yuv420p)      | —       |
| `video_frame_count`   | Total number of frames            | count   |

### Frame Info

| Metric              | Description                               | Unit     |
| ------------------- | ----------------------------------------- | -------- |
| `frame_idx`         | Zero-based frame index in decode order    | count    |
| `pts`               | Presentation timestamp                    | seconds  |
| `dts`               | Decoding timestamp                        | seconds  |
| `size`              | Frame size                                | bytes    |
| `frame_type`        | Frame type (1=I, 2=P, 3=B)                | enum     |
| `is_idr`            | Whether frame is an IDR/keyframe          | boolean  |
| `qp_avg`            | Average QP of all coding units            | QP index |
| `qp_stdev`          | Standard deviation of QP values           | QP index |
| `qp_min`            | Minimum QP value in frame                 | QP index |
| `qp_max`            | Maximum QP value in frame                 | QP index |
| `qp_init`           | Initial QP from slice/frame header        | QP index |
| `qp_bb_avg`         | Average QP excluding black borders        | QP index |
| `qp_bb_stdev`       | Std. dev. of QP excluding black borders   | QP index |
| `motion_avg`        | Average motion vector length              | sub-pel  |
| `motion_stdev`      | Std. dev. of motion vector lengths        | sub-pel  |
| `motion_x_avg`      | Average of absolute X components          | sub-pel  |
| `motion_y_avg`      | Average of absolute Y components          | sub-pel  |
| `motion_x_stdev`    | Std. dev. of X components                 | sub-pel  |
| `motion_y_stdev`    | Std. dev. of Y components                 | sub-pel  |
| `motion_diff_avg`   | Average motion vector prediction residual | sub-pel  |
| `motion_diff_stdev` | Std. dev. of motion vector residuals      | sub-pel  |
| `current_poc`       | Picture Order Count of current frame      | count    |
| `poc_diff`          | Minimum POC difference between frames     | count    |
| `motion_bit_count`  | Bits used for motion information          | bits     |
| `coefs_bit_count`   | Bits used for transform coefficients      | bits     |
| `mb_mv_count`       | Number of blocks with motion vectors      | count    |
| `mv_coded_count`    | Number of explicitly coded MVs            | count    |

A detailed description of all available metrics is available in [METRICS.md](METRICS.md).

For the implementation notes (i.e., what was modified to extract the metrics), see [DEVELOPERS.md](DEVELOPERS.md).

## API Integration

The project provides a C++ API in the `libvideoparser` library. See the `VideoParserCli` folder for an example of how to use the API.

API documentation is available in the `docs` folder. You can [view it at this location](https://raw.githack.com/aveq-research/videoparser-ng/master/docs/html/index.html).

## Building Manually

Follow the instructions below to build the project from source.

### Requirements

- Ubuntu 22.04 or higher, or macOS 12.0 or higher
- CMake 3.15 or higher
- A C++17 compiler (GCC 9.3 or higher, Clang 10.0 or higher)

You also need:

- `pkg-config`
- `ninja-build`

Note that you can skip the build step by just using the pre-built Docker image.

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
  nasm \
  zlib1g-dev \
  libbz2-dev
```

### Building

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

### Building with Docker

To build the project with Docker, run:

```bash
docker build -t videoparser-ng .
```

This will build the project and create a Docker image named `videoparser-ng`.

## Developer Guide

We have a more detailed guide for testing and the specific features implemented. See [DEVELOPERS.md](DEVELOPERS.md).

## Acknowledgements

If you use this project in your research, please reference this repository:

```bibtex
@misc{videoparser-ng,
  author       = {Werner Robitza and Jonatan Stenlund and others},
  title        = {VideoParser – The Next Generation},
  year         = {2024},
  publisher    = {GitHub},
  journal      = {GitHub repository},
  howpublished = {\url{https://github.com/aveq-research/videoparser-ng}}
}
```

Contributions to this project were funded by AVEQ GmbH and Ericsson. Contributors:

- Werner Robitza (AVEQ GmbH)
- Jonatan Stenlund (Ericsson, Luleå University of Technology): initial motion vector analysis implementation, see [`motion-vectors` branch](https://github.com/aveq-research/videoparser-ng/compare/master...motion-vectors)

This project would not have been possible without the major work that went into the original video parser. The main developers who have to be credited for this include Peter List (basic parser code, bulk of the implementation for H.264 and HEVC), Anton Schubert (VP9 implementation), Steve Göring (major code and build improvements), Werner Robitza (Python parser interface, debugging), and Rakesh Rao Ramachandra Rao (various code improvements and model training).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) if you want to contribute.

## License

Copyright (c) AVEQ GmbH, videoparser-ng contributors.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Also, see the LICENSE.txt file for details on third-party licenses.
