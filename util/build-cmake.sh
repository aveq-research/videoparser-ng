#!/usr/bin/env bash

set -e

cd "$(dirname "$0")/.."

mkdir -p build

pushd build

cmake ..
cmake --build .

popd
