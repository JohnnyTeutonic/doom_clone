#include "CUDARenderer.h"
#include "CUDAUtils.h"
#include <cmath>

// Global constants for CUDA kernels using separate scalars for color components
__constant__ unsigned char d_fogColor_r; // Red
__constant__ unsigned char d_fogColor_g; // Green
__constant__ unsigned char d_fogColor_b; // Blue
__constant__ unsigned char d_fogColor_a = 255; // Alpha with static initialization
__constant__ float d_fogDistance;
__constant__ float d_fogDensity;
__constant__ int d_screenWidth;
__constant__ int d_screenHeight;
__constant__ float d_renderDistance;
__constant__ bool d_fogEnabled;
__constant__ bool d_lightingEnabled;

// Device function to apply lighting to a color
__device__ CUDAColor applyLighting(const CUDAColor& color, float lightLevel) {
    CUDAColor result;
    
    if (lightLevel <= 0.0f) {
        // Complete darkness
        result.r = 0;
        result.g = 0;
        result.b = 0;
    } else if (lightLevel >= 1.0f) {
        // Full brightness
        result = color;
    } else {
        // Partial lighting
        result.r = static_cast<unsigned char>(color.r * lightLevel);
        result.g = static_cast<unsigned char>(color.g * lightLevel);
        result.b = static_cast<unsigned char>(color.b * lightLevel);
    }
    
    result.a = color.a;
    return result;
}

// Device function to apply fog to a color
__device__ CUDAColor applyFog(const CUDAColor& color, float distance) {
    if (!d_fogEnabled || distance <= 0.0f) {
        return color;
    }
    
    // Compute fog factor based on distance
    float fogFactor;
    
    if (distance >= d_renderDistance) {
        // Maximum fog
        fogFactor = 1.0f;
    } else if (distance <= d_fogDistance) {
        // No fog
        fogFactor = 0.0f;
    } else {
        // Exponential fog
        float relativeDistance = (distance - d_fogDistance) / (d_renderDistance - d_fogDistance);
        fogFactor = 1.0f - expf(-d_fogDensity * relativeDistance * relativeDistance);
        
        // Clamp to [0,1]
        fogFactor = fmaxf(0.0f, fminf(1.0f, fogFactor));
    }
    
    // Blend color with fog color, using the separate fog color components
    CUDAColor result;
    result.r = static_cast<unsigned char>(color.r * (1.0f - fogFactor) + d_fogColor_r * fogFactor);
    result.g = static_cast<unsigned char>(color.g * (1.0f - fogFactor) + d_fogColor_g * fogFactor);
    result.b = static_cast<unsigned char>(color.b * (1.0f - fogFactor) + d_fogColor_b * fogFactor);
    result.a = color.a;
    
    return result;
}

// Device function to sample texture
__device__ CUDAColor sampleTexture(cudaTextureObject_t texture, float u, float v) {
    // Ensure u,v are in range [0,1]
    u = u - floorf(u);
    v = v - floorf(v);
    
    // Sample texture
    uint4 texel = tex2D<uint4>(texture, u, v);
    
    // RGBA format from texture
    return CUDAColor(texel.x, texel.y, texel.z, texel.w);
}

// CUDA kernel for clearing the screen and z-buffer
__global__ void clearScreenKernel(uint32_t* pixelBuffer, float* zBuffer, CUDAColor backgroundColor) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= d_screenWidth || y >= d_screenHeight) {
        return;
    }
    
    int index = y * d_screenWidth + x;
    
    // Clear pixel buffer
    pixelBuffer[index] = (backgroundColor.a << 24) | (backgroundColor.r << 16) | 
                         (backgroundColor.g << 8) | backgroundColor.b;
    
    // Clear z-buffer to maximum distance
    zBuffer[index] = FLT_MAX;
}

// CUDA kernel for rendering wall spans
__global__ void renderWallSpansKernel(const CUDAWallSpan* wallSpans, int numWallSpans, 
                                     uint32_t* pixelBuffer, float* zBuffer, 
                                     cudaTextureObject_t* textures) {
    int spanIndex = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (spanIndex >= numWallSpans) {
        return;
    }
    
    const CUDAWallSpan& span = wallSpans[spanIndex];
    int x = span.x;
    
    // Check x bounds
    if (x < 0 || x >= d_screenWidth) {
        return;
    }
    
    // Calculate range for this span
    int yStart = max(0, span.y1);
    int yEnd = min(d_screenHeight - 1, span.y2);
    
    // Texture parameters
    float wallHeight = span.y2 - span.y1;
    
    // Iterate over vertical pixels for this wall span
    for (int y = yStart; y <= yEnd; ++y) {
        int index = y * d_screenWidth + x;
        
        // Depth test
        if (span.distance >= zBuffer[index]) {
            continue;
        }
        
        // Calculate texture coordinates
        float v = (y - span.y1) / wallHeight;
        float u = span.texU;
        
        // Sample texture
        CUDAColor color = sampleTexture(textures[span.textureId], u, v);
        
        // Apply lighting if enabled
        if (d_lightingEnabled) {
            color = applyLighting(color, span.lightLevel);
        }
        
        // Apply fog
        color = applyFog(color, span.distance);
        
        // Write to framebuffer
        pixelBuffer[index] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
        
        // Update z-buffer
        zBuffer[index] = span.distance;
    }
}

