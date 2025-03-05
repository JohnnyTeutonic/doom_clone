#include "cuda_renderer.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include "sprite.h"

// Forward declaration of the CUDA kernel function defined in cuda_renderer.cu
extern "C" __global__ void raycastKernel(
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

// Forward declaration of the CUDA error checking function defined in cuda_renderer.cu
extern "C" void checkCudaError(cudaError_t error, const char* message);

CudaRenderer::CudaRenderer()
    : m_screenWidth(0)
    , m_screenHeight(0)
    , m_sdlRenderer(nullptr)
    , m_frameTexture(nullptr)
    , m_textureManager(nullptr)
    , m_hostFrameBuffer(nullptr)
    , m_hostZBuffer(nullptr)
    , m_deviceFrameBuffer(nullptr)
    , m_deviceZBuffer(nullptr)
    , m_deviceMapData(nullptr)
    , m_devicePlayerData(nullptr)
    , m_deviceWallTextures(nullptr)
    , m_deviceFloorTextures(nullptr)
    , m_deviceCeilingTextures(nullptr)
    , m_wallTextureWidth(0)
    , m_wallTextureHeight(0)
    , m_cudaStream(0)
{
}

CudaRenderer::~CudaRenderer() {
    cleanup();
}

bool CudaRenderer::init(int screenWidth, int screenHeight, SDL_Renderer* sdlRenderer, TextureManager* textureManager) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_sdlRenderer = sdlRenderer;
    m_textureManager = textureManager;
    
    // Create frame texture
    m_frameTexture = SDL_CreateTexture(
        m_sdlRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_screenWidth,
        m_screenHeight
    );
    
    if (!m_frameTexture) {
        std::cerr << "Failed to create frame texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Allocate host memory
    m_hostFrameBuffer = new uint32_t[m_screenWidth * m_screenHeight];
    m_hostZBuffer = new float[m_screenWidth * m_screenHeight];
    
    // Initialize CUDA
    if (!initCuda()) {
        std::cerr << "Failed to initialize CUDA" << std::endl;
        return false;
    }
    
    // Allocate device memory
    cudaError_t cudaStatus;
    
    // Allocate frame buffer
    cudaStatus = cudaMalloc((void**)&m_deviceFrameBuffer, m_screenWidth * m_screenHeight * sizeof(uint32_t));
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Allocate Z-buffer
    cudaStatus = cudaMalloc((void**)&m_deviceZBuffer, m_screenWidth * m_screenHeight * sizeof(float));
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Allocate map data
    cudaStatus = cudaMalloc((void**)&m_deviceMapData, 1024 * 1024 * sizeof(int)); // Assuming max map size of 1024x1024
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Allocate player data
    cudaStatus = cudaMalloc((void**)&m_devicePlayerData, sizeof(PlayerData));
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Create CUDA stream
    cudaStatus = cudaStreamCreate(&m_cudaStream);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaStreamCreate failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Load textures
    m_wallTextureWidth = 64;  // Assuming 64x64 textures
    m_wallTextureHeight = 64;
    
    // Allocate texture memory on device
    // For wall textures, allocate space for multiple textures (at least 4)
    size_t wallTextureSize = 4 * m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t); // Space for 4 wall textures
    size_t floorCeilingTextureSize = m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t);
    
    cudaStatus = cudaMalloc((void**)&m_deviceWallTextures, wallTextureSize);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed for wall textures: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    cudaStatus = cudaMalloc((void**)&m_deviceFloorTextures, floorCeilingTextureSize);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed for floor textures: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    cudaStatus = cudaMalloc((void**)&m_deviceCeilingTextures, floorCeilingTextureSize);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaMalloc failed for ceiling textures: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    return true;
}

