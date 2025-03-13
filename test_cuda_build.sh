#!/bin/bash
# Test script for CUDA compilation

# Make sure CUDAUtils.h can be compiled
echo "Testing CUDA header compilation..."
nvcc -c src/CUDAUtils.h -o test_cuda_utils.o

if [ $? -eq 0 ]; then
    echo "CUDA header compilation successful!"
else
    echo "CUDA header compilation failed!"
    exit 1
fi

# Test compiling a simple CUDA program
echo "Testing CUDA renderer compilation..."
nvcc -c src/CUDARenderer.cu -o test_cuda_renderer.o

if [ $? -eq 0 ]; then
    echo "CUDA renderer compilation successful!"
    rm test_cuda_utils.o test_cuda_renderer.o
    echo "All tests passed!"
else
    echo "CUDA renderer compilation failed!"
    exit 1
fi

exit 0 