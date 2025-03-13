#!/bin/bash
# Script to check CUDA version and capabilities

echo "Checking CUDA installation..."

# Check if nvcc is installed
if command -v nvcc &> /dev/null; then
    echo "CUDA compiler (nvcc) found"
    nvcc --version
else
    echo "CUDA compiler (nvcc) not found in PATH"
    echo "Please make sure CUDA toolkit is installed and in your PATH"
    exit 1
fi

# Check for CUDA-capable devices
echo -e "\nChecking for CUDA-capable devices:"
if command -v nvidia-smi &> /dev/null; then
    nvidia-smi
else
    echo "nvidia-smi not found in PATH"
    echo "No NVIDIA GPU detected or drivers not installed properly"
    exit 1
fi

# Create a simple CUDA program to test compilation
echo -e "\nTesting CUDA compilation..."
cat > test_cuda.cu << EOF
#include <stdio.h>

__global__ void hello_kernel() {
    printf("Hello from the GPU!\n");
}

int main() {
    printf("Hello from the CPU!\n");
    hello_kernel<<<1, 1>>>();
    cudaDeviceSynchronize();
    return 0;
}
EOF

# Compile the test program
nvcc -o test_cuda test_cuda.cu

# Run the test program if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo -e "\nRunning test program..."
    ./test_cuda
    rm test_cuda test_cuda.cu
else
    echo "Compilation failed!"
    rm test_cuda.cu
    exit 1
fi

echo -e "\nCUDA check complete"
exit 0 