void CudaRenderer::cleanup() {
    // Free SDL resources
    if (m_frameTexture) {
        SDL_DestroyTexture(m_frameTexture);
        m_frameTexture = nullptr;
    }
    
    // Free host resources
    if (m_hostFrameBuffer) {
        delete[] m_hostFrameBuffer;
        m_hostFrameBuffer = nullptr;
    }
    
    if (m_hostZBuffer) {
        delete[] m_hostZBuffer;
        m_hostZBuffer = nullptr;
    }
    
    // Free device resources
    if (m_deviceFrameBuffer) {
        cudaFree(m_deviceFrameBuffer);
        m_deviceFrameBuffer = nullptr;
    }
    
    if (m_deviceZBuffer) {
        cudaFree(m_deviceZBuffer);
        m_deviceZBuffer = nullptr;
    }
    
    if (m_deviceMapData) {
        cudaFree(m_deviceMapData);
        m_deviceMapData = nullptr;
    }
    
    if (m_devicePlayerData) {
        cudaFree(m_devicePlayerData);
        m_devicePlayerData = nullptr;
    }
    
    // Free texture resources
    if (m_deviceWallTextures) {
        cudaFree(m_deviceWallTextures);
        m_deviceWallTextures = nullptr;
    }
    
    if (m_deviceFloorTextures) {
        cudaFree(m_deviceFloorTextures);
        m_deviceFloorTextures = nullptr;
    }
    
    if (m_deviceCeilingTextures) {
        cudaFree(m_deviceCeilingTextures);
        m_deviceCeilingTextures = nullptr;
    }
    
    // Destroy CUDA stream
    if (m_cudaStream) {
        cudaStreamDestroy(m_cudaStream);
        m_cudaStream = 0;
    }
    
    // Reset device
    cudaDeviceReset();
}

bool CudaRenderer::initCuda() {
    // Check if CUDA is available
    int deviceCount = 0;
    cudaError_t cudaStatus = cudaGetDeviceCount(&deviceCount);
    
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaGetDeviceCount failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    if (deviceCount == 0) {
        std::cerr << "No CUDA-capable devices found" << std::endl;
        return false;
    }
    
    // Select the first CUDA device
    cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaSetDevice failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Get device properties
    cudaDeviceProp deviceProp;
    cudaStatus = cudaGetDeviceProperties(&deviceProp, 0);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaGetDeviceProperties failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return false;
    }
    
    // Print device info
    std::cout << "Using CUDA device: " << deviceProp.name << std::endl;
    std::cout << "Compute capability: " << deviceProp.major << "." << deviceProp.minor << std::endl;
    std::cout << "Total global memory: " << deviceProp.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
    
    return true;
}

void CudaRenderer::copyMapToDevice(const Map& map, const Player& player) {
    if (!m_deviceMapData) return;
    
    // Get map dimensions
    int width = map.getWidth();
    int height = map.getHeight();
    
    // Create host buffer for map data
    int* hostMapData = new int[width * height];
    
    // Get visible sectors
    const std::vector<int>& visibleSectors = map.getVisibleSectors();
    
    // Debug: Count walls with each texture ID
    int wallCounts[4] = {0, 0, 0, 0};
    
    // Fill host buffer with map data, applying sector culling
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            CellType cellType = map.getCell(x, y);
            int cellValue = static_cast<int>(cellType);
            int sectorId = map.getSectorId(x, y);
            
            // If sector is not visible, mark as empty space (0) for rendering
            if (std::find(visibleSectors.begin(), visibleSectors.end(), sectorId) == visibleSectors.end()) {
                hostMapData[y * width + x] = 0; // Mark as empty for rendering
            } else {
                // For wall cells, encode the texture ID in the cell value
                if (cellType == CellType::Wall) {
                    // Get the wall texture ID (0-3) and add 1 to it (to make it 1-4)
                    // Then multiply by 100 and add the cell type (1 for wall)
                    // This way, the cell value will be 101, 201, 301, or 401 for walls with different textures
                    int textureId = map.getWallTexture(x, y);
                    
                    // Count walls with each texture ID
                    if (textureId >= 0 && textureId < 4) {
                        wallCounts[textureId]++;
                    }
                    
                    hostMapData[y * width + x] = ((textureId + 1) * 100) + cellValue;
                    
                    // Debug output for more walls
                    if ((x % 10 == 0 && y % 10 == 0) || (x == 10 && y == 10)) {
                        std::cout << "Wall at (" << x << "," << y << ") has texture ID " << textureId 
                                  << " and cell value " << hostMapData[y * width + x] << std::endl;
                    }
                } else {
                    hostMapData[y * width + x] = cellValue;
                }
            }
        }
    }
    
    // Debug: Print wall counts
    std::cout << "Wall texture counts:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "  Texture ID " << i << ": " << wallCounts[i] << " walls" << std::endl;
    }
    
    // Copy map data to device
    cudaMemcpy(m_deviceMapData, hostMapData, width * height * sizeof(int), cudaMemcpyHostToDevice);
    
    // Free host buffer
    delete[] hostMapData;
    
    // Update player position and direction on device
    PlayerData hostPlayerData;
    hostPlayerData.posX = player.getX();
    hostPlayerData.posY = player.getY();
    hostPlayerData.dirX = player.getDirX();
    hostPlayerData.dirY = player.getDirY();
    hostPlayerData.planeX = player.getPlaneX();
    hostPlayerData.planeY = player.getPlaneY();
    
    cudaMemcpy(m_devicePlayerData, &hostPlayerData, sizeof(PlayerData), cudaMemcpyHostToDevice);
}

