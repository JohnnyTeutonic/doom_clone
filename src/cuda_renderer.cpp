#include "cuda_renderer.h"
#include "engine.h"
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
    int textureHeight,
    CudaLight* lights,
    int numLights,
    CudaAmbientLight* ambient
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
    , m_deviceLights(nullptr)        // Initialize light pointer
    , m_deviceAmbient(nullptr)       // Initialize ambient light pointer
    , m_wallTextureWidth(64)         // Default texture dimensions (must be non-zero)
    , m_wallTextureHeight(64)        // Default texture dimensions (must be non-zero)
    , m_numActiveLights(0)          // Initialize active lights count
    , m_cudaStream(0)
    , m_frameReady(false)
{
    // Nothing to do here
}

CudaRenderer::~CudaRenderer() {
    cleanup();
}

bool CudaRenderer::init(int screenWidth, int screenHeight, SDL_Renderer* sdlRenderer, TextureManager* textureManager) {
    std::cout << "CudaRenderer: Initializing with screen dimensions " << screenWidth << "x" << screenHeight << std::endl;
    
    // First, make sure CUDA is available before attempting any CUDA operations
    if (!isCudaAvailable()) {
        std::cerr << "CUDA is not available on this system" << std::endl;
        return false;
    }
    
    // Print CUDA device information
    printDeviceInfo();
    
    // Set class members
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_sdlRenderer = sdlRenderer;
    m_textureManager = textureManager;
    
    // Get texture dimensions (assuming all textures are the same size)
    if (m_textureManager && m_textureManager->getTextureCount() > 0) {
        const Texture* texture = m_textureManager->getTexture(0);
        if (texture) {
            m_wallTextureWidth = texture->getWidth();
            m_wallTextureHeight = texture->getHeight();
        }
    }
    
    // Validate texture dimensions
    if (m_wallTextureWidth <= 0 || m_wallTextureHeight <= 0) {
        std::cerr << "Invalid texture dimensions, using default 64x64" << std::endl;
        m_wallTextureWidth = 64;
        m_wallTextureHeight = 64;
    }
    
    // Allocate host memory for frame buffer and z-buffer
    try {
        m_hostFrameBuffer = new uint32_t[screenWidth * screenHeight];
        m_hostZBuffer = new float[screenWidth * screenHeight];
        
        // Initialize host memory
        std::fill_n(m_hostFrameBuffer, screenWidth * screenHeight, 0);
        std::fill_n(m_hostZBuffer, screenWidth * screenHeight, 10000.0f);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Failed to allocate host memory: " << e.what() << std::endl;
        cleanup();
        return false;
    }
    
    // Create SDL texture for frame buffer
    m_frameTexture = SDL_CreateTexture(
        sdlRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        screenWidth,
        screenHeight
    );
    
    if (!m_frameTexture) {
        std::cerr << "Failed to create frame texture: " << SDL_GetError() << std::endl;
        cleanup();
        return false;
    }
    
    // Initialize CUDA resources
    return initCuda();
}

void CudaRenderer::cleanup() {
    // Save the current CUDA device before freeing resources
    int currentDevice = -1;
    cudaGetDevice(&currentDevice);
    
    std::cout << "CudaRenderer: Cleaning up resources..." << std::endl;
    
    // Free SDL resources safely
    if (m_frameTexture) {
        SDL_DestroyTexture(m_frameTexture);
        m_frameTexture = nullptr;
    }
    
    // Free host resources safely
    if (m_hostFrameBuffer) {
        delete[] m_hostFrameBuffer;
        m_hostFrameBuffer = nullptr;
    }
    
    if (m_hostZBuffer) {
        delete[] m_hostZBuffer;
        m_hostZBuffer = nullptr;
    }
    
    // Make sure we're operating on the correct CUDA device
    if (currentDevice >= 0) {
        cudaSetDevice(currentDevice);
    }
    
    // Free device resources safely with error checking
    auto safeFree = [](void** ptr) {
        if (*ptr) {
            cudaError_t error = cudaFree(*ptr);
            if (error != cudaSuccess) {
                std::cerr << "CUDA error freeing memory: " << cudaGetErrorString(error) << std::endl;
            }
            *ptr = nullptr;
        }
    };
    
    // Free device resources
    safeFree((void**)&m_deviceFrameBuffer);
    safeFree((void**)&m_deviceZBuffer);
    safeFree((void**)&m_deviceMapData);
    safeFree((void**)&m_devicePlayerData);
    safeFree((void**)&m_deviceWallTextures);
    safeFree((void**)&m_deviceFloorTextures);
    safeFree((void**)&m_deviceCeilingTextures);
    safeFree((void**)&m_deviceLights);
    safeFree((void**)&m_deviceAmbient);
    
    // Destroy CUDA stream
    if (m_cudaStream) {
        cudaStreamDestroy(m_cudaStream);
        m_cudaStream = 0;
    }
    
    // Reset device to clean state
    cudaDeviceReset();
    
    std::cout << "CudaRenderer: Cleanup complete" << std::endl;
}

