#!/bin/bash
# =============================================================================
# FULL DEBUG BUILD PIPELINE - EVERYTHING ENABLED (OUTPUT TO SCREEN)
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
# Configure sanitizer flags
# --------------------------
if [[ "$(uname -s)" == "Darwin" ]]; then
    # macOS / Apple Silicon
    SANITIZERS="-fsanitize=address,undefined"
else
    SANITIZERS="-fsanitize=address,undefined,leak,thread"
fi

# --------------------------
# Configure CMake
# --------------------------
echo "Configuring CMake (Debug + Sanitizers + Warnings + GoogleTest)..."
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
echo "Checking formatting..."
FORMAT_FILES=$(find "$PROJECT_ROOT/apps" "$PROJECT_ROOT/libs" \( -name "*.cpp" -o -name "*.h" \))
if ! clang-format --dry-run --Werror $FORMAT_FILES; then
  echo "Formatting violations detected! Run clang-format -i to fix."
  FORMAT_STATUS="FAILED"
else
  FORMAT_STATUS="PASSED"
fi

# --------------------------
# clang-tidy
# --------------------------
echo "Running clang-tidy..."
if command -v run-clang-tidy >/dev/null 2>&1; then
  if ! run-clang-tidy -p "$BUILD_DIR" -j "$NUM_CORES"; then
    CLANG_TIDY_STATUS="ISSUES FOUND"
  else
    CLANG_TIDY_STATUS="OK"
  fi
else
  CLANG_TIDY_STATUS="MISSING (install clang-tools)"
fi

# --------------------------
# Run unit tests
# --------------------------
echo "Running unit tests..."
if ! ctest --output-on-failure -j "$NUM_CORES"; then
  TEST_STATUS="FAILED"
else
  TEST_STATUS="PASSED"
fi

# --------------------------
# Run tests with sanitizers
# --------------------------
echo "Running tests with sanitizers..."
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
TSAN_OPTIONS=halt_on_error=1 \
ctest --output-on-failure -j "$NUM_CORES"
SANITIZER_STATUS="COMPLETED"

# --------------------------
# Summary report
# --------------------------
echo
echo "=============================="
echo "DEBUG BUILD SUMMARY"
echo "=============================="
echo "Formatting check: $FORMAT_STATUS"
echo "Clang-Tidy: $CLANG_TIDY_STATUS"
echo "Unit tests: $TEST_STATUS"
echo "Sanitizer tests: $SANITIZER_STATUS"
echo "=============================="
echo "Debug pipeline complete."