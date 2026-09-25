#!/usr/bin/env bash
#
# Build a static OpenCV from external/opencv against the shared ffmpeg
# libraries from `util/build-ffmpeg.sh --shared`.
#
# Only the modules needed for reading and analyzing video are built
# (core, imgproc, imgcodecs, videoio). OpenCV is pinned to a commit on its
# 4.x development branch, since releases up to 4.14 do not compile against
# ffmpeg's current API. The patches in util/patches/opencv are applied to the
# submodule before building.
#
# Intel IPP (ippicv) is linked in by default. Its license (Intel Simplified
# Software License) allows redistribution without modification, but the
# copyright notice and terms must be shipped with the software and its
# documentation. The license is installed to share/licenses/opencv4.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OPENCV_SRC="${PROJECT_ROOT}/external/opencv"
OPENCV_BUILD="${PROJECT_ROOT}/build/opencv"
FFMPEG_PREFIX="${PROJECT_ROOT}/build/ffmpeg-shared/install"

usage() {
  echo "Usage: $0 [options]"
  echo "  --ffmpeg-prefix <dir>  shared ffmpeg install (default: build/ffmpeg-shared/install)"
  echo "  --build-dir <dir>      build directory (default: build/opencv)"
  echo "  --prefix <dir>         install directory (default: <build dir>/install)"
  echo "  --without-ipp          build without Intel IPP"
  echo "  --clean                remove the previous build first"
  echo "  --help                 print this message"
  exit 1
}

prefix=""
withIpp=ON
clean=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ffmpeg-prefix)
      shift
      FFMPEG_PREFIX="$1"
      ;;
    --build-dir)
      shift
      OPENCV_BUILD="$1"
      ;;
    --prefix)
      shift
      prefix="$1"
      ;;
    --without-ipp)
      withIpp=OFF
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

prefix="${prefix:-${OPENCV_BUILD}/install}"

if [[ ! -f "${OPENCV_SRC}/CMakeLists.txt" ]]; then
  echo "OpenCV source not found, run: git submodule update --init external/opencv"
  exit 1
fi

if [[ ! -f "${FFMPEG_PREFIX}/lib/pkgconfig/libavcodec.pc" ]]; then
  echo "Shared ffmpeg not found in ${FFMPEG_PREFIX}, building it first..."
  "${SCRIPT_DIR}/build-ffmpeg.sh" --shared --prefix "${FFMPEG_PREFIX}"
fi

# Apply our patches to the OpenCV source, unless already applied
for patch in "${SCRIPT_DIR}"/patches/opencv/*.patch; do
  if git -C "${OPENCV_SRC}" apply --reverse --check "${patch}" 2>/dev/null; then
    continue
  fi
  echo "Applying $(basename "${patch}")"
  git -C "${OPENCV_SRC}" apply "${patch}"
done

startTime=$(date +%s)

if [[ "$clean" = true ]]; then
  rm -rf "${OPENCV_BUILD}/build"
fi

mkdir -p "${OPENCV_BUILD}/build"
cd "${OPENCV_BUILD}/build"

export PKG_CONFIG_PATH="${FFMPEG_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

cmakeFlags=(
  -DCMAKE_BUILD_TYPE=Release
  "-DCMAKE_INSTALL_PREFIX=${prefix}"
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DBUILD_SHARED_LIBS=OFF
  -DBUILD_LIST=core,imgproc,imgcodecs,videoio
  -DOPENCV_GENERATE_PKGCONFIG=ON
  # video input only through our ffmpeg
  -DWITH_FFMPEG=ON
  -DOPENCV_FFMPEG_USE_FIND_PACKAGE=OFF
  -DWITH_GSTREAMER=OFF
  -DWITH_V4L=OFF
  -DWITH_1394=OFF
  # no GUI, GPU or extra runtimes
  -DWITH_GTK=OFF
  -DWITH_QT=OFF
  -DWITH_OPENCL=OFF
  -DWITH_VA=OFF
  -DWITH_VA_INTEL=OFF
  -DWITH_ADE=OFF
  -DWITH_PROTOBUF=OFF
  -DWITH_EIGEN=OFF
  -DWITH_LAPACK=OFF
  "-DWITH_IPP=${withIpp}"
  # bundled JPEG and PNG only
  -DBUILD_JPEG=ON
  -DBUILD_PNG=ON
  -DWITH_TIFF=OFF
  -DWITH_WEBP=OFF
  -DWITH_OPENJPEG=OFF
  -DWITH_JASPER=OFF
  -DWITH_OPENEXR=OFF
  -DWITH_AVIF=OFF
  -DWITH_JPEGXL=OFF
  -DWITH_IMGCODEC_HDR=OFF
  # no tests, apps or bindings
  -DBUILD_TESTS=OFF
  -DBUILD_PERF_TESTS=OFF
  -DBUILD_EXAMPLES=OFF
  -DBUILD_DOCS=OFF
  -DBUILD_opencv_apps=OFF
  -DBUILD_JAVA=OFF
  -DBUILD_opencv_python3=OFF
)

cmake "${OPENCV_SRC}" "${cmakeFlags[@]}"

# Use MAKE_JOBS env var if set, otherwise use nproc
JOBS="${MAKE_JOBS:-$(nproc)}"
cmake --build . --parallel "${JOBS}"
cmake --install .

endTime=$(date +%s)

echo "OpenCV installed to ${prefix}, took $((endTime - startTime)) seconds"