bool CudaRenderer::initCuda() {
    cudaError_t error;
    
    // Create CUDA stream for asynchronous operations
    error = cudaStreamCreate(&m_cudaStream);
    if (error != cudaSuccess) {
        std::cerr << "Failed to create CUDA stream: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // === Allocate device memory with proper error handling ===
    
    // Allocate frame buffer
    error = cudaMalloc(&m_deviceFrameBuffer, m_screenWidth * m_screenHeight * sizeof(uint32_t));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for frame buffer: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate z-buffer
    error = cudaMalloc(&m_deviceZBuffer, m_screenWidth * m_screenHeight * sizeof(float));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for z-buffer: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate player data
    error = cudaMalloc(&m_devicePlayerData, sizeof(PlayerData));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for player data: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate map data (max size 100x100 for now)
    error = cudaMalloc(&m_deviceMapData, 100 * 100 * sizeof(int));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for map data: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Calculate texture sizes
    size_t wallTextureSize = 4 * m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t);
    size_t floorCeilingTextureSize = m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t);
    
    // Allocate wall textures
    error = cudaMalloc(&m_deviceWallTextures, wallTextureSize);
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for wall textures: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate floor textures
    error = cudaMalloc(&m_deviceFloorTextures, floorCeilingTextureSize);
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for floor textures: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate ceiling textures
    error = cudaMalloc(&m_deviceCeilingTextures, floorCeilingTextureSize);
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for ceiling textures: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate lights
    error = cudaMalloc(&m_deviceLights, MAX_CUDA_LIGHTS * sizeof(CudaLight));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for lights: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Allocate ambient light
    error = cudaMalloc(&m_deviceAmbient, sizeof(CudaAmbientLight));
    if (error != cudaSuccess) {
        std::cerr << "Failed to allocate device memory for ambient light: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Initialize device memory (synchronously for safety)
    error = cudaMemset(m_deviceFrameBuffer, 0, m_screenWidth * m_screenHeight * sizeof(uint32_t));
    if (error != cudaSuccess) {
        std::cerr << "Failed to initialize frame buffer: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    error = cudaMemset(m_deviceZBuffer, 0xFF, m_screenWidth * m_screenHeight * sizeof(float));
    if (error != cudaSuccess) {
        std::cerr << "Failed to initialize z-buffer: " << cudaGetErrorString(error) << std::endl;
        cleanup();
        return false;
    }
    
    // Initialize all device textures and lights
    error = cudaMemset(m_deviceWallTextures, 0, wallTextureSize);
    error = cudaMemset(m_deviceFloorTextures, 0, floorCeilingTextureSize);
    error = cudaMemset(m_deviceCeilingTextures, 0, floorCeilingTextureSize);
    error = cudaMemset(m_deviceLights, 0, MAX_CUDA_LIGHTS * sizeof(CudaLight));
    error = cudaMemset(m_deviceAmbient, 0, sizeof(CudaAmbientLight));
    
    // Setup default ambient light
    CudaAmbientLight defaultAmbient = {0.3f, 0.3f, 0.4f, 0.3f}; // Default ambient light
    error = cudaMemcpy(m_deviceAmbient, &defaultAmbient, sizeof(CudaAmbientLight), cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        std::cerr << "Failed to initialize ambient light: " << cudaGetErrorString(error) << std::endl;
        // Not critical, continue
    }
    
    // Synchronize to ensure all memory operations are complete
    cudaDeviceSynchronize();
    
    std::cout << "CudaRenderer: CUDA initialization successful" << std::endl;
    return true;
}

void CudaRenderer::copyMapToDevice(const Map& map, const Player& player) {
    if (!m_deviceMapData || !m_devicePlayerData) {
        std::cerr << "CUDA map or player memory not allocated" << std::endl;
        return;
    }
    
    // Get map dimensions safely
    int mapWidth = std::max(1, std::min(map.getWidth(), 100));  // Cap at 100 to match our allocation
    int mapHeight = std::max(1, std::min(map.getHeight(), 100)); // Cap at 100 to match our allocation
    
    // Ensure map dimensions are reasonable to avoid memory issues
    if (mapWidth <= 0 || mapHeight <= 0 || mapWidth > 100 || mapHeight > 100) {
        std::cerr << "Invalid map dimensions: " << mapWidth << "x" << mapHeight << std::endl;
        return;
    }
    
    try {
        // Create a flattened array for map data
        std::vector<int> hostMapData(mapWidth * mapHeight, 0); // Initialize with 0 (empty)
        
        // Fill map data array with cell types
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                CellType cellType = map.getCell(x, y);
                int cellValue = 0; // Default to empty
                
                // Convert cell type to value used by the raycast algorithm
                switch (cellType) {
                    case CellType::Wall:
                    case CellType::ElevatedWall:
                    case CellType::SecretWall:
                        // For walls, encode the texture ID
                        {
                            int textureId = map.getWallTexture(x, y);
                            cellValue = (textureId + 1) * 100 + 1; // Encode as texture ID + cell type
                        }
                        break;
                        
                    case CellType::Door:
                        // For doors, check if the door is open
                        {
                            // Use getDoorState instead of getDoorOpenAmount
                            DoorState doorState = map.getDoorState(x, y);
                            // If door is closed or closing, treat as a wall
                            if (doorState == DoorState::Closed || doorState == DoorState::Closing) {
                                cellValue = 1; // Treat as a regular wall
                            }
                        }
                        break;
                        
                    default:
                        cellValue = 0; // Empty or non-solid
                        break;
                }
                
                // Set the cell value in the host array
                hostMapData[y * mapWidth + x] = cellValue;
            }
        }
        
        // Copy map data to device
        cudaError_t error = cudaMemcpy(m_deviceMapData, hostMapData.data(), mapWidth * mapHeight * sizeof(int), cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy map data to device: " << cudaGetErrorString(error) << std::endl;
        return;
    }
    
        // Create player data safely
        PlayerData hostPlayerData;
        
        // Initialize with defaults in case of issues
        hostPlayerData.posX = 2.0f;
        hostPlayerData.posY = 2.0f;
        hostPlayerData.dirX = 1.0f;
        hostPlayerData.dirY = 0.0f;
        hostPlayerData.planeX = 0.0f;
        hostPlayerData.planeY = 0.66f;
        hostPlayerData.verticalAngle = 0.0f;
        hostPlayerData.jumpHeight = 0.0f;
        
        // Try to get actual player data
        try {
            hostPlayerData.posX = static_cast<float>(player.getX());
            hostPlayerData.posY = static_cast<float>(player.getY());
            hostPlayerData.dirX = static_cast<float>(player.getDirX());
            hostPlayerData.dirY = static_cast<float>(player.getDirY());
            hostPlayerData.planeX = static_cast<float>(player.getPlaneX());
            hostPlayerData.planeY = static_cast<float>(player.getPlaneY());
            hostPlayerData.verticalAngle = static_cast<float>(player.getVerticalAngle());
            hostPlayerData.jumpHeight = static_cast<float>(player.getJumpHeight());
        } catch (const std::exception& e) {
            std::cerr << "Error getting player data: " << e.what() << std::endl;
            // Continue with default values
        }
        
        // Copy player data to device
        error = cudaMemcpy(m_devicePlayerData, &hostPlayerData, sizeof(PlayerData), cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy player data to device: " << cudaGetErrorString(error) << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in copyMapToDevice: " << e.what() << std::endl;
    }
}

