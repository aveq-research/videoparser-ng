#!/usr/bin/env bash
#
# Build a static libvpx with only the VP9 encoder into build/libvpx, for the
# libvpx_vp9 encoder of the shared ffmpeg build (`util/build-ffmpeg.sh
# --shared`). The VP8 codec and the VP9 decoder are not built, so ffmpeg keeps
# decoding VP9 with its own (patched) decoder.
#
# The source is downloaded from the pinned release tag on GitHub and checked
# against its SHA-256 sum. To build offline, place the tarball at
# build/libvpx/libvpx-<version>.tar.gz first.
#
# The library is built with position-independent code, so that it can be
# linked into the shared libavcodec. libvpx's configure needs perl and diff
# (GNU diffutils, not the BusyBox one), and on x86_64 nasm or yasm.
#
# libvpx is licensed under BSD-3-Clause with an additional patent grant; the
# license is installed to share/licenses/libvpx.
#
# Set VP_CROSS_HOST to cross-compile, e.g. VP_CROSS_HOST=x86_64-w64-mingw32 for
# Windows x86_64 (with x86_64-w64-mingw32-gcc in PATH).

set -e

LIBVPX_VERSION="1.17.0"
LIBVPX_SHA256="1020f184046187baa2985dbde38e0691f49c44088bca7a1842b0236c6081dc0a"
LIBVPX_URL="https://github.com/webmproject/libvpx/archive/refs/tags/v${LIBVPX_VERSION}.tar.gz"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBVPX_DIR="${PROJECT_ROOT}/build/libvpx"

usage() {
  echo "Usage: $0 [options]"
  echo "  --prefix <dir>      install directory (default: build/libvpx/install)"
  echo "  --clean             remove the previous build first"
  echo "  --help              print this message"
  exit 1
}

prefix="${LIBVPX_DIR}/install"
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

for tool in perl diff make; do
  if ! command -v "${tool}" >/dev/null; then
    echo "${tool} not found, it is needed to build libvpx"
    exit 1
  fi
done

startTime=$(date +%s)

tarball="${LIBVPX_DIR}/libvpx-${LIBVPX_VERSION}.tar.gz"
srcDir="${LIBVPX_DIR}/libvpx-${LIBVPX_VERSION}"
buildDir="${LIBVPX_DIR}/build"

if [[ "$clean" = true ]]; then
  rm -rf "${srcDir}" "${buildDir}" "${prefix}"
fi

mkdir -p "${LIBVPX_DIR}"

if [[ ! -f "${tarball}" ]]; then
  echo "Downloading libvpx ${LIBVPX_VERSION}..."
  curl -fsSL -o "${tarball}.tmp" "${LIBVPX_URL}"
  mv "${tarball}.tmp" "${tarball}"
fi

if command -v sha256sum >/dev/null; then
  actualSha256="$(sha256sum "${tarball}" | cut -d ' ' -f 1)"
else
  actualSha256="$(shasum -a 256 "${tarball}" | cut -d ' ' -f 1)"
fi
if [[ "${actualSha256}" != "${LIBVPX_SHA256}" ]]; then
  echo "SHA-256 mismatch for ${tarball}: expected ${LIBVPX_SHA256}, got ${actualSha256}"
  exit 1
fi

if [[ ! -d "${srcDir}" ]]; then
  tar -C "${LIBVPX_DIR}" -xzf "${tarball}"
fi

# Target of libvpx's configure; the native target is detected
crossArgs=()
if [[ -n "${VP_CROSS_HOST:-}" ]]; then
  case "${VP_CROSS_HOST}" in
    x86_64-w64-mingw32)
      crossArgs=(--target=x86_64-win64-gcc)
      ;;
    *)
      echo "Unsupported VP_CROSS_HOST: ${VP_CROSS_HOST}"
      exit 1
      ;;
  esac
  export CROSS="${VP_CROSS_HOST}-"
fi

if [[ ! -f "${buildDir}/config.mk" ]]; then
  echo "Configuring libvpx..."
  mkdir -p "${buildDir}"
  (
    cd "${buildDir}"
    "${srcDir}/configure" "${crossArgs[@]}" \
      "--prefix=${prefix}" \
      --enable-static \
      --disable-shared \
      --enable-pic \
      --disable-vp8 \
      --enable-vp9-encoder \
      --disable-vp9-decoder \
      --enable-multithread \
      --enable-runtime-cpu-detect \
      --disable-examples \
      --disable-tools \
      --disable-docs \
      --disable-unit-tests \
      --disable-install-docs \
      --disable-install-bins \
      --disable-webm-io \
      --disable-libyuv
  )
fi

echo "Building libvpx..."

# Use MAKE_JOBS env var if set, otherwise detect the CPU count
JOBS="${MAKE_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"
make -C "${buildDir}" "-j${JOBS}"
make -C "${buildDir}" install

mkdir -p "${prefix}/share/licenses/libvpx"
cp "${srcDir}/LICENSE" "${srcDir}/PATENTS" "${prefix}/share/licenses/libvpx/"

endTime=$(date +%s)

echo "libvpx ${LIBVPX_VERSION} installed to ${prefix}, took $((endTime - startTime)) seconds"
