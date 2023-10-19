#!/usr/bin/env bash
#
# Push any existing changes on the ffmpeg branch

set -e

LOCAL_URL=https://git.aveq.info/aveq/cpp/ffmpeg.git
LOCAL_BRANCH=videoparser

cd "$(dirname "$0")/.." || exit 1

restore() {
    git switch "$LOCAL_BRANCH" || true
}

trap restore EXIT TERM

cd external/ffmpeg || exit 1

# check if the local branch exists and if there are no uncommitted changes
if ! git show-ref --verify --quiet "refs/heads/$LOCAL_BRANCH"; then
    echo "Local branch $LOCAL_BRANCH does not exist!"
    exit 1
fi

# check if last commit contains "videoparser"
if ! git log -1 --pretty=%B | grep -q videoparser; then
    echo "Last commit does not contain 'videoparser' string!"
    exit 1
fi

# show changes to be pushed and ask for confirmation
changes=$(git --no-pager diff --stat --cached local/"$LOCAL_BRANCH")

if [[ -z "$changes" ]]; then
    echo "No changes to be pushed!"
    exit 0
fi

echo "Changes to be pushed:"
echo "$changes"
echo "Push changes to $LOCAL_URL:$LOCAL_BRANCH? [y/N]"
read -r answer
if [[ "$answer" != "y" ]]; then
    echo "Aborting..."
    exit 1
fi

git add .
gc --amend --no-edit --date "$(date)"
git push -u local "$LOCAL_BRANCH" --force
