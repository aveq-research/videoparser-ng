#!/usr/bin/env bash
#
# Show the diff between the local and upstream ffmpeg branch

set -e

cd "$(dirname "$0")/.." || exit 1

git -C external/ffmpeg diff master
