#!/usr/bin/env bash

set -e

cd "$(dirname "$0")/../test" || exit 1

# Generate test videos for H.264, H.265, VP9, and AV1
for encoder in libx264 libx265 libvpx-vp9 libaom-av1; do
  echo "Generating test video for $encoder"
  ffmpeg \
    -y \
    -f lavfi \
    -i testsrc=duration=10:size=320x240:rate=30 \
    -c:v "$encoder" \
    -pix_fmt yuv420p \
    -an \
    "test-$encoder.mp4"
done

# Generate MPEG-2 test videos in MPEG-TS and MPEG-PS containers
for ext in ts mpg; do
  echo "Generating test video for mpeg2video ($ext)"
  ffmpeg \
    -y \
    -f lavfi \
    -i testsrc=duration=10:size=320x240:rate=30 \
    -c:v mpeg2video \
    -bf 2 \
    -q:v 10 \
    -pix_fmt yuv420p \
    -an \
    "test-mpeg2video.$ext"
done

echo "Done"
