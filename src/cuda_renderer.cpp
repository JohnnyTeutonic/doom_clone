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
    : m_isInitialized(false)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_sdlRenderer(nullptr)
    , m_frameTexture(nullptr)
    , m_textureManager(nullptr)
    , m_spriteManager(nullptr)
    , m_deviceMapData(nullptr)
    , m_devicePlayerData(nullptr)
    , m_deviceFrameBuffer(nullptr)
    , m_deviceZBuffer(nullptr)
    , m_deviceWallTextures(nullptr)
    , m_deviceFloorTextures(nullptr)
    , m_deviceCeilingTextures(nullptr)
    , m_deviceLights(nullptr)
    , m_hostFrameBuffer(nullptr)
    , m_hostZBuffer(nullptr)
    , m_wallTextureWidth(64)         // Default texture dimensions (must be non-zero)
    , m_wallTextureHeight(64)        // Default texture dimensions (must be non-zero)
    , m_ambientLightLevel(0.2f)      // Default ambient light level
    , m_frameReady(false)
{
    // Initialize wall texture variations with defaults (will be replaced by engine)
    m_wallTextureVariations = {0, 1, 2, 3};
    
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
        #if defined(__SSE2__) || defined(_MSC_VER)
        // SSE-optimized buffer initialization
        const size_t pixelCount = screenWidth * screenHeight;
        const size_t vectorizedSize = pixelCount / 4;
        const size_t remainder = pixelCount % 4;

        // Set up SSE constants
        __m128i zero_int = _mm_setzero_si128();
        __m128 far_clip = _mm_set1_ps(10000.0f);

        // Initialize frame buffer with zeros using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_si128((__m128i*)&m_hostFrameBuffer[i * 4], zero_int);
        }

        // Initialize Z-buffer with far clip value using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_ps(&m_hostZBuffer[i * 4], far_clip);
        }

        // Handle any remaining pixels
        for (size_t i = vectorizedSize * 4; i < pixelCount; ++i) {
            m_hostFrameBuffer[i] = 0;
            m_hostZBuffer[i] = 10000.0f;
        }
        #else
        // Standard initialization
        std::fill_n(m_hostFrameBuffer, screenWidth * screenHeight, 0);
        std::fill_n(m_hostZBuffer, screenWidth * screenHeight, 10000.0f);
        #endif
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
    
    // Mark as uninitialized
    m_isInitialized = false;
    
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
    m_isInitialized = true;
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
                            // IMPORTANT FIX: Always use texture 0 for all walls to ensure consistency
                            // This guarantees all walls use the same texture regardless of their position
                            // Original code: int textureId = map.getWallTexture(x, y);
                            int textureId = 0; // Force all walls to use texture 0
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
        // Use aligned memory allocation for better SSE performance
        #ifdef _MSC_VER
        wallTextureData = (uint32_t*)_aligned_malloc(4 * texSize, 16);
        floorTextureData = (uint32_t*)_aligned_malloc(texSize, 16);
        ceilingTextureData = (uint32_t*)_aligned_malloc(texSize, 16);
        #else
        wallTextureData = (uint32_t*)aligned_alloc(16, 4 * texSize);
        floorTextureData = (uint32_t*)aligned_alloc(16, texSize);
        ceilingTextureData = (uint32_t*)aligned_alloc(16, texSize);
        #endif
        
        if (!wallTextureData || !floorTextureData || !ceilingTextureData) {
            throw std::runtime_error("Failed to allocate aligned memory for textures");
        }
        
        // Initialize with default patterns as fallbacks using SSE where possible
        #if defined(__SSE2__) || defined(_MSC_VER)
        // SSE-optimized initialization for wall textures
        for (int i = 0; i < 4; i++) {
            for (int y = 0; y < m_wallTextureHeight; y++) {
                // Process pixels in batches of 4 using SSE
                int x = 0;
                for (; x + 3 < m_wallTextureWidth; x += 4) {
                    // Create authentic Doom-inspired texture patterns with SSE
                    __m128i r_vals = _mm_set1_epi32(0);
                    __m128i g_vals = _mm_set1_epi32(0);
                    __m128i b_vals = _mm_set1_epi32(0);
                    
                    for (int j = 0; j < 4; j++) {
                        // Base colors for different texture variations (authentic Doom palette)
                        uint8_t r, g, b;
                        int pixel_x = x + j;
                        
                        // Different pattern for each texture slot
                        switch (i) {
                            case 0: // Brown tech pattern (similar to STARTAN from Doom)
                                {
                                    bool largePattern = ((pixel_x / 16) + (y / 16)) % 2 == 0;
                                    bool edgeDetail = (pixel_x % 16 < 2) || (y % 16 < 2);
                                    bool smallDetail = ((pixel_x / 4) + (y / 4)) % 2 == 0;
                                    
                                    // Base brown color (authentic Doom STARTAN)
                                    r = 145; g = 123; b = 96;
                                    
                                    // Apply pattern variations
                                    if (edgeDetail) {
                                        // Darker lines/seams between concrete blocks
                                        r = 110; g = 90; b = 77;
                                    } else if (smallDetail) {
                                        // Random darker spots
                                        r = 130; g = 110; b = 85;
                                    } else if (largePattern) {
                                        // Random lighter spots
                                        r = 160; g = 140; b = 110;
                                    }
                                    
                                    // Add some noise based on the combination of position
                                    int noise = ((pixel_x * 7 + y * 13) % 8) - 4;
                                    r = std::min(255, std::max(0, static_cast<int>(r) + noise));
                                    g = std::min(255, std::max(0, static_cast<int>(g) + noise));
                                    b = std::min(255, std::max(0, static_cast<int>(b) + noise));
                                }
                                break;
                                
                            case 1: // Reddish demonic texture (like REDWALL)
                                {
                                    bool vertLine = (pixel_x % 32 < 2);
                                    bool horzLine = (y % 24 < 2);
                                    bool pattern = ((pixel_x / 8) ^ (y / 8)) & 1;
                                    
                                    // Deep red base
                                    r = 160; g = 70; b = 60;
                                    
                                    // Apply variations
                                    if (vertLine || horzLine) { r = 100; g = 40; b = 35; }
                                    if (pattern) { r -= 20; g -= 10; b -= 5; }
                                }
                                break;
                                
                            case 2: // Gray tech pattern (like COMPTILE)
                                {
                                    int cellX = pixel_x % 16;
                                    int cellY = y % 16;
                                    bool isBorder = cellX < 2 || cellY < 2 || cellX > 13 || cellY > 13;
                                    bool isInnerDetail = (cellX > 4 && cellX < 12 && cellY > 4 && cellY < 12);
                                    
                                    // Base gray color
                                    r = 120; g = 120; b = 130;
                                    
                                    // Apply grid pattern
                                    if (isBorder) { r = 70; g = 70; b = 80; }
                                    if (isInnerDetail) { r = 100; g = 100; b = 110; }
                                }
                                break;
                                
                            case 3: // Green-brown tech (like SLADWALL)
                                {
                                    int patternX = (pixel_x / 8) % 3;
                                    int patternY = (y / 8) % 3;
                                    bool edgeDetail = (pixel_x % 8 < 1) || (y % 8 < 1);
                                    
                                    // Greenish brown base
                                    r = 120; g = 110; b = 60;
                                    
                                    // Pattern variations
                                    if (patternX == 0 || patternY == 0) { r -= 20; g -= 15; }
                                    if (edgeDetail) { r = 70; g = 65; b = 35; }
                                }
                                break;
                                
                            default:
                                // Fallback brown
                                r = 120; g = 100; b = 80;
                        }
                        
                        // Only add noise to textures 1-3 (texture 0 already has its own noise pattern)
                        if (i > 0) {
                            int noise = ((pixel_x * 13 + y * 7) % 10) - 5;
                            r = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(r) + noise)));
                            g = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(g) + noise)));
                            b = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(b) + noise)));
                        }
                        
                        // Combine into final ARGB color
                        uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        
                        // Store color in our temporary array
                        // Using an array-based approach compatible with SSE2
                        static uint32_t colors[4];
                        colors[j] = color;
                        
                        // Only create the SSE register once we have all 4 colors
                        if (j == 3) {
                            // Create r_vals with all 4 colors at once (SSE2 compatible)
                            r_vals = _mm_setr_epi32(colors[0], colors[1], colors[2], colors[3]);
                            
                            // Store the four pixels at once (only when j==3, meaning we have all 4 pixels)
                            _mm_store_si128((__m128i*)&wallTextureData[i * m_wallTextureWidth * m_wallTextureHeight + y * m_wallTextureWidth + x], r_vals);
                        }
                    }
                }
                
                // Handle remaining pixels (if width is not multiple of 4)
                for (; x < m_wallTextureWidth; x++) {
                    // Create authentic Doom-inspired texture patterns
                    
                    // Base colors for different texture variations (authentic Doom palette)
                    uint8_t r, g, b;
                    
                    // Different pattern for each texture slot (same logic as above)
                    // ... existing pattern code for remaining pixels
                    switch (i) {
                        case 0: // Brown tech pattern
                            {
                                bool largePattern = ((x / 16) + (y / 16)) % 2 == 0;
                                bool edgeDetail = (x % 16 < 2) || (y % 16 < 2);
                                bool smallDetail = ((x / 4) + (y / 4)) % 2 == 0;
                                
                                r = 145; g = 123; b = 96;
                                
                                if (edgeDetail) {
                                    r = 110; g = 90; b = 77;
                                } else if (smallDetail) {
                                    r = 130; g = 110; b = 85;
                                } else if (largePattern) {
                                    r = 160; g = 140; b = 110;
                                }
                                
                                int noise = ((x * 7 + y * 13) % 8) - 4;
                                r = std::min(255, std::max(0, static_cast<int>(r) + noise));
                                g = std::min(255, std::max(0, static_cast<int>(g) + noise));
                                b = std::min(255, std::max(0, static_cast<int>(b) + noise));
                            }
                            break;
                        case 1: // Reddish demonic texture
                            {
                                bool vertLine = (x % 32 < 2);
                                bool horzLine = (y % 24 < 2);
                                bool pattern = ((x / 8) ^ (y / 8)) & 1;
                                
                                r = 160; g = 70; b = 60;
                                
                                if (vertLine || horzLine) { r = 100; g = 40; b = 35; }
                                if (pattern) { r -= 20; g -= 10; b -= 5; }
                            }
                            break;
                        case 2: // Gray tech pattern
                            {
                                int cellX = x % 16;
                                int cellY = y % 16;
                                bool isBorder = cellX < 2 || cellY < 2 || cellX > 13 || cellY > 13;
                                bool isInnerDetail = (cellX > 4 && cellX < 12 && cellY > 4 && cellY < 12);
                                
                                r = 120; g = 120; b = 130;
                                
                                if (isBorder) { r = 70; g = 70; b = 80; }
                                if (isInnerDetail) { r = 100; g = 100; b = 110; }
                            }
                            break;
                        case 3: // Green-brown tech
                            {
                                int patternX = (x / 8) % 3;
                                int patternY = (y / 8) % 3;
                                bool edgeDetail = (x % 8 < 1) || (y % 8 < 1);
                                
                                r = 120; g = 110; b = 60;
                                
                                if (patternX == 0 || patternY == 0) { r -= 20; g -= 15; }
                                if (edgeDetail) { r = 70; g = 65; b = 35; }
                            }
                            break;
                        default:
                            r = 120; g = 100; b = 80;
                    }
                    
                    if (i > 0) {
                        int noise = ((x * 13 + y * 7) % 10) - 5;
                        r = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(r) + noise)));
                        g = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(g) + noise)));
                        b = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(b) + noise)));
                    }
                    
                    uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    wallTextureData[i * m_wallTextureWidth * m_wallTextureHeight + y * m_wallTextureWidth + x] = color;
                }
            }
        }
        
        // Default floor texture (grid pattern) using SSE
        for (int y = 0; y < m_wallTextureHeight; y++) {
            int x = 0;
            __m128i grid_color = _mm_set1_epi32(0xFF444444);    // Dark grid color
            __m128i fill_color = _mm_set1_epi32(0xFF888888);    // Light fill color
            
            for (; x + 3 < m_wallTextureWidth; x += 4) {
                // Create a mask for which pixels are grid lines
                int mask = 0;
                for (int j = 0; j < 4; j++) {
                    bool isGrid = ((x + j) % 16 == 0) || (y % 16 == 0);
                    mask |= (isGrid ? (1 << j) : 0);
                }
                
                // Select colors based on mask
                __m128i colors;
                if (mask == 0) {
                    // All fill color
                    colors = fill_color;
                } else if (mask == 15) {
                    // All grid color
                    colors = grid_color;
                } else {
                    // Mixed - need to blend per pixel
                    colors = _mm_set_epi32(
                        (mask & 8) ? 0xFF444444 : 0xFF888888,
                        (mask & 4) ? 0xFF444444 : 0xFF888888,
                        (mask & 2) ? 0xFF444444 : 0xFF888888,
                        (mask & 1) ? 0xFF444444 : 0xFF888888
                    );
                }
                
                // Store four pixels at once
                _mm_store_si128((__m128i*)&floorTextureData[y * m_wallTextureWidth + x], colors);
            }
            
            // Handle remaining pixels
            for (; x < m_wallTextureWidth; x++) {
                bool isGrid = (x % 16 == 0) || (y % 16 == 0);
                uint32_t color = isGrid ? 0xFF444444 : 0xFF888888;
                floorTextureData[y * m_wallTextureWidth + x] = color;
            }
        }
        
        // Default ceiling texture (gradient) using SSE
        for (int y = 0; y < m_wallTextureHeight; y++) {
            uint8_t value = static_cast<uint8_t>(128 + (y * 127) / m_wallTextureHeight);
            uint32_t color = 0xFF000000 | (value << 16) | (value << 8) | value;
            
            // Create an SSE register with the same color for all 4 pixels
            __m128i color_vec = _mm_set1_epi32(color);
            
            // Process pixels in batches of 4
            int x = 0;
            for (; x + 3 < m_wallTextureWidth; x += 4) {
                _mm_store_si128((__m128i*)&ceilingTextureData[y * m_wallTextureWidth + x], color_vec);
            }
            
            // Handle remaining pixels
            for (; x < m_wallTextureWidth; x++) {
                ceilingTextureData[y * m_wallTextureWidth + x] = color;
            }
        }
        #else
        // Non-SSE fallback implementation (original code)
        // ... existing code for initializing textures
        // Initialize with default patterns as fallbacks
        for (int i = 0; i < 4; i++) {
            for (int y = 0; y < m_wallTextureHeight; y++) {
                for (int x = 0; x < m_wallTextureWidth; x++) {
                    // Create authentic Doom-inspired texture patterns
                    
                    // Base colors for different texture variations (authentic Doom palette)
                    uint8_t r, g, b;
                    
                    // Different pattern for each texture slot
                    switch (i) {
                        case 0: // Brown tech pattern (similar to STARTAN from Doom)
                            {
                                bool largePattern = ((x / 16) + (y / 16)) % 2 == 0;
                                bool edgeDetail = (x % 16 < 2) || (y % 16 < 2);
                                bool smallDetail = ((x / 4) + (y / 4)) % 2 == 0;
                                
                                // Base brown color (authentic Doom STARTAN)
                                r = 145; g = 123; b = 96;
                                
                                // Apply pattern variations
                                if (edgeDetail) {
                                    // Darker lines/seams between concrete blocks
                                    r = 110; g = 90; b = 77;
                                } else if (smallDetail) {
                                    // Random darker spots
                                    r = 130; g = 110; b = 85;
                                } else if (largePattern) {
                                    // Random lighter spots
                                    r = 160; g = 140; b = 110;
                                }
                                
                                // Add some noise based on the combination of position
                                int noise = ((x * 7 + y * 13) % 8) - 4;
                                r = std::min(255, std::max(0, static_cast<int>(r) + noise));
                                g = std::min(255, std::max(0, static_cast<int>(g) + noise));
                                b = std::min(255, std::max(0, static_cast<int>(b) + noise));
                            }
                            break;
                            
                        case 1: // Reddish demonic texture (like REDWALL)
                            {
                                bool vertLine = (x % 32 < 2);
                                bool horzLine = (y % 24 < 2);
                                bool pattern = ((x / 8) ^ (y / 8)) & 1;
                                
                                // Deep red base
                                r = 160; g = 70; b = 60;
                                
                                // Apply variations
                                if (vertLine || horzLine) { r = 100; g = 40; b = 35; }
                                if (pattern) { r -= 20; g -= 10; b -= 5; }
                            }
                            break;
                            
                        case 2: // Gray tech pattern (like COMPTILE)
                            {
                                int cellX = x % 16;
                                int cellY = y % 16;
                                bool isBorder = cellX < 2 || cellY < 2 || cellX > 13 || cellY > 13;
                                bool isInnerDetail = (cellX > 4 && cellX < 12 && cellY > 4 && cellY < 12);
                                
                                // Base gray color
                                r = 120; g = 120; b = 130;
                                
                                // Apply grid pattern
                                if (isBorder) { r = 70; g = 70; b = 80; }
                                if (isInnerDetail) { r = 100; g = 100; b = 110; }
                            }
                            break;
                            
                        case 3: // Green-brown tech (like SLADWALL)
                            {
                                int patternX = (x / 8) % 3;
                                int patternY = (y / 8) % 3;
                                bool edgeDetail = (x % 8 < 1) || (y % 8 < 1);
                                
                                // Greenish brown base
                                r = 120; g = 110; b = 60;
                                
                                // Pattern variations
                                if (patternX == 0 || patternY == 0) { r -= 20; g -= 15; }
                                if (edgeDetail) { r = 70; g = 65; b = 35; }
                            }
                            break;
                            
                        default:
                            // Fallback brown
                            r = 120; g = 100; b = 80;
                    }
                    
                    // Only add noise to textures 1-3 (texture 0 already has its own noise pattern)
                    if (i > 0) {
                        int noise = ((x * 13 + y * 7) % 10) - 5;
                        r = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(r) + noise)));
                        g = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(g) + noise)));
                        b = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(b) + noise)));
                    }
                    
                    // Combine into final ARGB color
                    uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    
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
        #endif
        
        // Load actual textures first for walls - use optimized memory copy
        #if defined(__SSE2__) || defined(_MSC_VER)
        for (int i = 0; i < 4 && i < m_wallTextureVariations.size(); i++) {
            // Get the correct texture ID from wall variations instead of using i directly
            int textureId = m_wallTextureVariations[i];
            const Texture* texture = m_textureManager->getTexture(textureId);
            
            if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
                const uint32_t* pixels = texture->getPixelData();
                if (pixels) {
                    // Copy to the corresponding section of wall texture data
                    uint32_t* dest = wallTextureData + (i * m_wallTextureWidth * m_wallTextureHeight);
                    const uint32_t* src = pixels;
                    size_t pixelCount = m_wallTextureWidth * m_wallTextureHeight;
                    
                    // Optimize memory copy using SSE
                    size_t vectorSize = pixelCount / 4;
                    size_t remainder = pixelCount % 4;
                    
                    // Copy 4 pixels at a time using SSE
                    for (size_t j = 0; j < vectorSize; ++j) {
                        __m128i pixels_vec = _mm_loadu_si128((__m128i*)src);
                        _mm_store_si128((__m128i*)dest, pixels_vec);
                        src += 4;
                        dest += 4;
                    }
                    
                    // Copy remaining pixels
                    for (size_t j = 0; j < remainder; ++j) {
                        *dest++ = *src++;
                    }
                } else {
                    std::cout << "CUDA: Wall texture " << textureId << " has no pixel data" << std::endl;
                }
            } else {
                std::cout << "CUDA: Wall texture ID " << textureId << " is invalid or has zero dimensions" << std::endl;
            }
        }
        #else
        // Non-SSE fallback for loading textures
        for (int i = 0; i < 4 && i < m_wallTextureVariations.size(); i++) {
            // Get the correct texture ID from wall variations instead of using i directly
            int textureId = m_wallTextureVariations[i];
            const Texture* texture = m_textureManager->getTexture(textureId);
            
            if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
                const uint32_t* pixels = texture->getPixelData();
                if (pixels) {
                    // Copy to the corresponding section of wall texture data
                    memcpy(
                        wallTextureData + (i * m_wallTextureWidth * m_wallTextureHeight),
                        pixels,
                        texSize
                    );
                } else {
                    std::cout << "CUDA: Wall texture " << textureId << " has no pixel data" << std::endl;
                }
            } else {
                std::cout << "CUDA: Wall texture ID " << textureId << " is invalid or has zero dimensions" << std::endl;
            }
        }
        #endif

        // If we don't have enough wall textures, fill in with fallbacks
        if (m_wallTextureVariations.size() < 4) {
            std::cout << "CUDA: Warning - Not enough wall textures provided (" << m_wallTextureVariations.size() 
                      << " out of 4), using fallbacks for remaining slots" << std::endl;
        }
        
        // Check if the first texture was loaded properly (slot 0)
        if (m_wallTextureVariations.size() > 0) {
            
            // Get the pixel data from the first texture slot
            uint32_t* firstTextureData = wallTextureData;
            
            // Copy the first texture to all other slots (1-3) to ensure consistency
            #if defined(__SSE2__) || defined(_MSC_VER)
            for (int i = 1; i < 4; i++) {
                uint32_t* dest = wallTextureData + (i * m_wallTextureWidth * m_wallTextureHeight);
                const uint32_t* src = firstTextureData;
                size_t pixelCount = m_wallTextureWidth * m_wallTextureHeight;
                
                // Optimize copy with SSE
                size_t vectorSize = pixelCount / 4;
                size_t remainder = pixelCount % 4;
                
                for (size_t j = 0; j < vectorSize; ++j) {
                    __m128i pixels_vec = _mm_load_si128((__m128i*)src);
                    _mm_store_si128((__m128i*)dest, pixels_vec);
                    src += 4;
                    dest += 4;
                }
                
                // Copy remaining pixels
                for (size_t j = 0; j < remainder; ++j) {
                    *dest++ = *src++;
                }
            }
            #else
            // Non-SSE fallback
            for (int i = 1; i < 4; i++) {
                memcpy(
                    wallTextureData + (i * m_wallTextureWidth * m_wallTextureHeight),
                    firstTextureData,
                    texSize
                );
            }
            #endif
        }
        
        // Load floor texture (using texture index 4)
        bool floorTextureLoaded = false;
        const Texture* floorTexture = m_textureManager->getTexture(4);
        if (floorTexture && floorTexture->getWidth() > 0 && floorTexture->getHeight() > 0) {
            const uint32_t* floorPixels = floorTexture->getPixelData();
            if (floorPixels) {
                #if defined(__SSE2__) || defined(_MSC_VER)
                // Optimize copy with SSE
                size_t pixelCount = m_wallTextureWidth * m_wallTextureHeight;
                size_t vectorSize = pixelCount / 4;
                size_t remainder = pixelCount % 4;
                
                uint32_t* dest = floorTextureData;
                const uint32_t* src = floorPixels;
                
                for (size_t j = 0; j < vectorSize; ++j) {
                    __m128i pixels_vec = _mm_loadu_si128((__m128i*)src);
                    _mm_store_si128((__m128i*)dest, pixels_vec);
                    src += 4;
                    dest += 4;
                }
                
                // Copy remaining pixels
                for (size_t j = 0; j < remainder; ++j) {
                    *dest++ = *src++;
                }
                #else
                // Non-SSE fallback
                memcpy(floorTextureData, floorPixels, texSize);
                #endif
                floorTextureLoaded = true;
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
                #if defined(__SSE2__) || defined(_MSC_VER)
                // Optimize copy with SSE
                size_t pixelCount = m_wallTextureWidth * m_wallTextureHeight;
                size_t vectorSize = pixelCount / 4;
                size_t remainder = pixelCount % 4;
                
                uint32_t* dest = ceilingTextureData;
                const uint32_t* src = ceilingPixels;
                
                for (size_t j = 0; j < vectorSize; ++j) {
                    __m128i pixels_vec = _mm_loadu_si128((__m128i*)src);
                    _mm_store_si128((__m128i*)dest, pixels_vec);
                    src += 4;
                    dest += 4;
                }
                
                // Copy remaining pixels
                for (size_t j = 0; j < remainder; ++j) {
                    *dest++ = *src++;
                }
                #else
                // Non-SSE fallback
                memcpy(ceilingTextureData, ceilingPixels, texSize);
                #endif
                ceilingTextureLoaded = true;
            }
        }
        
        if (!ceilingTextureLoaded) {
            std::cout << "Using default ceiling texture pattern" << std::endl;
        }
        
        // Copy textures to device with error checking - use CUDA streams for asynchronous transfers
        cudaError_t error;
        
        // Launch 3 asynchronous copies in parallel when possible
        error = cudaMemcpyAsync(m_deviceWallTextures, wallTextureData, 4 * texSize, cudaMemcpyHostToDevice, m_cudaStream);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy wall textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
        
        error = cudaMemcpyAsync(m_deviceFloorTextures, floorTextureData, texSize, cudaMemcpyHostToDevice, m_cudaStream);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy floor textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
        
        error = cudaMemcpyAsync(m_deviceCeilingTextures, ceilingTextureData, texSize, cudaMemcpyHostToDevice, m_cudaStream);
        if (error != cudaSuccess) {
            std::cerr << "Failed to copy ceiling textures to device: " << cudaGetErrorString(error) << std::endl;
            throw std::runtime_error("CUDA memory copy failed");
        }
        
        // Synchronize to ensure all copies are complete
        cudaStreamSynchronize(m_cudaStream);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in copyTexturesToDevice: " << e.what() << std::endl;
    }
    
    // Free host memory with proper aligned free
    #ifdef _MSC_VER
    if (wallTextureData) _aligned_free(wallTextureData);
    if (floorTextureData) _aligned_free(floorTextureData);
    if (ceilingTextureData) _aligned_free(ceilingTextureData);
    #else
    free(wallTextureData);
    free(floorTextureData);
    free(ceilingTextureData);
    #endif
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
    
    // CRITICAL FIX: Copy z-buffer back to host for sprite depth testing
    cudaStatus = cudaMemcpy(m_hostZBuffer, m_deviceZBuffer, 
                m_screenWidth * m_screenHeight * sizeof(float), 
                cudaMemcpyDeviceToHost);
    if (cudaStatus != cudaSuccess) {
        std::cerr << "Failed to copy z-buffer back to host: " << cudaGetErrorString(cudaStatus) << std::endl;
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
    
    // Always use the singleton instance for consistency
    SpriteManager* spriteManager = SpriteManager::getInstance();
    if (!spriteManager) {
        std::cerr << "ERROR: SpriteManager singleton is null in CudaRenderer::renderSprites!" << std::endl;
        return;
    }
    
    // Make sure our member pointer is in sync with the singleton
    if (m_spriteManager != spriteManager) {
        std::cout << "CUDA: Updating sprite manager pointer to match singleton" << std::endl;
        m_spriteManager = spriteManager;
    }
    
    // Use getActiveSprites() to get only active and visible sprites
    const std::vector<Sprite*> sprites = spriteManager->getActiveSprites();
    
    // Debug: Also check all sprites
    const std::vector<Sprite*>& allSprites = spriteManager->getSprites();
        
    // Count sprites by type for debugging
    int impCount = 0;
    int enemyCount = 0;
    int itemCount = 0;
    int otherCount = 0;
    
    for (const Sprite* sprite : sprites) {
        switch (sprite->getType()) {
            case SpriteType::ImpEnemy:
                impCount++;
                break;
            case SpriteType::Enemy:
                enemyCount++;
                break;
            case SpriteType::Item:
                itemCount++;
                break;
            default:
                otherCount++;
                break;
        }
    }
    
    std::cout << "CUDA: Active sprite types: " << impCount << " imps, " << enemyCount << " enemies, " 
              << itemCount << " items, " << otherCount << " other" << std::endl;
    
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
    int renderedCount = 0;
    for (auto& pair : sortedSprites) {
        Sprite* sprite = pair.second;
        
        // Calculate sprite position relative to camera
        float spriteX = sprite->getX() - player.getX();
        float spriteY = sprite->getY() - player.getY();
        
        // Transform sprite with the inverse camera matrix
        float invDet = 1.0f / (player.getPlaneX() * player.getDirY() - player.getDirX() * player.getPlaneY());
        float transformX = invDet * (player.getDirY() * spriteX - player.getDirX() * spriteY);
        float transformY = invDet * (-player.getPlaneY() * spriteX + player.getPlaneX() * spriteY);
        
        // Skip sprites behind the camera
        if (transformY <= 0.1f) {
            continue;
        }
        
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
        
        // Skip if the sprite is completely off-screen
        if (drawStartX >= m_screenWidth || drawEndX < 0 || drawStartY >= m_screenHeight || drawEndY < 0) {
            continue;
        }
        
        // Get sprite texture
        int textureId = sprite->getTextureId();
        SDL_Texture* texture = m_textureManager->getSDLTexture(textureId);
        if (!texture) {
            std::cerr << "CUDA: Invalid texture ID " << textureId << " for sprite type " 
                      << static_cast<int>(sprite->getType()) << std::endl;
            
            // Additional debugging for imp textures
            if (sprite->getType() == SpriteType::ImpEnemy) {
                std::cerr << "CUDA: Failed to get SDL texture for imp with texture ID " << textureId << std::endl;
                
                // Try to get the texture directly to see if it exists
                const Texture* tex = m_textureManager->getTexture(textureId);
                if (tex) {
                    std::cerr << "CUDA: Texture exists but SDL_Texture is null. Dimensions: " 
                              << tex->getWidth() << "x" << tex->getHeight() << std::endl;
                } else {
                    std::cerr << "CUDA: Texture does not exist in TextureManager" << std::endl;
                }
            }
            
            continue;
        }
        
        // Check texture properties
        Uint32 format;
        int access, w, h;
        SDL_QueryTexture(texture, &format, &access, &w, &h);
        
        
        // Set up source and destination rectangles
        SDL_Rect srcRect = { 0, 0, sprite->getWidth(), sprite->getHeight() };
        
        // For animated sprites, use the current frame
        if (sprite->getCurrentFrame() > 0) {
            srcRect.x = sprite->getCurrentFrame() * sprite->getWidth();
        }
        
        SDL_Rect dstRect = { drawStartX, drawStartY, drawEndX - drawStartX, drawEndY - drawStartY };
        
        // SIMPLER APPROACH: Only render sprites that are in front of walls
        // Calculate the average depth of the center of the sprite
        int spriteCenterX = (drawStartX + drawEndX) / 2;
        int spriteCenterY = (drawStartY + drawEndY) / 2;
        
        // Check if the center point is valid
        if (spriteCenterX >= 0 && spriteCenterX < m_screenWidth && 
            spriteCenterY >= 0 && spriteCenterY < m_screenHeight) {
            
            // Get wall distance at this point
            float wallDist = m_hostZBuffer[spriteCenterY * m_screenWidth + spriteCenterX];
            
            // Only render if the sprite is in front of the wall
            // Add a small bias to prevent z-fighting
            if (transformY <= wallDist + 0.1f) {
                // Ensure texture blend mode is set to BLEND for proper transparency
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                
                // Render sprite
                if (SDL_RenderCopy(m_sdlRenderer, texture, &srcRect, &dstRect) != 0) {
                    std::cerr << "CUDA: Failed to render sprite: " << SDL_GetError() << std::endl;
                } else {
                    renderedCount++;
                    
                    // Debug info for imp sprites
                    if (sprite->getType() == SpriteType::ImpEnemy) {
                        std::cout << "CUDA: Rendered imp at (" << sprite->getX() << ", " << sprite->getY() 
                                  << ") with texture ID " << textureId << " - Wall dist: " << wallDist 
                                  << ", Sprite dist: " << transformY << std::endl;
                    }
                }
            } else {
                // Debug info when sprite is occluded
                if (sprite->getType() == SpriteType::ImpEnemy) {
                    std::cout << "CUDA: Imp at (" << sprite->getX() << ", " << sprite->getY() 
                              << ") occluded - Wall dist: " << wallDist 
                              << ", Sprite dist: " << transformY << std::endl;
                }
            }
        }
    }
    
    std::cout << "CUDA: Successfully rendered " << renderedCount << " sprites" << std::endl;
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