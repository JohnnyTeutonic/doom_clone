#!/usr/bin/env pwsh

# Change to the project directory
cd $PSScriptRoot

# Create a build directory if it doesn't exist
if (-not (Test-Path -Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

# Run commands in WSL
wsl bash -c "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone && cmake -B build"
wsl bash -c "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone && cmake --build build"
wsl bash -c "cd /mnt/c/Users/jonat/OneDrive/Documents/doom_clone/build/bin && ./doom_clone" 