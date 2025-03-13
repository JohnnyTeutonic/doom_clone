// Define ENABLE_CUDA for compiling with CUDA support
#define ENABLE_CUDA

#ifdef ENABLE_CUDA

#include "TextureManager.h"
#include "CUDAUtils.h"
#include <iostream>

// Create CUDA texture for a loaded texture
bool Texture::createCUDATexture() {
    // Ensure the texture has pixel data
    if (m_pixels.empty() || m_width <= 0 || m_height <= 0) {
        std::cerr << "Cannot create CUDA texture: no pixel data" << std::endl;
        return false;
    }
    
    // Create CUDA texture using the global function from CUDAUtils.h
    return ::createCUDATexture(&m_cudaTexture, m_pixels.data(), m_width, m_height);
}

// Create CUDA textures for all loaded textures
bool TextureManager::createAllCUDATextures() {
    // Clear existing CUDA texture objects
    freeCUDATextureObjects();
    
    // Resize the CUDA texture objects vector
    m_cudaTextureObjects.resize(m_textures.size());
    
    // Create CUDA textures for each texture
    for (size_t i = 0; i < m_textures.size(); ++i) {
        if (!m_textures[i]->createCUDATexture()) {
            std::cerr << "Failed to create CUDA texture for texture " << i << std::endl;
            return false;
        }
        
        // Store the texture object
        m_cudaTextureObjects[i] = m_textures[i]->getCUDATextureObject();
    }
    
    // Allocate device memory for texture objects array
    if (m_deviceTextureObjects) {
        cudaFree(m_deviceTextureObjects);
        m_deviceTextureObjects = nullptr;
    }
    
    // Allocate device memory for texture objects
    CUDA_CHECK_ERROR(cudaMalloc(&m_deviceTextureObjects, m_cudaTextureObjects.size() * sizeof(cudaTextureObject_t)));
    
    // Copy texture objects to device
    CUDA_CHECK_ERROR(cudaMemcpy(m_deviceTextureObjects, m_cudaTextureObjects.data(), 
                              m_cudaTextureObjects.size() * sizeof(cudaTextureObject_t),
                              cudaMemcpyHostToDevice));
    
    return true;
}

// Get all CUDA texture objects
cudaTextureObject_t* TextureManager::getCUDATextureObjects() {
    return m_deviceTextureObjects;
}

// Free CUDA texture objects
void TextureManager::freeCUDATextureObjects() {
    // Free device memory
    if (m_deviceTextureObjects) {
        cudaFree(m_deviceTextureObjects);
        m_deviceTextureObjects = nullptr;
    }
    
    // Clear the vector of texture objects
    m_cudaTextureObjects.clear();
}

#endif // ENABLE_CUDA 