#!/usr/bin/env bash

set -e

cd "$(dirname "$0")/.."

mkdir -p build

pushd build

cmake ..
cmake --build .

popd

echo "Done. You can find the binary at: "
echo
echo "  build/VideoParserCli/video-parser"