void CudaRenderer::copyTexturesToDevice() {
    if (!m_deviceWallTextures || !m_deviceFloorTextures || !m_deviceCeilingTextures) {
        std::cerr << "Texture device memory not allocated!" << std::endl;
        return;
    }
    
    // Check if texture manager exists
    if (!m_textureManager) {
        std::cerr << "No texture manager available!" << std::endl;
        return;
    }
    
    // Check texture dimensions
    if (m_wallTextureWidth <= 0 || m_wallTextureHeight <= 0) {
        std::cerr << "Invalid texture dimensions!" << std::endl;
        return;
    }
    
    // Create buffers for wall, floor, and ceiling textures
    size_t texSize = m_wallTextureWidth * m_wallTextureHeight * sizeof(uint32_t);
    
    uint32_t* wallTextureData = nullptr;
    uint32_t* floorTextureData = nullptr;
    uint32_t* ceilingTextureData = nullptr;
    
    try {
        wallTextureData = new uint32_t[4 * m_wallTextureWidth * m_wallTextureHeight];
        floorTextureData = new uint32_t[m_wallTextureWidth * m_wallTextureHeight];
        ceilingTextureData = new uint32_t[m_wallTextureWidth * m_wallTextureHeight];
        
        // Initialize with default patterns as fallbacks
        for (int i = 0; i < 4; i++) {
        for (int y = 0; y < m_wallTextureHeight; y++) {
            for (int x = 0; x < m_wallTextureWidth; x++) {
                    bool isEven = ((x / 8) + (y / 8)) % 2 == 0;
                    uint32_t color = isEven ? 0xFFAAAAAA : 0xFF555555;
                    
                    // Add a colored border based on texture number
                    if (x < 2 || x >= m_wallTextureWidth - 2 || y < 2 || y >= m_wallTextureHeight - 2) {
                        switch (i) {
                            case 0: color = 0xFF0000FF; break; // Blue border
                            case 1: color = 0xFF00FF00; break; // Green border
                            case 2: color = 0xFFFF0000; break; // Red border
                            case 3: color = 0xFFFFFF00; break; // Yellow border
                        }
                    }
                    
                    wallTextureData[i * m_wallTextureWidth * m_wallTextureHeight + y * m_wallTextureWidth + x] = color;
                }
            }
        }
        
        // Default floor texture (grid pattern)
        for (int y = 0; y < m_wallTextureHeight; y++) {
            for (int x = 0; x < m_wallTextureWidth; x++) {
                bool isGrid = (x % 16 == 0) || (y % 16 == 0);
                uint32_t color = isGrid ? 0xFF444444 : 0xFF888888;
                floorTextureData[y * m_wallTextureWidth + x] = color;
            }
        }
        
        // Default ceiling texture (gradient)
                for (int y = 0; y < m_wallTextureHeight; y++) {
                    for (int x = 0; x < m_wallTextureWidth; x++) {
                uint8_t value = static_cast<uint8_t>(128 + (y * 127) / m_wallTextureHeight);
                uint32_t color = 0xFF000000 | (value << 16) | (value << 8) | value;
                ceilingTextureData[y * m_wallTextureWidth + x] = color;
            }
        }
        
        // Load actual textures first for walls
        for (int i = 0; i < 4; i++) {
            const Texture* texture = m_textureManager->getTexture(i);
            if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
                const uint32_t* pixels = texture->getPixelData();
                if (pixels) {
                    // Copy to the corresponding section of wall texture data
                    memcpy(
                        wallTextureData + (i * m_wallTextureWidth * m_wallTextureHeight),
                        pixels,
                        texSize
                    );
                }
            }
        }
        
        // Load floor texture (using texture index 4)
        bool floorTextureLoaded = false;
        const Texture* floorTexture = m_textureManager->getTexture(4);
        if (floorTexture && floorTexture->getWidth() > 0 && floorTexture->getHeight() > 0) {
            const uint32_t* floorPixels = floorTexture->getPixelData();
            if (floorPixels) {
                memcpy(floorTextureData, floorPixels, texSize);
                floorTextureLoaded = true;
                std::cout << "Floor texture loaded from texture index 4" << std::endl;
            }
        }
        
        if (!floorTextureLoaded) {
            std::cout << "Using default floor texture pattern" << std::endl;
        }
        
        // Load ceiling texture (using texture index 5)
        bool ceilingTextureLoaded = false;
        const Texture* ceilingTexture = m_textureManager->getTexture(5);
        if (ceilingTexture && ceilingTexture->getWidth() > 0 && ceilingTexture->getHeight() > 0) {
            const uint32_t* ceilingPixels = ceilingTexture->getPixelData();
            if (ceilingPixels) {
                memcpy(ceilingTextureData, ceilingPixels, texSize);
                ceilingTextureLoaded = true;
                std::cout << "Ceiling texture loaded from texture index 5" << std::endl;
            }
        }
        
        if (!ceilingTextureLoaded) {
            std::cout << "Using default ceiling texture pattern" << std::endl;
        }
        
        // Copy textures to device with error checking
        cudaError_t error;
        
        error = cudaMemcpy(m_deviceWallTextures, wallTextureData, 4 * texSize, cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy wall textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
        
        error = cudaMemcpy(m_deviceFloorTextures, floorTextureData, texSize, cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy floor textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
        
        error = cudaMemcpy(m_deviceCeilingTextures, ceilingTextureData, texSize, cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy ceiling textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in copyTexturesToDevice: " << e.what() << std::endl;
    }
    
    // Free host memory
    delete[] wallTextureData;
    delete[] floorTextureData;
    delete[] ceilingTextureData;
}

void CudaRenderer::generateFrame(const Map& map, const Player& player) {
    // Ensure CUDA memory is allocated
    if (!m_deviceFrameBuffer || !m_deviceZBuffer || !m_deviceMapData || 
        !m_devicePlayerData || !m_deviceWallTextures || 
        !m_deviceFloorTextures || !m_deviceCeilingTextures ||
        !m_deviceLights || !m_deviceAmbient) {
        std::cerr << "CUDA memory not properly allocated - cannot generate frame" << std::endl;
        m_frameReady = false;
        return;
    }
    
    // Get lighting system from the map's engine
    try {
        Engine* engine = map.getEngine();
        if (!engine) {
            std::cerr << "Warning: Map does not have a valid Engine reference" << std::endl;
            // Initialize with default ambient light instead of returning
            CudaAmbientLight defaultAmbient = {0.3f, 0.3f, 0.3f, 0.5f}; // Default ambient light
            cudaError_t error = cudaMemcpy(m_deviceAmbient, &defaultAmbient, sizeof(CudaAmbientLight), cudaMemcpyHostToDevice);
            if (error != cudaSuccess) {
                std::cerr << "Warning: Failed to set default ambient light: " << cudaGetErrorString(error) << std::endl;
            }
            m_numActiveLights = 0; // No active lights
        } else {
            // Use the engine's lighting system
            const LightingSystem& lightingSystem = engine->getRenderer().getLightingSystem();
            
            // Copy lights to device
            copyLightsToDevice(lightingSystem);
        }
    
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
            m_wallTextureHeight,
            m_deviceLights,
            m_numActiveLights,
            m_deviceAmbient
    );
    
    // Check for kernel errors
    cudaError_t cudaStatus = cudaGetLastError();
    if (cudaStatus != cudaSuccess) {
        std::cerr << "Kernel launch failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        m_frameReady = false;
        return;
    }
    
    // Wait for kernel to finish
    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        std::cerr << "cudaDeviceSynchronize failed: " << cudaGetErrorString(cudaStatus) << std::endl;
        m_frameReady = false;
        return;
    }
    
    // Copy frame buffer back to host
        cudaStatus = cudaMemcpy(m_hostFrameBuffer, m_deviceFrameBuffer, 
                   m_screenWidth * m_screenHeight * sizeof(uint32_t), 
                   cudaMemcpyDeviceToHost);
        if (cudaStatus != cudaSuccess) {
            std::cerr << "Failed to copy frame buffer back to host: " << cudaGetErrorString(cudaStatus) << std::endl;
            m_frameReady = false;
            return;
        }
    
    // Update SDL texture with frame buffer
    SDL_UpdateTexture(m_frameTexture, NULL, m_hostFrameBuffer, m_screenWidth * sizeof(uint32_t));
    
        // Set frame ready flag
    m_frameReady = true;
    
    } catch (const std::exception& e) {
        std::cerr << "Exception in generateFrame: " << e.what() << std::endl;
        m_frameReady = false;
    }
}

