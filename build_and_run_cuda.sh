#!/bin/bash
# Build and run script for DOOM Clone with CUDA support

# Check if CUDA is installed
echo "Checking for CUDA installation..."
if command -v nvcc &> /dev/null; then
    echo "CUDA compiler (nvcc) found:"
    nvcc --version
else
    echo "CUDA compiler (nvcc) not found in PATH. Please make sure CUDA toolkit is installed and in your PATH."
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir -p build
fi

# Navigate to build directory
cd build

# Configure with CMake, explicitly enabling CUDA
echo "Configuring with CMake (CUDA enabled)..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=ON

# Build
echo "Building..."
cmake --build . -- -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful! Running game..."
    
    # Navigate to bin directory
    cd bin
    
    # Run the game
    ./doom_clone
    
    # Return to original directory
    cd ../..
else
    echo "Build failed with exit code $?"
    
    # Return to original directory
    cd ..
fi 