#!/bin/bash
set -e  # Exit immediately on any error

# Determine the project root (parent of scripts directory)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Building project at $PROJECT_ROOT"

# --------------------------
# Create top-level build folder
# --------------------------
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# --------------------------
# Configure CMake (default: Release)
# --------------------------
cmake .. -DCMAKE_BUILD_TYPE=Release

# --------------------------
# Build all targets in parallel
# --------------------------
NUM_CORES=$(sysctl -n hw.ncpu)
cmake --build . -j"$NUM_CORES"

echo "Build complete!"