#!/usr/bin/env bash
#
# Common functions for rebase scripts (ffmpeg and libaom)
#
# Usage: source this file from rebase-ffmpeg.sh or rebase-libaom.sh
# Required variables before sourcing:
#   PROJECT_ROOT - path to the project root
#   COMPONENT - "ffmpeg" or "libaom"
#   LOCAL_BRANCH - the local branch name (e.g., "videoparser")
#   UPSTREAM_BRANCH - the upstream branch name (e.g., "master" or "main")

# Restore function - aborts rebase and switches back to local branch
restore() {
    if git rebase --abort 2>/dev/null; then
        echo "Aborted in-progress rebase."
    fi
    git switch "$LOCAL_BRANCH" 2>/dev/null || true
}

# Ask user what to do on error
ask_user() {
    local original_head="$1"
    echo ""
    echo "ERROR: Rebase failed (likely due to conflicts)."
    echo ""
    echo "Options:"
    echo "  [m] Manual resolve - keep rebase state, exit script to fix conflicts"
    echo "  [a] Abort - restore to original state ($original_head)"
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
            echo "To abort later: git rebase --abort && git reset --hard $original_head"
            # Disable traps so we don't auto-restore on exit
            trap - ERR EXIT
            exit 1
            ;;
        *)
            echo ""
            echo "Aborting and restoring to original state..."
            restore
            git reset --hard "$original_head"
            echo "Restored $LOCAL_BRANCH to $original_head"
            trap - ERR EXIT
            exit 1
            ;;
    esac
}

# Build the component (ffmpeg or libaom)
# For libaom, also rebuilds ffmpeg since it depends on libaom
build_component() {
    echo ""
    echo "Verifying $COMPONENT build..."
    cd "$PROJECT_ROOT" || exit 1
    if ! "./util/build-${COMPONENT}.sh" --reconfigure; then
        echo ""
        echo "ERROR: $COMPONENT build failed!"
        return 1
    fi
    echo "$COMPONENT build verification passed."

    # If we updated libaom, ffmpeg needs to be rebuilt too
    if [[ "$COMPONENT" == "libaom" ]]; then
        echo ""
        echo "Rebuilding ffmpeg (depends on libaom)..."
        if ! ./util/build-ffmpeg.sh --reconfigure; then
            echo ""
            echo "ERROR: ffmpeg rebuild failed!"
            return 1
        fi
        echo "ffmpeg rebuild passed."
    fi

    return 0
}

# Build the main project and run tests
build_and_test() {
    cd "$PROJECT_ROOT" || exit 1

    echo ""
    echo "Building main project..."
    rm -rf build
    if ! ./util/build-cmake.sh; then
        echo ""
        echo "ERROR: Main project build failed!"
        return 1
    fi
    echo "Main project build passed."

    echo ""
    echo "Running tests..."
    if ! uv run test/test.py; then
        echo ""
        echo "ERROR: Tests failed!"
        return 1
    fi
    echo "Tests passed."
    return 0
}

# Prompt user and optionally push/commit changes
prompt_and_push() {
    local new_ref="$1"

    cd "$PROJECT_ROOT/external/$COMPONENT" || exit 1

    echo ""
    read -rp "Tests passed. Push $COMPONENT and commit/push main repo? [y/N]: " choice
    case "$choice" in
        y|Y)
            git switch "$UPSTREAM_BRANCH"
            echo "Attempting fast-forward merge of $UPSTREAM_BRANCH ..."
            git merge upstream/"$UPSTREAM_BRANCH" --ff-only || (
                echo "Fast-forward merge failed!"
                echo "This should not happen on $UPSTREAM_BRANCH..."
            )

            git push --force local "$LOCAL_BRANCH"
            git push local "$UPSTREAM_BRANCH"

            git switch "$LOCAL_BRANCH"

            cd "$PROJECT_ROOT" || exit 1
            git add "external/$COMPONENT"
            git commit -m "chore: update $COMPONENT to $new_ref"

            echo "Done!"
            ;;
        *)
            echo ""
            echo "Skipping push/commit. To push manually later:"
            echo "  cd external/$COMPONENT"
            echo "  git switch $UPSTREAM_BRANCH"
            echo "  git merge upstream/$UPSTREAM_BRANCH --ff-only"
            echo "  git push --force local $LOCAL_BRANCH"
            echo "  git push local $UPSTREAM_BRANCH"
            echo "  git switch $LOCAL_BRANCH"
            echo "  cd ../.."
            echo "  git add external/$COMPONENT"
            echo "  git commit -m \"chore: update $COMPONENT to $new_ref\""
            ;;
    esac
}

# Handle --continue flag
handle_continue() {
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

    if ! build_component; then
        echo "Fix the issues and re-run: $0 --continue"
        exit 1
    fi

    if ! build_and_test; then
        echo "Fix the issues and re-run: $0 --continue"
        exit 1
    fi

    cd "$PROJECT_ROOT/external/$COMPONENT" || exit 1
    local new_ref
    new_ref=$(git rev-parse --short HEAD)

    prompt_and_push "$new_ref"
    exit 0
}

# Check prerequisites before rebasing
check_prerequisites() {
    # check if the local branch exists and if there are no uncommitted changes
    if ! git show-ref --verify --quiet "refs/heads/$LOCAL_BRANCH"; then
        echo "Local branch $LOCAL_BRANCH does not exist!"
        exit 1
    fi

    if ! git diff-index --quiet HEAD --; then
        echo "There are uncommitted changes!"
        exit 1
    fi
}

# Setup remotes
setup_remotes() {
    local upstream_url="$1"
    local local_url="$2"

    if ! git remote | grep -q '^upstream$'; then
        echo "Adding remote $upstream_url ..."
        git remote add upstream "$upstream_url"
    fi

    if ! git remote | grep -q '^local$'; then
        echo "Adding remote $local_url ..."
        git remote add local "$local_url"
    fi
}

# Perform the rebase
do_rebase() {
    echo "Fetching upstream changes from $UPSTREAM_BRANCH ..."
    git fetch upstream "$UPSTREAM_BRANCH"

    git switch "$LOCAL_BRANCH"
    echo "Rebasing $LOCAL_BRANCH onto upstream/$UPSTREAM_BRANCH ..."
    git rebase upstream/"$UPSTREAM_BRANCH"

    # update the commit message, set a new date
    git commit --amend --no-edit --date "$(date -R)"
}

# Post-rebase: build, test, and optionally push
post_rebase() {
    if ! build_component; then
        echo "Fix the build issues, then run: $0 --continue"
        trap - ERR EXIT
        exit 1
    fi

    if ! build_and_test; then
        echo "Fix the issues, then run: $0 --continue"
        trap - ERR EXIT
        exit 1
    fi

    cd "$PROJECT_ROOT/external/$COMPONENT" || exit 1
    local new_ref
    new_ref=$(git rev-parse --short HEAD)

    prompt_and_push "$new_ref"
}
