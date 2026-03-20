# Scrimmage

A C++ project with a main application and three libraries.

## Project Structure

```
scrimmage/
├── CMakeLists.txt           # Root CMake configuration
├── src/                     # Main application
│   ├── CMakeLists.txt
│   └── main.cpp
├── libs/
│   ├── math-lib/           # Math utilities library
│   ├── util-lib/           # String utilities library
│   └── networking-lib/     # Network utilities library
└── cmake/                  # Shared CMake modules (optional)
```

## Building

```bash
cd scrimmage
cmake -B build
cmake --build build
```

## Running

```bash
./build/src/scrimmage-app
```
