#!/usr/bin/env bash
#
# Print the changes between a tag and the previous tag as Markdown,
# grouped by conventional-commit type.
#
# Usage: util/release-notes.sh <tag> [<repository URL>]

set -euo pipefail

tag="${1:?usage: $0 <tag> [<repository URL>]}"
repo_url="${2:-https://github.com/aveq-research/videoparser-ng}"

prev_tag="$(git describe --tags --abbrev=0 "${tag}^" 2>/dev/null || true)"
if [[ -n "$prev_tag" ]]; then
  range="${prev_tag}..${tag}"
else
  range="$tag"
fi

declare -A items
groups=("Features" "Bug Fixes" "Performance" "Documentation" "Build/CI" "Other")

while IFS=$'\t' read -r hash subject; do
  # Skip version bump commits
  [[ "$subject" =~ ^bump\ version ]] && continue

  type="${subject%%[(:!]*}"
  case "$type" in
    feat) group="Features" ;;
    fix) group="Bug Fixes" ;;
    perf) group="Performance" ;;
    docs) group="Documentation" ;;
    build | ci) group="Build/CI" ;;
    *) group="Other" ;;
  esac

  # Strip the type prefix if the subject follows the conventional format
  if [[ "$subject" =~ ^[a-z]+(\([^\)]*\))?!?:\ (.*)$ ]]; then
    text="${BASH_REMATCH[2]}"
    scope="${BASH_REMATCH[1]}"
    [[ -n "$scope" ]] && text="**${scope:1:-1}:** ${text}"
  else
    text="$subject"
  fi

  items[$group]+="- ${text} ([${hash:0:7}](${repo_url}/commit/${hash}))"$'\n'
done < <(git log --no-merges --format='%H%x09%s' "$range")

echo "## Changes"
echo
if [[ -n "$prev_tag" ]]; then
  echo "Changes since [${prev_tag}](${repo_url}/compare/${prev_tag}...${tag})."
  echo
fi
for group in "${groups[@]}"; do
  [[ -z "${items[$group]:-}" ]] && continue
  echo "### ${group}"
  echo
  printf '%s' "${items[$group]}"
  echo
done