void CudaRenderer::copyTexturesToDevice() {
    // Check if texture manager is available
    if (!m_textureManager) {
        std::cerr << "Error: Texture manager is null" << std::endl;
        return;
    }
    
    // Create temporary buffers for texture data
    const int numWallTextures = 4; // Load 4 wall textures (IDs 0-3)
    uint32_t* wallTextureData = new uint32_t[numWallTextures * m_wallTextureWidth * m_wallTextureHeight];
    uint32_t* floorTextureData = new uint32_t[m_wallTextureWidth * m_wallTextureHeight];
    uint32_t* ceilingTextureData = new uint32_t[m_wallTextureWidth * m_wallTextureHeight];
    
    // Initialize texture data to default colors in case of errors
    for (int i = 0; i < numWallTextures * m_wallTextureWidth * m_wallTextureHeight; i++) {
        wallTextureData[i] = 0xFF808080;  // Gray
    }
    
    for (int i = 0; i < m_wallTextureWidth * m_wallTextureHeight; i++) {
        floorTextureData[i] = 0xFF404040;  // Dark gray
        ceilingTextureData[i] = 0xFF606060;  // Medium gray
    }
    
    // Load wall textures (IDs 0-3)
    bool wallTexturesLoaded = false;
    for (int texId = 0; texId < numWallTextures; texId++) {
        SDL_Texture* wallTexture = m_textureManager->getSDLTexture(texId);
        if (!wallTexture) {
            std::cerr << "Wall texture ID " << texId << " not found" << std::endl;
            continue;
        }
        
        // Create temporary surface for this wall texture
        SDL_Surface* wallSurface = SDL_CreateRGBSurface(0, m_wallTextureWidth, m_wallTextureHeight, 32,
                                                      0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
        if (!wallSurface) {
            std::cerr << "Failed to create temporary surface for wall texture " << texId << ": " << SDL_GetError() << std::endl;
            continue;
        }
        
        // Create temporary render target
        SDL_Texture* wallTarget = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                                  SDL_TEXTUREACCESS_TARGET, m_wallTextureWidth, m_wallTextureHeight);
        if (!wallTarget) {
            std::cerr << "Failed to create temporary render target for wall texture " << texId << ": " << SDL_GetError() << std::endl;
            SDL_FreeSurface(wallSurface);
            continue;
        }
        
        // Save current render target
        SDL_Texture* oldTarget = SDL_GetRenderTarget(m_sdlRenderer);
        
        // Copy wall texture data
        SDL_SetRenderTarget(m_sdlRenderer, wallTarget);
        SDL_RenderCopy(m_sdlRenderer, wallTexture, NULL, NULL);
        SDL_RenderReadPixels(m_sdlRenderer, NULL, SDL_PIXELFORMAT_ARGB8888, wallSurface->pixels, wallSurface->pitch);
        
        // Restore original render target
        SDL_SetRenderTarget(m_sdlRenderer, oldTarget);
        
        // Copy data from surface to our texture data array
        SDL_LockSurface(wallSurface);
        
        // Debug: Print some pixel values to verify texture data
        std::cout << "Texture ID " << texId << " sample pixels:" << std::endl;
        
        for (int y = 0; y < m_wallTextureHeight; y++) {
            for (int x = 0; x < m_wallTextureWidth; x++) {
                int srcIndex = y * (wallSurface->pitch / 4) + x;
                int destIndex = (texId * m_wallTextureHeight + y) * m_wallTextureWidth + x;
                Uint32* wallPixels = (Uint32*)wallSurface->pixels;
                
                wallTextureData[destIndex] = wallPixels[srcIndex];
                
            }
        }
        
        SDL_UnlockSurface(wallSurface);
        SDL_FreeSurface(wallSurface);
        SDL_DestroyTexture(wallTarget);
        
        wallTexturesLoaded = true;
        std::cout << "Successfully loaded wall texture ID " << texId << std::endl;
    }
    
    // Get floor texture (ID 1)
    SDL_Texture* floorTexture = m_textureManager->getSDLTexture(1);
    if (!floorTexture) {
        std::cerr << "Floor texture not found" << std::endl;
    }
    
    // Get ceiling texture (ID 2)
    SDL_Texture* ceilingTexture = m_textureManager->getSDLTexture(2);
    if (!ceilingTexture) {
        std::cerr << "Ceiling texture not found" << std::endl;
    }
    
    // Process floor and ceiling textures if they exist
    bool floorCeilingLoaded = false;
    if (floorTexture && ceilingTexture) {
        // Create temporary surfaces
        SDL_Surface* floorSurface = SDL_CreateRGBSurface(0, m_wallTextureWidth, m_wallTextureHeight, 32,
                                                       0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
        SDL_Surface* ceilingSurface = SDL_CreateRGBSurface(0, m_wallTextureWidth, m_wallTextureHeight, 32,
                                                         0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
        
        if (!floorSurface || !ceilingSurface) {
            std::cerr << "Failed to create temporary surfaces for floor/ceiling: " << SDL_GetError() << std::endl;
        } else {
            // Create temporary render targets
            SDL_Texture* floorTarget = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                                       SDL_TEXTUREACCESS_TARGET, m_wallTextureWidth, m_wallTextureHeight);
            SDL_Texture* ceilingTarget = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                                         SDL_TEXTUREACCESS_TARGET, m_wallTextureWidth, m_wallTextureHeight);
            
            if (!floorTarget || !ceilingTarget) {
                std::cerr << "Failed to create temporary render targets for floor/ceiling: " << SDL_GetError() << std::endl;
            } else {
                // Save current render target
                SDL_Texture* oldTarget = SDL_GetRenderTarget(m_sdlRenderer);
                
                // Copy floor texture data
                SDL_SetRenderTarget(m_sdlRenderer, floorTarget);
                SDL_RenderCopy(m_sdlRenderer, floorTexture, NULL, NULL);
                SDL_RenderReadPixels(m_sdlRenderer, NULL, SDL_PIXELFORMAT_ARGB8888, floorSurface->pixels, floorSurface->pitch);
                
                // Copy ceiling texture data
                SDL_SetRenderTarget(m_sdlRenderer, ceilingTarget);
                SDL_RenderCopy(m_sdlRenderer, ceilingTexture, NULL, NULL);
                SDL_RenderReadPixels(m_sdlRenderer, NULL, SDL_PIXELFORMAT_ARGB8888, ceilingSurface->pixels, ceilingSurface->pitch);
                
                // Restore original render target
                SDL_SetRenderTarget(m_sdlRenderer, oldTarget);
                
                // Copy data from surfaces to our texture data arrays
                SDL_LockSurface(floorSurface);
                SDL_LockSurface(ceilingSurface);
                
                for (int y = 0; y < m_wallTextureHeight; y++) {
                    for (int x = 0; x < m_wallTextureWidth; x++) {
                        int index = y * m_wallTextureWidth + x;
                        Uint32* floorPixels = (Uint32*)floorSurface->pixels;
                        Uint32* ceilingPixels = (Uint32*)ceilingSurface->pixels;
                        
                        floorTextureData[index] = floorPixels[y * (floorSurface->pitch / 4) + x];
                        ceilingTextureData[index] = ceilingPixels[y * (ceilingSurface->pitch / 4) + x];
                    }
                }
                
                SDL_UnlockSurface(floorSurface);
                SDL_UnlockSurface(ceilingSurface);
                
                SDL_DestroyTexture(floorTarget);
                SDL_DestroyTexture(ceilingTarget);
                
                floorCeilingLoaded = true;
                std::cout << "Successfully loaded floor and ceiling textures" << std::endl;
            }
            
            SDL_FreeSurface(floorSurface);
            SDL_FreeSurface(ceilingSurface);
        }
    }
    
    // Copy texture data to device
    cudaError_t cudaStatus;
    
    if (wallTexturesLoaded) {
        cudaStatus = cudaMemcpy(m_deviceWallTextures, wallTextureData, 
                               numWallTextures * m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t), 
                               cudaMemcpyHostToDevice);
        if (cudaStatus != cudaSuccess) {
            std::cerr << "Failed to copy wall textures to device: " << cudaGetErrorString(cudaStatus) << std::endl;
        } else {
            std::cout << "Successfully copied " << numWallTextures << " wall textures to device" << std::endl;
        }
    }
    
    if (floorCeilingLoaded) {
        cudaStatus = cudaMemcpy(m_deviceFloorTextures, floorTextureData, 
                               m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t), 
                               cudaMemcpyHostToDevice);
        if (cudaStatus != cudaSuccess) {
            std::cerr << "Failed to copy floor texture to device: " << cudaGetErrorString(cudaStatus) << std::endl;
        }
        
        cudaStatus = cudaMemcpy(m_deviceCeilingTextures, ceilingTextureData, 
                               m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t), 
                               cudaMemcpyHostToDevice);
        if (cudaStatus != cudaSuccess) {
            std::cerr << "Failed to copy ceiling texture to device: " << cudaGetErrorString(cudaStatus) << std::endl;
        }
    }
    
    // Free temporary buffers
    delete[] wallTextureData;
    delete[] floorTextureData;
    delete[] ceilingTextureData;
}

