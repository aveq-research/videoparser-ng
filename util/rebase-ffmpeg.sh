#!/usr/bin/env bash
#
# Rebase the modified ffmpeg version to a new upstream version.

FFMPEG_UPSTREAM_URL=https://github.com/FFmpeg/FFmpeg.git
FFMPEG_UPSTREAM_BRANCH=master

LOCAL_URL=https://git.aveq.info/aveq/cpp/ffmpeg.git
LOCAL_BRANCH=videoparser

cd "$(dirname "$0")/.." || exit 1

restore() {
    git switch "$LOCAL_BRANCH"
}

trap restore EXIT INT TERM

# check if git submodule has any uncommitted changes
if ! git diff --quiet --exit-code --cached external/ffmpeg; then
    echo "There are uncommitted changes in ffmpeg submodule. Please commit or stash them first."
    exit 1
fi

cd external/ffmpeg || exit 1

if ! git remote | grep -q '^upstream$'; then
    echp "Addomg remote $FFMPEG_UPSTREAM_URL ..."
    git remote add upstream "$FFMPEG_UPSTREAM_URL"
fi

if ! git remote | grep -q '^local$'; then
    echp "Addomg remote $LOCAL_URL ..."
    git remote add local "$LOCAL_URL"
fi

echo "Fetching upstream changes from master ..."
git fetch upstream "$FFMPEG_UPSTREAM_BRANCH"

git switch "$LOCAL_BRANCH"
echo "Attempting fast-forward merge of $LOCAL_BRANCH ..."
git merge upstream/"$FFMPEG_UPSTREAM_BRANCH" --ff-only || (
    echo "Fast-forward merge failed. Rebasing ..."
    git rebase upstream/"$FFMPEG_UPSTREAM_BRANCH"
)

git switch "$FFMPEG_UPSTREAM_BRANCH"
echo "Attempting fast-forward merge of $FFMPEG_UPSTREAM_BRANCH ..."
git merge upstream/"$FFMPEG_UPSTREAM_BRANCH" --ff-only || (
    echo "Fast-forward merge failed!"
    echo "This should not happen on master..."
)

# push the changes to the local remote
git push local "$LOCAL_BRANCH"

# also push the master branch to the local remote
git push local "$FFMPEG_UPSTREAM_BRANCH"

# restore the original branch
git switch "$LOCAL_BRANCH"
