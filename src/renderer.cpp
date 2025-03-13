#include "renderer.h"
#include "map.h"
#include "utils.h"
#include "player.h"
#include "TextureManager.h"
#include "engine.h"
#include <iostream>
#include <limits>
#include <cstring>

// Define ENABLE_CUDA for CUDA support
#define ENABLE_CUDA

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

// Forward declarations for texture functions
Color getDoomFloorColor(int x, int y, int floorTileSize);
Color getDoomCeilingColor(int x, int y, int tileSize);
Color getDoomWallColor(int textureId, double u, double v, double distance);
Vec2 pentagramPoint(int pointIndex, double centerX, double centerY, double radius);
bool isInsidePentagram(double x, double y, double centerX, double centerY, double radius, double lineWidth);

Renderer::Renderer() :
    m_window(nullptr),
    m_renderer(nullptr),
    m_frameBuffer(nullptr),
    m_pixelBuffer(nullptr),
    m_screenWidth(0),
    m_screenHeight(0),
    m_zBuffer(nullptr),
    m_renderDistance(1000.0),
    m_fogEnabled(true),
    m_fogColor(0, 0, 0),
    m_fogDistance(500.0),
    m_fogDensity(0.05),
    m_lightingEnabled(true),
    m_gammaCorrectionEnabled(true),
    m_textureFiltering(true),
    m_debugMode(false),
    m_backgroundColor(0, 0, 0),
    m_textureManager(nullptr),
    m_engine(nullptr),
    m_cudaRenderingEnabled(false),
    m_cudaInitialized(false)
#ifdef ENABLE_CUDA
    , m_cudaRenderer(nullptr)
