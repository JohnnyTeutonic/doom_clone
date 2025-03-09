# DOOM Clone - Sector-Based BSP Renderer

A modern DOOM-style game engine using sector-based rendering with Binary Space Partitioning (BSP).

## Features

- Sector-based world representation
- Binary Space Partitioning (BSP) for efficient rendering
- Support for curved walls
- Portal rendering for windows and doors
- Dynamic lighting
- SDL2-based rendering with hardware acceleration

## Building the Project

### Prerequisites

- CMake 3.10+
- C++ Compiler with C++17 support (GCC, Clang, or MSVC)
- SDL2 and related libraries:
  - SDL2
  - SDL2_image
  - SDL2_ttf
  - SDL2_mixer

#### Windows (Visual Studio / MSVC)

1. Install SDL2 development libraries for Visual C++
   - Download the development libraries from https://www.libsdl.org/
   - Extract to a directory (e.g., `C:\SDL2`)
   - Do the same for SDL2_image, SDL2_ttf, and SDL2_mixer

2. Build with CMake:
   ```
   mkdir build
   cd build
   cmake -G "Visual Studio 17 2022" -A x64 -DSDL2_DIR=C:/SDL2/cmake ..
   cmake --build . --config Release
   ```

3. Copy SDL2.dll and other DLLs to the same directory as the executable.

#### Windows (MinGW)

1. Install SDL2 development libraries for MinGW
   - Download the development libraries from https://www.libsdl.org/
   - Extract to a directory (e.g., `C:\SDL2-MinGW`)

2. Build with CMake:
   ```
   mkdir build
   cd build
   cmake -G "MinGW Makefiles" -DSDL2_DIR=C:/SDL2-MinGW/cmake ..
   cmake --build .
   ```

3. Copy SDL2.dll and other DLLs to the same directory as the executable.

#### Linux (Debian/Ubuntu)

1. Install required packages:
   ```
   sudo apt update
   sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
   ```

2. Build with CMake:
   ```
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

#### macOS

1. Install required packages with Homebrew:
   ```
   brew install cmake sdl2 sdl2_image sdl2_ttf sdl2_mixer
   ```

2. Build with CMake:
   ```
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

## Running the Engine

After building, run the executable from the build directory:

```
./doom_clone
```

### Command-line Options

- `--fullscreen`: Run in fullscreen mode
- `--windowed`: Run in windowed mode
- `--width N`: Set window width to N pixels
- `--height N`: Set window height to N pixels
- `--fps N`: Set target FPS to N
- `--no-vsync`: Disable vertical sync
- `--no-fps`: Hide FPS counter
- `--help`: Show help message

## Controls

- WASD: Move
- Mouse: Look around
- Space: Jump
- Ctrl: Crouch
- Left Mouse Button: Shoot
- Right Mouse Button: Alternate fire
- E: Use/Interact
- 1-9: Select weapon
- Escape: Open menu/Exit

## Project Structure

- `src/`: Source code
  - `utils.h/cpp`: Common utilities
  - `map.h/cpp`: Map structures and BSP implementation
  - `player.h/cpp`: Player mechanics
  - `renderer.h/cpp`: Rendering system
  - `main.cpp`: Application entry point
- `assets/`: Game assets (textures, sounds, etc.)
- `CMakeLists.txt`: CMake build script

## License

This project is released under the MIT License. See the LICENSE file for details. 