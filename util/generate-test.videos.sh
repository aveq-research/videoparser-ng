#!/usr/bin/env bash

set -e

cd "$(dirname "$0")/../test" || exit 1

# Generate test videos for H.264, H.265, and VP9
for encoder in libx264 libx265 libvpx-vp9; do
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

echo "Done"
