#!/usr/bin/env bash
#
# Rebase the modified libaom version to a new upstream version.

set -e

# Configuration (used by rebase-common.sh)
UPSTREAM_URL=https://aomedia.googlesource.com/aom
UPSTREAM_BRANCH=main
LOCAL_URL=https://github.com/aveq-research/libaom.git
LOCAL_BRANCH=videoparser
COMPONENT=libaom

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Source common functions
# shellcheck source=rebase-common.sh
source "$PROJECT_ROOT/util/rebase-common.sh"

cd "$PROJECT_ROOT/external/$COMPONENT" || exit 1

# Handle --continue flag
if [[ "${1:-}" == "--continue" ]]; then
    handle_continue
fi

# Store original state for recovery
ORIGINAL_HEAD=$(git rev-parse HEAD)

trap 'ask_user "$ORIGINAL_HEAD"' ERR
trap restore EXIT

check_prerequisites
setup_remotes "$UPSTREAM_URL" "$LOCAL_URL"
do_rebase
post_rebase
