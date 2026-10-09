#!/usr/bin/env bash
#
# Build libaom in the external/libaom directory
#
# Set VP_CROSS_HOST to cross-compile with a toolchain file from util/cross,
# e.g. VP_CROSS_HOST=x86_64-w64-mingw32 for Windows x86_64.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBAOM_SRC="${SCRIPT_DIR}/../external/libaom"
LIBAOM_BUILD="${VP_LIBAOM_BUILD_DIR:-${LIBAOM_SRC}/aom_build}"

crossArgs=()
if [[ -n "${VP_CROSS_HOST:-}" ]]; then
  toolchain="${SCRIPT_DIR}/cross/${VP_CROSS_HOST}.cmake"
  if [[ ! -f "$toolchain" ]]; then
    echo "No CMake toolchain file for VP_CROSS_HOST=${VP_CROSS_HOST} in util/cross"
    exit 1
  fi
  crossArgs=("-DCMAKE_TOOLCHAIN_FILE=${toolchain}")
fi

if [[ ! -d "$LIBAOM_SRC" ]]; then
  echo "libaom source directory not found at: $LIBAOM_SRC"
  echo "Did you run: git submodule update --init --recursive?"
  exit 1
fi

usage() {
  echo "Usage: $0 [options]"
  echo "  --reconfigure       reconfigure libaom"
  echo "  --clean             clean libaom build (implies reconfigure)"
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
  echo "Cleaning libaom build..."
  rm -rf "$LIBAOM_BUILD"
fi

mkdir -p "$LIBAOM_BUILD"
cd "$LIBAOM_BUILD"

if [[ ! -f "CMakeCache.txt" ]] || [[ "$reconfigure" = true ]] || [[ "$clean" = true ]]; then
  echo "Configuring libaom..."

  cmake "$LIBAOM_SRC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_DOCS=OFF \
    -DENABLE_EXAMPLES=OFF \
    -DENABLE_TESTS=OFF \
    -DENABLE_TOOLS=OFF \
    -DCONFIG_AV1_ENCODER=0 \
    -DCONFIG_MULTITHREAD=1 \
    -DCONFIG_PIC=1 \
    -DCONFIG_INSPECTION=1 \
    "${crossArgs[@]}"
fi

echo "Building libaom..."

# Use MAKE_JOBS env var if set, otherwise auto-detect
JOBS="${MAKE_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"
cmake --build . --parallel "${JOBS}"

endTime=$(date +%s)

echo "libaom build complete, took $((endTime - startTime)) seconds"
echo "Static library: ${LIBAOM_BUILD}/libaom.a"
