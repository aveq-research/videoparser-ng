#!/usr/bin/env bash
#
# Rebase the modified ffmpeg version to a new upstream version.

FFMPEG_UPSTREAM_URL=https://github.com/FFmpeg/FFmpeg.git
FFMPEG_UPSTREAM_BRANCH=master

LOCAL_URL=https://git.aveq.info/aveq/cpp/ffmpeg.git
LOCAL_BRANCH=videoparser

cd "$(dirname "$0")/.." || exit 1

# check if git submodule is unclean
if ! git submodule status | grep -q '^[-+U]'; then
    echo "The git submodule is unclean."
    exit 1
fi

cd external/ffmpeg || exit 1

if ! git remote | grep -q '^upstream$'; then
    git remote add upstream "$FFMPEG_UPSTREAM_URL"
fi

if ! git remote | grep -q '^local$'; then
    git remote add local "$LOCAL_URL"
fi

echo "Fetching upstream changes from master ..."
git fetch upstream "$FFMPEG_UPSTREAM_BRANCH"

git checkout "$LOCAL_BRANCH"
echo "Attempting fast-forward merge ..."

git merge upstream/"$FFMPEG_UPSTREAM_BRANCH" --ff-only || (
    echo "Fast-forward merge failed. Rebasing ..."
    git rebase upstream/"$FFMPEG_UPSTREAM_BRANCH"
)
