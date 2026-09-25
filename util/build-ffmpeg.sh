#!/usr/bin/env bash
#
# Build ffmpeg in the external/ffmpeg directory.
#
# With --shared, build shared libraries with swscale and swresample instead,
# for use by other programs (e.g. OpenCV). The source is copied to
# build/ffmpeg-shared/src, since ffmpeg cannot be built out of tree once the
# source directory holds the static build.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
LIBAOM_BUILD="${PROJECT_ROOT}/external/libaom/aom_build"

FFMPEG_SRC="${PROJECT_ROOT}/external/ffmpeg"
SHARED_BUILD="${PROJECT_ROOT}/build/ffmpeg-shared"

# Build libaom if not already built
if [[ ! -f "${LIBAOM_BUILD}/libaom.a" ]]; then
  echo "Building libaom first..."
  "${SCRIPT_DIR}/build-libaom.sh"
fi

usage() {
  echo "Usage: $0 [options]"
  echo "  --reconfigure       reconfigure ffmpeg"
  echo "  --clean             clean ffmpeg build (implies reconfigure)"
  echo "  --shared            build shared libraries into build/ffmpeg-shared"
  echo "  --prefix <dir>      install directory for --shared (default: build/ffmpeg-shared/install)"
  echo "  --help              print this message"
  exit 1
}

reconfigure=false
clean=false
shared=false
prefix="${SHARED_BUILD}/install"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconfigure)
      reconfigure=true
      ;;
    --clean)
      clean=true
      ;;
    --shared)
      shared=true
      ;;
    --prefix)
      shift
      prefix="$1"
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

if [[ "$shared" = true ]]; then
  # Copy tracked and untracked (but not ignored) source files. tar keeps the
  # modification times, so make only rebuilds what changed.
  mkdir -p "${SHARED_BUILD}/src"
  git -C "${FFMPEG_SRC}" ls-files -z --cached --others --exclude-standard |
    tar -C "${FFMPEG_SRC}" --null -T - -cf - |
    tar -C "${SHARED_BUILD}/src" -xf -
  cd "${SHARED_BUILD}/src"
else
  cd "${FFMPEG_SRC}" || (echo "ffmpeg directory not found!" && exit 1)
fi

# Explicitly set SRC_PATH to current directory
SRC_PATH="$(pwd)"
export SRC_PATH

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

  # VP_EXTRA_CFLAGS can be used to pass additional compiler flags
  # e.g., VP_EXTRA_CFLAGS="-DVP_MV_POC_NORMALIZATION=1" to enable POC-based MV normalization
  EXTRA_CFLAGS="-I${LIBAOM_SRC} -I${LIBAOM_BUILD}"
  if [[ -n "${VP_EXTRA_CFLAGS:-}" ]]; then
    EXTRA_CFLAGS="${EXTRA_CFLAGS} ${VP_EXTRA_CFLAGS}"
  fi

  configureFlags=(
    --disable-programs
    --disable-doc
    --disable-stripping
    --enable-pthreads
    --enable-debug=2
    --disable-avfilter
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
    --enable-parser=mpegvideo
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
    --enable-demuxer=mpegps
    # for AOM (vendored)
    --enable-libaom
    "--extra-cflags=${EXTRA_CFLAGS}"
    "--extra-ldflags=-L${LIBAOM_BUILD}"
    # to make bit count work for CABAC
    --disable-inline-asm
  )

  if [[ "$shared" = true ]]; then
    configureFlags+=(
      --enable-shared
      --disable-static
      "--prefix=${prefix}"
      # needed by OpenCV's videoio
      --enable-swscale
      --enable-swresample
      --disable-avdevice
    )
    # Find the other ffmpeg libraries in the same directory. configure expands
    # "$" once and make twice, hence the escaping.
    if [[ "$(uname)" = Linux ]]; then
      configureFlags+=("--extra-ldsoflags=-Wl,-rpath,'\\\$\\\$\\\$\\\$ORIGIN'")
    fi
  else
    configureFlags+=(
      --enable-static
      --disable-swscale
      --disable-swresample
    )
  fi

  ./configure "${configureFlags[@]}"
fi

echo "Building ffmpeg..."

# Use MAKE_JOBS env var if set, otherwise use nproc
JOBS="${MAKE_JOBS:-$(nproc)}"
make "-j${JOBS}"

if [[ "$shared" = true ]]; then
  make install
  echo "ffmpeg shared libraries installed to ${prefix}"
fi

endTime=$(date +%s)

echo "ffmpeg build complete, took $((endTime - startTime)) seconds"
