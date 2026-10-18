#!/usr/bin/env bash
#
# Generate interlaced and soft-telecine MPEG-TS test videos in test/.
#
# Needs ffmpeg with libx264, and mpeg2enc from mjpegtools for the soft-telecine
# clip (set MPEG2ENC to its path if it is not in the PATH). Each clip has 2
# seconds of a 320x240 test pattern:
#
# - test-mpeg2video-tff.ts, test-mpeg2video-bff.ts: MPEG-2, 25 interlaced
#   frames per second, top or bottom field first
# - test-libx264-tff.ts, test-libx264-bff.ts: H.264 (MBAFF), the same
# - test-mpeg2video-pulldown.ts: MPEG-2, 320x224, 23.976 progressive frames per
#   second with repeat_first_field for display at 29.97 frames per second (soft
#   telecine)

set -e

cd "$(dirname "$0")/../test" || exit 1

MPEG2ENC="${MPEG2ENC:-mpeg2enc}"

# 50 fps source, woven into 25 fps frames of two fields
interlaced() {
  local scan="$1"
  shift
  ffmpeg -y -v error \
    -f lavfi -i testsrc2=duration=2:size=320x240:rate=50 \
    -vf "interlace=scan=${scan}:lowpass=linear" \
    -flags +ildct+ilme \
    -bf 2 \
    -an \
    "$@"
}

echo "Generating interlaced MPEG-2 and H.264 test videos"
interlaced tff -c:v mpeg2video -alternate_scan 1 -field_order tt -q:v 10 test-mpeg2video-tff.ts
interlaced bff -c:v mpeg2video -alternate_scan 1 -field_order bb -q:v 10 test-mpeg2video-bff.ts
interlaced tff -c:v libx264 -field_order tt test-libx264-tff.ts
interlaced bff -c:v libx264 -field_order bb test-libx264-bff.ts

echo "Generating soft-telecine MPEG-2 test video"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
# mpeg2enc: generic MPEG-2 (-f 3), progressive frames (-I 0), 3:2 pulldown
# (-p), display frame rate 29.97 (-F 4)
ffmpeg -v error \
  -f lavfi -i testsrc2=duration=2:size=320x224:rate=24000/1001 \
  -pix_fmt yuv420p -strict -1 -f yuv4mpegpipe - |
  "$MPEG2ENC" -v 0 -f 3 -I 0 -p -n n -F 4 -b 500 -V 224 -a 1 -g 12 -G 12 \
    -R 2 -o "$tmp/pulldown.m2v"
ffmpeg -y -v error -fflags +genpts -i "$tmp/pulldown.m2v" -c copy -f mpegts \
  test-mpeg2video-pulldown.ts

echo "Done"
