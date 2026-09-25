#!/usr/bin/env bash
#
# Build a static libvmaf with its built-in models into build/libvmaf, for the
# libvmaf filter of the shared ffmpeg build (`util/build-ffmpeg.sh --shared`).
#
# The source is downloaded from the pinned release on GitHub and checked
# against its SHA-256 sum. To build offline, place the tarball at
# build/libvmaf/vmaf-<version>.tar.gz first.
#
# The built-in models are embedded with xxd. If xxd is not installed,
# util/xxd-fallback.sh is used instead.
#
# libvmaf is licensed under BSD-2-Clause-Patent; the license is installed to
# share/licenses/libvmaf.

set -e

LIBVMAF_VERSION="3.2.1"
LIBVMAF_SHA256="5df7386911bc15fd1ca783132528748d219768ae4fc5f8e0b61184f041648092"
LIBVMAF_URL="https://github.com/Netflix/vmaf/archive/refs/tags/v${LIBVMAF_VERSION}.tar.gz"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBVMAF_DIR="${PROJECT_ROOT}/build/libvmaf"

usage() {
  echo "Usage: $0 [options]"
  echo "  --prefix <dir>      install directory (default: build/libvmaf/install)"
  echo "  --clean             remove the previous build first"
  echo "  --help              print this message"
  exit 1
}

prefix="${LIBVMAF_DIR}/install"
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

for tool in meson ninja; do
  if ! command -v "${tool}" >/dev/null; then
    echo "${tool} not found, it is needed to build libvmaf"
    exit 1
  fi
done

startTime=$(date +%s)

tarball="${LIBVMAF_DIR}/vmaf-${LIBVMAF_VERSION}.tar.gz"
srcDir="${LIBVMAF_DIR}/vmaf-${LIBVMAF_VERSION}"
buildDir="${LIBVMAF_DIR}/build"

if [[ "$clean" = true ]]; then
  rm -rf "${srcDir}" "${buildDir}" "${prefix}"
fi

mkdir -p "${LIBVMAF_DIR}"

if [[ ! -f "${tarball}" ]]; then
  echo "Downloading libvmaf ${LIBVMAF_VERSION}..."
  curl -fsSL -o "${tarball}.tmp" "${LIBVMAF_URL}"
  mv "${tarball}.tmp" "${tarball}"
fi

if command -v sha256sum >/dev/null; then
  actualSha256="$(sha256sum "${tarball}" | cut -d ' ' -f 1)"
else
  actualSha256="$(shasum -a 256 "${tarball}" | cut -d ' ' -f 1)"
fi
if [[ "${actualSha256}" != "${LIBVMAF_SHA256}" ]]; then
  echo "SHA-256 mismatch for ${tarball}: expected ${LIBVMAF_SHA256}, got ${actualSha256}"
  exit 1
fi

if [[ ! -d "${srcDir}" ]]; then
  tar -C "${LIBVMAF_DIR}" -xzf "${tarball}"
fi

# meson looks for xxd in PATH
if ! command -v xxd >/dev/null; then
  echo "xxd not found, using util/xxd-fallback.sh"
  mkdir -p "${LIBVMAF_DIR}/bin"
  ln -sf "${SCRIPT_DIR}/xxd-fallback.sh" "${LIBVMAF_DIR}/bin/xxd"
  export PATH="${LIBVMAF_DIR}/bin:${PATH}"
fi

if [[ ! -f "${buildDir}/build.ninja" ]]; then
  echo "Configuring libvmaf..."
  meson setup "${buildDir}" "${srcDir}/libvmaf" \
    --prefix "${prefix}" \
    --libdir lib \
    --buildtype release \
    --default-library static \
    -Db_staticpic=true \
    -Denable_tests=false \
    -Denable_docs=false \
    -Denable_tools=false \
    -Dbuilt_in_models=true \
    -Denable_float=false
fi

# Without built-in models, `model=version=...` in the libvmaf filter fails
# at run time, so check that they are compiled in
if ! grep -q "VMAF_BUILT_IN_MODELS 1" "${buildDir}/src/config.h"; then
  echo "libvmaf was configured without built-in models"
  exit 1
fi

echo "Building libvmaf..."

# Use MAKE_JOBS env var if set, otherwise use nproc
JOBS="${MAKE_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"
meson compile -C "${buildDir}" -j "${JOBS}"
meson install -C "${buildDir}" --quiet

# libvmaf contains C++ code (libsvm), which its pkg-config file does not
# list, so add the C++ runtime for static linking
if [[ "$(uname)" = Darwin ]]; then
  cxxLib="-lc++"
else
  cxxLib="-lstdc++"
fi
sed -i.bak "s/^Libs: \(.*\)/Libs: \1 ${cxxLib}/" "${prefix}/lib/pkgconfig/libvmaf.pc"
rm -f "${prefix}/lib/pkgconfig/libvmaf.pc.bak"

mkdir -p "${prefix}/share/licenses/libvmaf"
cp "${srcDir}/LICENSE" "${prefix}/share/licenses/libvmaf/LICENSE"

endTime=$(date +%s)

echo "libvmaf ${LIBVMAF_VERSION} installed to ${prefix}, took $((endTime - startTime)) seconds"