#endif
{
}

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(int screenWidth, int screenHeight, bool fullscreen, bool vsync)
{
    // Store screen dimensions
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    
    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create window
    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    
    m_window = SDL_CreateWindow(
        "DOOM Clone",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_screenWidth,
        m_screenHeight,
        windowFlags
    );
    
    if (!m_window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (vsync) {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }
    
    m_renderer = SDL_CreateRenderer(m_window, -1, rendererFlags);
    
    if (!m_renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create framebuffer texture
    m_frameBuffer = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_screenWidth,
        m_screenHeight
    );
    
    if (!m_frameBuffer) {
        std::cerr << "Framebuffer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Allocate pixel buffer
    m_pixelBuffer = new uint32_t[m_screenWidth * m_screenHeight];
    
    // Allocate Z-buffer
    m_zBuffer = new double[m_screenWidth * m_screenHeight];
    
    // Initialize buffers
    clear();
    
#ifdef ENABLE_CUDA
    // Check if CUDA is available
    m_cudaRenderingEnabled = isCUDAAvailable();
    
    // Enable CUDA by default if available
    if (m_cudaRenderingEnabled) {
        std::cout << "CUDA is available, initializing CUDA renderer..." << std::endl;
        
        // Initialize CUDA right away
        if (!initCUDA()) {
            std::cerr << "Failed to initialize CUDA, falling back to CPU rendering" << std::endl;
            m_cudaRenderingEnabled = false;
        } else {
            std::cout << "CUDA renderer initialized successfully!" << std::endl;
        }
    } else {
        std::cout << "CUDA is not available, using CPU renderer" << std::endl;
    }
#else
    m_cudaRenderingEnabled = false;
    std::cout << "CUDA support is not enabled in this build" << std::endl;
#endif
    
    return true;
}

void Renderer::shutdown()
{
    // Free resources
    if (m_zBuffer) {
        delete[] m_zBuffer;
        m_zBuffer = nullptr;
    }
    
    if (m_pixelBuffer) {
        delete[] m_pixelBuffer;
        m_pixelBuffer = nullptr;
    }
    
    if (m_frameBuffer) {
        SDL_DestroyTexture(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
    
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
#ifdef ENABLE_CUDA
    // Free CUDA resources
    if (m_cudaRenderer) {
        m_cudaRenderer->shutdown();
        delete m_cudaRenderer;
        m_cudaRenderer = nullptr;
    }
    
    m_cudaInitialized = false;
#endif
}

void Renderer::beginFrame()
{
    // Clear buffers
    clear();
    
    // Reset render spans
    m_wallSpans.clear();
    m_floorCeilingSpans.clear();
    m_spriteSpans.clear();
}

void Renderer::endFrame()
{
    // Update texture with pixel buffer
    SDL_UpdateTexture(
        m_frameBuffer,
        nullptr,
        m_pixelBuffer,
        m_screenWidth * sizeof(uint32_t)
    );
    
    // Clear renderer
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    
    // Copy framebuffer to renderer
    SDL_RenderCopy(m_renderer, m_frameBuffer, nullptr, nullptr);
    
    // Present renderer - this makes everything visible
    SDL_RenderPresent(m_renderer);
}

void Renderer::clear(const Color& color)
{
    // Clear pixel buffer with the same format as setPixel
    uint32_t colorValue = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    for (int i = 0; i < m_screenWidth * m_screenHeight; ++i) {
        m_pixelBuffer[i] = colorValue;
    }
    
    // Clear Z-buffer
    for (int i = 0; i < m_screenWidth * m_screenHeight; ++i) {
        m_zBuffer[i] = std::numeric_limits<double>::max();
    }
}

void Renderer::renderMap(Map* map, Camera* camera)
{
    if (!map || !camera) {
        return;
    }
    
#ifdef ENABLE_CUDA
    // Use CUDA rendering if enabled and initialized
    if (m_cudaRenderingEnabled && m_cudaInitialized && m_cudaRenderer) {
        // Render using CUDA
        renderMapCUDA(map, camera);
        return;
    }
#endif
    
    // Fallback to CPU rendering
    
    // Ensure all structures are enclosed before rendering
    ensureStructuresEnclosed(map);
    
    // Clear z-buffer
    for (int i = 0; i < m_screenWidth * m_screenHeight; ++i) {
        m_zBuffer[i] = std::numeric_limits<double>::max();
    }
    
    // Clear span lists
    m_wallSpans.clear();
    m_floorCeilingSpans.clear();
    m_spriteSpans.clear();
    
    // Render skybox first
    renderSkybox(camera);
    
    // Get visible walls using BSP traversal
    std::vector<std::shared_ptr<Wall>> visibleWalls;
    map->getVisibleWalls(camera->getPosition(), visibleWalls);
    
    // Process visible walls
    processVisibleWalls(visibleWalls, camera);
    
    // Render walls, floors, ceilings, and sprites
    drawWallSpans(camera);
    drawFloorCeilingSpans(camera);
    drawSpriteSpans();
    
    // Render sun rays for volumetric lighting effect
    renderSunRays(camera);
}

void Renderer::setPixel(int x, int y, const Color& color)
{
    // Bounds check
    if (x < 0 || x >= m_screenWidth || y < 0 || y >= m_screenHeight) {
        return;
    }
    
    // Set pixel in buffer - use BGRA format for SDL (common on Windows platforms)
    // This is equivalent to ARGB in memory due to byte order
    uint32_t colorValue = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    m_pixelBuffer[y * m_screenWidth + x] = colorValue;
}

// Updated implementations
void Renderer::renderWalls(Map* map, Camera* camera) 
{
    // This is now handled by drawWallSpans()
}

void Renderer::renderFloorsCeilings(Map* map, Camera* camera) 
{
    // This is now handled by drawFloorCeilingSpans()
}

void Renderer::renderSprites(Map* map, Camera* camera) 
{
    // This is now handled by drawSpriteSpans()
}

// Add this new method before renderSkybox
double Renderer::calculateSunLighting(const Vec2& worldPos, Camera* camera) {
    if (!camera) return 1.0;
    
    // Get sun position in world space - fixed position regardless of camera
    // Increase height and adjust position to match the visible sun in the skybox
    double sunWorldX = 50.0; // Fixed sun position in world
    double sunWorldY = 50.0;
    double sunHeight = 50.0;  // Increased height of sun above ground for stronger lighting
    
    // Calculate vector from point to sun
    Vec2 toSun(sunWorldX - worldPos.x, sunWorldY - worldPos.y);
    double distToSun = toSun.length();
    
    // Normalize the vector
    toSun = toSun.normalized();
    
    // Increase base ambient lighting for better visibility
    double ambientLight = 0.5;  // Increased from 0.3 to 0.5
    
    // Distance attenuation - closer to sun = brighter
    // Reduce the falloff rate to allow light to reach further
    double distanceFactor = std::max(0.0, 1.0 - (distToSun / 150.0));  // Increased from 100.0 to 150.0
    
    // Add pulsing effect to simulate ray movement
    double time = SDL_GetTicks() / 1000.0; // Get time in seconds
    double pulse = 0.15 * sin(time * 2.0); // Slightly stronger pulsing effect
    
    // Combine factors for final lighting with increased intensity
    double lightIntensity = ambientLight + distanceFactor * 0.9 + pulse * 0.15;  // Increased multipliers
    
    // Ensure light intensity is in valid range but allow for brighter highlights
    return std::min(1.2, std::max(ambientLight, lightIntensity));  // Allow values slightly above 1.0 for bright highlights
}

// New function to calculate lighting with surface normal consideration
double Renderer::calculateSunLightingWithNormal(const Vec2& worldPos, const Vec2& normal, Camera* camera) {
    if (!camera) return 1.0;
    
    // Get sun position in world space - fixed position regardless of camera
    // Match the sun position with the visible sun in the skybox
    double sunWorldX = 50.0; // Fixed sun position in world
    double sunWorldY = 50.0;
    double sunHeight = 50.0;  // Increased height for stronger lighting
    
    // Calculate vector from point to sun
    Vec2 toSun(sunWorldX - worldPos.x, sunWorldY - worldPos.y);
    double distToSun = toSun.length();
    
    // Normalize the vector
    toSun = toSun.normalized();
    
    // Increase base ambient lighting for better visibility
    double ambientLight = 0.5;  // Increased from 0.3 to 0.5
    
    // Calculate how directly the surface faces the sun (dot product)
    double normalFactor = std::max(0.0, normal.dot(toSun));
    
    // Apply a power function to make the lighting more dramatic
    normalFactor = pow(normalFactor, 0.8);  // Less than 1.0 makes lighting more diffuse
    
    // Distance attenuation - closer to sun = brighter
    // Reduce the falloff rate to allow light to reach further
    double distanceFactor = std::max(0.0, 1.0 - (distToSun / 150.0));  // Increased from 100.0 to 150.0
    
    // Add pulsing effect to simulate ray movement
    double time = SDL_GetTicks() / 1000.0; // Get time in seconds
    double pulse = 0.15 * sin(time * 2.0); // Slightly stronger pulsing effect
    
    // Combine factors for final lighting with increased intensity
    double lightIntensity = ambientLight + normalFactor * distanceFactor * 0.9 + pulse * 0.15;  // Increased multipliers
    
    // Ensure light intensity is in valid range but allow for brighter highlights
    return std::min(1.2, std::max(ambientLight, lightIntensity));  // Allow values slightly above 1.0 for bright highlights
}

void Renderer::renderSkybox(Camera* camera) 
{
    // Get the camera angle to position the sun correctly
    double cameraAngle = camera ? camera->getAngle() : 0.0;
    
    // Create a hellish DOOM-like skybox
    for (int y = 0; y < m_screenHeight / 4; y++) {
        // Calculate sky color - gradient from dark red to orange-red
        double t = static_cast<double>(y) / (m_screenHeight / 4);
        Color skyColor = Color::lerp(
            Color(50, 10, 5),   // Dark blood red at top
            Color(120, 30, 10), // Fiery orange-red at horizon
            t
        );
        
        // Draw skybox line
        for (int x = 0; x < m_screenWidth; x++) {
            // Add some variation/clouds to the sky
            double cloudNoise = 0.0;
            
            // Simple procedural "cloud" effect
            int cloudX = x / 20;
            int cloudY = y / 10;
            int noise = ((cloudX * 7) + (cloudY * 19)) % 10;
            
            if (noise > 6) { // Darker spots in the sky
                cloudNoise = -10.0;
            } else if (noise < 3) { // Lighter spots
                cloudNoise = 10.0;
            }
            
            // Calculate sun position based on camera angle
            // Position the sun more centrally and make it larger
            double sunPositionX = m_screenWidth * 0.5 + sin(cameraAngle) * m_screenWidth * 0.2;
            double sunPositionY = m_screenHeight * 0.15; // Lower in the sky to be more visible
            double sunRadius = m_screenHeight * 0.1;    // Larger sun
            
            // Calculate distance from current pixel to sun center
            double distToSun = sqrt(pow(x - sunPositionX, 2) + pow(y - sunPositionY, 2));
            
            // Adjust color with noise and sun effect
            Color finalColor;
            
            // If pixel is in sun's core or outer glow
            if (distToSun < sunRadius) {
                // Inner sun - very bright yellow-white core
                double sunIntensity = 1.0 - (distToSun / sunRadius);
                sunIntensity = pow(sunIntensity, 0.6); // Make the falloff more gradual
                finalColor = Color::lerp(
                    Color(255, 200, 100), // Outer sun (orange-yellow)
                    Color(255, 255, 230), // Inner sun (bright white-yellow)
                    sunIntensity
                );
                
                // Add sun rays
                if (distToSun < sunRadius * 0.7) {
                    // Create ray effect emanating from sun center
                    double angle = atan2(y - sunPositionY, x - sunPositionX);
                    double rayIntensity = 0.5 + 0.5 * sin(angle * 8.0); // 8 rays
                    rayIntensity = pow(rayIntensity, 0.5); // Sharpen rays
                    
                    // Extend rays beyond sun
                    double rayLength = sunRadius * 4.0; // Longer rays
                    if (distToSun < rayLength) {
                        double rayFalloff = 1.0 - (distToSun / rayLength);
                        rayFalloff = pow(rayFalloff, 1.3); // Adjust falloff
                        
                        // Add ray color
                        Color rayColor(255, 255, 200);
                        finalColor = Color::lerp(finalColor, rayColor, rayIntensity * rayFalloff * 0.5); // Stronger ray effect
                    }
                }
            } 
            // Outer glow of the sun - extend this further
            else if (distToSun < sunRadius * 3.5) { // Increased from 2.5 to 3.5
                double glowIntensity = 1.0 - ((distToSun - sunRadius) / (sunRadius * 2.5)); // Increased glow range
                glowIntensity = pow(glowIntensity, 0.7); // More gradual falloff
                Color glowColor = Color(
                    std::min(255, static_cast<int>(220 * glowIntensity) + skyColor.r), // Brighter glow
                    std::min(255, static_cast<int>(180 * glowIntensity) + skyColor.g),
                    std::min(255, static_cast<int>(120 * glowIntensity) + skyColor.b)
                );
                finalColor = glowColor;
                
                // Add stronger ray effect in glow region
                double angle = atan2(y - sunPositionY, x - sunPositionX);
                double rayIntensity = 0.4 + 0.6 * sin(angle * 8.0); // Stronger contrast
                rayIntensity *= glowIntensity * 0.4; // Increased from 0.3
                
                Color rayColor(255, 255, 200);
                finalColor = Color::lerp(finalColor, rayColor, rayIntensity);
            }
            // Regular sky with clouds - make it brighter overall
            else {
                finalColor = Color(
                    std::min(255, std::max(0, static_cast<int>(skyColor.r * 1.2 + cloudNoise))), // Multiply by 1.2 for brighter sky
                    std::min(255, std::max(0, static_cast<int>(skyColor.g * 1.2 + cloudNoise * 0.5))),
                    std::min(255, std::max(0, static_cast<int>(skyColor.b * 1.2 + cloudNoise * 0.2)))
                );
            }
            
            setPixel(x, y, finalColor);
            
            // Set z-buffer to a very large value for skybox
            m_zBuffer[y * m_screenWidth + x] = 0.01;
        }
    }
    
    // Add a smooth transition from skybox to ceiling
    int transitionHeight = 20; // Height of the transition area
    int startY = m_screenHeight / 4;
    int endY = startY + transitionHeight;
    
    // Get current time for animation
    double time = SDL_GetTicks() / 1000.0;
    
    // Calculate sun position for ray casting
    double sunPositionX = m_screenWidth * 0.5 + sin(cameraAngle) * m_screenWidth * 0.2;
    double sunPositionY = m_screenHeight * 0.15;
    
    for (int y = startY; y < endY; y++) {
        // Calculate blend factor (0 at start, 1 at end)
        double blend = static_cast<double>(y - startY) / transitionHeight;
        blend = pow(blend, 0.7); // Make transition more gradual
        
        // Get ceiling color at horizon
        Color ceilingColor(45, 20, 10); // Base ceiling color
        
        // Blend from sky to ceiling
        Color skyColor = Color(120, 30, 10); // Horizon color
        Color transitionColor = Color::lerp(skyColor, ceilingColor, blend);
        
        for (int x = 0; x < m_screenWidth; x++) {
            // Calculate distance from sun for light effect
            double distToSun = sqrt(pow(x - sunPositionX, 2) + pow(y - sunPositionY, 2));
            double sunInfluence = std::max(0.0, 1.0 - distToSun / (m_screenWidth * 0.3));
            
            // Add subtle ray effect in transition area
            double angle = atan2(y - sunPositionY, x - sunPositionX);
            double rayEffect = 0.2 * sin(angle * 8.0 + time * 2.0);
            rayEffect *= sunInfluence;
            
            // Apply ray effect to transition color
            Color finalColor = transitionColor;
            if (rayEffect > 0.05) {
                Color rayColor(255, 255, 220);
                finalColor = Color::lerp(finalColor, rayColor, rayEffect);
            }
            
            setPixel(x, y, finalColor);
            
            // Set z-buffer to allow rays to pass through
            m_zBuffer[y * m_screenWidth + x] = 5.0; // Small value but larger than skybox
        }
    }
}

void Renderer::processVisibleWalls(const std::vector<std::shared_ptr<Wall>>& walls, Camera* camera)
{
    if (!camera) return;
    
    double eyeX, eyeY, eyeZ, dirX, dirY, dirZ;
    camera->getViewMatrix(eyeX, eyeY, eyeZ, dirX, dirY, dirZ);
    
    // Get camera FOV and calculate projection variables
    double fov = camera->getFOV();
    double halfFovTan = tan(fov / 2.0);
    double aspectRatio = static_cast<double>(m_screenWidth) / m_screenHeight;
    
    // Process each visible wall
    for (const auto& wall : walls) {
        // Get wall points in world space
        Vec2 start = wall->getStart();
        Vec2 end = wall->getEnd();
        
        // Calculate wall direction and normal
        Vec2 wallDir = (end - start).normalized();
        Vec2 wallNormal(-wallDir.y, wallDir.x); // Perpendicular to wall direction
        
        // Transform points to camera space
        // First, translate relative to camera position
        double startX = start.x - eyeX;
        double startY = start.y - eyeY;
        double endX = end.x - eyeX;
        double endY = end.y - eyeY;
        
        // Rotate points around camera (inverse of camera rotation)
        double cosAngle = cos(-camera->getAngle());
        double sinAngle = sin(-camera->getAngle());
        
        double rotStartX = startX * cosAngle - startY * sinAngle;
        double rotStartY = startX * sinAngle + startY * cosAngle;
        double rotEndX = endX * cosAngle - endY * sinAngle;
        double rotEndY = endX * sinAngle + endY * cosAngle;
        
        // Skip walls behind the camera
        if (rotStartX < 0.1 && rotEndX < 0.1) {
            continue;
        }
        
        // Clip walls that are partially behind the camera
        if (rotStartX < 0.1) {
            // Interpolate to find the intersection with the near plane
            double t = (0.1 - rotStartX) / (rotEndX - rotStartX);
            rotStartY = rotStartY + t * (rotEndY - rotStartY);
            rotStartX = 0.1;
        } else if (rotEndX < 0.1) {
            // Interpolate to find the intersection with the near plane
            double t = (0.1 - rotEndX) / (rotStartX - rotEndX);
            rotEndY = rotEndY + t * (rotStartY - rotEndY);
            rotEndX = 0.1;
        }
        
        // Calculate screen x-coordinates
        double startScreenX = (rotStartY / rotStartX / halfFovTan / aspectRatio + 1.0) * m_screenWidth / 2.0;
        double endScreenX = (rotEndY / rotEndX / halfFovTan / aspectRatio + 1.0) * m_screenWidth / 2.0;
        
        // Skip walls that are completely off-screen
        if ((startScreenX < 0 && endScreenX < 0) || 
            (startScreenX >= m_screenWidth && endScreenX >= m_screenWidth)) {
            continue;
        }
        
        // Use default sector values since we don't have sector information in our simple map
        double floorHeight = 0.0;
        double ceilingHeight = 4.0;
        double lightLevel = 1.0;
        
        // Calculate wall height (for now, all walls have the same height)
        double wallHeight = 4.0;
        double bottomZ = floorHeight;
        double topZ = bottomZ + wallHeight;
        
        // Calculate wall distances for perspective correction
        double startDist = rotStartX;
        double endDist = rotEndX;
        
        // Calculate wall heights on screen
        double startScreenBottomY = m_screenHeight / 2.0 * (1.0 + (eyeZ - bottomZ) / (rotStartX * halfFovTan));
        double startScreenTopY = m_screenHeight / 2.0 * (1.0 + (eyeZ - topZ) / (rotStartX * halfFovTan));
        double endScreenBottomY = m_screenHeight / 2.0 * (1.0 + (eyeZ - bottomZ) / (rotEndX * halfFovTan));
        double endScreenTopY = m_screenHeight / 2.0 * (1.0 + (eyeZ - topZ) / (rotEndX * halfFovTan));
        
        // Clamp to screen bounds
        int screenStartX = static_cast<int>(startScreenX);
        int screenEndX = static_cast<int>(endScreenX);
        
        // Skip if entirely off-screen
        if (screenEndX < 0 || screenStartX >= m_screenWidth) {
            continue;
        }
        
        // Clamp to screen width
        screenStartX = std::max(0, screenStartX);
        screenEndX = std::min(m_screenWidth - 1, screenEndX);
        
        // Calculate texture mapping parameters
        double wallLength = Vec2(end - start).length();
        double textureScaleX = 1.0 / wallLength;
        
        // Wall direction for texture mapping - REMOVED DUPLICATE DECLARATION
        // Using the wallDir already declared above
        
        // Calculate wall center for lighting
        Vec2 wallCenter = Vec2((start.x + end.x) * 0.5, (start.y + end.y) * 0.5);
        
        // Calculate sun lighting using wall normal
        // Get sun position in world space
        double sunWorldX = 50.0;
        double sunWorldY = 50.0;
        
        // Calculate vector from wall center to sun
        Vec2 toSun(sunWorldX - wallCenter.x, sunWorldY - wallCenter.y);
        toSun = toSun.normalized();
        
        // Calculate how directly the wall faces the sun (dot product)
        double normalFactor = std::max(0.0, wallNormal.dot(toSun));
        
        // Calculate distance-based attenuation
        double distToSun = Vec2(sunWorldX - wallCenter.x, sunWorldY - wallCenter.y).length();
        double distanceFactor = std::max(0.0, 1.0 - (distToSun / 100.0));
        
        // Base ambient lighting
        double ambientLight = 0.3;
        
        // Calculate final lighting for this wall
        double wallLighting = ambientLight + normalFactor * distanceFactor * 0.7;
        
        // Generate wall spans for each vertical column
        for (int x = screenStartX; x <= screenEndX; x++) {
            // Calculate t parameter for interpolation (0 to 1 along the wall)
            double t = (x - startScreenX) / (endScreenX - startScreenX);
            if (endScreenX == startScreenX) t = 0; // Avoid division by zero
            
            // Interpolate wall height at this column
            double screenBottomY = lerp(startScreenBottomY, endScreenBottomY, t);
            double screenTopY = lerp(startScreenTopY, endScreenTopY, t);
            
            // Ensure screenBottomY is properly floored to avoid gaps between walls and floor
            // This addresses the issue with the wall-to-floor transition
            screenBottomY = std::ceil(screenBottomY);
            
            // Interpolate distance for Z-buffer
            double distance = lerp(startDist, endDist, t);
            
            // Calculate texture U coordinate - use absolute position along the wall
            // This ensures consistent texturing regardless of wall angle
            Vec2 pointOnWall = start + wallDir * (t * wallLength);
            double worldDistance = (pointOnWall - start).length();
            
            // The texture coordinate repeats every 2 world units
            double textureRepeat = 2.0;
            double u = (worldDistance / textureRepeat);
            
            // Create wall span
            WallSpan span;
            span.x = x;
            span.y1 = static_cast<int>(screenTopY);
            span.y2 = static_cast<int>(screenBottomY);
            span.z1 = topZ;
            span.z2 = bottomZ;
            span.u = u;
            span.texU = u; // Set texU as well for newer implementations
            span.distance = distance;
            span.textureId = wall->getTextureId();
            span.isPortal = false; // Simple map doesn't have portals
            span.lightLevel = wallLighting; // Use our calculated lighting
            span.sector = nullptr; // No sector info in simple map
            span.wall = nullptr; // No wall pointer needed
            
            // Add to wall spans for rendering
            m_wallSpans.push_back(span);
        }
    }
}

void Renderer::calculateWallSpans(Wall* wall, Camera* camera, Sector* sector) 
{
    // This functionality is now handled by processVisibleWalls
}

void Renderer::calculateFloorCeilingSpans(Sector* sector, Camera* camera) 
{
    // Simplified floor/ceiling rendering could be added here
}

void Renderer::calculateSpriteSpans(Sprite* sprite, Camera* camera) 
{
    // Sprite rendering not implemented in this basic version
}

void Renderer::drawWallSpans(Camera* camera)  // Add camera parameter
{
    if (!camera) return;
    
    // Get sun position for lighting calculations
    double sunX = 50.0;
    double sunY = 50.0;
    double sunZ = 50.0; // Increased height for stronger lighting
    
    // Get current time for pulsing effect
    double time = SDL_GetTicks() / 1000.0;
    double pulse = 0.15 * sin(time * 1.5); // Pulsing effect
    
    // Base ambient light level
    double ambientLight = 0.5; // Increased from 0.3 for better visibility
    
    // Process each wall span
    for (const auto& span : m_wallSpans) {
        // Skip invalid spans
        if (span.y1 >= span.y2 || span.x < 0 || span.x >= m_screenWidth) {
            continue;
        }
        
        // Calculate texture coordinates
        double u = span.texU;
        
        // Calculate lighting
        double lightLevel = span.lightLevel;
        
        // Apply distance-based attenuation
        double distanceFactor = 1.0 - std::min(1.0, span.distance / 150.0); // Increased from 100.0
        
        // Add pulsing effect
        lightLevel += pulse;
        
        // Ensure lighting is in valid range but allow for brighter highlights
        lightLevel = std::min(1.2, std::max(ambientLight, lightLevel));
        
        // Boost lighting by 30% for better visibility
        lightLevel = std::min(1.2, lightLevel * 1.3);
        
        // Calculate wall height for texture mapping
        double wallHeight = span.z2 - span.z1;
        
        // Draw the wall span
        for (int y = span.y1; y <= span.y2; y++) {
            // Skip if outside screen bounds
            if (y < 0 || y >= m_screenHeight) {
                continue;
            }
            
            // Calculate texture v-coordinate
            double v = (y - span.y1) / static_cast<double>(span.y2 - span.y1);
            
            // Get wall color from texture
            Color wallColor = getDoomWallColor(span.textureId, u, v, span.distance);
            
            // Apply lighting
            wallColor = Color(
                std::min(255, static_cast<int>(wallColor.r * lightLevel)),
                std::min(255, static_cast<int>(wallColor.g * lightLevel)),
                std::min(255, static_cast<int>(wallColor.b * lightLevel))
            );
            
            // Apply reduced fog effect for better visibility
            double fogFactor = std::min(1.0, span.distance / 100.0);
            fogFactor = pow(fogFactor, 0.8); // Less aggressive fog falloff
            
            // Reduce fog effect for better visibility
            double minVisibility = 0.92; // Increased from 0.85
            fogFactor *= (1.0 - minVisibility);
            
            Color foggedColor = Color::lerp(
                wallColor,
                Color(30, 10, 10), // Darker fog color for better contrast
                fogFactor
            );
            
            // Calculate pixel index
            int index = y * m_screenWidth + span.x;
            
            // Only draw if this pixel is closer than what's already there
            if (span.distance < m_zBuffer[index]) {
                // Set the pixel
                setPixel(span.x, y, foggedColor);
                
                // Update z-buffer
                m_zBuffer[index] = span.distance;
            }
        }
        
        // Add wall edge enhancement to fix gaps
        // Draw a 1-pixel wide vertical line at the edges of walls to ensure enclosure
        if (span.textureId >= 0) { // Only for real walls, not portals
            // Get the wall edges
            int leftX = span.x - 1;
            int rightX = span.x + 1;
            
            // Process left edge if it's within screen bounds
            if (leftX >= 0) {
                for (int y = span.y1; y <= span.y2; y++) {
                    if (y < 0 || y >= m_screenHeight) continue;
                    
                    int index = y * m_screenWidth + leftX;
                    
                    // Only draw if there's no wall already there (to fill gaps)
                    if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                        // Get wall color but darker for edge
                        double v = (y - span.y1) / static_cast<double>(span.y2 - span.y1);
                        Color edgeColor = getDoomWallColor(span.textureId, u, v, span.distance);
                        
                        // Make edge darker
                        edgeColor = Color(
                            static_cast<int>(edgeColor.r * 0.7),
                            static_cast<int>(edgeColor.g * 0.7),
                            static_cast<int>(edgeColor.b * 0.7)
                        );
                        
                        // Apply lighting and fog
                        edgeColor = Color(
                            std::min(255, static_cast<int>(edgeColor.r * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.g * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.b * lightLevel))
                        );
                        
                        // Set the pixel and update z-buffer
                        setPixel(leftX, y, edgeColor);
                        m_zBuffer[index] = span.distance + 0.1; // Slightly behind main wall
                    }
                }
            }
            
            // Process right edge if it's within screen bounds
            if (rightX < m_screenWidth) {
                for (int y = span.y1; y <= span.y2; y++) {
                    if (y < 0 || y >= m_screenHeight) continue;
                    
                    int index = y * m_screenWidth + rightX;
                    
                    // Only draw if there's no wall already there (to fill gaps)
                    if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                        // Get wall color but darker for edge
                        double v = (y - span.y1) / static_cast<double>(span.y2 - span.y1);
                        Color edgeColor = getDoomWallColor(span.textureId, u, v, span.distance);
                        
                        // Make edge darker
                        edgeColor = Color(
                            static_cast<int>(edgeColor.r * 0.7),
                            static_cast<int>(edgeColor.g * 0.7),
                            static_cast<int>(edgeColor.b * 0.7)
                        );
                        
                        // Apply lighting and fog
                        edgeColor = Color(
                            std::min(255, static_cast<int>(edgeColor.r * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.g * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.b * lightLevel))
                        );
                        
                        // Set the pixel and update z-buffer
                        setPixel(rightX, y, edgeColor);
                        m_zBuffer[index] = span.distance + 0.1; // Slightly behind main wall
                    }
                }
            }
            
            // Add wider edge enhancement for better enclosure (2 pixels on each side)
            int farLeftX = span.x - 2;
            int farRightX = span.x + 2;
            
            // Process far left edge if it's within screen bounds
            if (farLeftX >= 0) {
                for (int y = span.y1; y <= span.y2; y++) {
                    if (y < 0 || y >= m_screenHeight) continue;
                    
                    int index = y * m_screenWidth + farLeftX;
                    
                    // Only draw if there's no wall already there (to fill gaps)
                    if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                        // Get wall color but darker for edge
                        double v = (y - span.y1) / static_cast<double>(span.y2 - span.y1);
                        Color edgeColor = getDoomWallColor(span.textureId, u, v, span.distance);
                        
                        // Make edge even darker
                        edgeColor = Color(
                            static_cast<int>(edgeColor.r * 0.5),
                            static_cast<int>(edgeColor.g * 0.5),
                            static_cast<int>(edgeColor.b * 0.5)
                        );
                        
                        // Apply lighting and fog
                        edgeColor = Color(
                            std::min(255, static_cast<int>(edgeColor.r * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.g * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.b * lightLevel))
                        );
                        
                        // Set the pixel and update z-buffer
                        setPixel(farLeftX, y, edgeColor);
                        m_zBuffer[index] = span.distance + 0.2; // Even further behind
                    }
                }
            }
            
            // Process far right edge if it's within screen bounds
            if (farRightX < m_screenWidth) {
                for (int y = span.y1; y <= span.y2; y++) {
                    if (y < 0 || y >= m_screenHeight) continue;
                    
                    int index = y * m_screenWidth + farRightX;
                    
                    // Only draw if there's no wall already there (to fill gaps)
                    if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                        // Get wall color but darker for edge
                        double v = (y - span.y1) / static_cast<double>(span.y2 - span.y1);
                        Color edgeColor = getDoomWallColor(span.textureId, u, v, span.distance);
                        
                        // Make edge even darker
                        edgeColor = Color(
                            static_cast<int>(edgeColor.r * 0.5),
                            static_cast<int>(edgeColor.g * 0.5),
                            static_cast<int>(edgeColor.b * 0.5)
                        );
                        
                        // Apply lighting and fog
                        edgeColor = Color(
                            std::min(255, static_cast<int>(edgeColor.r * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.g * lightLevel)),
                            std::min(255, static_cast<int>(edgeColor.b * lightLevel))
                        );
                        
                        // Set the pixel and update z-buffer
                        setPixel(farRightX, y, edgeColor);
                        m_zBuffer[index] = span.distance + 0.2; // Even further behind
                    }
                }
            }
        }
    }
    
    // Add horizontal connections between adjacent wall spans to fill vertical gaps
    // This is a post-processing step after all walls are drawn
    for (int x = 0; x < m_screenWidth; x++) {
        for (int y = 0; y < m_screenHeight; y++) {
            // Skip skybox and transition area
            if (y < m_screenHeight / 4 + 20) continue;
            
            int index = y * m_screenWidth + x;
            
            // Skip if this pixel already has a wall
            if (m_zBuffer[index] < std::numeric_limits<double>::max()) continue;
            
            // Check if there are walls to the left and right
            bool hasWallLeft = false;
            bool hasWallRight = false;
            double leftDist = 0.0;
            double rightDist = 0.0;
            int leftTextureId = 0;
            int rightTextureId = 0;
            
            // Check left
            if (x > 0) {
                int leftIndex = y * m_screenWidth + (x - 1);
                if (m_zBuffer[leftIndex] < std::numeric_limits<double>::max()) {
                    hasWallLeft = true;
                    leftDist = m_zBuffer[leftIndex];
                    
                    // Try to determine texture ID from nearby wall spans
                    for (const auto& span : m_wallSpans) {
                        if (span.x == x - 1 && y >= span.y1 && y <= span.y2) {
                            leftTextureId = span.textureId;
                            break;
                        }
                    }
                }
            }
            
            // Check right
            if (x < m_screenWidth - 1) {
                int rightIndex = y * m_screenWidth + (x + 1);
                if (m_zBuffer[rightIndex] < std::numeric_limits<double>::max()) {
                    hasWallRight = true;
                    rightDist = m_zBuffer[rightIndex];
                    
                    // Try to determine texture ID from nearby wall spans
                    for (const auto& span : m_wallSpans) {
                        if (span.x == x + 1 && y >= span.y1 && y <= span.y2) {
                            rightTextureId = span.textureId;
                            break;
                        }
                    }
                }
            }
            
            // If we have walls on both sides, fill the gap
            if (hasWallLeft && hasWallRight) {
                // Use the closer wall's distance and texture
                double distance = std::min(leftDist, rightDist);
                int textureId = (leftDist < rightDist) ? leftTextureId : rightTextureId;
                
                // If we couldn't determine a texture ID, use a default
                if (textureId <= 0) textureId = 1;
                
                // Calculate a reasonable u,v coordinate
                double u = 0.5; // Middle of texture
                double v = (y % 64) / 64.0; // Some variation based on y
                
                // Get wall color
                Color wallColor = getDoomWallColor(textureId, u, v, distance);
                
                // Apply lighting (use a slightly darker value for the connection)
                double connectionLightLevel = 0.4 + 0.1 * sin(time * 1.5 + x * 0.1 + y * 0.1);
                wallColor = Color(
                    std::min(255, static_cast<int>(wallColor.r * connectionLightLevel)),
                    std::min(255, static_cast<int>(wallColor.g * connectionLightLevel)),
                    std::min(255, static_cast<int>(wallColor.b * connectionLightLevel))
                );
                
                // Apply fog
                double fogFactor = std::min(1.0, distance / 100.0);
                fogFactor = pow(fogFactor, 0.8);
                fogFactor *= 0.08; // Reduced fog effect
                
                Color foggedColor = Color::lerp(
                    wallColor,
                    Color(30, 10, 10),
                    fogFactor
                );
                
                // Set the pixel and update z-buffer
                setPixel(x, y, foggedColor);
                m_zBuffer[index] = distance + 0.05; // Slightly behind the main walls
            }
        }
    }
}

