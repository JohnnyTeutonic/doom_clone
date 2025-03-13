#ifndef CUDA_RENDERER_H
#define CUDA_RENDERER_H

// Ensure ENABLE_CUDA is defined
#define ENABLE_CUDA

#include "CUDAUtils.h"
#include <cstdint>
#include <cfloat>

// Forward declarations needed by CUDA
class Map;
class Camera;
class Sector;
class Wall;
class Sprite;

// CUDA version of wall span structure
struct CUDAWallSpan {
    int x;                  // Screen x-coordinate
    int y1, y2;             // Top and bottom y-coordinates
    float texU;             // Texture u-coordinate
    float distance;         // Distance to wall
    int textureId;          // Wall texture ID
    bool isPortal;          // Whether this span is a portal
    bool isFlipped;         // Whether the texture should be horizontally flipped
    float lightLevel;       // Light level for this span
    
    CUDAWallSpan() : 
        x(0), y1(0), y2(0), texU(0.0f), 
        distance(0.0f), textureId(-1), isPortal(false), 
        isFlipped(false), lightLevel(1.0f) {}
};

// CUDA version of floor/ceiling span structure
struct CUDAFloorCeilingSpan {
    int x;                  // Screen x-coordinate
    int y;                  // Screen y-coordinate
    float u, v;             // Texture coordinates
    float distance;         // Distance to point
    int textureId;          // Texture ID
    bool isFloor;           // Whether this is a floor (true) or ceiling (false)
    float lightLevel;       // Light level for this span
    
    CUDAFloorCeilingSpan() : 
        x(0), y(0), u(0.0f), v(0.0f), 
        distance(0.0f), textureId(-1), isFloor(true), 
        lightLevel(1.0f) {}
};

// CUDA version of sprite span structure
struct CUDASpriteSpan {
    int x;                  // Screen x-coordinate
    int y1, y2;             // Top and bottom y-coordinates
    float u;                // Texture u-coordinate
    float distance;         // Distance to sprite
    int textureId;          // Sprite texture ID
    float lightLevel;       // Light level for this span
    
    CUDASpriteSpan() : 
        x(0), y1(0), y2(0), u(0.0f), 
        distance(0.0f), textureId(-1), lightLevel(1.0f) {}
};

// CUDA renderer class
class CUDARenderer {
public:
    CUDARenderer(int screenWidth, int screenHeight) :
        m_screenWidth(screenWidth),
        m_screenHeight(screenHeight),
        d_pixelBuffer(nullptr),
        d_zBuffer(nullptr),
        d_textures(nullptr),
        d_wallSpans(nullptr),
        d_floorCeilingSpans(nullptr),
        d_spriteSpans(nullptr),
        h_wallSpans(nullptr),
        h_floorCeilingSpans(nullptr),
        h_spriteSpans(nullptr),
        m_numTextures(0),
        m_numWallSpans(0),
        m_numFloorCeilingSpans(0),
        m_numSpriteSpans(0),
        m_maxWallSpans(0),
        m_maxFloorCeilingSpans(0),
        m_maxSpriteSpans(0),
        m_renderDistance(1000.0f),
        m_fogEnabled(true),
        m_fogColor(0, 0, 0),
        m_fogDistance(500.0f),
        m_fogDensity(0.05f),
        m_lightingEnabled(true) {}
    
    ~CUDARenderer() {
        shutdown();
    }
    
    // Initialize the CUDA renderer
    bool init();
    
    // Shutdown and free resources
    void shutdown();
    
    // Clear the screen
    void clear(const CUDAColor& color = CUDAColor(0, 0, 0));
    
    // Set rendering options
    void setRenderDistance(float distance);
    void setFogEnabled(bool enabled);
    void setFogColor(const CUDAColor& color);
    void setFogDistance(float distance);
    void setFogDensity(float density);
    void setLightingEnabled(bool enabled);
    
    // Add spans for rendering
    void addWallSpan(const CUDAWallSpan& span);
    void addFloorCeilingSpan(const CUDAFloorCeilingSpan& span);
    void addSpriteSpan(const CUDASpriteSpan& span);
    
    // Set textures for rendering
    void setTextures(cudaTextureObject_t* textures, int numTextures);
    
    // Free textures
    void freeTextures();
    
    // Begin rendering process
    void beginRender();
    
    // End rendering and copy result to host
    void endRender(uint32_t* hostPixelBuffer);
    
private:
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // Device memory
    uint32_t* d_pixelBuffer;     // Pixel buffer on device
    float* d_zBuffer;            // Z-buffer on device
    cudaTextureObject_t* d_textures; // Texture objects on device
    CUDAWallSpan* d_wallSpans;   // Wall spans on device
    CUDAFloorCeilingSpan* d_floorCeilingSpans; // Floor/ceiling spans on device
    CUDASpriteSpan* d_spriteSpans; // Sprite spans on device
    
    // Host memory for spans
    CUDAWallSpan* h_wallSpans;   // Wall spans on host
    CUDAFloorCeilingSpan* h_floorCeilingSpans; // Floor/ceiling spans on host
    CUDASpriteSpan* h_spriteSpans; // Sprite spans on host
    
    // Texture count
    int m_numTextures;
    
    // Span counts
    int m_numWallSpans;
    int m_numFloorCeilingSpans;
    int m_numSpriteSpans;
    
    // Maximum span counts
    int m_maxWallSpans;
    int m_maxFloorCeilingSpans;
    int m_maxSpriteSpans;
    
    // Rendering parameters
    float m_renderDistance;
    bool m_fogEnabled;
    CUDAColor m_fogColor;
    float m_fogDistance;
    float m_fogDensity;
    bool m_lightingEnabled;
};

#endif // CUDA_RENDERER_H 