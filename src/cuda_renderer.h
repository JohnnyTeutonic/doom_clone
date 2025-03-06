#pragma once

#include <SDL2/SDL.h>
#include <cuda_runtime.h>
#include <vector>
#include "utils.h"
#include "map.h"
#include "player.h"
#include "texture.h"
#include "lighting.h"  // Include lighting system header

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

// Light types for CUDA
enum class CudaLightType {
    Point = 0,       // Light emanating from a point
    Directional = 1, // Light coming from a direction
    Flickering = 2,  // Light that flickers
    Pulsing = 3,     // Light that pulses
    Strobe = 4,      // Light that strobes on and off
    Glow = 5         // Ambient glow
};

// Structure to hold light data for CUDA
struct CudaLight {
    int type;  // Use int instead of CudaLightType for CUDA compatibility
    float posX;
    float posY;
    float dirX;
    float dirY;
    float r;
    float g;
    float b;
    float intensity;
    float radius;
    float effectSpeed;
    float effectIntensity;
    float effectTimer;
    int enabled;
};

// Structure to hold ambient light data
struct CudaAmbientLight {
    float r;
    float g;
    float b;
    float intensity;
};

// Maximum number of lights supported in CUDA kernel
#define MAX_CUDA_LIGHTS 32

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
    int textureHeight,
    CudaLight* lights,          // Add light array parameter
    int numLights,              // Add number of lights parameter
    CudaAmbientLight* ambient   // Add ambient light parameter
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
    SpriteManager* m_spriteManager;  // Add sprite manager pointer
    
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
    CudaLight* m_deviceLights;          // Add device memory for lights
    CudaAmbientLight* m_deviceAmbient;  // Add device memory for ambient light
    int m_wallTextureWidth;
    int m_wallTextureHeight;
    int m_numActiveLights;              // Number of active lights
    
    // CUDA stream
    cudaStream_t m_cudaStream;
    
    // Frame generation state
    bool m_frameReady;
    
    // Initialize CUDA resources
    bool initCuda();
    
    // Copy map and texture data to device
    void copyMapToDevice(const Map& map, const Player& player);
    void copyTexturesToDevice();
    void copyLightsToDevice(const LightingSystem& lightingSystem); // Add method to copy lights
    
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
    
    void renderSprites(const Map& map, const Player& player);

    // Check if CUDA is available
    static bool isCudaAvailable();
    
    // Debug methods
    void printDeviceInfo() const;
    
    // Set sprite manager
    void setSpriteManager(SpriteManager* spriteManager) { m_spriteManager = spriteManager; }
}; 