#!/bin/bash
set -e  # Exit on any error

echo "Cleaning all build artifacts..."

# Remove top-level build folder
if [ -d "build" ]; then
    echo "Removing top-level build/"
    rm -rf build
fi

# Remove build folders in apps
for app_dir in apps/*; do
    if [ -d "$app_dir/build" ]; then
        echo "Removing $app_dir/build/"
        rm -rf "$app_dir/build"
    fi
done

# Remove build folders in libs
for lib_dir in libs/*; do
    if [ -d "$lib_dir/build" ]; then
        echo "Removing $lib_dir/build/"
        rm -rf "$lib_dir/build"
    fi
done

# Optional: remove CMake cache files in tests
if [ -d "tests/build" ]; then
    echo "Removing tests/build/"
    rm -rf tests/build
fi

echo "Clean complete!"