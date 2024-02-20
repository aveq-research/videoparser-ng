#!/usr/bin/env bash
#
# Build ffmpeg in the external/ffmpeg directory

set -e

cd "$(dirname "$0")/../external/ffmpeg" || (echo "ffmpeg directory not found!" && exit 1)

usage() {
  echo "Usage: $0 [options]"
  echo "  --reconfigure:      reconfigure ffmpeg"
  echo "  --help:             print this message"
  exit 1
}

reconfigure=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconfigure)
      reconfigure=true
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

if [[ ! -f config.h ]] || [[ "$reconfigure" = true ]]; then
  echo "Configuring ffmpeg..."

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
    # --disable-parsers
    # --enable-parser=h264
    # --enable-parser=hevc
    # --enable-parser=vp9
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
  )

  ./configure "${configureFlags[@]}"
fi

echo "Building ffmpeg..."

make "-j$(nproc)"

endTime=$(date +%s)

echo "ffmpeg build complete, took $((endTime - startTime)) seconds"
