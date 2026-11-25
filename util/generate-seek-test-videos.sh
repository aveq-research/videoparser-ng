#!/usr/bin/env bash
#
# Generate short-GOP test videos for seeking and the key frame scan in
# test/seek/.
#
# Needs ffmpeg with libx264, libx265, libvpx-vp9 and libaom. Each clip has
# 3 seconds of a 320x240 test pattern at 25 frames per second with a key frame
# every 12 frames (MPEG-2: every 12 or 15 frames), in several containers:
#
# - h264-closed.*: H.264 with B-frames and closed GOPs (IDR frames)
# - h264-open.*: H.264 with open GOPs (I-frames with a recovery point, whose
#   leading B-frames refer to the GOP before)
# - h264-tff.ts: H.264, interlaced (MBAFF), top field first
# - hevc-closed.*: HEVC with closed GOPs (IDR frames)
# - hevc-open.*: HEVC with open GOPs (CRA frames with leading pictures)
# - vp9.*, av1.*: VP9 and AV1 with fixed key frame intervals
# - mpeg2-closed.ts, mpeg2-open.*: MPEG-2 with closed or open GOPs
# - h264-paff-twice.264: test/test-h264-paff.264 twice, for two IDR frames
#   with field pictures

set -e

cd "$(dirname "$0")/../test" || exit 1
mkdir -p seek
cd seek

source_video() {
  ffmpeg -y -v error \
    -f lavfi -i testsrc2=duration=3:size=320x240:rate=25 \
    -pix_fmt yuv420p -an "$@"
}

x264_closed="keyint=12:min-keyint=12:scenecut=0:bframes=2"
x264_open="keyint=12:min-keyint=12:scenecut=0:bframes=2:open-gop=1"
x265_closed="keyint=12:min-keyint=12:scenecut=0:bframes=2:open-gop=0:log-level=error"
x265_open="keyint=12:min-keyint=12:scenecut=0:bframes=2:open-gop=1:log-level=error"

echo "Generating H.264 test videos"
for ext in mp4 ts mkv 264; do
  source_video -c:v libx264 -x264-params "$x264_closed" "h264-closed.$ext"
done
for ext in mp4 ts 264; do
  source_video -c:v libx264 -x264-params "$x264_open" "h264-open.$ext"
done
# 50 fps source, woven into 25 fps frames of two fields
ffmpeg -y -v error \
  -f lavfi -i testsrc2=duration=3:size=320x240:rate=50 \
  -vf "interlace=scan=tff:lowpass=linear" -flags +ildct+ilme \
  -c:v libx264 -x264-params "$x264_closed" -field_order tt -an h264-tff.ts

echo "Generating HEVC test videos"
for ext in mp4 265; do
  source_video -c:v libx265 -x265-params "$x265_closed" "hevc-closed.$ext"
done
for ext in mp4 ts 265; do
  source_video -c:v libx265 -x265-params "$x265_open" "hevc-open.$ext"
done

echo "Generating VP9 and AV1 test videos"
for ext in webm mkv; do
  source_video -c:v libvpx-vp9 -g 12 -keyint_min 12 -deadline realtime \
    -cpu-used 8 -b:v 300k "vp9.$ext"
done
for ext in mp4 mkv; do
  source_video -c:v libaom-av1 -g 12 -keyint_min 12 -cpu-used 8 \
    -usage realtime -b:v 300k "av1.$ext"
done

echo "Generating MPEG-2 test videos"
source_video -c:v mpeg2video -g 12 -bf 2 -flags +cgop -sc_threshold 1000000000 \
  -q:v 8 mpeg2-closed.ts
for ext in ts mpg m2v mp4; do
  source_video -c:v mpeg2video -g 15 -bf 2 -sc_threshold 1000000000 \
    -q:v 8 "mpeg2-open.$ext"
done

cat ../test-h264-paff.264 ../test-h264-paff.264 >h264-paff-twice.264

echo "Done"