void Renderer::drawFloorCeilingSpans(Camera* camera)
{
    // Render floor and ceiling with DOOM-style textures
    const int tileSize = 32; // Size of floor tiles in world units
    double cameraHeight = 0.6; // Camera height (eye level)
    
    // Calculate the horizon line (vertical center of the screen)
    int horizonY = m_screenHeight / 2;
    
    // Render floor (bottom half of screen)
    for (int y = horizonY; y < m_screenHeight; y++) {
        // Calculate position relative to horizon (0 at horizon, 1 at bottom of screen)
        double relativeY = (y - horizonY) / static_cast<double>(m_screenHeight - horizonY);
        
        // Apply DOOM-like perspective for floor
        // This formula creates the classic DOOM floor perspective with correct depth
        double rowDistance = cameraHeight / (2.0 * relativeY - 1.0 + 1e-5); // Add small epsilon to avoid division by zero
        
        // Distance factor for fog effect - reduce fog for better visibility
        double distFactor = std::min(1.0, rowDistance / 30.0); // Increased from 20.0 to 30.0
        
        for (int x = 0; x < m_screenWidth; x++) {
            // Skip if a wall has already been drawn here (check z-buffer)
            if (m_zBuffer[y * m_screenWidth + x] < std::numeric_limits<double>::max()) {
                continue;
            }
            
            // Calculate ray direction for this pixel
            double rayRatio = (2.0 * x / static_cast<double>(m_screenWidth) - 1.0); // -1 to 1 across screen width
            Vec2 camDir = camera->getDirection();
            Vec2 camRight = camera->getRight();
            
            // Calculate ray direction using camera direction and right vectors
            double rayDirX = camDir.x + camRight.x * rayRatio * 1.2; // Adjust FOV here to match walls
            double rayDirY = camDir.y + camRight.y * rayRatio * 1.2;
            double dirLen = sqrt(rayDirX * rayDirX + rayDirY * rayDirY);
            rayDirX /= dirLen; // Normalize
            rayDirY /= dirLen;
            
            // Calculate the real-world floor coordinate
            double floorX = camera->getPosition().x + rowDistance * rayDirX;
            double floorY = camera->getPosition().y + rowDistance * rayDirY;
            
            // Convert to integer coordinates for texture lookup
            int texX = static_cast<int>(floorX * tileSize);
            int texY = static_cast<int>(floorY * tileSize);
            
            // Get the color for this floor position
            Color floorColor = getDoomFloorColor(texX, texY, tileSize);
            
            // Calculate lighting for floor (floor normal is always (0,0,1) - pointing up)
            // Get sun position in world space
            double sunWorldX = 50.0;
            double sunWorldY = 50.0;
            double sunWorldZ = 50.0; // Increased sun height to match other changes
            
            // Calculate 3D vector from floor point to sun
            double toSunX = sunWorldX - floorX;
            double toSunY = sunWorldY - floorY;
            double toSunZ = sunWorldZ - 0.0; // Floor is at Z=0
            
            // Normalize the vector
            double toSunLength = sqrt(toSunX*toSunX + toSunY*toSunY + toSunZ*toSunZ);
            toSunX /= toSunLength;
            toSunY /= toSunLength;
            toSunZ /= toSunLength;
            
            // Dot product with floor normal (0,0,1) is just the Z component of the normalized vector
            double normalFactor = std::max(0.0, toSunZ);
            
            // Apply a power function to make the lighting more dramatic
            normalFactor = pow(normalFactor, 0.8); // Less than 1.0 makes lighting more diffuse
            
            // Distance-based attenuation
            double distToSun = sqrt(pow(sunWorldX - floorX, 2) + pow(sunWorldY - floorY, 2) + pow(sunWorldZ, 2));
            double distanceFactor = std::max(0.0, 1.0 - (distToSun / 150.0)); // Increased from 100.0 to 150.0
            
            // Base ambient lighting - increased for better visibility
            double ambientLight = 0.5; // Increased from 0.3 to 0.5
            
            // Calculate final lighting with increased intensity
            double floorLighting = ambientLight + normalFactor * distanceFactor * 0.9; // Increased from 0.7 to 0.9
            
            // Add subtle pulsing effect
            double time = SDL_GetTicks() / 1000.0;
            double pulse = 0.05 * sin(time * 2.0);
            floorLighting += pulse;
            
            // Ensure lighting is in valid range but allow for brighter highlights
            floorLighting = std::min(1.2, std::max(ambientLight, floorLighting));
            
            // Apply lighting to the floor color
            floorColor = Color(
                std::min(255, static_cast<int>(floorColor.r * floorLighting)),
                std::min(255, static_cast<int>(floorColor.g * floorLighting)),
                std::min(255, static_cast<int>(floorColor.b * floorLighting))
            );
            
            // Apply reduced fog effect to allow lighting to be visible
            Color foggedFloorColor = Color::lerp(
                floorColor,
                Color(50, 50, 50), // Lighter fog color (increased from 40,40,40)
                distFactor * 0.4  // Reduced fog intensity (from 0.5 to 0.4)
            );
            
            // Set the pixel for the floor
            setPixel(x, y, foggedFloorColor);
            
            // Calculate mirror position for ceiling
            int ceilingY = m_screenHeight - y - 1;
            
            // Only draw ceiling if not already occupied by a wall or sky
            // Skip rendering ceiling in the top quarter of screen where skybox is
            if (ceilingY < m_screenHeight / 4) {
                // This is skybox territory, don't draw ceiling here
                continue;
            }
            
            if (m_zBuffer[ceilingY * m_screenWidth + x] == std::numeric_limits<double>::max()) {
                // Get ceiling color - darker variation of floor
                Color ceilingColor = getDoomCeilingColor(texX, texY, tileSize);
                
                // Calculate lighting for ceiling (ceiling normal is always (0,0,-1) - pointing down)
                // Dot product with ceiling normal (0,0,-1) is the negative of the Z component
                double ceilingNormalFactor = std::max(0.0, -toSunZ);
                
                // Apply a power function to make the lighting more dramatic
                ceilingNormalFactor = pow(ceilingNormalFactor, 0.8); // Less than 1.0 makes lighting more diffuse
                
                // Calculate final lighting for ceiling with increased intensity
                double ceilingLighting = ambientLight + ceilingNormalFactor * distanceFactor * 0.9; // Increased from 0.7 to 0.9
                
                // Add subtle pulsing effect
                ceilingLighting += pulse;
                
                // Ensure lighting is in valid range but allow for brighter highlights
                ceilingLighting = std::min(1.2, std::max(ambientLight, ceilingLighting));
                
                // Apply lighting to the ceiling color
                ceilingColor = Color(
                    std::min(255, static_cast<int>(ceilingColor.r * ceilingLighting)),
                    std::min(255, static_cast<int>(ceilingColor.g * ceilingLighting)),
                    std::min(255, static_cast<int>(ceilingColor.b * ceilingLighting))
                );
                
                // Apply reduced fog effect to ceiling
                Color foggedCeilingColor = Color::lerp(
                    ceilingColor,
                    Color(40, 35, 35), // Slightly lighter fog for ceiling (increased from 30,25,25)
                    distFactor * 0.5   // Reduced fog intensity (from 0.6 to 0.5)
                );
                
                // Set the pixel for the ceiling
                setPixel(x, ceilingY, foggedCeilingColor);
            }
        }
    }
}

