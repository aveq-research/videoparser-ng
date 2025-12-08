#!/usr/bin/env bash
#
# Release the project and bump version number in the process.

set -e

cd "$(dirname "$0")"

FORCE=false

usage() {
    echo "Usage: $0 [options] VERSION"
    echo
    echo "VERSION:"
    echo "  major: bump major version number"
    echo "  minor: bump minor version number"
    echo "  patch: bump patch version number"
    echo
    echo "Options:"
    echo "  -f, --force:  force release"
    echo "  -h, --help:   show this help message"
    exit 1
}

# parse args
while [ "$#" -gt 0 ]; do
    case "$1" in
    -f | --force)
        FORCE=true
        shift
        ;;
    -h | --help)
        usage
        ;;
    *)
        break
        ;;
    esac
done

# check if version is specified
if [ "$#" -ne 1 ]; then
    usage
fi

if [ "$1" != "major" ] && [ "$1" != "minor" ] && [ "$1" != "patch" ]; then
    usage
fi

# check if git is clean and force is not enabled
if ! git diff-index --quiet HEAD -- && [ "$FORCE" = false ]; then
    echo "Error: git is not clean. Please commit all changes first."
    exit 1
fi

if ! command -v uv &> /dev/null; then
    echo "Error: uv is not installed. Please install uv from https://docs.astral.sh/uv/"
    exit 1
fi

VERSION_FILE="VERSION"
VERSION_HEADER_FILE="VideoParser/VideoParser.h"

if [ ! -f "$VERSION_FILE" ]; then
    echo "Error: VERSION file not found."
    exit 1
fi

VERSION=$(cat "$VERSION_FILE")

new_version=$(uvx --from semver pysemver bump "$1" "$VERSION")

echo "Would bump version:"
echo "$VERSION -> $new_version"

# prompt for confirmation
if [ "$FORCE" = false ]; then
    read -p "Do you want to release? [yY] " -n 1 -r
    echo
else
    REPLY="y"
fi
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    # update the VERSION file
    echo "$new_version" > "$VERSION_FILE"

    # update the C++ header file
    BASE_LIST=($(echo $new_version | tr '.' ' '))
    V_MAJOR=${BASE_LIST[0]}
    V_MINOR=${BASE_LIST[1]}
    V_PATCH=${BASE_LIST[2]}

    perl -pi -e "s/#define VIDEOPARSER_VERSION_MAJOR .*/#define VIDEOPARSER_VERSION_MAJOR $V_MAJOR/" "$VERSION_HEADER_FILE"
    perl -pi -e "s/#define VIDEOPARSER_VERSION_MINOR .*/#define VIDEOPARSER_VERSION_MINOR $V_MINOR/" "$VERSION_HEADER_FILE"
    perl -pi -e "s/#define VIDEOPARSER_VERSION_PATCH .*/#define VIDEOPARSER_VERSION_PATCH $V_PATCH/" "$VERSION_HEADER_FILE"

    # commit changes
    git add "$VERSION_FILE" "$VERSION_HEADER_FILE"
    git commit -m "bump version to $new_version"
    git tag -a "v$new_version" -m "v$new_version"

    # generate changelog
    uvx git+https://github.com/pawamoy/git-changelog@15bcad73cc328ef7572515f599826fcf9bb2f4ad --include-all --convention angular --sections :all: > CHANGELOG.md

    git add CHANGELOG.md
    git commit --no-verify --amend --no-edit
    git tag -a -f -m "v$new_version" "v$new_version"

    # push changes
    git push origin master
    git push origin "v$new_version"
else
    echo "Aborted."
    exit 1
fi