// CUDA kernel for rendering floor and ceiling spans
__global__ void renderFloorCeilingSpansKernel(const CUDAFloorCeilingSpan* floorCeilingSpans, 
                                            int numSpans, uint32_t* pixelBuffer, 
                                            float* zBuffer, cudaTextureObject_t* textures) {
    int spanIndex = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (spanIndex >= numSpans) {
        return;
    }
    
    const CUDAFloorCeilingSpan& span = floorCeilingSpans[spanIndex];
    int x = span.x;
    int y = span.y;
    
    // Check bounds
    if (x < 0 || x >= d_screenWidth || y < 0 || y >= d_screenHeight) {
        return;
    }
    
    int index = y * d_screenWidth + x;
    
    // Depth test
    if (span.distance >= zBuffer[index]) {
        return;
    }
    
    // Sample texture
    CUDAColor color;
    if (span.textureId >= 0) {
        color = sampleTexture(textures[span.textureId], span.u, span.v);
    } else {
        // Default color for missing texture
        color = span.isFloor ? CUDAColor(80, 80, 80) : CUDAColor(60, 60, 80);
    }
    
    // Apply lighting if enabled
    if (d_lightingEnabled) {
        color = applyLighting(color, span.lightLevel);
    }
    
    // Apply fog
    color = applyFog(color, span.distance);
    
    // Write to framebuffer
    pixelBuffer[index] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    
    // Update z-buffer
    zBuffer[index] = span.distance;
}

// CUDA kernel for rendering sprite spans
__global__ void renderSpriteSpansKernel(const CUDASpriteSpan* spriteSpans, 
                                      int numSpans, uint32_t* pixelBuffer, 
                                      float* zBuffer, cudaTextureObject_t* textures) {
    int spanIndex = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (spanIndex >= numSpans) {
        return;
    }
    
    const CUDASpriteSpan& span = spriteSpans[spanIndex];
    int x = span.x;
    
    // Check x bounds
    if (x < 0 || x >= d_screenWidth) {
        return;
    }
    
    // Calculate range for this span
    int yStart = max(0, span.y1);
    int yEnd = min(d_screenHeight - 1, span.y2);
    
    // Texture parameters
    float spriteHeight = span.y2 - span.y1;
    
    // Iterate over vertical pixels for this sprite span
    for (int y = yStart; y <= yEnd; ++y) {
        int index = y * d_screenWidth + x;
        
        // Depth test
        if (span.distance >= zBuffer[index]) {
            continue;
        }
        
        // Calculate texture coordinates
        float v = (y - span.y1) / spriteHeight;
        float u = span.u;
        
        // Sample texture
        CUDAColor color = sampleTexture(textures[span.textureId], u, v);
        
        // Skip transparent pixels
        if (color.a < 128) {
            continue;
        }
        
        // Apply lighting if enabled
        if (d_lightingEnabled) {
            color = applyLighting(color, span.lightLevel);
        }
        
        // Apply fog
        color = applyFog(color, span.distance);
        
        // Write to framebuffer
        pixelBuffer[index] = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
        
        // Update z-buffer
        zBuffer[index] = span.distance;
    }
}