void CudaRenderer::render(const Map& map, const Player& player) {
    // Copy map data to device
    copyMapToDevice(map, player);
    
    // Copy textures to device if needed
    copyTexturesToDevice();
    
    // Set up kernel launch parameters
    dim3 blockSize(16, 16);
    dim3 gridSize((m_screenWidth + blockSize.x - 1) / blockSize.x, 
                  (m_screenHeight + blockSize.y - 1) / blockSize.y);
    
    // Launch kernel using a wrapper function
    launchRaycastKernel(
        gridSize,
        blockSize,
        m_deviceFrameBuffer,
        m_deviceZBuffer,
        m_screenWidth,
        m_screenHeight,
        m_deviceMapData,
        map.getWidth(),
        map.getHeight(),
        m_devicePlayerData,
        m_deviceWallTextures,
        m_deviceFloorTextures,
        m_deviceCeilingTextures,
        m_wallTextureWidth,
        m_wallTextureHeight
    );
    
    // Check for kernel errors
    cudaError_t cudaStatus = cudaGetLastError();
    if (cudaStatus != cudaSuccess) {
        std::cerr << "Kernel launch failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return;
    }
    
    // Wait for kernel to finish
    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaDeviceSynchronize failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        return;
    }
    
    // Copy frame buffer back to host
    cudaMemcpy(m_hostFrameBuffer, m_deviceFrameBuffer, m_screenWidth * m_screenHeight * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    
    // Update SDL texture with frame buffer
    SDL_UpdateTexture(m_frameTexture, NULL, m_hostFrameBuffer, m_screenWidth * sizeof(uint32_t));
    
    // Render the texture to the screen
    SDL_RenderCopy(m_sdlRenderer, m_frameTexture, NULL, NULL);
}

