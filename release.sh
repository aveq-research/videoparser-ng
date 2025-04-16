#!/bin/bash
#
# Bump the version, run auto-changelog, and push to Git
#
# Based on:
# - https://gist.github.com/pete-otaqui/4188238
# - https://gist.github.com/mareksuscak/1f206fbc3bb9d97dec9c

set -e

GREEN="\033[0;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
WHITE="\033[1;37m"
RESET="\033[0m"

LATEST_HASH=$(git log --pretty=format:'%h' -n 1)

QUESTION_FLAG="${GREEN}?"
NOTICE_FLAG="${CYAN}❯"

PUSHING_MSG="${NOTICE_FLAG} Pushing new version to the ${WHITE}origin${RESET}..."

usage() {
  echo "Usage: $0 [-h] [-f]"
  echo
  echo "  -h  Show this help message"
  echo "  -f  Force the version bump"
}

FORCE=0

while getopts "hf" opt; do
  case $opt in
    h)
      usage
      exit 0
      ;;
    f)
      FORCE=1
      ;;
    \?)
      usage
      exit 1
      ;;
  esac
done

if command -v ggrep > /dev/null; then
  GREP="ggrep"
else
  GREP="grep"
fi

if ! command -v uvx > /dev/null; then
  echo "uvx is not installed. Please install 'uv' first from https://docs.astral.sh/uv/getting-started/installation/."
  exit 1
fi

VERSION_FILE="VERSION"
VERSION_HEADER_FILE="VideoParser/VideoParser.h"

if [[ -n "$(git status --porcelain)" ]] && [ $FORCE -eq 0 ]; then
    echo "Working directory is dirty. Please commit all changes before running this script, or use -f to force the version bump."
    exit 1
fi

CURRENT_VERSION=$(cat $VERSION_FILE)

BASE_LIST=($(echo $CURRENT_VERSION | tr '.' ' '))
V_MAJOR=${BASE_LIST[0]}
V_MINOR=${BASE_LIST[1]}
V_PATCH=${BASE_LIST[2]}
echo -e "${NOTICE_FLAG} Current version: ${WHITE}$CURRENT_VERSION"
echo -e "${NOTICE_FLAG} Latest commit hash: ${WHITE}$LATEST_HASH"
V_PATCH=$((V_PATCH + 1))
SUGGESTED_VERSION="$V_MAJOR.$V_MINOR.$V_PATCH"
echo -ne "${QUESTION_FLAG} ${CYAN}Enter a version number [${WHITE}$SUGGESTED_VERSION${CYAN}]: "
read INPUT_STRING
if [ "$INPUT_STRING" = "" ]; then
    INPUT_STRING=$SUGGESTED_VERSION
fi
echo -e "${NOTICE_FLAG} Will set new version to be ${WHITE}$INPUT_STRING"

# update the version file
echo "$INPUT_STRING" > $VERSION_FILE

# update the version header file
NEW_VERSION=($(echo $INPUT_STRING | tr '.' ' '))
perl -pi -e "s/#define VIDEOPARSER_VERSION_MAJOR .*/#define VIDEOPARSER_VERSION_MAJOR ${NEW_VERSION[0]}/" $VERSION_HEADER_FILE
perl -pi -e "s/#define VIDEOPARSER_VERSION_MINOR .*/#define VIDEOPARSER_VERSION_MINOR ${NEW_VERSION[1]}/" $VERSION_HEADER_FILE
perl -pi -e "s/#define VIDEOPARSER_VERSION_PATCH .*/#define VIDEOPARSER_VERSION_PATCH ${NEW_VERSION[2]}/" $VERSION_HEADER_FILE

git add "$VERSION_FILE" "$VERSION_HEADER_FILE"

# bump initially but to not push yet
git commit --no-verify -m "Bump version to ${INPUT_STRING}."
git tag -a -m "Tag version ${INPUT_STRING}." "v$INPUT_STRING"

# generate the changelog
uvx --with pystache gitchangelog > CHANGELOG.md

# add the changelog and docs, and amend them to the previous commit and tag
git add CHANGELOG.md
git commit --no-verify --amend --no-edit
git tag -a -f -m "Tag version ${INPUT_STRING}." "v$INPUT_STRING"

# push to remote
echo -e "$PUSHING_MSG"
git push && git push --tags
