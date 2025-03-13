#ifndef CUDA_UTILS_H
#define CUDA_UTILS_H

// Include centralized CUDA configuration
#include "CUDAConfig.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>
#include <math.h>  // For math functions used in device code
#include "Common.h"

// Forward declaration of Color class if necessary
// struct Color;

// Macro for checking CUDA errors
#define CUDA_CHECK_ERROR(call) \
do { \
    cudaError_t error = call; \
    if (error != cudaSuccess) { \
        fprintf(stderr, "CUDA error at %s:%d - %s\n", __FILE__, __LINE__, \
                cudaGetErrorString(error)); \
        exit(EXIT_FAILURE); \
    } \
} while(0)

// CUDA version of Color structure
struct CUDAColor {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    
    __host__ __device__ CUDAColor() : r(0), g(0), b(0), a(255) {}
    
    __host__ __device__ CUDAColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255) 
        : r(red), g(green), b(blue), a(alpha) {}
    
    // Convert from regular Color
    __host__ CUDAColor(const Color& color) 
        : r(color.r), g(color.g), b(color.b), a(color.a) {}
    
    // Convert to regular Color
    __host__ operator Color() const { 
        return Color(r, g, b, a); 
    }
    
    // Blend with another color
    __device__ CUDAColor blend(const CUDAColor& other, float alpha) const {
        CUDAColor result;
        result.r = static_cast<unsigned char>(r * (1.0f - alpha) + other.r * alpha);
        result.g = static_cast<unsigned char>(g * (1.0f - alpha) + other.g * alpha);
        result.b = static_cast<unsigned char>(b * (1.0f - alpha) + other.b * alpha);
        result.a = a;
        return result;
    }
};

// CUDA version of Vec2 structure
struct CUDAVec2 {
    float x;
    float y;
    
    __host__ __device__ CUDAVec2() : x(0.0f), y(0.0f) {}
    __host__ __device__ CUDAVec2(float x_, float y_) : x(x_), y(y_) {}
    
    // Convert from regular Vec2
    __host__ CUDAVec2(const Vec2& vec) : x((float)vec.x), y((float)vec.y) {}
    
    // Convert to regular Vec2
    __host__ operator Vec2() const { 
        return Vec2(x, y); 
    }
    
    // Vector operations
    __device__ CUDAVec2 operator+(const CUDAVec2& other) const {
        return CUDAVec2(x + other.x, y + other.y);
    }
    
    __device__ CUDAVec2 operator-(const CUDAVec2& other) const {
        return CUDAVec2(x - other.x, y - other.y);
    }
    
    __device__ CUDAVec2 operator*(float scalar) const {
        return CUDAVec2(x * scalar, y * scalar);
    }
    
    __device__ float dot(const CUDAVec2& other) const {
        return x * other.x + y * other.y;
    }
    
    __device__ float length() const {
        return sqrtf(x * x + y * y);
    }
    
    __device__ CUDAVec2 normalize() const {
        float len = length();
        if (len > 0.0f) {
            return CUDAVec2(x / len, y / len);
        }
        return *this;
    }
};

// CUDA texture data
struct CUDATexture {
    cudaArray* array;
    cudaTextureObject_t textureObject;
    int width;
    int height;
    
    // Constructor
    CUDATexture() : array(nullptr), textureObject(0), width(0), height(0) {}
    
    // Destructor
    ~CUDATexture() {
        if (textureObject) {
            cudaDestroyTextureObject(textureObject);
        }
        if (array) {
            cudaFreeArray(array);
        }
    }
};

// Initialize CUDA
inline bool initCUDA() {
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    
    if (error != cudaSuccess) {
        fprintf(stderr, "CUDA error: unable to get device count - %s\n", cudaGetErrorString(error));
        return false;
    }
    
    if (deviceCount == 0) {
        fprintf(stderr, "No CUDA-capable devices found\n");
        return false;
    }
    
    // Use the device with the highest compute capability
    int bestDevice = 0;
    int highestComputeCapability = 0;
    
    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp deviceProp;
        cudaGetDeviceProperties(&deviceProp, i);
        
        int computeCapability = deviceProp.major * 10 + deviceProp.minor;
        if (computeCapability > highestComputeCapability) {
            highestComputeCapability = computeCapability;
            bestDevice = i;
        }
    }
    
    cudaSetDevice(bestDevice);
    
    // Print device information
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, bestDevice);
    printf("Using CUDA device %d: %s\n", bestDevice, deviceProp.name);
    printf("Compute capability: %d.%d\n", deviceProp.major, deviceProp.minor);
    printf("Total global memory: %zu MB\n", deviceProp.totalGlobalMem / (1024 * 1024));
    
    return true;
}

// Create CUDA texture from pixel data
inline bool createCUDATexture(CUDATexture* texture, const uint32_t* pixels, int width, int height) {
    if (!texture || !pixels || width <= 0 || height <= 0) {
        return false;
    }
    
    // Channel format description: 4 channels, 8 bits each, unsigned
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc(8, 8, 8, 8, cudaChannelFormatKindUnsigned);
    
    // Allocate CUDA array
    CUDA_CHECK_ERROR(cudaMallocArray(&texture->array, &channelDesc, width, height));
    
    // Copy pixel data to array
    CUDA_CHECK_ERROR(cudaMemcpyToArray(texture->array, 0, 0, pixels, width * height * sizeof(uint32_t), cudaMemcpyHostToDevice));
    
    // Create texture object
    cudaResourceDesc resDesc = {};
    resDesc.resType = cudaResourceTypeArray;
    resDesc.res.array.array = texture->array;
    
    cudaTextureDesc texDesc = {};
    texDesc.addressMode[0] = cudaAddressModeWrap;
    texDesc.addressMode[1] = cudaAddressModeWrap;
    texDesc.filterMode = cudaFilterModeLinear;
    texDesc.readMode = cudaReadModeElementType;
    texDesc.normalizedCoords = 1;
    
    CUDA_CHECK_ERROR(cudaCreateTextureObject(&texture->textureObject, &resDesc, &texDesc, nullptr));
    
    texture->width = width;
    texture->height = height;
    
    return true;
}

// Destroy a CUDA texture
inline void destroyCUDATexture(CUDATexture* texture) {
    if (!texture) {
        return;
    }
    
    if (texture->textureObject) {
        cudaDestroyTextureObject(texture->textureObject);
        texture->textureObject = 0;
    }
    
    if (texture->array) {
        cudaFreeArray(texture->array);
        texture->array = nullptr;
    }
    
    texture->width = 0;
    texture->height = 0;
}

#endif // CUDA_UTILS_H 