// Define ENABLE_CUDA for CUDA support
#define ENABLE_CUDA

#ifdef ENABLE_CUDA

#include "renderer.h"
#include "map.h"
#include "utils.h"
#include "player.h"
#include "TextureManager.h"
#include "engine.h"
#include "CUDARenderer.h"
#include "CUDAConfig.h"
#include <iostream>

// Check if CUDA is available
bool Renderer::isCUDAAvailable() const {
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    
    if (error != cudaSuccess) {
        return false;
    }
    
    return deviceCount > 0;
}

// Initialize CUDA rendering
bool Renderer::initCUDA() {
    // Create CUDA renderer
    m_cudaRenderer = new CUDARenderer(m_screenWidth, m_screenHeight);
    
    // Initialize the renderer
    if (!m_cudaRenderer->init()) {
        std::cerr << "Failed to initialize CUDA renderer" << std::endl;
        delete m_cudaRenderer;
        m_cudaRenderer = nullptr;
        return false;
    }
    
    // Set rendering parameters
    m_cudaRenderer->setRenderDistance(m_renderDistance);
    m_cudaRenderer->setFogEnabled(m_fogEnabled);
    m_cudaRenderer->setFogColor(CUDAColor(m_fogColor.r, m_fogColor.g, m_fogColor.b));
    m_cudaRenderer->setFogDistance(m_fogDistance);
    m_cudaRenderer->setFogDensity(m_fogDensity);
    m_cudaRenderer->setLightingEnabled(m_lightingEnabled);
    
    // Create CUDA textures for all loaded textures
    if (!m_textureManager || !m_textureManager->createAllCUDATextures()) {
        std::cerr << "Failed to create CUDA textures" << std::endl;
        delete m_cudaRenderer;
        m_cudaRenderer = nullptr;
        return false;
    }
    
    // Set textures for the CUDA renderer
    m_cudaRenderer->setTextures(m_textureManager->getCUDATextureObjects(), m_textureManager->getTextureCount());
    
    m_cudaInitialized = true;
    return true;
}

// Render the map using CUDA
void Renderer::renderMapCUDA(Map* map, Camera* camera) {
    if (!map || !camera || !m_cudaRenderer) {
        return;
    }
    
    // Begin CUDA rendering
    m_cudaRenderer->beginRender();
    
    // Clear with background color
    m_cudaRenderer->clear(CUDAColor(m_backgroundColor.r, m_backgroundColor.g, m_backgroundColor.b));
    
    // Ensure all structures are enclosed before rendering
    ensureStructuresEnclosed(map);
    
    // Clear span lists
    m_wallSpans.clear();
    m_floorCeilingSpans.clear();
    m_spriteSpans.clear();
    
    // Get visible walls using BSP traversal
    std::vector<std::shared_ptr<Wall>> visibleWalls;
    map->getVisibleWalls(camera->getPosition(), visibleWalls);
    
    // Process visible walls to calculate spans
    processVisibleWalls(visibleWalls, camera);
    
    // Convert wall spans to CUDA format and add to CUDA renderer
    for (const auto& span : m_wallSpans) {
        CUDAWallSpan cudaSpan;
        convertWallSpanToCUDA(span, cudaSpan);
        m_cudaRenderer->addWallSpan(cudaSpan);
    }
    
    // Convert floor/ceiling spans to CUDA format and add to CUDA renderer
    for (const auto& span : m_floorCeilingSpans) {
        CUDAFloorCeilingSpan cudaSpan;
        convertFloorCeilingSpanToCUDA(span, cudaSpan);
        m_cudaRenderer->addFloorCeilingSpan(cudaSpan);
    }
    
    // Convert sprite spans to CUDA format and add to CUDA renderer
    for (const auto& span : m_spriteSpans) {
        CUDASpriteSpan cudaSpan;
        convertSpriteSpanToCUDA(span, cudaSpan);
        m_cudaRenderer->addSpriteSpan(cudaSpan);
    }
    
    // End CUDA rendering and copy result to pixel buffer
    m_cudaRenderer->endRender(m_pixelBuffer);
}

// Convert a wall span to CUDA format
void Renderer::convertWallSpanToCUDA(const WallSpan& span, CUDAWallSpan& cudaSpan) {
    cudaSpan.x = span.x;
    cudaSpan.y1 = span.y1;
    cudaSpan.y2 = span.y2;
    cudaSpan.texU = span.texU;
    cudaSpan.distance = span.distance;
    cudaSpan.textureId = span.textureId;
    cudaSpan.isPortal = span.isPortal;
    cudaSpan.isFlipped = span.isFlipped;
    cudaSpan.lightLevel = span.lightLevel;
}

// Convert a floor/ceiling span to CUDA format
void Renderer::convertFloorCeilingSpanToCUDA(const FloorCeilingSpan& span, CUDAFloorCeilingSpan& cudaSpan) {
    cudaSpan.x = span.x;
    cudaSpan.y = span.y;
    cudaSpan.u = span.u;
    cudaSpan.v = span.v;
    cudaSpan.distance = span.distance;
    cudaSpan.textureId = span.textureId;
    cudaSpan.isFloor = span.isFloor;
    cudaSpan.lightLevel = span.lightLevel;
}

// Convert a sprite span to CUDA format
void Renderer::convertSpriteSpanToCUDA(const SpriteSpan& span, CUDASpriteSpan& cudaSpan) {
    cudaSpan.x = span.x;
    cudaSpan.y1 = span.y1;
    cudaSpan.y2 = span.y2;
    cudaSpan.u = span.u;
    cudaSpan.distance = span.distance;
    cudaSpan.textureId = span.textureId;
    cudaSpan.lightLevel = span.lightLevel;
}

#endif // ENABLE_CUDA 