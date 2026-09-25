#!/usr/bin/env bash
#
# Build ffmpeg in the external/ffmpeg directory.
#
# With --shared, build shared libraries with swscale and swresample instead,
# for use by other programs (e.g. OpenCV). The source is copied to
# build/ffmpeg-shared/src, since ffmpeg cannot be built out of tree once the
# source directory holds the static build.
#
# The shared build also includes libavfilter with a few filters (scaling,
# deinterlacing, and the psnr, ssim and libvmaf metrics), a static libvmaf
# from `util/build-libvmaf.sh`, and the ffmpeg and ffprobe programs. The
# programs can decode to the null muxer and write raw video (plain or as
# YUV4MPEG) to a file or pipe. All options stay LGPL. As with the library,
# the patched decoders need one thread: run `ffmpeg -threads 1 -i <input>`.
#
# With --legacy, build with VP_MV_POC_NORMALIZATION=1 (legacy mode) in a copy
# of the source in build/ffmpeg-legacy/src, next to the normal build.
# Combined with --shared, the build goes to build/ffmpeg-shared-legacy.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
LIBAOM_BUILD="${PROJECT_ROOT}/external/libaom/aom_build"

FFMPEG_SRC="${PROJECT_ROOT}/external/ffmpeg"

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
  echo "  --legacy            build in legacy mode into build/ffmpeg-legacy"
  echo "  --prefix <dir>      install directory for --shared (default: build/ffmpeg-shared/install)"
  echo "  --exe-rpath <dirs>  runpath of the programs for --shared: directories relative"
  echo "                      to the program, separated by ':' (default: ../lib)"
  echo "  --help              print this message"
  exit 1
}

reconfigure=false
clean=false
shared=false
legacy=false
prefix=""
exeRpath="../lib"

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
    --legacy)
      legacy=true
      ;;
    --prefix)
      shift
      prefix="$1"
      ;;
    --exe-rpath)
      shift
      exeRpath="$1"
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

# Directory for builds outside of external/ffmpeg
copyBuild="${PROJECT_ROOT}/build/ffmpeg"
[[ "$shared" = true ]] && copyBuild="${copyBuild}-shared"
[[ "$legacy" = true ]] && copyBuild="${copyBuild}-legacy"
prefix="${prefix:-${copyBuild}/install}"

LIBVMAF_PREFIX="${PROJECT_ROOT}/build/libvmaf/install"

# Build libvmaf for the shared build if not already built
if [[ "$shared" = true ]] && [[ ! -f "${LIBVMAF_PREFIX}/lib/libvmaf.a" ]]; then
  echo "Building libvmaf first..."
  "${SCRIPT_DIR}/build-libvmaf.sh" --prefix "${LIBVMAF_PREFIX}"
fi

if [[ "$legacy" = true ]]; then
  VP_EXTRA_CFLAGS="${VP_EXTRA_CFLAGS:+${VP_EXTRA_CFLAGS} }-DVP_MV_POC_NORMALIZATION=1"
fi

if [[ "$shared" = true ]] || [[ "$legacy" = true ]]; then
  # Copy tracked and untracked (but not ignored) source files. tar keeps the
  # modification times, so make only rebuilds what changed.
  mkdir -p "${copyBuild}/src"
  git -C "${FFMPEG_SRC}" ls-files -z --cached --others --exclude-standard |
    tar -C "${FFMPEG_SRC}" --null -T - -cf - |
    tar -C "${copyBuild}/src" -xf -
  # The copy is not a git checkout of ffmpeg, so give ffmpeg's version script
  # the version it would find in external/ffmpeg
  ffmpegVersion="$(git -C "${FFMPEG_SRC}" describe --tags --match N 2>/dev/null ||
    git -C "${FFMPEG_SRC}" describe --tags --always 2>/dev/null || true)"
  if [[ -n "${ffmpegVersion}" ]] && [[ "$(cat "${copyBuild}/src/FF_VERSION" 2>/dev/null)" != "${ffmpegVersion}" ]]; then
    echo "${ffmpegVersion}" > "${copyBuild}/src/FF_VERSION"
    rm -f "${copyBuild}/src/.version"
  fi
  cd "${copyBuild}/src"
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
    export PKG_CONFIG_PATH="${LIBVMAF_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    configureFlags+=(
      --enable-shared
      --disable-static
      "--prefix=${prefix}"
      # needed by OpenCV's videoio and the scale and aresample filters
      --enable-swscale
      --enable-swresample
      --disable-avdevice
      # no system libraries except zlib and bzip2 (e.g. no X11, SDL, ALSA,
      # lzma or hardware decoders)
      --disable-autodetect
      --enable-zlib
      --enable-bzlib
      --enable-pthreads
      # programs, e.g. for VMAF: ffmpeg -i ref -i dist -lavfi libvmaf -f null -
      --enable-ffmpeg
      --enable-ffprobe
      --disable-ffplay
      # only the filters needed for scaling, deinterlacing and full-reference
      # metrics; the buffer and buffersink filters are always built
      --enable-avfilter
      --disable-filters
      --enable-filter=scale,format,fps,setpts,crop,pad
      --enable-filter=bwdif,yadif
      --enable-filter=psnr,ssim,libvmaf
      --enable-filter=aresample,aformat,split,null,anull
      --enable-libvmaf
      # static libvmaf (and libaom) with their dependencies
      --pkg-config-flags=--static
      # decoding to the null muxer, and raw video (plain or as YUV4MPEG),
      # e.g. for piping deinterlaced video to another program
      --enable-muxer=null,yuv4mpegpipe,rawvideo
      --enable-encoder=wrapped_avframe,rawvideo
      --enable-demuxer=yuv4mpegpipe
      # AV1 and VP9 elementary streams (OBU, Annex B, IVF)
      --enable-demuxer=obu,av1,ivf
      # audio frame splitting, e.g. for AAC, AC-3 and MPEG audio in MPEG-TS
      --enable-parser=aac,aac_latm,ac3,mpegaudio
      --enable-protocol=pipe
    )
    # Find the other ffmpeg libraries in the same directory, and the libraries
    # from the programs. configure expands "$" once, and make twice for the
    # libraries and once for the programs, hence the escaping.
    if [[ "$(uname)" = Linux ]]; then
      exeRunpath=""
      IFS=: read -r -a exeRpathDirs <<< "${exeRpath}"
      for dir in "${exeRpathDirs[@]}"; do
        exeRunpath="${exeRunpath:+${exeRunpath}:}\\\$\\\$ORIGIN/${dir}"
      done
      configureFlags+=(
        "--extra-ldsoflags=-Wl,-rpath,'\\\$\\\$\\\$\\\$ORIGIN'"
        "--extra-ldexeflags=-Wl,-rpath,'${exeRunpath}'"
      )
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