void CudaRenderer::blitFrameBuffer() {
    
    if (!m_frameReady) {
        std::cerr << "No frame ready to blit!" << std::endl;
        return;
    }
    
    // Simply render the texture to the screen
    // We don't use NULL for the destination rect to ensure it's properly scaled
    SDL_Rect destRect = {0, 0, m_screenWidth, m_screenHeight};
    
    // Save current renderer state
    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(m_sdlRenderer, &oldBlendMode);
    
    // Set blend mode to NONE for the background frame
    // This ensures the frame completely overwrites whatever was there before
    SDL_SetTextureBlendMode(m_frameTexture, SDL_BLENDMODE_NONE);
    
    // Render the texture (the 3D view)
    SDL_RenderCopy(m_sdlRenderer, m_frameTexture, NULL, &destRect);
    
    // Restore renderer state
    SDL_SetRenderDrawBlendMode(m_sdlRenderer, oldBlendMode);
    
}

void CudaRenderer::render(const Map& map, const Player& player) {
    
    // Generate the frame
    generateFrame(map, player);
    
    // Blit the frame buffer
    blitFrameBuffer();
    
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
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    
    if (error != cudaSuccess) {
        std::cerr << "CUDA error checking availability: " << cudaGetErrorString(error) << std::endl;
        return false;
    }
    
    if (deviceCount == 0) {
        std::cerr << "No CUDA-capable devices found" << std::endl;
        return false;
    }
    
    // Select the first CUDA device
    error = cudaSetDevice(0);
    if (error != cudaSuccess) {
        std::cerr << "Failed to select CUDA device: " << cudaGetErrorString(error) << std::endl;
        return false;
    }
    
    // Get device properties to ensure we can work with this GPU
    cudaDeviceProp deviceProp;
    error = cudaGetDeviceProperties(&deviceProp, 0);
    if (error != cudaSuccess) {
        std::cerr << "Failed to get CUDA device properties: " << cudaGetErrorString(error) << std::endl;
        return false;
    }
    
    // Check compute capability (minimum 3.0 required for our features)
    if (deviceProp.major < 3) {
        std::cerr << "CUDA device has insufficient compute capability: " 
                  << deviceProp.major << "." << deviceProp.minor 
                  << " (minimum 3.0 required)" << std::endl;
        return false;
    }
    
    std::cout << "CUDA device available: " << deviceProp.name << std::endl;
    std::cout << "Compute capability: " << deviceProp.major << "." << deviceProp.minor << std::endl;
    std::cout << "Total global memory: " << deviceProp.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
    
    return true;
}