// Initialize CUDA renderer
bool CUDARenderer::init() {
    if (!initCUDA()) {
        fprintf(stderr, "Failed to initialize CUDA\n");
        return false;
    }
    
    // Allocate device memory for pixel buffer
    size_t pixelBufferSize = m_screenWidth * m_screenHeight * sizeof(uint32_t);
    CUDA_CHECK_ERROR(cudaMalloc(&d_pixelBuffer, pixelBufferSize));
    
    // Allocate device memory for z-buffer
    size_t zBufferSize = m_screenWidth * m_screenHeight * sizeof(float);
    CUDA_CHECK_ERROR(cudaMalloc(&d_zBuffer, zBufferSize));
    
    // Set constant parameters
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_screenWidth, &m_screenWidth, sizeof(int)));
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_screenHeight, &m_screenHeight, sizeof(int)));
    
    // Initialize GPU texture array
    d_textures = nullptr;
    m_numTextures = 0;
    
    // Set maximum spans
    m_maxWallSpans = 10000;
    m_maxFloorCeilingSpans = 100000;
    m_maxSpriteSpans = 10000;
    
    // Allocate device memory for spans
    CUDA_CHECK_ERROR(cudaMalloc(&d_wallSpans, m_maxWallSpans * sizeof(CUDAWallSpan)));
    CUDA_CHECK_ERROR(cudaMalloc(&d_floorCeilingSpans, m_maxFloorCeilingSpans * sizeof(CUDAFloorCeilingSpan)));
    CUDA_CHECK_ERROR(cudaMalloc(&d_spriteSpans, m_maxSpriteSpans * sizeof(CUDASpriteSpan)));
    
    // Allocate host memory for spans
    h_wallSpans = new CUDAWallSpan[m_maxWallSpans];
    h_floorCeilingSpans = new CUDAFloorCeilingSpan[m_maxFloorCeilingSpans];
    h_spriteSpans = new CUDASpriteSpan[m_maxSpriteSpans];
    
    // Reset counts
    m_numWallSpans = 0;
    m_numFloorCeilingSpans = 0;
    m_numSpriteSpans = 0;
    
    // Set rendering parameters
    setFogEnabled(true);
    setFogColor(CUDAColor(0, 0, 0));
    setFogDistance(500.0f);
    setFogDensity(0.05f);
    setRenderDistance(1000.0f);
    setLightingEnabled(true);
    
    return true;
}

// Shutdown CUDA renderer
void CUDARenderer::shutdown() {
    // Free device memory
    if (d_pixelBuffer) {
        cudaFree(d_pixelBuffer);
        d_pixelBuffer = nullptr;
    }
    
    if (d_zBuffer) {
        cudaFree(d_zBuffer);
        d_zBuffer = nullptr;
    }
    
    if (d_wallSpans) {
        cudaFree(d_wallSpans);
        d_wallSpans = nullptr;
    }
    
    if (d_floorCeilingSpans) {
        cudaFree(d_floorCeilingSpans);
        d_floorCeilingSpans = nullptr;
    }
    
    if (d_spriteSpans) {
        cudaFree(d_spriteSpans);
        d_spriteSpans = nullptr;
    }
    
    // Free host memory
    if (h_wallSpans) {
        delete[] h_wallSpans;
        h_wallSpans = nullptr;
    }
    
    if (h_floorCeilingSpans) {
        delete[] h_floorCeilingSpans;
        h_floorCeilingSpans = nullptr;
    }
    
    if (h_spriteSpans) {
        delete[] h_spriteSpans;
        h_spriteSpans = nullptr;
    }
    
    // Free textures
    freeTextures();
}

// Clear the screen
void CUDARenderer::clear(const CUDAColor& color) {
    // Calculate grid and block dimensions
    dim3 blockSize(16, 16);
    dim3 gridSize((m_screenWidth + blockSize.x - 1) / blockSize.x, 
                 (m_screenHeight + blockSize.y - 1) / blockSize.y);
    
    // Launch kernel
    clearScreenKernel<<<gridSize, blockSize>>>(d_pixelBuffer, d_zBuffer, color);
    
    // Check for errors
    CUDA_CHECK_ERROR(cudaGetLastError());
}

// Set rendering parameters
void CUDARenderer::setRenderDistance(float distance) {
    m_renderDistance = distance;
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_renderDistance, &m_renderDistance, sizeof(float)));
}

void CUDARenderer::setFogEnabled(bool enabled) {
    m_fogEnabled = enabled;
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogEnabled, &m_fogEnabled, sizeof(bool)));
}

void CUDARenderer::setFogColor(const CUDAColor& color) {
    m_fogColor = color;
    // Copy each component separately 
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogColor_r, &color.r, sizeof(unsigned char)));
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogColor_g, &color.g, sizeof(unsigned char)));
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogColor_b, &color.b, sizeof(unsigned char)));
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogColor_a, &color.a, sizeof(unsigned char)));
}

void CUDARenderer::setFogDistance(float distance) {
    m_fogDistance = distance;
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogDistance, &m_fogDistance, sizeof(float)));
}

void CUDARenderer::setFogDensity(float density) {
    m_fogDensity = density;
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_fogDensity, &m_fogDensity, sizeof(float)));
}

void CUDARenderer::setLightingEnabled(bool enabled) {
    m_lightingEnabled = enabled;
    CUDA_CHECK_ERROR(cudaMemcpyToSymbol(d_lightingEnabled, &m_lightingEnabled, sizeof(bool)));
}

// Add a wall span for rendering
void CUDARenderer::addWallSpan(const CUDAWallSpan& span) {
    if (m_numWallSpans >= m_maxWallSpans) {
        fprintf(stderr, "Maximum wall spans reached\n");
        return;
    }
    
    h_wallSpans[m_numWallSpans++] = span;
}

