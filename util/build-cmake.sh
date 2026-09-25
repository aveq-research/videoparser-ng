#!/usr/bin/env bash
#
# Build the library and CLI in build/.
#
# With --legacy, build against the legacy-mode ffmpeg (VP_MV_POC_NORMALIZATION=1)
# in build/legacy instead, and install an SDK (lib/ and include/) to
# build/legacy/sdk. The normal build is not affected.

set -e

usage() {
  echo "Usage: $0 [options]"
  echo "  --legacy            build in legacy mode into build/legacy"
  echo "  --prefix <dir>      install an SDK to this directory (default for --legacy: build/legacy/sdk)"
  echo "  --help              print this message"
  exit 1
}

legacy=false
prefix=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --legacy)
      legacy=true
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

cd "$(dirname "$0")/.."

buildDir=build
cmakeFlags=(-DVIDEOPARSER_LEGACY=OFF)
if [[ "$legacy" = true ]]; then
  buildDir=build/legacy
  cmakeFlags=(-DVIDEOPARSER_LEGACY=ON)
  prefix="${prefix:-${buildDir}/sdk}"
  # The library is compiled before its pre-build step builds ffmpeg, and the
  # legacy ffmpeg headers only exist after the first build, so build it first
  util/build-ffmpeg.sh --legacy
fi

mkdir -p "${buildDir}"

cmake -S . -B "${buildDir}" "${cmakeFlags[@]}"
cmake --build "${buildDir}"

if [[ -n "$prefix" ]]; then
  cmake --install "${buildDir}" --prefix "${prefix}"
fi

echo "Done. You can find the binary at: "
echo
echo "  ${buildDir}/VideoParserCli/video-parser"
if [[ -n "$prefix" ]]; then
  echo
  echo "SDK installed to: ${prefix}"
fi