void CudaRenderer::copyLightsToDevice(const LightingSystem& lightingSystem) {
    // Create host buffers for light data
    CudaLight hostLights[MAX_CUDA_LIGHTS];
    memset(hostLights, 0, MAX_CUDA_LIGHTS * sizeof(CudaLight)); // Zero-initialize all lights
    
    CudaAmbientLight hostAmbient;
    
    // Set ambient light data
    Color ambientColor = lightingSystem.getAmbientColor();
    double ambientIntensity = lightingSystem.getAmbientIntensity();
    
    hostAmbient.r = ambientColor.r / 255.0f;
    hostAmbient.g = ambientColor.g / 255.0f;
    hostAmbient.b = ambientColor.b / 255.0f;
    hostAmbient.intensity = ambientIntensity;
    
    // Copy ambient light to device
    cudaError_t cudaStatus = cudaMemcpy(m_deviceAmbient, &hostAmbient, sizeof(CudaAmbientLight), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "Failed to copy ambient light to device: " << cudaGetErrorString(cudaStatus) << std::endl;
        // Set safe default value for number of active lights
        m_numActiveLights = 0;
        return;
    }
    
    // Get lights from the lighting system
    const std::vector<Light>& lights = lightingSystem.getLights();
    
    // Calculate number of lights to copy (limited by MAX_CUDA_LIGHTS)
    int numLightsToCopy = std::min(static_cast<int>(lights.size()), MAX_CUDA_LIGHTS);
    
    // Fill host light buffer with data from the lighting system
    for (int i = 0; i < numLightsToCopy; i++) {
        const Light& light = lights[i];
        
        // Skip disabled lights here rather than in the kernel
        if (!light.enabled) {
            continue;
        }
        
        // Map light type from LightingSystem to CUDA
        int cudaLightType;
        switch (light.type) {
            case LightType::Point:
                cudaLightType = 0; // Point
                break;
            case LightType::Directional:
                cudaLightType = 1; // Directional
                break;
            case LightType::Flickering:
                cudaLightType = 2; // Flickering
                break;
            case LightType::Pulsing:
                cudaLightType = 3; // Pulsing
                break;
            case LightType::Strobe:
                cudaLightType = 4; // Strobe
                break;
            case LightType::Glow:
                cudaLightType = 5; // Glow
                break;
            default:
                cudaLightType = 0; // Default to Point
                break;
        }
        
        // Fill struct with light data
        hostLights[i].type = cudaLightType;
        hostLights[i].posX = light.position.x;
        hostLights[i].posY = light.position.y;
        hostLights[i].dirX = light.direction.x;
        hostLights[i].dirY = light.direction.y;
        hostLights[i].r = light.color.r / 255.0f;   // Convert from [0-255] to [0-1]
        hostLights[i].g = light.color.g / 255.0f;
        hostLights[i].b = light.color.b / 255.0f;
        hostLights[i].intensity = light.intensity;
        hostLights[i].radius = light.radius;
        hostLights[i].effectSpeed = light.effectSpeed;
        hostLights[i].effectIntensity = light.effectIntensity;
        hostLights[i].effectTimer = light.effectTimer;
        hostLights[i].enabled = 1; // We've already filtered disabled lights
    }
    
    // Copy light data to device
    cudaStatus = cudaMemcpy(m_deviceLights, hostLights, MAX_CUDA_LIGHTS * sizeof(CudaLight), cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "Failed to copy lights to device: " << cudaGetErrorString(cudaStatus) << std::endl;
        m_numActiveLights = 0;
        return;
    }
    
    // Store the number of lights for the kernel
    m_numActiveLights = numLightsToCopy;
}