// Add a floor/ceiling span for rendering
void CUDARenderer::addFloorCeilingSpan(const CUDAFloorCeilingSpan& span) {
    if (m_numFloorCeilingSpans >= m_maxFloorCeilingSpans) {
        fprintf(stderr, "Maximum floor/ceiling spans reached\n");
        return;
    }
    
    h_floorCeilingSpans[m_numFloorCeilingSpans++] = span;
}

// Add a sprite span for rendering
void CUDARenderer::addSpriteSpan(const CUDASpriteSpan& span) {
    if (m_numSpriteSpans >= m_maxSpriteSpans) {
        fprintf(stderr, "Maximum sprite spans reached\n");
        return;
    }
    
    h_spriteSpans[m_numSpriteSpans++] = span;
}

// Set textures
void CUDARenderer::setTextures(cudaTextureObject_t* textures, int numTextures) {
    // Free existing textures
    freeTextures();
    
    // Allocate device memory for texture array
    CUDA_CHECK_ERROR(cudaMalloc(&d_textures, numTextures * sizeof(cudaTextureObject_t)));
    
    // Copy texture objects to device
    CUDA_CHECK_ERROR(cudaMemcpy(d_textures, textures, numTextures * sizeof(cudaTextureObject_t), cudaMemcpyHostToDevice));
    
    m_numTextures = numTextures;
}

// Free textures
void CUDARenderer::freeTextures() {
    if (d_textures) {
        cudaFree(d_textures);
        d_textures = nullptr;
    }
    
    m_numTextures = 0;
}

// Begin rendering
void CUDARenderer::beginRender() {
    // Reset span counts
    m_numWallSpans = 0;
    m_numFloorCeilingSpans = 0;
    m_numSpriteSpans = 0;
}

// End rendering and copy result to host
void CUDARenderer::endRender(uint32_t* hostPixelBuffer) {
    // Upload wall spans to device
    if (m_numWallSpans > 0) {
        CUDA_CHECK_ERROR(cudaMemcpy(d_wallSpans, h_wallSpans, 
                                  m_numWallSpans * sizeof(CUDAWallSpan), 
                                  cudaMemcpyHostToDevice));
    }
    
    // Upload floor/ceiling spans to device
    if (m_numFloorCeilingSpans > 0) {
        CUDA_CHECK_ERROR(cudaMemcpy(d_floorCeilingSpans, h_floorCeilingSpans, 
                                  m_numFloorCeilingSpans * sizeof(CUDAFloorCeilingSpan), 
                                  cudaMemcpyHostToDevice));
    }
    
    // Upload sprite spans to device
    if (m_numSpriteSpans > 0) {
        CUDA_CHECK_ERROR(cudaMemcpy(d_spriteSpans, h_spriteSpans, 
                                  m_numSpriteSpans * sizeof(CUDASpriteSpan), 
                                  cudaMemcpyHostToDevice));
    }
    
    // Render wall spans
    if (m_numWallSpans > 0) {
        int blockSize = 256;
        int gridSize = (m_numWallSpans + blockSize - 1) / blockSize;
        
        renderWallSpansKernel<<<gridSize, blockSize>>>(d_wallSpans, m_numWallSpans, 
                                                    d_pixelBuffer, d_zBuffer, 
                                                    d_textures);
        CUDA_CHECK_ERROR(cudaGetLastError());
    }
    
    // Render floor/ceiling spans
    if (m_numFloorCeilingSpans > 0) {
        int blockSize = 256;
        int gridSize = (m_numFloorCeilingSpans + blockSize - 1) / blockSize;
        
        renderFloorCeilingSpansKernel<<<gridSize, blockSize>>>(d_floorCeilingSpans, 
                                                            m_numFloorCeilingSpans, 
                                                            d_pixelBuffer, d_zBuffer, 
                                                            d_textures);
        CUDA_CHECK_ERROR(cudaGetLastError());
    }
    
    // Render sprite spans
    if (m_numSpriteSpans > 0) {
        int blockSize = 256;
        int gridSize = (m_numSpriteSpans + blockSize - 1) / blockSize;
        
        renderSpriteSpansKernel<<<gridSize, blockSize>>>(d_spriteSpans, 
                                                       m_numSpriteSpans, 
                                                       d_pixelBuffer, d_zBuffer, 
                                                       d_textures);
        CUDA_CHECK_ERROR(cudaGetLastError());
    }
    
    // Copy result to host
    CUDA_CHECK_ERROR(cudaMemcpy(hostPixelBuffer, d_pixelBuffer, 
                              m_screenWidth * m_screenHeight * sizeof(uint32_t), 
                              cudaMemcpyDeviceToHost));
} 