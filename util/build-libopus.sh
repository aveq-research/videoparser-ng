#!/usr/bin/env bash
#
# Build a static libopus into build/libopus, for the libopus encoder of the
# shared ffmpeg build (`util/build-ffmpeg.sh --shared`). ffmpeg keeps decoding
# Opus with its own decoder.
#
# The source is downloaded from the pinned release on downloads.xiph.org and
# checked against its SHA-256 sum. To build offline, place the tarball at
# build/libopus/opus-<version>.tar.gz first.
#
# The library is built with CMake and position-independent code, so that it
# can be linked into the shared libavcodec. The optional neural network
# features (deep PLC, DRED, OSCE) stay disabled.
#
# libopus is licensed under BSD-3-Clause; the license (with its patent notes)
# is installed to share/licenses/libopus.
#
# Set VP_CROSS_HOST to cross-compile with a toolchain file from util/cross,
# e.g. VP_CROSS_HOST=x86_64-w64-mingw32 for Windows x86_64.

set -e

LIBOPUS_VERSION="1.6.1"
LIBOPUS_SHA256="6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1"
LIBOPUS_URL="https://downloads.xiph.org/releases/opus/opus-${LIBOPUS_VERSION}.tar.gz"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBOPUS_DIR="${PROJECT_ROOT}/build/libopus"

usage() {
  echo "Usage: $0 [options]"
  echo "  --prefix <dir>      install directory (default: build/libopus/install)"
  echo "  --clean             remove the previous build first"
  echo "  --help              print this message"
  exit 1
}

prefix="${LIBOPUS_DIR}/install"
clean=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      shift
      prefix="$1"
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

tarball="${LIBOPUS_DIR}/opus-${LIBOPUS_VERSION}.tar.gz"
srcDir="${LIBOPUS_DIR}/opus-${LIBOPUS_VERSION}"
buildDir="${LIBOPUS_DIR}/build"

if [[ "$clean" = true ]]; then
  rm -rf "${srcDir}" "${buildDir}" "${prefix}"
fi

mkdir -p "${LIBOPUS_DIR}"

if [[ ! -f "${tarball}" ]]; then
  echo "Downloading libopus ${LIBOPUS_VERSION}..."
  curl -fsSL -o "${tarball}.tmp" "${LIBOPUS_URL}"
  mv "${tarball}.tmp" "${tarball}"
fi

if command -v sha256sum >/dev/null; then
  actualSha256="$(sha256sum "${tarball}" | cut -d ' ' -f 1)"
else
  actualSha256="$(shasum -a 256 "${tarball}" | cut -d ' ' -f 1)"
fi
if [[ "${actualSha256}" != "${LIBOPUS_SHA256}" ]]; then
  echo "SHA-256 mismatch for ${tarball}: expected ${LIBOPUS_SHA256}, got ${actualSha256}"
  exit 1
fi

if [[ ! -d "${srcDir}" ]]; then
  tar -C "${LIBOPUS_DIR}" -xzf "${tarball}"
fi

crossArgs=()
if [[ -n "${VP_CROSS_HOST:-}" ]]; then
  toolchain="${SCRIPT_DIR}/cross/${VP_CROSS_HOST}.cmake"
  if [[ ! -f "$toolchain" ]]; then
    echo "No CMake toolchain file for VP_CROSS_HOST=${VP_CROSS_HOST} in util/cross"
    exit 1
  fi
  crossArgs=("-DCMAKE_TOOLCHAIN_FILE=${toolchain}")
  # The stack protector and fortify options would add a dependency on
  # libssp with mingw-w64
  if [[ "${VP_CROSS_HOST}" = *mingw* ]]; then
    crossArgs+=(-DOPUS_STACK_PROTECTOR=OFF -DOPUS_FORTIFY_SOURCE=OFF)
  fi
fi

# The target of the build, recorded in the build directory and in the
# install prefix. A build for another target (VP_CROSS_HOST or
# MACOSX_DEPLOYMENT_TARGET) is removed and configured again.
# util/build-ffmpeg.sh computes the same string.
target="${VP_CROSS_HOST:-native}${MACOSX_DEPLOYMENT_TARGET:+ macOS ${MACOSX_DEPLOYMENT_TARGET}}"
targetFile="${prefix}/share/libopus-target"
if [[ -d "${buildDir}" ]] && [[ "$(cat "${buildDir}/vp-target" 2>/dev/null)" != "${target}" ]]; then
  echo "Removing the libopus build for another target..."
  rm -rf "${buildDir}"
fi
rm -f "${targetFile}"

if [[ ! -f "${buildDir}/CMakeCache.txt" ]]; then
  echo "Configuring libopus..."
  cmake -S "${srcDir}" -B "${buildDir}" "${crossArgs[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_INSTALL_PREFIX=${prefix}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPUS_BUILD_SHARED_LIBRARY=OFF \
    -DOPUS_BUILD_TESTING=OFF \
    -DOPUS_BUILD_PROGRAMS=OFF \
    -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON \
    -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF \
    -DOPUS_DRED=OFF \
    -DOPUS_OSCE=OFF
fi
echo "${target}" > "${buildDir}/vp-target"

echo "Building libopus..."

# Use MAKE_JOBS env var if set, otherwise detect the CPU count
JOBS="${MAKE_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"
cmake --build "${buildDir}" --parallel "${JOBS}"
cmake --install "${buildDir}"

mkdir -p "${prefix}/share/licenses/libopus"
cp "${srcDir}/COPYING" "${prefix}/share/licenses/libopus/COPYING"
echo "${target}" > "${targetFile}"

endTime=$(date +%s)

echo "libopus ${LIBOPUS_VERSION} installed to ${prefix}, took $((endTime - startTime)) seconds"