void Renderer::drawSpriteSpans() 
{
    // Not implemented in this basic version
}

Color Renderer::applyLighting(const Color& color, double lightLevel) 
{ 
    return Color::lerp(Color(0, 0, 0), color, lightLevel); 
}

Color Renderer::applyFog(const Color& color, double distance) 
{ 
    double fogFactor = 1.0 - std::min(1.0, distance / m_fogDistance);
    return Color::lerp(m_fogColor, color, fogFactor);
}

double Renderer::calculateLightLevel(Sector* sector, const Vec2& position, double height) 
{ 
    return sector ? sector->getLightLevel() : 1.0;
}

Color Renderer::sampleTexture(int textureId, double u, double v, bool filter) 
{ 
    // Simplified texture sampling - we're using solid colors in this basic version
    return Colors::WHITE;
}

void Renderer::drawText(const std::string& text, int x, int y, const Color& color) 
{ 
    // Simple text rendering
    int charWidth = 8;
    int charHeight = 16;
    
    for (size_t i = 0; i < text.length(); i++) {
        // Draw a simple rectangle for each character
        int startX = x + i * charWidth;
        for (int cy = 0; cy < charHeight; cy++) {
            for (int cx = 0; cx < charWidth - 1; cx++) {
                setPixel(startX + cx, y + cy, color);
            }
        }
    }
}

void Renderer::renderHUD(Player* player) 
{ 
    if (!player) return;
    
    // Draw a simple crosshair at the center of the screen
    int centerX = m_screenWidth / 2;
    int centerY = m_screenHeight / 2;
    int crosshairSize = 10;
    
    // Horizontal line
    for (int x = centerX - crosshairSize; x <= centerX + crosshairSize; x++) {
        setPixel(x, centerY, Colors::WHITE);
    }
    
    // Vertical line
    for (int y = centerY - crosshairSize; y <= centerY + crosshairSize; y++) {
        setPixel(centerX, y, Colors::WHITE);
    }
    
    // Draw health indicator
    std::string healthText = "Health: " + std::to_string(player->getHealth());
    drawText(healthText, 20, m_screenHeight - 40, Colors::RED);
    
    // Draw minimap in the top-right corner
    Map* map = player->getMap();
    if (map) {
        renderMinimap(map, player, m_screenWidth - 150, 20, 130);
    }
    
    // Draw currently selected weapon
    if (player->getCurrentWeapon() == WeaponType::CHAINSAW) {
        renderWeapon();
    }
}

// Function to print current working directory
void printCurrentDirectory() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::cout << "Current working directory: " << cwd << std::endl;
    } else {
        std::cerr << "Error getting current directory" << std::endl;
    }
}

