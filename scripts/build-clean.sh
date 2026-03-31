#!/bin/bash
set -e  # Exit immediately on any error

# Determine the project root (parent of the scripts directory)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Cleaning project at $PROJECT_ROOT"

# --------------------------
# Remove top-level build folder
# --------------------------
if [ -d "$PROJECT_ROOT/build" ]; then
    echo "Removing $PROJECT_ROOT/build/"
    rm -rf "$PROJECT_ROOT/build"
fi

# --------------------------
# Remove build folders in apps
# --------------------------
for app_dir in "$PROJECT_ROOT"/apps/*; do
    if [ -d "$app_dir/build" ]; then
        echo "Removing $app_dir/build/"
        rm -rf "$app_dir/build"
    fi
done

# --------------------------
# Remove build folders in libs
# --------------------------
for lib_dir in "$PROJECT_ROOT"/libs/*; do
    if [ -d "$lib_dir/build" ]; then
        echo "Removing $lib_dir/build/"
        rm -rf "$lib_dir/build"
    fi
done

# --------------------------
# Remove build folder in tests
# --------------------------
if [ -d "$PROJECT_ROOT/tests/build" ]; then
    echo "Removing $PROJECT_ROOT/tests/build/"
    rm -rf "$PROJECT_ROOT/tests/build"
fi

echo "All build artifacts cleaned!"