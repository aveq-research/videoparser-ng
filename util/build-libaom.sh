#!/usr/bin/env bash
#
# Build libaom in the external/libaom directory

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBAOM_SRC="${SCRIPT_DIR}/../external/libaom"
LIBAOM_BUILD="${LIBAOM_SRC}/aom_build"

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
    -DCONFIG_INSPECTION=1
fi

echo "Building libaom..."

cmake --build . --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

endTime=$(date +%s)

echo "libaom build complete, took $((endTime - startTime)) seconds"
echo "Static library: ${LIBAOM_BUILD}/libaom.a"