void CudaRenderer::renderSprites(const Map& map, const Player& player) {
    // Get visible sectors
    const std::vector<int>& visibleSectors = map.getVisibleSectors();
    
    // Get sprites from sprite manager
    const std::vector<Sprite*>& sprites = SpriteManager::getInstance()->getSprites();
    
    // Sort sprites by distance (furthest first for correct alpha blending)
    std::vector<std::pair<float, Sprite*>> sortedSprites;
    for (auto sprite : sprites) {
        // Skip sprites in non-visible sectors
        int spriteSectorId = map.getSectorId(static_cast<int>(sprite->getX()), static_cast<int>(sprite->getY()));
        if (std::find(visibleSectors.begin(), visibleSectors.end(), spriteSectorId) == visibleSectors.end()) {
            continue;
        }
        
        // Calculate distance to player
        float dx = sprite->getX() - player.getX();
        float dy = sprite->getY() - player.getY();
        float distance = dx * dx + dy * dy;  // Squared distance is enough for sorting
        
        sortedSprites.push_back(std::make_pair(distance, sprite));
    }
    
    // Sort sprites by distance (furthest first)
    std::sort(sortedSprites.begin(), sortedSprites.end(), 
        [](const std::pair<float, Sprite*>& a, const std::pair<float, Sprite*>& b) {
            return a.first > b.first;
        });
    
    // Render sprites using SDL (for now, could be moved to CUDA in the future)
    for (auto& pair : sortedSprites) {
        Sprite* sprite = pair.second;
        
        // Calculate sprite position relative to camera
        float spriteX = sprite->getX() - player.getX();
        float spriteY = sprite->getY() - player.getY();
        
        // Transform sprite with the inverse camera matrix
        float invDet = 1.0f / (player.getPlaneX() * player.getDirY() - player.getDirX() * player.getPlaneY());
        float transformX = invDet * (player.getDirY() * spriteX - player.getDirX() * spriteY);
        float transformY = invDet * (-player.getPlaneY() * spriteX + player.getPlaneX() * spriteY);
        
        // Calculate sprite screen position
        int spriteScreenX = static_cast<int>((m_screenWidth / 2) * (1 + transformX / transformY));
        
        // Calculate sprite height and width on screen
        int spriteHeight = abs(static_cast<int>(m_screenHeight / transformY));
        int spriteWidth = spriteHeight;  // Assuming square sprites
        
        // Calculate drawing boundaries
        int drawStartX = std::max(0, spriteScreenX - spriteWidth / 2);
        int drawEndX = std::min(m_screenWidth - 1, spriteScreenX + spriteWidth / 2);
        int drawStartY = std::max(0, m_screenHeight / 2 - spriteHeight / 2);
        int drawEndY = std::min(m_screenHeight - 1, m_screenHeight / 2 + spriteHeight / 2);
        
        // Get sprite texture
        SDL_Texture* texture = m_textureManager->getSDLTexture(sprite->getTextureId());
        if (!texture) continue;
        
        // Set up source and destination rectangles
        SDL_Rect srcRect = { 0, 0, sprite->getWidth(), sprite->getHeight() };
        SDL_Rect dstRect = { drawStartX, drawStartY, drawEndX - drawStartX, drawEndY - drawStartY };
        
        // Render sprite
        SDL_RenderCopy(m_sdlRenderer, texture, &srcRect, &dstRect);
    }
}

