# Doom Clone

A simple 3D first-person shooter game built using raycasting techniques similar to the original Doom and Wolfenstein 3D.

## Features

- 3D raycasting engine
- Texture-mapped walls, floor, and ceiling
- Sprite-based enemies and items
- Minimap for navigation
- Weapon system with ammo management
- Simple collision detection

## Dependencies

This project requires the following libraries:
- SDL2
- SDL2_image
- SDL2_ttf
- SDL2_mixer (optional, for sound)
- PulseAudio

## Building on WSL2 (Windows Subsystem for Linux)

### Quick setup

I've provided a setup script for WSL2 users:

```bash
# Make the script executable
chmod +x setup_wsl2.sh

# Run the setup script
./setup_wsl2.sh
```

### Manual setup

If you prefer to install dependencies manually:

```bash
# Update package repositories
sudo apt update

# Install required packages
sudo apt install -y build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev pkg-config pulseaudio libsdl2-mixer-2.0-0 libsdl2-mixer-dev
```

### Building the game

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make

# Run the game
./bin/doom_clone
```

## Building on Windows

1. Install CMake (https://cmake.org/download/)
2. Install SDL2, SDL2_image, and SDL2_ttf development libraries for Windows
3. Configure your project with the appropriate paths to the SDL libraries

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake (adjust paths as needed)
cmake -DSDL2_DIR=path/to/SDL2/cmake -DSDL2_IMAGE_DIR=path/to/SDL2_image/cmake -DSDL2_TTF_DIR=path/to/SDL2_ttf/cmake ..

# Build (using your preferred IDE or build system)
```

## Controls

- WASD or Arrow keys: Move and turn
- Up/Down: Look around
- Space: Fire weapon
- R: Reload
- ESC: Quit or pause game
- F1: Toggle FPS display
- F2: Toggle minimap
- F3: Toggle weapon display

## License

This project is open source and available under the MIT License.

## Credits

- Raycasting techniques inspired by [Lode's Computer Graphics Tutorial](https://lodev.org/cgtutor/raycasting.html)
- Textures from [OpenGameArt.org](https://opengameart.org) 