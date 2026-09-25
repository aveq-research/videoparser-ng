#!/usr/bin/env bash
#
# Build the library and CLI in build/.
#
# With --legacy, build against the legacy-mode ffmpeg (VP_MV_POC_NORMALIZATION=1)
# in build/legacy instead, and install an SDK (lib/ and include/) to
# build/legacy/sdk. The normal build is not affected.
#
# With --shared, build libvideoparser as a shared library against the shared
# ffmpeg (`util/build-ffmpeg.sh --shared`) in build/shared (or
# build/shared-legacy with --legacy), and install an SDK with the shared
# libraries, the CLI and the ffmpeg programs to build/shared[-legacy]/sdk.
#
# Arguments after -- are passed to CMake, for example
# -- -DVIDEOPARSER_CLI_RPATH='$ORIGIN/../lib'.

set -e

usage() {
  echo "Usage: $0 [options]"
  echo "  --legacy            build in legacy mode into build/legacy"
  echo "  --shared            build a shared library into build/shared"
  echo "  --prefix <dir>      install an SDK to this directory (default for --legacy: build/legacy/sdk)"
  echo "  --help              print this message"
  exit 1
}

legacy=false
shared=false
prefix=""
extraCmakeArgs=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --legacy)
      legacy=true
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
    --)
      shift
      extraCmakeArgs=("$@")
      break
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
ffmpegArgs=()
if [[ "$shared" = true ]]; then
  buildDir=build/shared
  cmakeFlags=(-DVIDEOPARSER_SHARED=ON)
  ffmpegArgs=(--shared)
  if [[ "$legacy" = true ]]; then
    buildDir=build/shared-legacy
    cmakeFlags+=(-DVIDEOPARSER_LEGACY=ON)
    ffmpegArgs+=(--legacy)
  else
    cmakeFlags+=(-DVIDEOPARSER_LEGACY=OFF)
  fi
  prefix="${prefix:-${buildDir}/sdk}"
elif [[ "$legacy" = true ]]; then
  buildDir=build/legacy
  cmakeFlags=(-DVIDEOPARSER_LEGACY=ON)
  ffmpegArgs=(--legacy)
  prefix="${prefix:-${buildDir}/sdk}"
fi

# The library is compiled before its pre-build step builds ffmpeg, and the
# legacy and shared ffmpeg headers only exist after the first build, so build
# it first
if [[ ${#ffmpegArgs[@]} -gt 0 ]]; then
  util/build-ffmpeg.sh "${ffmpegArgs[@]}"
fi

mkdir -p "${buildDir}"

cmake -S . -B "${buildDir}" "${cmakeFlags[@]}" "${extraCmakeArgs[@]}"
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