// Render the current weapon
void Renderer::renderWeapon() 
{
    // Use static variables to only load the image once
    static SDL_Surface* chainsawSurface = nullptr;
    
    if (!chainsawSurface) {
        // We already have debug info showing the image loads correctly
        std::string imagePath = "bin/assets/textures/chainsaw.png";
        chainsawSurface = IMG_Load(imagePath.c_str());
        
        if (!chainsawSurface) {
            std::cerr << "ERROR: Failed to load chainsaw image: " << IMG_GetError() << std::endl;
            return;
        }
    }
    
    // Draw directly to the pixel buffer (skipping SDL texture/renderer)
    if (chainsawSurface) {
        // Get image dimensions
        int imgWidth = chainsawSurface->w;
        int imgHeight = chainsawSurface->h;
        
        // Make the chainsaw larger
        float scale = 2.0f; // Double the size
        
        int displayWidth = (int)(imgWidth * scale);
        int displayHeight = (int)(imgHeight * scale);
        
        // Position in the center bottom of the screen
        int posX = (m_screenWidth - displayWidth) / 2;
        int posY = m_screenHeight - displayHeight - 20; // Add padding from bottom
        
        // Draw the chainsaw directly to pixel buffer
        SDL_LockSurface(chainsawSurface);
        
        Uint32* pixels = (Uint32*)chainsawSurface->pixels;
        int pitch = chainsawSurface->pitch / sizeof(Uint32);
        
        // Get format information
        SDL_PixelFormat* format = chainsawSurface->format;
        
        // Draw the image
        for (int y = 0; y < imgHeight; y++) {
            for (int x = 0; x < imgWidth; x++) {
                // Calculate scaled coordinates
                int destX = posX + (int)(x * scale);
                int destY = posY + (int)(y * scale);
                
                // Check bounds
                if (destX >= 0 && destX < m_screenWidth && destY >= 0 && destY < m_screenHeight) {
                    // Get pixel color from surface
                    Uint32 pixel = pixels[y * pitch + x];
                    
                    // Extract color components
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
                    
                    // Skip white pixels (making them transparent)
                    if (!(r > 240 && g > 240 && b > 240)) {
                        // Draw the pixel
                        setPixel(destX, destY, Color(r, g, b, a));
                        
                        // For scaled image, fill the scaled area
                        if (scale > 1.0f) {
                            for (int sy = 0; sy < (int)scale; sy++) {
                                for (int sx = 0; sx < (int)scale; sx++) {
                                    int fillX = posX + (int)(x * scale) + sx;
                                    int fillY = posY + (int)(y * scale) + sy;
                                    if (fillX >= 0 && fillX < m_screenWidth && 
                                        fillY >= 0 && fillY < m_screenHeight) {
                                        setPixel(fillX, fillY, Color(r, g, b, a));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        SDL_UnlockSurface(chainsawSurface);
    }
}

void Renderer::renderMinimap(Map* map, Player* player, int x, int y, int size)
{
    if (!map || !player) return;
    
    // Draw background rectangle with a dark gray (not black) color
    Rect minimapRect(x, y, size, size);
    renderDebugRect(minimapRect, Color(40, 40, 40, 255), true);
    
    // Get player position as the center of the minimap view
    Vec2 playerPos = player->getPosition();
    
    // Adjust scale to ensure walls are visible - use a fixed, appropriate scale
    double scale = size / 100.0; // Smaller denominator to show more of the larger map
    
    // Draw all walls directly using setPixel for better visibility
    const auto& sectors = map->getSectors();
    for (const auto& sector : sectors) {
        const auto& walls = sector->getWalls();
        
        // Choose wall color
        Color wallColor(200, 200, 200); // Bright white/gray for standard walls
        
        // Special color for the staircase sector
        double floorHeight = sector->getFloorHeight();
        if (std::abs(floorHeight) < 0.1 && sector->getFloorTextureId() == 3) {
            wallColor = Color(0, 255, 0); // Bright green for stairs
        }
        
        // Special color for the upper room
        if (floorHeight > 2.5) {
            wallColor = Color(255, 128, 0); // Orange for upper room
        }
        
        // Draw walls
        for (const auto& wall : walls) {
            Vec2 start = wall->getStart();
            Vec2 end = wall->getEnd();
            
            // Convert to minimap coordinates, centered on player
            int startX = x + size/2 + static_cast<int>((start.x - playerPos.x) * scale);
            int startY = y + size/2 + static_cast<int>((start.y - playerPos.y) * scale);
            int endX = x + size/2 + static_cast<int>((end.x - playerPos.x) * scale);
            int endY = y + size/2 + static_cast<int>((end.y - playerPos.y) * scale);
            
            // Draw a thicker line for better visibility
            drawThickLine(startX, startY, endX, endY, wallColor, 2);
            
            // Add a yellow dot for portal entrances
            if (wall->isPortal()) {
                int midX = (startX + endX) / 2;
                int midY = (startY + endY) / 2;
                
                // Special highlight for staircase entrance
                if (wall->getAdjoiningSector() && wall->getAdjoiningSector()->getFloorTextureId() == 3) {
                    // Draw a bright yellow dot
                    for (int dx = -3; dx <= 3; dx++) {
                        for (int dy = -3; dy <= 3; dy++) {
                            if (dx*dx + dy*dy <= 9) { // Circle with radius 3
                                setPixel(midX + dx, midY + dy, Color(255, 255, 0));
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Draw player position (always at center with player-centered map)
    int playerX = x + size/2;
    int playerY = y + size/2;
    
    // Draw player position as a red diamond
    for (int dx = -4; dx <= 4; dx++) {
        for (int dy = -4; dy <= 4; dy++) {
            if (std::abs(dx) + std::abs(dy) <= 4) { // Diamond shape
                setPixel(playerX + dx, playerY + dy, Color(255, 0, 0));
            }
        }
    }
    
    // Draw player direction as a bright yellow line
    Vec2 dirVec = player->getDirection().normalized() * 8;
    int dirX = playerX + static_cast<int>(dirVec.x);
    int dirY = playerY + static_cast<int>(dirVec.y);
    drawThickLine(playerX, playerY, dirX, dirY, Color(255, 255, 0), 2);
    
    // Draw minimap border
    renderDebugRect(minimapRect, Color(255, 255, 255), false);
}

// Helper function to draw a thick line
void Renderer::drawThickLine(int x1, int y1, int x2, int y2, const Color& color, int thickness) {
    // Basic line drawing using Bresenham's algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        // Draw a filled circle at this point for thickness
        for (int tx = -thickness/2; tx <= thickness/2; tx++) {
            for (int ty = -thickness/2; ty <= thickness/2; ty++) {
                if (tx*tx + ty*ty <= (thickness*thickness)/4) {
                    setPixel(x1 + tx, y1 + ty, color);
                }
            }
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void Renderer::renderDebugInfo(const std::string& text, int x, int y, const Color& color) 
{ 
    drawText(text, x, y, color);
}

void Renderer::renderDebugLine(const Vec2& start, const Vec2& end, const Color& color) 
{ 
    // Bresenham's line algorithm
    int x0 = static_cast<int>(start.x);
    int y0 = static_cast<int>(start.y);
    int x1 = static_cast<int>(end.x);
    int y1 = static_cast<int>(end.y);
    
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        setPixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Renderer::renderDebugRect(const Rect& rect, const Color& color, bool filled) 
{ 
    int x = static_cast<int>(rect.x);
    int y = static_cast<int>(rect.y);
    int w = static_cast<int>(rect.width);
    int h = static_cast<int>(rect.height);
    
    if (filled) {
        // Draw filled rectangle
        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                setPixel(px, py, color);
            }
        }
    } else {
        // Draw rectangle outline
        for (int px = x; px < x + w; px++) {
            setPixel(px, y, color);
            setPixel(px, y + h - 1, color);
        }
        for (int py = y; py < y + h; py++) {
            setPixel(x, py, color);
            setPixel(x + w - 1, py, color);
        }
    }
}

void Renderer::renderDebugCircle(const Vec2& center, double radius, const Color& color, bool filled) 
{ 
    int x0 = static_cast<int>(center.x);
    int y0 = static_cast<int>(center.y);
    int r = static_cast<int>(radius);
    
    // Midpoint circle algorithm
    int x = r;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        if (filled) {
            // Draw filled lines
            for (int px = x0 - x; px <= x0 + x; px++) setPixel(px, y0 + y, color);
            for (int px = x0 - x; px <= x0 + x; px++) setPixel(px, y0 - y, color);
            for (int px = x0 - y; px <= x0 + y; px++) setPixel(px, y0 + x, color);
            for (int px = x0 - y; px <= x0 + y; px++) setPixel(px, y0 - x, color);
        } else {
            // Draw circle points
            setPixel(x0 + x, y0 + y, color);
            setPixel(x0 - x, y0 + y, color);
            setPixel(x0 + x, y0 - y, color);
            setPixel(x0 - x, y0 - y, color);
            setPixel(x0 + y, y0 + x, color);
            setPixel(x0 - y, y0 + x, color);
            setPixel(x0 + y, y0 - x, color);
            setPixel(x0 - y, y0 - x, color);
        }
        
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

bool Renderer::takeScreenshot(const std::string& filename) 
{ 
    // Not implemented in this basic version
    return false;
}

// Calculate a point on a pentagram
Vec2 pentagramPoint(int pointIndex, double centerX, double centerY, double radius) {
    const double angle = (2 * PI * pointIndex / 5) - PI / 2; // Start from the top
    return Vec2(centerX + radius * cos(angle), centerY + radius * sin(angle));
}

// Function to determine if a point is inside a pentagram
bool isInsidePentagram(double x, double y, double centerX, double centerY, double radius, double lineWidth) {
    // Calculate the five points of the pentagram
    std::vector<Vec2> points;
    for (int i = 0; i < 5; i++) {
        points.push_back(pentagramPoint(i, centerX, centerY, radius));
    }
    
    // Draw the star pattern
    for (int i = 0; i < 5; i++) {
        // Connect to points that are 2 steps away (0->2, 1->3, etc.)
        int j = (i + 2) % 5;
        
        // Check if point is near this line
        Vec2 a = points[i];
        Vec2 b = points[j];
        
        // Calculate distance from point to line segment
        double lineLength = sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
        if (lineLength == 0) continue;
        
        // Calculate the projection of the point onto the line
        double t = ((x - a.x) * (b.x - a.x) + (y - a.y) * (b.y - a.y)) / (lineLength * lineLength);
        
        // If t is outside [0,1], the closest point is one of the endpoints
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        
        // Calculate the closest point on the line
        double closestX = a.x + t * (b.x - a.x);
        double closestY = a.y + t * (b.y - a.y);
        
        // Calculate distance to the line
        double distance = sqrt(pow(x - closestX, 2) + pow(y - closestY, 2));
        
        // If distance is less than line width, point is inside pentagram
        if (distance < lineWidth) {
            return true;
        }
    }
    
    return false;
}

// Generate a DOOM-like floor texture (pentagrams)
Color getDoomFloorColor(int x, int y, int floorTileSize) {
    // Default floor and pentagram colors (dark reddish tones)
    // Brighten the base colors for better visibility
    Color baseFloorColor(60, 30, 30);     // Brighter brown-red (was 40,20,20)
    Color floorBorderColor(80, 40, 40);   // Brighter border (was 60,30,30)
    Color pentagramColor(160, 20, 20);    // Brighter blood red (was 120,0,0)

    // Calculate tile coordinates
    int tileX = x / floorTileSize;
    int tileY = y / floorTileSize;
    
    // Local coordinates within the tile (centered)
    double localX = (x % floorTileSize) / static_cast<double>(floorTileSize);
    double localY = (y % floorTileSize) / static_cast<double>(floorTileSize);
    
    // Add a border around each tile
    double borderWidth = 0.03;
    if (localX < borderWidth || localX > 1 - borderWidth || 
        localY < borderWidth || localY > 1 - borderWidth) {
        return floorBorderColor;
    }
    
    // Check if we're in an "odd" tile (checkerboard pattern)
    bool isOddTile = (tileX + tileY) % 2 == 1;
    
    // Every odd tile gets a pentagram
    if (isOddTile) {
        // Coordinates relative to center of tile
        double relX = localX - 0.5;
        double relY = localY - 0.5;
        
        // Check if point is inside the pentagram
        double pentagramRadius = 0.35; // Size of pentagram
        double lineWidth = 0.05;      // Width of pentagram lines
        
        if (isInsidePentagram(relX, relY, 0, 0, pentagramRadius, lineWidth)) {
            // Add subtle glow effect to pentagrams
            double distToCenter = sqrt(relX*relX + relY*relY);
            double glowFactor = 1.0 - std::min(1.0, distToCenter / 0.2);
            glowFactor = std::max(0.0, glowFactor);
            
            // Add pulsing effect to pentagrams
            double time = SDL_GetTicks() / 1000.0;
            double pulse = 0.2 * sin(time * 1.5);
            
            return Color(
                std::min(255, static_cast<int>(pentagramColor.r + glowFactor * 40 + pulse * 20)),
                std::min(255, static_cast<int>(pentagramColor.g + glowFactor * 10)),
                std::min(255, static_cast<int>(pentagramColor.b + glowFactor * 10))
            );
        }
    }
    
    // Return the base floor color with slight variation
    int variation = (x % 7) - (y % 5); // Subtle texture variation
    return Color(
        std::min(255, std::max(0, static_cast<int>(baseFloorColor.r) + variation)),
        std::min(255, std::max(0, static_cast<int>(baseFloorColor.g) + variation / 2)),
        std::min(255, std::max(0, static_cast<int>(baseFloorColor.b) + variation / 2))
    );
}

// Generate a DOOM-like ceiling texture
Color getDoomCeilingColor(int x, int y, int tileSize) {
    // Default ceiling colors (dark red-orange)
    // Brighten the base colors for better visibility
    Color baseCeilingColor(45, 20, 10);      // Brighter dark red-brown (was 30,10,5)
    Color ceilingPatternColor(70, 30, 15);   // Brighter pattern (was 50,20,10)
    Color glowingCrackColor(110, 50, 15);    // Brighter orange-red "cracks" (was 80,30,5)
    
    // Calculate tile coordinates
    int tileX = x / tileSize;
    int tileY = y / tileSize;
    
    // Local coordinates within the tile (0 to 1)
    double localX = (x % tileSize) / static_cast<double>(tileSize);
    double localY = (y % tileSize) / static_cast<double>(tileSize);
    
    // Create a grid pattern with "cracks"
    double gridLineWidth = 0.05;
    
    // Main grid pattern
    if (localX < gridLineWidth || localX > 1.0 - gridLineWidth ||
        localY < gridLineWidth || localY > 1.0 - gridLineWidth) {
        return ceilingPatternColor;
    }
    
    // Create random cracks (using deterministic variation based on position)
    int seed = (tileX * 12345 + tileY * 67890) % 100;
    if (seed < 40) {  // Increased from 30% to 40% of tiles have cracks
        // Calculate crack pattern (diagonal cracks)
        double diag1 = fabs(localX - localY);
        double diag2 = fabs(localX - (1.0 - localY));
        
        // If near diagonal and seed-dependent pattern matches
        if ((diag1 < 0.05 || diag2 < 0.05) && (seed % 3 == (x * y) % 3)) {
            // Add subtle pulsing glow to cracks
            double time = SDL_GetTicks() / 1000.0;
            double pulse = 0.15 * sin(time * 1.2 + seed * 0.1);
            
            return Color(
                std::min(255, static_cast<int>(glowingCrackColor.r + pulse * 20)),
                std::min(255, static_cast<int>(glowingCrackColor.g + pulse * 10)),
                std::min(255, static_cast<int>(glowingCrackColor.b))
            );
        }
    }
    
    // Add subtle variation to base color
    int variation = ((x + y) % 5) - 2;
    return Color(
        std::min(255, std::max(0, static_cast<int>(baseCeilingColor.r) + variation)),
        std::min(255, std::max(0, static_cast<int>(baseCeilingColor.g) + variation / 2)),
        std::min(255, std::max(0, static_cast<int>(baseCeilingColor.b)))
    );
}

// Function to generate a DOOM-like wall texture - fixed version
Color getDoomWallColor(int textureId, double u, double v, double distance) {
    // Texture coordinates (scaled to 0-1 range)
    double texU = u - floor(u);
    double texV = v - floor(v);
    
    // Convert to pixel coordinates (0-64 range for texture size)
    int texX = static_cast<int>(texU * 64) % 64;
    int texY = static_cast<int>(texV * 64) % 64;
    
    // Base colors for various wall types - enhanced for better visibility
    Color stoneColor(90, 75, 65);        // Stone base - brightened
    Color metalColor(100, 100, 110);     // Metal base - brightened
    Color techColor(80, 90, 100);        // Tech base - brightened
    Color demonicColor(100, 50, 50);     // Demonic base - brightened
    Color accentColor(150, 40, 30);      // Accent color (red) - more vibrant
    Color darkAccent(40, 30, 30);        // Dark accent - slightly brightened
    
    // Select texture type based on ID
    int textureType = textureId % 5;
    
    // Calculate pixel color based on texture type
    Color baseColor;
    
    switch(textureType) {
        case 0: { // Metal panels with rivets
            // Grid layout
            int panelSizeX = 16;
            int panelSizeY = 32;
            int panelX = texX / panelSizeX;
            int panelY = texY / panelSizeY;
            
            // Panel edge detection (darker edges) - wider edges for better enclosure
            int edgeWidth = 3; // Increased from 2
            bool isEdge = (texX % panelSizeX < edgeWidth) || 
                         (texX % panelSizeX >= panelSizeX - edgeWidth) ||
                         (texY % panelSizeY < edgeWidth) ||
                         (texY % panelSizeY >= panelSizeY - edgeWidth);
            
            // Rivet detection (small circular rivets at panel corners)
            int rivetSize = 3;
            bool isRivet = false;
            
            // Check each corner of the panel
            for (int cornerX = 0; cornerX <= 1; cornerX++) {
                for (int cornerY = 0; cornerY <= 1; cornerY++) {
                    int rivetCenterX = cornerX * panelSizeX;
                    int rivetCenterY = cornerY * panelSizeY;
                    
                    // Distance from this pixel to the rivet center
                    int dx = (texX % panelSizeX) - rivetCenterX;
                    int dy = (texY % panelSizeY) - rivetCenterY;
                    int distSq = dx*dx + dy*dy;
                    
                    if (distSq < rivetSize*rivetSize) {
                        isRivet = true;
                    }
                }
            }
            
            // Color selection
            if (isRivet) {
                baseColor = darkAccent; // Dark rivet
            } else if (isEdge) {
                baseColor = Color(70, 70, 80); // Darker edge - more contrast
            } else {
                // Slight variation to panel base color
                int variation = ((panelX + panelY) % 3) - 1;
                baseColor = Color(
                    std::min(255, std::max(0, static_cast<int>(metalColor.r) + variation)),
                    std::min(255, std::max(0, static_cast<int>(metalColor.g) + variation)),
                    std::min(255, std::max(0, static_cast<int>(metalColor.b) + variation))
                );
            }
            break;
        }
        
        case 1: { // Stone/brick wall with cracks
            // Brick layout
            int brickWidth = 16;
            int brickHeight = 8;
            int offsetPerRow = 8; // Offset every other row
            
            // Calculate brick coordinates
            int row = texY / brickHeight;
            int col = texX / brickWidth;
            int rowOffset = (row % 2) * offsetPerRow;
            int effectiveX = texX + rowOffset;
            col = effectiveX / brickWidth;
            
            // Brick edge detection - wider mortar for better enclosure
            int mortarWidth = 2; // Increased from 1
            bool isMortar = (effectiveX % brickWidth < mortarWidth) || 
                           (effectiveX % brickWidth >= brickWidth - mortarWidth) ||
                           (texY % brickHeight < mortarWidth) ||
                           (texY % brickHeight >= brickHeight - mortarWidth);
            
            // Crack pattern (seeded by position)
            int seed = (col * 1234 + row * 5678) % 100;
            bool hasCrack = (seed < 25); // Increased from 15% to 25% chance for a crack
            
            // Determine crack pattern
            bool isOnCrack = false;
            if (hasCrack) {
                int crackX = brickWidth / 2 + (seed % 3) - 1;
                int crackY = brickHeight / 2 + ((seed / 3) % 3) - 1;
                int localX = effectiveX % brickWidth;
                int localY = texY % brickHeight;
                
                int dx = localX - crackX;
                int dy = localY - crackY;
                
                // Check if point is on a jagged line crack - wider cracks
                isOnCrack = (abs(dx) <= 2 && abs(dy) <= 4) || // Increased width
                            (abs(dx) <= 3 && abs(dy) <= 2);    // Increased width
            }
            
            // Color selection
            if (isMortar) {
                baseColor = Color(30, 25, 25); // Darker mortar for better contrast
            } else if (isOnCrack) {
                baseColor = Color(20, 20, 20); // Dark crack
            } else {
                // Slight variation to brick color
                int variation = ((col + row) % 5) - 2;
                baseColor = Color(
                    std::min(255, std::max(0, static_cast<int>(stoneColor.r) + variation)),
                    std::min(255, std::max(0, static_cast<int>(stoneColor.g) + variation)),
                    std::min(255, std::max(0, static_cast<int>(stoneColor.b) + variation))
                );
            }
            break;
        }
        
        case 2: { // Tech/circuit pattern
            // Grid layout
            int gridSize = 8;
            
            // Grid lines - wider for better enclosure
            bool isGridLine = (texX % gridSize <= 2) || (texY % gridSize <= 2); // Increased width
            
            // Circuit patterns (horizontal and vertical lines)
            bool isCircuitH = ((texY % (gridSize * 4)) / gridSize == 1) && 
                                 ((texX % gridSize) > 2) && ((texX % gridSize) < gridSize - 2);
            
            bool isCircuitV = ((texX % (gridSize * 4)) / gridSize == 2) && 
                                 ((texY % gridSize) > 2) && ((texY % gridSize) < gridSize - 2);
            
            // "Components" at certain intersections
            int blockX = texX / gridSize;
            int blockY = texY / gridSize;
            int centerDistSq = ((texX % gridSize - gridSize/2) * (texX % gridSize - gridSize/2) + 
                              (texY % gridSize - gridSize/2) * (texY % gridSize - gridSize/2));
            bool isComponent = ((blockX + blockY) % 7 == 0) && (centerDistSq < 9);
            
            // Color selection
            if (isComponent) {
                baseColor = accentColor; // Red component
            } else if (isGridLine || isCircuitH || isCircuitV) {
                baseColor = Color(120, 130, 140); // Lighter circuit line for better contrast
            } else {
                baseColor = techColor;
            }
            break;
        }
        
        case 3: { // Demonic symbols and runes
            // Background with subtle pattern
            int patternX = texX / 8;
            int patternY = texY / 8;
            bool isDarker = ((patternX + patternY) % 2 == 0);
            
            // Create a pentagram centered in the texture
            double centerX = 32.0;
            double centerY = 32.0;
            double radius = 24.0;
            double lineWidth = 3.0; // Increased from 2.0 for better visibility
            
            // Check if on pentagram
            bool onPentagram = isInsidePentagram(
                texX - centerX, texY - centerY, 
                0, 0, radius, lineWidth);
            
            // Rune-like symbols in the corners
            bool onRune = false;
            
            // Four corner runes
            for (int cornerX = 0; cornerX <= 1; cornerX++) {
                for (int cornerY = 0; cornerY <= 1; cornerY++) {
                    int runeX = cornerX * 48 + 8;
                    int runeY = cornerY * 48 + 8;
                    
                    // Simple rune patterns (box with cross) - wider for better visibility
                    int dx = abs(texX - runeX);
                    int dy = abs(texY - runeY);
                    
                    if ((dx < 4 && dy < 7) || (dx < 7 && dy < 4)) { // Increased size
                        onRune = true;
                    }
                }
            }
            
            // Color selection
            if (onPentagram) {
                baseColor = accentColor; // Red pentagram
            } else if (onRune) {
                baseColor = Color(180, 40, 20); // Brighter rune for better contrast
            } else {
                // Alternate slightly darker/lighter background
                baseColor = isDarker ? 
                    Color(std::max(0, static_cast<int>(demonicColor.r) - 15),
                          std::max(0, static_cast<int>(demonicColor.g) - 10),
                          std::max(0, static_cast<int>(demonicColor.b) - 10)) : 
                    demonicColor;
            }
            break;
        }
        
        case 4: { // Hellish flesh wall (veins and organic texture)
            // Base color with organic-looking noise
            int noiseX = texX / 4;
            int noiseY = texY / 4;
            int noise = ((noiseX * 7) + (noiseY * 19)) % 10;
            
            // Veins (curved lines) - wider for better visibility
            bool onVein = false;
            
            // Create a few veins with different paths
            for (int vein = 0; vein < 3; vein++) {
                double veinX = 20 + vein * 15;
                
                // Sine wave path
                double amplitude = 15.0;
                double frequency = 0.05;
                double phase = vein * PI / 3;
                
                // Calculate points along the vein
                double veinY = 32 + amplitude * sin(frequency * texX + phase);
                double veinDist = fabs(texY - veinY);
                
                if (veinDist < 3.0) { // Increased from 2.0
                    onVein = true;
                }
            }
            
            // Blood spots (small circles)
            bool inBloodSpot = false;
            
            // Create several blood spots
            for (int spot = 0; spot < 5; spot++) {
                int spotX = (spot * 37) % 64;
                int spotY = (spot * 23) % 64;
                int spotRadius = 4 + (spot % 3); // Increased from 3+
                
                int dx = texX - spotX;
                int dy = texY - spotY;
                if (dx*dx + dy*dy < spotRadius*spotRadius) {
                    inBloodSpot = true;
                }
            }
            
            // Color selection
            if (inBloodSpot) {
                baseColor = Color(140, 30, 30); // Brighter red blood for better contrast
            } else if (onVein) {
                baseColor = Color(160, 40, 40); // Brighter vein for better contrast
            } else {
                // Flesh texture with noise
                baseColor = Color(
                    std::min(255, std::max(0, 120 - noise * 2)),  // Brighter reddish base
                    std::min(255, std::max(0, 60 - noise)),       // Increased from 50
                    std::min(255, std::max(0, 60 - noise))        // Increased from 50
                );
            }
            break;
        }
        
        default:
            baseColor = Color(200, 200, 200); // Light gray (fallback)
    }
    
    // Apply distance-based darkening with less fog for better visibility
    double fogFactor = 1.0 - std::min(1.0, distance / 25.0); // Increased from 20.0
    return Color::lerp(Color(0, 0, 0), baseColor, static_cast<float>(1.0 - fogFactor * 0.8)); // Reduced fog effect
}

// New method to calculate shadow map by casting rays from the sun
void Renderer::calculateShadowMap(Camera* camera, bool* shadowMap, double sunX, double sunY, double sunZ)
{
    if (!camera || !shadowMap) return;
    
    // Get camera position
    Vec2 cameraPos = camera->getPosition();
    double cameraHeight = 0.6; // Camera height (eye level)
    
    // Calculate the horizon line (vertical center of the screen)
    int horizonY = m_screenHeight / 2;
    
    // Get map from engine if available
    Map* map = m_engine ? m_engine->getMap() : nullptr;
    if (!map) {
        // If no map is available, use a simplified approach
        for (int y = 0; y < m_screenHeight; y++) {
            for (int x = 0; x < m_screenWidth; x++) {
                int index = y * m_screenWidth + x;
                shadowMap[index] = false; // No shadows without map data
            }
        }
        return;
    }
    
    // Process each pixel on screen
    for (int y = 0; y < m_screenHeight; y++) {
        for (int x = 0; x < m_screenWidth; x++) {
            // Skip if no geometry at this pixel (z-buffer is at max)
            int index = y * m_screenWidth + x;
            if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                shadowMap[index] = false;
                continue;
            }
            
            // Get the world position of this pixel
            double worldX, worldY, worldZ;
            
            // For floor pixels (below horizon)
            if (y > horizonY) {
                // Calculate position relative to horizon (0 at horizon, 1 at bottom of screen)
                double relativeY = (y - horizonY) / static_cast<double>(m_screenHeight - horizonY);
                
                // Apply DOOM-like perspective for floor
                double rowDistance = cameraHeight / (2.0 * relativeY - 1.0 + 1e-5);
                
                // Calculate ray direction for this pixel
                double rayRatio = (2.0 * x / static_cast<double>(m_screenWidth) - 1.0);
                Vec2 camDir = camera->getDirection();
                Vec2 camRight = camera->getRight();
                
                // Calculate ray direction using camera direction and right vectors
                double rayDirX = camDir.x + camRight.x * rayRatio * 1.2; // Adjust FOV here to match walls
                double rayDirY = camDir.y + camRight.y * rayRatio * 1.2;
                double dirLen = sqrt(rayDirX * rayDirX + rayDirY * rayDirY);
                rayDirX /= dirLen; // Normalize
                rayDirY /= dirLen;
                
                // Calculate the real-world floor coordinate
                worldX = cameraPos.x + rowDistance * rayDirX;
                worldY = cameraPos.y + rowDistance * rayDirY;
                worldZ = 0.0; // Floor is at Z=0
            }
            // For ceiling pixels (above horizon but below skybox)
            else if (y >= m_screenHeight / 4 && y < horizonY) {
                // Calculate position relative to horizon (0 at horizon, 1 at top of ceiling)
                double relativeY = (horizonY - y) / static_cast<double>(horizonY - m_screenHeight / 4);
                
                // Apply DOOM-like perspective for ceiling
                double rowDistance = cameraHeight / (2.0 * relativeY - 1.0 + 1e-5);
                
                // Calculate ray direction for this pixel
                double rayRatio = (2.0 * x / static_cast<double>(m_screenWidth) - 1.0);
                Vec2 camDir = camera->getDirection();
                Vec2 camRight = camera->getRight();
                
                // Calculate ray direction using camera direction and right vectors
                double rayDirX = camDir.x + camRight.x * rayRatio * 1.2;
                double rayDirY = camDir.y + camRight.y * rayRatio * 1.2;
                double dirLen = sqrt(rayDirX * rayDirX + rayDirY * rayDirY);
                rayDirX /= dirLen;
                rayDirY /= dirLen;
                
                // Calculate the real-world ceiling coordinate
                worldX = cameraPos.x + rowDistance * rayDirX;
                worldY = cameraPos.y + rowDistance * rayDirY;
                worldZ = 4.0; // Ceiling is at Z=4.0
            }
            // For wall pixels
            else if (y >= m_screenHeight / 4) {
                // For walls, we need to use the z-buffer to determine distance
                double wallDist = m_zBuffer[index];
                
                // Calculate ray direction for this pixel
                double rayRatio = (2.0 * x / static_cast<double>(m_screenWidth) - 1.0);
                Vec2 camDir = camera->getDirection();
                Vec2 camRight = camera->getRight();
                
                // Calculate ray direction using camera direction and right vectors
                double rayDirX = camDir.x + camRight.x * rayRatio * 1.2;
                double rayDirY = camDir.y + camRight.y * rayRatio * 1.2;
                double dirLen = sqrt(rayDirX * rayDirX + rayDirY * rayDirY);
                rayDirX /= dirLen;
                rayDirY /= dirLen;
                
                // Calculate the real-world wall coordinate
                worldX = cameraPos.x + wallDist * rayDirX;
                worldY = cameraPos.y + wallDist * rayDirY;
                
                // Estimate Z based on screen Y position relative to horizon
                double relativeY = (double)(horizonY - y) / (double)(horizonY - m_screenHeight / 4);
                worldZ = 2.0 + relativeY * 2.0; // Approximate height between 0 and 4
            }
            else {
                // Skybox pixels - no shadow calculation needed
                continue;
            }
            
            // Now we have the world position, check if it's in shadow
            // Cast a ray from this point to the sun
            double toSunX = sunX - worldX;
            double toSunY = sunY - worldY;
            double toSunZ = sunZ - worldZ;
            
            // Normalize the vector
            double distToSun = sqrt(toSunX*toSunX + toSunY*toSunY + toSunZ*toSunZ);
            toSunX /= distToSun;
            toSunY /= distToSun;
            toSunZ /= distToSun;
            
            // Check if ray hits any walls using the map's ray casting functionality
            bool isInShadow = false;
            
            // Use a smaller step size for more accurate shadows
            double stepSize = 0.2;
            double maxDist = distToSun; // Only check up to the sun's distance
            
            // Start a bit away from the surface to avoid self-shadowing
            Vec2 rayStart(worldX + toSunX * 0.1, worldY + toSunY * 0.1);
            Vec2 rayDir(toSunX, toSunY);
            
            // Cast ray through the map
            for (double t = 0.1; t < maxDist && !isInShadow; t += stepSize) {
                double checkX = worldX + toSunX * t;
                double checkY = worldY + toSunY * t;
                double checkZ = worldZ + toSunZ * t;
                
                // If we're close to the sun, we're done
                if (sqrt(pow(checkX - sunX, 2) + pow(checkY - sunY, 2) + pow(checkZ - sunZ, 2)) < 5.0) {
                    break;
                }
                
                // Check if there's a wall at this position using the map data
                // Get the map cell coordinates
                int cellX = static_cast<int>(checkX);
                int cellY = static_cast<int>(checkY);
                
                // Check if this point is inside a wall
                // Use the map's collision detection if available
                Vec2 checkPoint(checkX, checkY);
                
                // Check if the point is inside any wall
                const auto& sectors = map->getSectors();
                for (const auto& sector : sectors) {
                    const auto& walls = sector->getWalls();
                    for (const auto& wall : walls) {
                        // Simple check: if the ray passes close to a wall segment
                        Vec2 wallStart = wall->getStart();
                        Vec2 wallEnd = wall->getEnd();
                        
                        // Calculate distance from point to line segment
                        Vec2 wallVec = wallEnd - wallStart;
                        double wallLength = wallVec.length();
                        if (wallLength < 0.001) continue; // Skip degenerate walls
                        
                        Vec2 wallDir = wallVec / wallLength;
                        Vec2 toPoint = checkPoint - wallStart;
                        
                        // Project point onto wall line
                        double projection = toPoint.dot(wallDir);
                        
                        // Check if projection is within wall segment
                        if (projection >= 0 && projection <= wallLength) {
                            // Calculate perpendicular distance
                            double perpDist = (toPoint - wallDir * projection).length();
                            
                            // If close enough to wall and within height range
                            if (perpDist < 0.1 && checkZ >= 0 && checkZ <= 4.0) {
                                isInShadow = true;
                                break;
                            }
                        }
                    }
                    if (isInShadow) break;
                }
            }
            
            // Mark this pixel as in shadow or not
            shadowMap[index] = isInShadow;
        }
    }
}

// New method to render sun rays that cast into the scene
void Renderer::renderSunRays(Camera* camera)
{
    if (!camera) return;
    
    // Get sun position in screen space
    double cameraAngle = camera->getAngle();
    double sunPositionX = m_screenWidth * 0.5 + sin(cameraAngle) * m_screenWidth * 0.2;
    double sunPositionY = m_screenHeight * 0.15;
    
    // Get sun position in world space
    double sunWorldX = 50.0;
    double sunWorldY = 50.0;
    double sunWorldZ = 50.0;
    
    // Create a shadow map buffer
    bool* shadowMap = new bool[m_screenWidth * m_screenHeight];
    memset(shadowMap, 0, m_screenWidth * m_screenHeight * sizeof(bool));
    
    // Calculate shadow map by casting rays from the sun
    calculateShadowMap(camera, shadowMap, sunWorldX, sunWorldY, sunWorldZ);
    
    // Create a buffer to store wall shadow intensity
    double* wallShadowIntensity = new double[m_screenWidth * m_screenHeight];
    for (int i = 0; i < m_screenWidth * m_screenHeight; i++) {
        wallShadowIntensity[i] = 0.0;
    }
    
    // First pass: Cast shadow rays from sun to create shadow patterns on walls
    castShadowRaysOnWalls(camera, wallShadowIntensity, sunWorldX, sunWorldY, sunWorldZ);
    
    // Apply shadow patterns to the scene
    applyShadowsToScene(wallShadowIntensity);
    
    // Number of rays to cast
    const int numRays = 16; // Increased from 12 for more rays
    
    // Ray properties
    const double rayLength = m_screenHeight * 1.5; // Long enough to reach across screen
    const double rayWidth = 4.0;  // Increased from 3.0 for more visible rays
    const double rayFadeStart = 0.3; // Start fading at 30% of ray length
    
    // Get current time for animation
    double time = SDL_GetTicks() / 1000.0;
    
    // Draw rays
    for (int i = 0; i < numRays; i++) {
        // Calculate ray angle with some animation
        double baseAngle = (2.0 * PI * i) / numRays;
        double animatedAngle = baseAngle + 0.2 * sin(time * 0.5 + i * 0.2);
        
        // Calculate ray intensity with animation
        double rayIntensity = 0.5 + 0.3 * sin(time * 1.0 + i * 0.5); // Increased from 0.4+0.2 for brighter rays
        
        // Calculate ray direction
        double rayDirX = cos(animatedAngle);
        double rayDirY = sin(animatedAngle);
        
        // Track if we've hit a shadow
        bool inShadow = false;
        double shadowStartT = 0;
        
        // Draw the ray
        for (double t = 0; t < rayLength; t += 0.5) {
            // Calculate position along ray
            int rayX = static_cast<int>(sunPositionX + rayDirX * t);
            int rayY = static_cast<int>(sunPositionY + rayDirY * t);
            
            // Skip if out of bounds
            if (rayX < 0 || rayX >= m_screenWidth || rayY < 0 || rayY >= m_screenHeight) {
                continue;
            }
            
            // Get index for this pixel
            int index = rayY * m_screenWidth + rayX;
            
            // Check if this point is in shadow
            bool pointInShadow = shadowMap[index];
            
            // If we just entered shadow, mark the transition point
            if (!inShadow && pointInShadow) {
                inShadow = true;
                shadowStartT = t;
            }
            
            // Special handling for the transition area (just below skybox)
            bool isInTransitionArea = (rayY >= m_screenHeight / 4 && rayY < m_screenHeight / 4 + 20);
            
            // For transition area, always allow rays to pass through
            if (isInTransitionArea) {
                // Calculate fade based on distance along ray
                double rayProgress = t / rayLength;
                double fade = 1.0 - std::max(0.0, (rayProgress - rayFadeStart) / (1.0 - rayFadeStart));
                
                // Calculate intensity for transition area - brighter to ensure visibility
                double transitionIntensity = rayIntensity * fade * 0.5;
                
                // Draw ray at this point with width
                for (int dx = -static_cast<int>(rayWidth/2); dx <= static_cast<int>(rayWidth/2); dx++) {
                    for (int dy = -static_cast<int>(rayWidth/2); dy <= static_cast<int>(rayWidth/2); dy++) {
                        // Calculate distance from ray center
                        double dist = sqrt(dx*dx + dy*dy);
                        if (dist <= rayWidth/2) {
                            // Calculate intensity based on distance from ray center
                            double pointIntensity = transitionIntensity * (1.0 - dist/(rayWidth/2));
                            
                            // Get pixel coordinates
                            int px = rayX + dx;
                            int py = rayY + dy;
                            
                            // Skip if out of bounds
                            if (px < 0 || px >= m_screenWidth || py < 0 || py >= m_screenHeight) {
                                continue;
                            }
                            
                            // Get current color
                            uint32_t currentPixel = m_pixelBuffer[py * m_screenWidth + px];
                            uint8_t r = (currentPixel >> 16) & 0xFF;
                            uint8_t g = (currentPixel >> 8) & 0xFF;
                            uint8_t b = currentPixel & 0xFF;
                            
                            // Blend with ray color (bright yellow-white)
                            Color rayColor(255, 255, 220);
                            Color blendedColor(
                                std::min(255, static_cast<int>(r + (rayColor.r - r) * pointIntensity)),
                                std::min(255, static_cast<int>(g + (rayColor.g - g) * pointIntensity)),
                                std::min(255, static_cast<int>(b + (rayColor.b - b) * pointIntensity))
                            );
                            
                            // Set the pixel
                            setPixel(px, py, blendedColor);
                        }
                    }
                }
                
                continue; // Skip the rest of the loop for transition area
            }
            
            // Skip skybox area
            if (rayY < m_screenHeight / 4) {
                continue;
            }
            
            // Check z-buffer to see if we hit a wall or floor
            if (m_zBuffer[index] < std::numeric_limits<double>::max()) {
                // We hit something - check distance
                double hitDist = m_zBuffer[index];
                
                // Calculate fade based on distance along ray
                double rayProgress = t / rayLength;
                double fade = 1.0 - std::max(0.0, (rayProgress - rayFadeStart) / (1.0 - rayFadeStart));
                
                // Adjust intensity based on z-buffer value (closer objects get brighter rays)
                double distanceFactor = std::max(0.0, 1.0 - hitDist / 20.0);
                
                // Check if this point is in shadow
                double finalIntensity;
                if (inShadow) {
                    // Calculate how far into shadow we are
                    double shadowDepth = (t - shadowStartT) / 20.0; // Shadow fades over 20 units
                    shadowDepth = std::min(1.0, shadowDepth);
                    
                    // In shadow - greatly reduce intensity but not completely dark
                    // Increased from 0.05 to 0.1 for more visible shadows in the red environment
                    finalIntensity = rayIntensity * fade * distanceFactor * 0.1 * (1.0 - shadowDepth * 0.7);
                } else {
                    // In light - normal intensity
                    // Increased from 0.3 to 0.4 for more visible rays
                    finalIntensity = rayIntensity * fade * distanceFactor * 0.4;
                }
                
                // Only draw if intensity is significant
                if (finalIntensity > 0.02) {
                    // Draw ray at this point with width
                    for (int dx = -static_cast<int>(rayWidth/2); dx <= static_cast<int>(rayWidth/2); dx++) {
                        for (int dy = -static_cast<int>(rayWidth/2); dy <= static_cast<int>(rayWidth/2); dy++) {
                            // Calculate distance from ray center
                            double dist = sqrt(dx*dx + dy*dy);
                            if (dist <= rayWidth/2) {
                                // Calculate intensity based on distance from ray center
                                double pointIntensity = finalIntensity * (1.0 - dist/(rayWidth/2));
                                
                                // Get pixel coordinates
                                int px = rayX + dx;
                                int py = rayY + dy;
                                
                                // Skip if out of bounds
                                if (px < 0 || px >= m_screenWidth || py < 0 || py >= m_screenHeight) {
                                    continue;
                                }
                                
                                // Get current color
                                uint32_t currentPixel = m_pixelBuffer[py * m_screenWidth + px];
                                uint8_t r = (currentPixel >> 16) & 0xFF;
                                uint8_t g = (currentPixel >> 8) & 0xFF;
                                uint8_t b = currentPixel & 0xFF;
                                
                                // Blend with ray color (bright yellow-white)
                                // Adjusted to be more visible against red background
                                Color rayColor(255, 255, 220);
                                Color blendedColor(
                                    std::min(255, static_cast<int>(r + (rayColor.r - r) * pointIntensity)),
                                    std::min(255, static_cast<int>(g + (rayColor.g - g) * pointIntensity)),
                                    std::min(255, static_cast<int>(b + (rayColor.b - b) * pointIntensity))
                                );
                                
                                // Set the pixel
                                setPixel(px, py, blendedColor);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Add volumetric light shafts from sun position
    const int numShafts = 8; // Increased from 6 for more shafts
    const double shaftWidth = 20.0; // Increased from 15.0 for wider shafts
    
    for (int i = 0; i < numShafts; i++) {
        // Calculate shaft angle with animation
        double shaftAngle = (PI / 4) + (PI / 2) * i / numShafts + 0.05 * sin(time * 0.3 + i * 0.7);
        
        // Calculate shaft direction (mostly downward)
        double shaftDirX = 0.3 * cos(shaftAngle);
        double shaftDirY = 1.0; // Always pointing down
        
        // Normalize direction
        double length = sqrt(shaftDirX*shaftDirX + shaftDirY*shaftDirY);
        shaftDirX /= length;
        shaftDirY /= length;
        
        // Shaft length
        double shaftLength = m_screenHeight;
        
        // Track if we've hit a shadow
        bool inShadow = false;
        double shadowStartT = 0;
        
        // Draw the shaft
        for (double t = 0; t < shaftLength; t += 1.0) {
            // Calculate position along shaft
            int shaftX = static_cast<int>(sunPositionX + shaftDirX * t);
            int shaftY = static_cast<int>(sunPositionY + shaftDirY * t);
            
            // Skip if out of bounds
            if (shaftX < 0 || shaftX >= m_screenWidth || shaftY < 0 || shaftY >= m_screenHeight) {
                continue;
            }
            
            // Get index for this pixel
            int index = shaftY * m_screenWidth + shaftX;
            
            // Special handling for the transition area (just below skybox)
            bool isInTransitionArea = (shaftY >= m_screenHeight / 4 && shaftY < m_screenHeight / 4 + 20);
            
            // For transition area, always allow shafts to pass through
            if (isInTransitionArea) {
                // Calculate fade based on distance along shaft
                double shaftProgress = t / shaftLength;
                double fade = 1.0 - shaftProgress;
                
                // Add pulsing effect
                double pulse = 0.15 * sin(time * 1.5 + i * 1.0 + shaftProgress * 5.0);
                
                // Calculate intensity for transition area - brighter to ensure visibility
                double transitionIntensity = 0.3 * fade + pulse;
                
                // Draw shaft at this point with width
                for (int dx = -static_cast<int>(shaftWidth/2); dx <= static_cast<int>(shaftWidth/2); dx++) {
                    // Calculate distance from shaft center
                    double dist = abs(dx);
                    if (dist <= shaftWidth/2) {
                        // Calculate intensity based on distance from shaft center
                        double pointIntensity = transitionIntensity * (1.0 - dist/(shaftWidth/2));
                        
                        // Get pixel coordinates
                        int px = shaftX + dx;
                        int py = shaftY;
                        
                        // Skip if out of bounds
                        if (px < 0 || px >= m_screenWidth || py < 0 || py >= m_screenHeight) {
                            continue;
                        }
                        
                        // Get current color
                        uint32_t currentPixel = m_pixelBuffer[py * m_screenWidth + px];
                        uint8_t r = (currentPixel >> 16) & 0xFF;
                        uint8_t g = (currentPixel >> 8) & 0xFF;
                        uint8_t b = currentPixel & 0xFF; 
                    }
                }
            }
        }
    }
}

// New method to cast shadow rays on walls to create shadow patterns
void Renderer::castShadowRaysOnWalls(Camera* camera, double* wallShadowIntensity, double sunX, double sunY, double sunZ)
{
    if (!camera || !wallShadowIntensity) return;
    
    // Get map from engine if available
    Map* map = m_engine ? m_engine->getMap() : nullptr;
    if (!map) return;
    
    // Get camera position
    Vec2 cameraPos = camera->getPosition();
    
    // Calculate the horizon line (vertical center of the screen)
    int horizonY = m_screenHeight / 2;
    
    // Number of shadow rays to cast from the sun
    const int numRays = 400; // Many rays for detailed shadows
    
    // Cast rays in all directions from the sun
    for (int i = 0; i < numRays; i++) {
        // Calculate ray angle (full 360 degrees)
        double angle = (2.0 * PI * i) / numRays;
        
        // Calculate ray direction (in 3D)
        double rayDirX = cos(angle);
        double rayDirY = sin(angle);
        double rayDirZ = -0.5; // Downward component to hit walls
        
        // Normalize the direction
        double dirLen = sqrt(rayDirX*rayDirX + rayDirY*rayDirY + rayDirZ*rayDirZ);
        rayDirX /= dirLen;
        rayDirY /= dirLen;
        rayDirZ /= dirLen;
        
        // Maximum ray distance
        double maxDist = 100.0;
        
        // Track if we've hit a wall
        bool hitWall = false;
        Vec2 hitPoint;
        double hitDist = 0.0;
        
        // Cast ray through the map
        for (double t = 0.1; t < maxDist && !hitWall; t += 0.1) { // Smaller step size for accuracy
            // Calculate current position along ray
            double checkX = sunX + rayDirX * t;
            double checkY = sunY + rayDirY * t;
            double checkZ = sunZ + rayDirZ * t;
            
            // Skip if below ground or above ceiling
            if (checkZ < 0.0 || checkZ > 4.0) continue;
            
            // Check if this point is inside any wall
            Vec2 checkPoint(checkX, checkY);
            
            // Check against all walls in the map
            const auto& sectors = map->getSectors();
            for (const auto& sector : sectors) {
                const auto& walls = sector->getWalls();
                for (const auto& wall : walls) {
                    // Get wall segment
                    Vec2 wallStart = wall->getStart();
                    Vec2 wallEnd = wall->getEnd();
                    
                    // Calculate distance from point to line segment
                    Vec2 wallVec = wallEnd - wallStart;
                    double wallLength = wallVec.length();
                    if (wallLength < 0.001) continue; // Skip degenerate walls
                    
                    Vec2 wallDir = wallVec / wallLength;
                    Vec2 toPoint = checkPoint - wallStart;
                    
                    // Project point onto wall line
                    double projection = toPoint.dot(wallDir);
                    
                    // Check if projection is within wall segment
                    if (projection >= 0 && projection <= wallLength) {
                        // Calculate perpendicular distance
                        double perpDist = (toPoint - wallDir * projection).length();
                        
                        // If close enough to wall
                        if (perpDist < 0.1) {
                            hitWall = true;
                            hitPoint = checkPoint;
                            hitDist = t;
                            break;
                        }
                    }
                }
                if (hitWall) break;
            }
        }
        
        // If we hit a wall, project it to screen space and mark shadow
        if (hitWall) {
            // Transform hit point to camera space
            double hitX = hitPoint.x - cameraPos.x;
            double hitY = hitPoint.y - cameraPos.y;
            
            // Rotate point around camera (inverse of camera rotation)
            double cosAngle = cos(-camera->getAngle());
            double sinAngle = sin(-camera->getAngle());
            
            double rotHitX = hitX * cosAngle - hitY * sinAngle;
            double rotHitY = hitX * sinAngle + hitY * cosAngle;
            
            // Skip if behind camera
            if (rotHitX < 0.1) continue;
            
            // Calculate screen coordinates
            double fov = camera->getFOV();
            double halfFovTan = tan(fov / 2.0);
            double aspectRatio = static_cast<double>(m_screenWidth) / m_screenHeight;
            
            double screenX = (rotHitY / rotHitX / halfFovTan / aspectRatio + 1.0) * m_screenWidth / 2.0;
            
            // Estimate wall height based on distance
            double wallHeight = 4.0; // Standard wall height
            double eyeHeight = 0.6; // Camera height
            
            // Calculate screen Y coordinates for top and bottom of wall
            double screenBottomY = m_screenHeight / 2.0 * (1.0 + (eyeHeight - 0.0) / (rotHitX * halfFovTan));
            double screenTopY = m_screenHeight / 2.0 * (1.0 + (eyeHeight - wallHeight) / (rotHitX * halfFovTan));
            
            // Skip if off screen
            if (screenX < 0 || screenX >= m_screenWidth) continue;
            
            // Calculate shadow intensity based on distance
            double shadowIntensity = std::max(0.0, 1.0 - hitDist / 30.0); // Stronger shadows
            shadowIntensity = pow(shadowIntensity, 0.4); // Adjusted curve for stronger shadows
            
            // Boost shadow intensity for more pronounced effect
            shadowIntensity = std::min(1.0, shadowIntensity * 1.5);
            
            // Mark shadow on wall in screen space
            int x = static_cast<int>(screenX);
            
            // Widen the shadow effect for better visibility
            for (int offsetX = -1; offsetX <= 1; offsetX++) {
                int shadowX = x + offsetX;
                if (shadowX < 0 || shadowX >= m_screenWidth) continue;
                
                for (int y = static_cast<int>(screenTopY); y <= static_cast<int>(screenBottomY); y++) {
                    if (y < 0 || y >= m_screenHeight) continue;
                    
                    int index = y * m_screenWidth + shadowX;
                    
                    // Only mark if this is a wall pixel (check z-buffer)
                    if (m_zBuffer[index] < std::numeric_limits<double>::max()) {
                        // Store the maximum shadow intensity for this pixel
                        double currentIntensity = wallShadowIntensity[index];
                        double newIntensity = shadowIntensity;
                        if (offsetX != 0) newIntensity *= 0.8; // Slightly less intense for adjacent pixels
                        
                        wallShadowIntensity[index] = std::max(currentIntensity, newIntensity);
                    }
                }
            }
        }
    }
}

// New method to apply shadow patterns to the scene
void Renderer::applyShadowsToScene(double* wallShadowIntensity)
{
    if (!wallShadowIntensity) return;
    
    // Apply shadows to the entire scene
    for (int y = 0; y < m_screenHeight; y++) {
        for (int x = 0; x < m_screenWidth; x++) {
            int index = y * m_screenWidth + x;
            
            // Skip pixels with no shadow
            if (wallShadowIntensity[index] <= 0.0) continue;
            
            // Skip skybox and transition area
            if (y < m_screenHeight / 4 + 20) continue;
            
            // Get current pixel color
            uint32_t currentPixel = m_pixelBuffer[index];
            uint8_t r = (currentPixel >> 16) & 0xFF;
            uint8_t g = (currentPixel >> 8) & 0xFF;
            uint8_t b = currentPixel & 0xFF;
            
            // Calculate shadow factor (0 = no shadow, 1 = full shadow)
            double shadowFactor = wallShadowIntensity[index] * 0.9; // Strong shadow effect
            
            // Apply shadow by darkening the pixel
            // Use a non-linear darkening to create more contrast
            int newR = static_cast<int>(r * pow(1.0 - shadowFactor * 0.6, 1.2)); // Less darkening for red (to maintain visibility)
            int newG = static_cast<int>(g * pow(1.0 - shadowFactor, 1.5));       // More aggressive darkening for green
            int newB = static_cast<int>(b * pow(1.0 - shadowFactor, 1.5));       // More aggressive darkening for blue
            
            // Add a slight color tint to shadowed areas for better visibility
            if (shadowFactor > 0.4) {
                // Fix type mismatch by using consistent int types
                newR = std::max(10, newR);
                newG = std::min(newG, static_cast<int>(g * 0.8));
                newB = std::min(newB, static_cast<int>(b * 0.8));
            }
            
            // Create color with the adjusted values, ensuring they're in valid range
            Color shadowedColor(
                static_cast<uint8_t>(std::max(0, std::min(255, newR))),
                static_cast<uint8_t>(std::max(0, std::min(255, newG))),
                static_cast<uint8_t>(std::max(0, std::min(255, newB)))
            );
            
            // Set the pixel
            setPixel(x, y, shadowedColor);
        }
    }
}

// New method to ensure all structures are enclosed by filling gaps between walls
void Renderer::ensureStructuresEnclosed(Map* map)
{
    if (!map) return;
    
    // Get all sectors
    const auto& sectors = map->getSectors();
    
    // Process each sector
    for (const auto& sector : sectors) {
        // Get all walls in this sector
        const auto& walls = sector->getWalls();
        
        // Skip if there are no walls
        if (walls.empty()) continue;
        
        // Create a list of wall endpoints
        std::vector<Vec2> endpoints;
        std::vector<bool> isStartPoint;
        std::vector<int> wallIndices;
        
        // Collect all wall endpoints
        for (size_t i = 0; i < walls.size(); ++i) {
            const auto& wall = walls[i];
            
            // Add start point
            endpoints.push_back(wall->getStart());
            isStartPoint.push_back(true);
            wallIndices.push_back(static_cast<int>(i));
            
            // Add end point
            endpoints.push_back(wall->getEnd());
            isStartPoint.push_back(false);
            wallIndices.push_back(static_cast<int>(i));
        }
        
        // Find disconnected endpoints (gaps)
        std::vector<std::pair<Vec2, Vec2>> gaps;
        
        // For each endpoint, find if it's connected to another wall
        for (size_t i = 0; i < endpoints.size(); ++i) {
            const Vec2& point = endpoints[i];
            bool isStart = isStartPoint[i];
            int wallIdx = wallIndices[i];
            
            // Skip if this is a start point (we're looking for end points that aren't connected)
            if (isStart) continue;
            
            // Check if this end point is connected to any other wall's start point
            bool isConnected = false;
            for (size_t j = 0; j < endpoints.size(); ++j) {
                if (i == j) continue; // Skip same point
                
                const Vec2& otherPoint = endpoints[j];
                bool otherIsStart = isStartPoint[j];
                
                // If the other point is a start point and is at the same position, they're connected
                if (otherIsStart && (point - otherPoint).lengthSquared() < 0.01) {
                    isConnected = true;
                    break;
                }
            }
            
            // If not connected, we found a gap
            if (!isConnected) {
                // Find the closest start point that isn't connected to anything
                double minDist = std::numeric_limits<double>::max();
                Vec2 closestPoint;
                bool foundClosest = false;
                
                for (size_t j = 0; j < endpoints.size(); ++j) {
                    if (i == j) continue; // Skip same point
                    
                    const Vec2& otherPoint = endpoints[j];
                    bool otherIsStart = isStartPoint[j];
                    
                    // Only consider start points
                    if (!otherIsStart) continue;
                    
                    // Check if this start point is already connected to an end point
                    bool otherIsConnected = false;
                    for (size_t k = 0; k < endpoints.size(); ++k) {
                        if (j == k) continue; // Skip same point
                        
                        const Vec2& thirdPoint = endpoints[k];
                        bool thirdIsStart = isStartPoint[k];
                        
                        // If the third point is an end point and is at the same position, they're connected
                        if (!thirdIsStart && (otherPoint - thirdPoint).lengthSquared() < 0.01) {
                            otherIsConnected = true;
                            break;
                        }
                    }
                    
                    // If not connected, check distance
                    if (!otherIsConnected) {
                        double dist = (point - otherPoint).length();
                        if (dist < minDist) {
                            minDist = dist;
                            closestPoint = otherPoint;
                            foundClosest = true;
                        }
                    }
                }
                
                // If we found a closest unconnected start point, add a gap
                if (foundClosest) {
                    gaps.push_back(std::make_pair(point, closestPoint));
                }
            }
        }
        
        // Create new walls to fill the gaps
        for (const auto& gap : gaps) {
            // Create a new wall to fill the gap
            std::shared_ptr<Wall> newWall = std::make_shared<Wall>(
                gap.first,  // Start point (end of one wall)
                gap.second, // End point (start of another wall)
                walls[0]->getTextureId(), // Use the same texture as the first wall
                WallType::NORMAL
            );
            
            // Add the wall to the sector
            sector->addWall(newWall);
        }
    }
}
