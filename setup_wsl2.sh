#!/bin/bash

# Doom Clone WSL2 Setup Script
echo "Setting up dependencies for Doom Clone on WSL2..."

# Update package repositories
echo "Updating package repositories..."
sudo apt update

# Install required packages
echo "Installing required packages..."
sudo apt install -y \
    build-essential \
    cmake \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    libsdl2-mixer-dev \
    pkg-config \
    pulseaudio \
    libsdl2-mixer-2.0-0 \
    libwebp-dev \
    libwebpdemux2 \

echo "All required packages installed."
echo "To build the game:"
echo "  mkdir -p build"
echo "  cd build"
echo "  cmake .."
echo "  make"
echo "To run the game: ./bin/doom_clone" 