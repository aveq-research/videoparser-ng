#!/usr/bin/env bash
#
# Build ffmpeg in the external/ffmpeg directory

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
LIBAOM_BUILD="${PROJECT_ROOT}/external/libaom/aom_build"

cd "${PROJECT_ROOT}/external/ffmpeg" || (echo "ffmpeg directory not found!" && exit 1)

# Explicitly set SRC_PATH to current directory
SRC_PATH="$(pwd)"
export SRC_PATH

# Build libaom if not already built
if [[ ! -f "${LIBAOM_BUILD}/libaom.a" ]]; then
  echo "Building libaom first..."
  "${SCRIPT_DIR}/build-libaom.sh"
fi

usage() {
  echo "Usage: $0 [options]"
  echo "  --reconfigure       reconfigure ffmpeg"
  echo "  --clean             clean ffmpeg build (implies reconfigure)"
  echo "  --help              print this message"
  exit 1
}

reconfigure=false
clean=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconfigure)
      reconfigure=true
      ;;
    --clean)
      clean=true
      ;;
    --help)
      usage
      ;;
    *)
      echo "Unknown option: $1"
      usage
      ;;
  esac
  shift
done

startTime=$(date +%s)

if [[ "$clean" = true ]]; then
  echo "Cleaning ffmpeg build..."

  # Only run make clean if config.mak exists! (In a Docker environment, this is not the case, and the makefile fails because
  # it expects SRC_PATH to be set.)
  if [[ -f ffbuild/config.mak ]]; then
    make clean
  fi
  rm -f config.h
fi

if [[ ! -f config.h ]] || [[ "$reconfigure" = true ]]; then
  echo "Configuring ffmpeg..."

  # Paths for vendored libaom
  LIBAOM_SRC="${PROJECT_ROOT}/external/libaom"

  # Set PKG_CONFIG_PATH so ffmpeg's configure can find libaom via pkg-config
  export PKG_CONFIG_PATH="${LIBAOM_BUILD}:${PKG_CONFIG_PATH:-}"

  configureFlags=(
    --disable-programs
    --disable-doc
    --disable-stripping
    --enable-static
    --enable-pthreads
    --enable-debug=2
    # disable filters and scaling
    --disable-avfilter
    --disable-swscale
    --disable-swresample
    # hardware acceleration
    --disable-audiotoolbox
    --disable-videotoolbox
    --disable-vaapi
    --disable-vdpau
    --disable-vulkan
    # other third-party libs
    --disable-securetransport
    --disable-iconv
    --disable-libdrm
    # no output needed, no filters
    --disable-encoders
    --disable-muxers
    --disable-outdevs
    --disable-bsfs
    # disable all but file protocol
    --disable-indevs
    --disable-protocols
    --enable-protocol=file
    # FIXME: below is not working, leading to linking errors down the line
    # only specific decoders
    # --disable-decoders
    # --enable-decoder=h264
    # --enable-decoder=hevc
    # --enable-decoder=vp9
    # --enable-decoder=aac
    --disable-parsers
    --enable-parser=h264
    --enable-parser=hevc
    --enable-parser=vp9
    --enable-parser=av1
    --enable-parser=vorbis
    # needs lzma, we don't need it
    --disable-decoder=tiff
    # only specific demuxers
    --disable-demuxers
    --enable-demuxer=h264
    --enable-demuxer=hevc
    --enable-demuxer=avi
    --enable-demuxer=matroska
    --enable-demuxer=mov
    --enable-demuxer=mpegvideo
    --enable-demuxer=mpegts
    # for AOM (vendored)
    --enable-libaom
    "--extra-cflags=-I${LIBAOM_SRC} -I${LIBAOM_BUILD}"
    "--extra-ldflags=-L${LIBAOM_BUILD}"
    # to make bit count work for CABAC
    --disable-inline-asm
  )

  ./configure "${configureFlags[@]}"
fi

echo "Building ffmpeg..."

# Use MAKE_JOBS env var if set, otherwise use nproc
JOBS="${MAKE_JOBS:-$(nproc)}"
make "-j${JOBS}"

endTime=$(date +%s)

echo "ffmpeg build complete, took $((endTime - startTime)) seconds"
