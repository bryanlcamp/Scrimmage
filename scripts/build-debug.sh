#!/bin/bash
# =============================================================================
# FULL DEBUG BUILD PIPELINE - EVERYTHING ENABLED
# =============================================================================
set -e
set -o pipefail

# --------------------------
# Resolve project root
# --------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
echo "Running FULL debug pipeline for project at $PROJECT_ROOT"

# --------------------------
# Clean all build artifacts
# --------------------------
echo "Cleaning all build artifacts..."
rm -rf "$PROJECT_ROOT/build" "$PROJECT_ROOT/build-debug"

for lib_dir in "$PROJECT_ROOT"/libs/*; do
  if [ -d "$lib_dir/build" ]; then
    echo "Cleaning $lib_dir/build"
    rm -rf "$lib_dir/build"
  fi
done

# --------------------------
# Setup build directory
# --------------------------
BUILD_DIR="$PROJECT_ROOT/build-debug"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

NUM_CORES=$(sysctl -n hw.ncpu)

# --------------------------
# Configure CMake
# --------------------------
echo "Configuring CMake (Debug + Sanitizers + Warnings + GoogleTest)..."

SANITIZERS="-fsanitize=address,undefined,leak,thread"
CXX_FLAGS="-g -O0 -fno-omit-frame-pointer -fno-inline -fstack-protector-strong $SANITIZERS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnull-dereference"

cmake "$PROJECT_ROOT" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_WARNINGS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS="$CXX_FLAGS"

# --------------------------
# Build everything
# --------------------------
echo "Building all libraries and executables..."
cmake --build . -j"$NUM_CORES"

# --------------------------
# clang-format check
# --------------------------
echo "Running clang-format..."
FORMAT_FILES=$(find "$PROJECT_ROOT/apps" "$PROJECT_ROOT/libs" \( -name "*.cpp" -o -name "*.h" \) -print0)
if ! echo "$FORMAT_FILES" | xargs -0 clang-format --dry-run --Werror; then
  FORMAT_STATUS="FAILED"
else
  FORMAT_STATUS="PASSED"
fi

# --------------------------
# clang-tidy
# --------------------------
echo "Running clang-tidy..."
if command -v run-clang-tidy >/dev/null 2>&1; then
  CLANG_TIDY_LOG="$BUILD_DIR/clang-tidy.log"
  run-clang-tidy -p "$BUILD_DIR" -j "$NUM_CORES" | tee "$CLANG_TIDY_LOG"
  CLANG_TIDY_STATUS="COMPLETED"
else
  CLANG_TIDY_LOG="N/A"
  CLANG_TIDY_STATUS="MISSING"
fi

# --------------------------
# Run unit tests (GoogleTest + GoogleMock)
# --------------------------
echo "Running all unit tests..."
TEST_LOG="$BUILD_DIR/unit-tests.log"
if ctest --output-on-failure -j "$NUM_CORES" | tee "$TEST_LOG"; then
  TEST_STATUS="PASSED"
else
  TEST_STATUS="FAILED"
fi

# --------------------------
# Run tests with sanitizers
# --------------------------
echo "Running tests with sanitizers..."
SANITIZER_LOG="$BUILD_DIR/sanitizer-tests.log"
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
TSAN_OPTIONS=halt_on_error=1 \
ctest --output-on-failure -j "$NUM_CORES" | tee "$SANITIZER_LOG"
SANITIZER_STATUS="COMPLETED"

# --------------------------
# Summary report
# --------------------------
echo
echo "=============================="
echo "DEBUG BUILD SUMMARY"
echo "=============================="
echo "Formatting check: $FORMAT_STATUS"
echo "Clang-Tidy: $CLANG_TIDY_STATUS (log: $CLANG_TIDY_LOG)"
echo "Unit tests: $TEST_STATUS (log: $TEST_LOG)"
echo "Sanitizer tests: $SANITIZER_STATUS (log: $SANITIZER_LOG)"
echo "=============================="
echo "Debug pipeline complete."