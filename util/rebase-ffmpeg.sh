#!/usr/bin/env bash
#
# Rebase the modified ffmpeg version to a new upstream version.

set -e

FFMPEG_UPSTREAM_URL=https://github.com/FFmpeg/FFmpeg.git
FFMPEG_UPSTREAM_BRANCH=master

LOCAL_URL=https://github.com/aveq-research/ffmpeg.git
LOCAL_BRANCH=videoparser

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT/external/ffmpeg" || exit 1

# Handle --continue flag for resuming after manual conflict resolution
if [[ "${1:-}" == "--continue" ]]; then
    echo "Continuing after manual conflict resolution..."

    # Check if we're still in a rebase - if so, tell user to finish it first
    if [[ -d .git/rebase-merge ]] || [[ -d .git/rebase-apply ]]; then
        echo "Rebase still in progress. Please complete it first:"
        echo "  1. Fix remaining conflicts"
        echo "  2. git add <fixed-files>"
        echo "  3. git rebase --continue"
        echo "  4. Then re-run: $0 --continue"
        exit 1
    fi

    echo "Rebase complete."

    git switch "$LOCAL_BRANCH"
    git commit --amend --no-edit --date "$(date -R)"

    # Verify build works before pushing
    echo ""
    echo "Verifying ffmpeg build..."
    cd "$PROJECT_ROOT" || exit 1
    if ! ./util/build-ffmpeg.sh --reconfigure; then
        echo ""
        echo "ERROR: Build failed! Fix the issues and re-run: $0 --continue"
        exit 1
    fi
    cd "$PROJECT_ROOT/external/ffmpeg" || exit 1
    echo "Build verification passed."
    echo ""

    echo "Pushing to remote..."
    git switch "$FFMPEG_UPSTREAM_BRANCH"
    git merge upstream/"$FFMPEG_UPSTREAM_BRANCH" --ff-only || true

    git push --force local "$LOCAL_BRANCH"
    git push local "$FFMPEG_UPSTREAM_BRANCH"

    git switch "$LOCAL_BRANCH"

    cd "$PROJECT_ROOT" || exit 1
    git add external/ffmpeg
    git commit -m "Update ffmpeg to latest version"

    echo "Done!"
    exit 0
fi

# Store original state for recovery
ORIGINAL_HEAD=$(git rev-parse HEAD)

restore() {
    if git rebase --abort 2>/dev/null; then
        echo "Aborted in-progress rebase."
    fi
    git switch "$LOCAL_BRANCH" 2>/dev/null || true
}

ask_user() {
    echo ""
    echo "ERROR: Rebase failed (likely due to conflicts)."
    echo ""
    echo "Options:"
    echo "  [m] Manual resolve - keep rebase state, exit script to fix conflicts"
    echo "  [a] Abort - restore to original state ($ORIGINAL_HEAD)"
    echo ""
    read -rp "Choose [m/a]: " choice
    case "$choice" in
        m|M)
            echo ""
            echo "Exiting for manual resolution. When done:"
            echo "  1. Fix conflicts in the files listed above"
            echo "  2. git add <fixed-files>"
            echo "  3. git rebase --continue"
            echo "  4. Re-run this script with --continue (or manually push)"
            echo ""
            echo "To abort later: git rebase --abort && git reset --hard $ORIGINAL_HEAD"
            # Disable traps so we don't auto-restore on exit
            trap - ERR EXIT
            exit 1
            ;;
        *)
            echo ""
            echo "Aborting and restoring to original state..."
            restore
            git reset --hard "$ORIGINAL_HEAD"
            echo "Restored $LOCAL_BRANCH to $ORIGINAL_HEAD"
            trap - ERR EXIT
            exit 1
            ;;
    esac
}

trap ask_user ERR
trap restore EXIT

# check if the local branch exists and if there are no uncommitted changes
if ! git show-ref --verify --quiet "refs/heads/$LOCAL_BRANCH"; then
    echo "Local branch $LOCAL_BRANCH does not exist!"
    exit 1
fi

if ! git diff-index --quiet HEAD --; then
    echo "There are uncommitted changes!"
    exit 1
fi

if ! git remote | grep -q '^upstream$'; then
    echo "Adding remote $FFMPEG_UPSTREAM_URL ..."
    git remote add upstream "$FFMPEG_UPSTREAM_URL"
fi

if ! git remote | grep -q '^local$'; then
    echo "Adding remote $LOCAL_URL ..."
    git remote add local "$LOCAL_URL"
fi

echo "Fetching upstream changes from master ..."
git fetch upstream "$FFMPEG_UPSTREAM_BRANCH"

git switch "$LOCAL_BRANCH"
echo "Rebasing $LOCAL_BRANCH onto upstream/$FFMPEG_UPSTREAM_BRANCH ..."
git rebase upstream/"$FFMPEG_UPSTREAM_BRANCH"

# update the commit message, set a new date
git commit --amend --no-edit --date "$(date -R)"

# Verify build works before pushing
echo ""
echo "Verifying ffmpeg build..."
cd "$PROJECT_ROOT" || exit 1
if ! ./util/build-ffmpeg.sh --reconfigure; then
    echo ""
    echo "ERROR: Build failed after rebase!"
    echo "Fix the build issues, then run: $0 --continue"
    trap - ERR EXIT
    exit 1
fi
cd "$PROJECT_ROOT/external/ffmpeg" || exit 1
echo "Build verification passed."
echo ""

git switch "$FFMPEG_UPSTREAM_BRANCH"
echo "Attempting fast-forward merge of $FFMPEG_UPSTREAM_BRANCH ..."
git merge upstream/"$FFMPEG_UPSTREAM_BRANCH" --ff-only || (
    echo "Fast-forward merge failed!"
    echo "This should not happen on master..."
)

# push the changes to the local remote
git push --force local "$LOCAL_BRANCH"

# also push the master branch to the local remote
git push local "$FFMPEG_UPSTREAM_BRANCH"

# restore the original branch
git switch "$LOCAL_BRANCH"

# update the submodule
cd "$PROJECT_ROOT" || exit 1
git add external/ffmpeg
git commit -m "Update ffmpeg to latest version"
