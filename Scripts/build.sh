#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# macOS / Linux 빌드.
set -euo pipefail

cd "$(dirname "$0")/.."

CONFIG="RelWithDebInfo"
BUILD_DIR="build"
RUN_TESTS=0
CLEAN=0
WERROR=0

while [ $# -gt 0 ]; do
  case "$1" in
    --debug)     CONFIG="Debug" ;;
    --release)   CONFIG="Release" ;;
    --clean)     CLEAN=1 ;;
    --test)      RUN_TESTS=1 ;;
    --werror)    WERROR=1 ;;
    --build-dir) shift; BUILD_DIR="$1" ;;
    -h|--help)
      echo "사용법: $0 [--debug|--release] [--clean] [--test] [--werror] [--build-dir <경로>]"
      exit 0 ;;
    *) echo "알 수 없는 인자: $1" >&2; exit 2 ;;
  esac
  shift
done

[ "$CLEAN" = "1" ] && rm -rf "$BUILD_DIR"

GENERATOR=()
command -v ninja >/dev/null 2>&1 && GENERATOR=(-G Ninja)

CMAKE_ARGS=(-S . -B "$BUILD_DIR" "${GENERATOR[@]}" "-DCMAKE_BUILD_TYPE=$CONFIG")
[ "$WERROR" = "1" ] && CMAKE_ARGS+=(-DALICE_WARNINGS_AS_ERRORS=ON)

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel

echo
echo "  빌드 완료"
echo "    $BUILD_DIR/bin/alice"
echo "    $BUILD_DIR/bin/Alice.Tests"

if [ "$RUN_TESTS" = "1" ]; then
  echo
  "./$BUILD_DIR/bin/Alice.Tests"
  "./$BUILD_DIR/bin/alice" check Samples
fi