void CudaRenderer::renderUI(const Player& player) {
    // Render health bar
    int healthBarWidth = 200;
    int healthBarHeight = 20;
    int healthBarX = 20;
    int healthBarY = m_screenHeight - healthBarHeight - 20;
    
    // Background
    SDL_Rect healthBarBg = { healthBarX, healthBarY, healthBarWidth, healthBarHeight };
    SDL_SetRenderDrawColor(m_sdlRenderer, 100, 100, 100, 255);
    SDL_RenderFillRect(m_sdlRenderer, &healthBarBg);
    
    // Health fill
    int healthFillWidth = static_cast<int>(healthBarWidth * (player.getHealth() / 100.0f));
    SDL_Rect healthBarFill = { healthBarX, healthBarY, healthFillWidth, healthBarHeight };
    SDL_SetRenderDrawColor(m_sdlRenderer, 255, 0, 0, 255);
    SDL_RenderFillRect(m_sdlRenderer, &healthBarFill);
    
    // Border
    SDL_SetRenderDrawColor(m_sdlRenderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_sdlRenderer, &healthBarBg);
    
    // Render ammo count
    // TODO: Implement ammo count rendering
}

bool CudaRenderer::isCudaAvailable() {
    int deviceCount = 0;
    cudaError_t cudaStatus = cudaGetDeviceCount(&deviceCount);
    
    if (cudaStatus != cudaSuccess || deviceCount == 0) {
        return false;
    }
    
    return true;
} 