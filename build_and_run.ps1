#!/usr/bin/env pwsh

# Change to the project directory
cd $PSScriptRoot

# Create a build directory if it doesn't exist
if (-not (Test-Path -Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

# Run CMake to generate build files
wsl -- "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone; cmake -B build"

# Build the project
wsl -- "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone; cmake --build build"

# Run the game
wsl -- "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone/build/bin; ./doom_clone" 