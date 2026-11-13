#!/bin/sh
# Builds and runs the core-logic unit tests (components/*/core) with the system
# compiler. No Chromium checkout needed. Requires: clang++, GoogleTest
# (macOS: `brew install googletest`).
set -eu
cd "$(dirname "$0")/.."

out=out/core
mkdir -p "$out/include/testing/gtest/include"
# Mimic Chromium's include roots: "browser/..." is this repo,
# "testing/gtest/include/gtest/..." is GoogleTest.
ln -sfn "$PWD" "$out/include/browser"
gtest=$(brew --prefix googletest 2>/dev/null || echo /usr)
ln -sfn "$gtest/include/gtest" "$out/include/testing/gtest/include/gtest"

for dir in components/*/core; do
  name=$(basename "$(dirname "$dir")")
  echo "== $name"
  clang++ -std=c++20 -Wall -Wextra -Werror -g \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -I "$out/include" -I "$gtest/include" \
    "$dir"/*.cc -L "$gtest/lib" -lgtest -lgtest_main \
    -o "$out/${name}_unittests"
  "$out/${name}_unittests"
done