void CudaRenderer::printDeviceInfo() const {
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    
    if (deviceCount == 0) {
        std::cerr << "No CUDA devices found!" << std::endl;
        return;
    }
    
    std::cout << "Found " << deviceCount << " CUDA device(s)" << std::endl;
    
    // Get current device
    int currentDevice = 0;
    cudaGetDevice(&currentDevice);
    
    // Get device properties
    cudaDeviceProp deviceProps;
    cudaGetDeviceProperties(&deviceProps, currentDevice);
    
    // Print device info
    std::cout << "Using CUDA device #" << currentDevice << ": " << deviceProps.name << std::endl;
    std::cout << "  Compute capability: " << deviceProps.major << "." << deviceProps.minor << std::endl;
    std::cout << "  Total global memory: " << (deviceProps.totalGlobalMem / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Multiprocessors: " << deviceProps.multiProcessorCount << std::endl;
    std::cout << "  Clock rate: " << (deviceProps.clockRate / 1000) << " MHz" << std::endl;
    std::cout << "  Max threads per block: " << deviceProps.maxThreadsPerBlock << std::endl;
    std::cout << "  Max threads dimensions: (" 
              << deviceProps.maxThreadsDim[0] << ", "
              << deviceProps.maxThreadsDim[1] << ", "
              << deviceProps.maxThreadsDim[2] << ")" << std::endl;
    std::cout << "  Max grid dimensions: (" 
              << deviceProps.maxGridSize[0] << ", "
              << deviceProps.maxGridSize[1] << ", "
              << deviceProps.maxGridSize[2] << ")" << std::endl;
    std::cout << "  Warp size: " << deviceProps.warpSize << std::endl;
    std::cout << "  Memory clock rate: " << (deviceProps.memoryClockRate / 1000) << " MHz" << std::endl;
    std::cout << "  Memory bus width: " << deviceProps.memoryBusWidth << " bits" << std::endl;
    std::cout << "  L2 cache size: " << (deviceProps.l2CacheSize / 1024) << " KB" << std::endl;
} 