#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-msys"

echo "=========================================="
echo " BeaconNode Identity Hardening Build"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo ""

echo "--- Step 1: CMake Configure (regenerates protobufs + links libsodium) ---"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DBEACONNODE_ENABLE_ASIO_NETWORKING=ON \
    -DBEACONNODE_ENABLE_LIBSODIUM=ON \
    -DBEACONNODE_ENABLE_PROTOBUF=ON

echo ""
echo "--- Step 2: Build all targets ---"
cmake --build "$BUILD_DIR"

echo ""
echo "--- Step 3: Run test suite ---"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo ""
echo "=========================================="
echo " Identity hardening build complete."
echo "=========================================="
