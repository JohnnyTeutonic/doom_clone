#pragma once

#include <SDL2/SDL.h>
#include <cuda_runtime.h>
#include <vector>
#include "utils.h"
#include "map.h"
#include "player.h"
#include "texture.h"

// Structure to hold player data for CUDA
struct PlayerData {
    float posX;
    float posY;
    float dirX;
    float dirY;
    float planeX;
    float planeY;
    float verticalAngle;  // Look up/down angle
    float jumpHeight;     // Current jump height
};

// Forward declaration of CUDA wrapper function
extern "C" void launchRaycastKernel(
    dim3 gridSize,
    dim3 blockSize,
    uint32_t* frameBuffer,
    float* zBuffer,
    int screenWidth,
    int screenHeight,
    int* mapData,
    int mapWidth,
    int mapHeight,
    PlayerData* playerData,
    uint32_t* wallTextures,
    uint32_t* floorTextures,
    uint32_t* ceilingTextures,
    int textureWidth,
    int textureHeight
);

// CUDA Renderer class for hardware-accelerated rendering
class CudaRenderer {
private:
    int m_screenWidth;
    int m_screenHeight;
    
    // SDL resources
    SDL_Renderer* m_sdlRenderer;
    SDL_Texture* m_frameTexture;
    TextureManager* m_textureManager;
    
    // Host resources
    uint32_t* m_hostFrameBuffer;
    float* m_hostZBuffer;
    
    // Device resources
    uint32_t* m_deviceFrameBuffer;
    float* m_deviceZBuffer;
    int* m_deviceMapData;
    PlayerData* m_devicePlayerData;
    
    // Texture resources
    uint32_t* m_deviceWallTextures;
    uint32_t* m_deviceFloorTextures;
    uint32_t* m_deviceCeilingTextures;
    int m_wallTextureWidth;
    int m_wallTextureHeight;
    
    // CUDA stream
    cudaStream_t m_cudaStream;
    
    // Frame generation state
    bool m_frameReady;
    
    // Initialize CUDA resources
    bool initCuda();
    
    // Copy map and texture data to device
    void copyMapToDevice(const Map& map, const Player& player);
    void copyTexturesToDevice();
    
    void renderSprites(const Map& map, const Player& player);
    void renderUI(const Player& player);
    
public:
    CudaRenderer();
    ~CudaRenderer();
    
    // Initialize the renderer
    bool init(int screenWidth, int screenHeight, SDL_Renderer* sdlRenderer, TextureManager* textureManager);
    
    // Clean up resources
    void cleanup();
    
    // Generate a frame (process the ray-casting)
    void generateFrame(const Map& map, const Player& player);
    
    // Blit the frame buffer to the screen
    void blitFrameBuffer();
    
    // Original render method (now split into generateFrame and blitFrameBuffer)
    void render(const Map& map, const Player& player);
    
    // Check if CUDA is available
    static bool isCudaAvailable();
}; 