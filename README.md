# VideoParser NG

- [Requirements](#requirements)
  - [Installation under macOS](#installation-under-macos)
  - [Installation under Ubuntu](#installation-under-ubuntu)
- [Build](#build)
- [Testing](#testing)
- [Maintenance](#maintenance)
  - [Fetching new FFmpeg commits](#fetching-new-ffmpeg-commits)
- [License](#license)

## Requirements

- CMake 3.15 or higher
- C++17 compiler

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
  zlib1g-dev
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

## Testing

Generate the test videos:

```bash
util/generate-test-videos.sh
```

Then, **TODO**

## Maintenance

### Fetching new FFmpeg commits

Run the script:

```bash
util/rebase-ffmpeg.sh
```

## License

Copyright (c) 2023 AVEQ GmbH.

This project is licensed under the GNU GPLv3 License - see the LICENSE.md file for details.

See the LICENSE.md file for details on third-party licenses.
