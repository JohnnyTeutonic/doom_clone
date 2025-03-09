#include "renderer.h"
#include "map.h"
#include "utils.h"
#include "player.h"
#include <iostream>
#include <limits>

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
    m_engine(nullptr)
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
    
    // Present renderer
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
    
    // Reset Z-buffer for the new frame
    for (int i = 0; i < m_screenWidth * m_screenHeight; ++i) {
        m_zBuffer[i] = std::numeric_limits<double>::max();
    }
    
    // Render skybox first (furthest away)
    renderSkybox(camera);
    
    // Get visible walls from BSP tree
    std::vector<std::shared_ptr<Wall>> visibleWalls = map->getVisibleWalls(camera->getPosition());
    
    // DEBUG: Make sure we actually have walls
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 60 == 0) {  // Print only once per second (assuming 60 FPS)
        std::cout << "Frame " << frameCount << ": Processing " << visibleWalls.size() << " visible walls" << std::endl;
    }
    
    // If no walls, create some test walls
    if (visibleWalls.empty()) {
        // Create synthetic test walls if none are found
        Vec2 camPos = camera->getPosition();
        double camAngle = camera->getAngle();
        
        // Create walls in cardinal directions around camera
        for (int i = 0; i < 4; i++) {
            double angle = i * PI / 2 + camAngle;
            Vec2 start(camPos.x + cos(angle) * 2 - sin(angle), camPos.y + sin(angle) * 2 + cos(angle));
            Vec2 end(camPos.x + cos(angle) * 2 + sin(angle), camPos.y + sin(angle) * 2 - cos(angle));
            
            auto wall = std::make_shared<Wall>(start, end);
            wall->setTextureId(i);
            visibleWalls.push_back(wall);
        }
        
        std::cout << "WARNING: No walls found! Added " << visibleWalls.size() << " test walls." << std::endl;
    }
    
    // DIRECT WALL DEBUG: Draw walls directly to screen without any intermediate processing
    double eyeX, eyeY, eyeZ, dirX, dirY, dirZ;
    camera->getViewMatrix(eyeX, eyeY, eyeZ, dirX, dirY, dirZ);
    
    // Get camera FOV and calculate projection variables
    double fov = camera->getFOV();
    double halfFovTan = tan(fov / 2.0);
    double aspectRatio = static_cast<double>(m_screenWidth) / m_screenHeight;
    
    // Draw each wall directly
    for (const auto& wall : visibleWalls) {
        Vec2 start = wall->getStart();
        Vec2 end = wall->getEnd();
        
        // Transform points to camera space
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
        
        // Calculate screen x-coordinates
        double startScreenX = (rotStartY / rotStartX / halfFovTan / aspectRatio + 1.0) * m_screenWidth / 2.0;
        double endScreenX = (rotEndY / rotEndX / halfFovTan / aspectRatio + 1.0) * m_screenWidth / 2.0;
        
        // Draw a bright line on screen to show where wall should be
        int x1 = static_cast<int>(startScreenX);
        int x2 = static_cast<int>(endScreenX);
        
        // Only draw if in screen bounds
        if ((x1 >= 0 && x1 < m_screenWidth) || (x2 >= 0 && x2 < m_screenWidth)) {
            // Use a bright distinctive color based on wall ID
            int textureId = wall->getTextureId() % 4;
            Color debugColor;
            
            switch (textureId) {
                case 0: debugColor = Color(255, 255, 255); break; // White
                case 1: debugColor = Color(255, 0, 255); break;   // Magenta
                case 2: debugColor = Color(255, 255, 0); break;   // Yellow
                case 3: debugColor = Color(0, 255, 255); break;   // Cyan
                default: debugColor = Color(255, 0, 0); break;    // Red (fallback)
            }
            
            // Draw colored vertical lines at wall positions
            for (int x = std::max(0, x1); x <= std::min(m_screenWidth - 1, x2); x++) {
                for (int y = 0; y < m_screenHeight; y++) {
                    // Draw every 5th line for a dashed effect
                    if ((x - x1) % 5 == 0) {
                        // Force pixel to be visible regardless of Z-buffer
                        setPixel(x, y, debugColor);
                    }
                }
            }
        }
    }
    
    // Process visible walls to generate spans
    processVisibleWalls(map, camera, visibleWalls);
    
    // DEBUG: Verify spans are being created
    if (frameCount % 60 == 0) {  // Print only once per second
        std::cout << "  Generated " << m_wallSpans.size() << " wall spans" << std::endl;
    }
    
    // Render all spans in the correct order:
    // 1. Draw floor first (which checks z-buffer to avoid overwriting walls)
    drawFloorCeilingSpans();
    
    // 2. Draw walls (sorted back to front)
    drawWallSpans();
    
    // 3. Finally draw sprites
    drawSpriteSpans();
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

void Renderer::renderSkybox(Camera* camera) 
{
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
            
            // Adjust color with noise
            Color finalColor = Color(
                std::min(255, std::max(0, static_cast<int>(skyColor.r + cloudNoise))),
                std::min(255, std::max(0, static_cast<int>(skyColor.g + cloudNoise * 0.5))),
                std::min(255, std::max(0, static_cast<int>(skyColor.b + cloudNoise * 0.2)))
            );
            
            setPixel(x, y, finalColor);
        }
    }
}

void Renderer::processVisibleWalls(Map* map, Camera* camera, const std::vector<std::shared_ptr<Wall>>& visibleWalls) 
{
    if (!camera) return;
    
    double eyeX, eyeY, eyeZ, dirX, dirY, dirZ;
    camera->getViewMatrix(eyeX, eyeY, eyeZ, dirX, dirY, dirZ);
    
    // Get camera FOV and calculate projection variables
    double fov = camera->getFOV();
    double halfFovTan = tan(fov / 2.0);
    double aspectRatio = static_cast<double>(m_screenWidth) / m_screenHeight;
    
    // Process each visible wall
    for (const auto& wall : visibleWalls) {
        if (!wall) continue;
        
        // Get wall points in world space
        Vec2 start = wall->getStart();
        Vec2 end = wall->getEnd();
        
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
        
        // Determine which sector the wall belongs to
        Sector* sector = wall->getAdjoiningSector();
        // If it's not a portal wall or the adjoining sector is null, use current sector
        if (!wall->isPortal() || !sector) {
            // Try to find the containing sector
            auto containingSector = map->findSectorContainingPoint((start + end) * 0.5);
            if (containingSector) {
                sector = containingSector.get();
            }
        }
        
        if (!sector) continue; // Skip if we can't determine the sector
        
        // Get wall height information
        double wallHeight = wall->getHeight();
        if (wallHeight <= 0.0) {
            // If wall doesn't have its own height, use sector ceiling height
            wallHeight = sector->getCeilingHeight() - sector->getFloorHeight();
        }
        
        // Calculate lower and upper bounds of wall
        double bottomZ = sector->getFloorHeight() + wall->getBottomOffset();
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
        double wallLength = wall->getLength();
        double textureScaleX = 1.0 / wallLength;
        
        // Wall direction for texture mapping
        Vec2 wallDir = (end - start).normalized();
        
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
            span.distance = distance;
            span.textureId = wall->getTextureId();
            span.isPortal = wall->isPortal();
            span.lightLevel = sector->getLightLevel();
            span.sector = sector;
            span.wall = wall.get();
            
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

void Renderer::drawWallSpans() 
{
    // Sort spans by distance (far to near) for proper rendering
    std::sort(m_wallSpans.begin(), m_wallSpans.end(), [](const WallSpan& a, const WallSpan& b) {
        return a.distance > b.distance;
    });
    
    // Counter for debug visualization
    static int frameCounter = 0;
    frameCounter++;
    
    // Draw each wall span
    for (const auto& span : m_wallSpans) {
        // Skip portal walls for this simplified version
        if (span.isPortal) continue;
        
        // Clamp Y values to screen bounds
        int y1 = std::max(0, span.y1);
        int y2 = std::min(m_screenHeight - 1, span.y2);
        
        // SUPER OBVIOUS PATTERN:
        // Use bright, unmistakable colors and patterns to test if walls are rendering at all
        
        // Draw the vertical strip
        for (int y = y1; y <= y2; y++) {
            // Create a very visible pattern 
            // - Alternate every 8 pixels vertically (bright yellow stripes on red)
            // - Make walls pulse over time to make sure rendering updates
            Color wallColor;
            
            // Vertical stripe pattern
            bool isStripe = ((y / 8) % 2 == 0);
            
            // Time-based pulsing (changes every ~30 frames)
            bool pulsePhase = ((frameCounter / 30) % 2 == 0);
            
            // Extremely bright contrasting colors
            if (isStripe) {
                // Bright yellow stripes
                wallColor = pulsePhase ? Color(255, 255, 0) : Color(200, 200, 0);
            } else {
                // Bright red background
                wallColor = pulsePhase ? Color(255, 0, 0) : Color(200, 0, 0);
            }
            
            // Special mode - draw diagonal pattern based on world position
            if (span.x % 3 == 0) {
                // Blue diagonal pattern for every third vertical strip
                wallColor = Color(0, 0, 255);
            }
            
            // Absolutely no blending or lighting - we want raw color
            // Draw regardless of Z-buffer - force the walls to be visible
            int index = y * m_screenWidth + span.x;
            if (index >= 0 && index < m_screenWidth * m_screenHeight) {
                // Force override Z-buffer 
                m_zBuffer[index] = 0.0; // Closest possible distance
                // Directly set the pixel - maximum visibility
                setPixel(span.x, y, wallColor);
            }
        }
    }
}

void Renderer::drawFloorCeilingSpans() 
{
    int horizonY = m_screenHeight / 2; // Middle of the screen is the horizon

    // Draw ceiling (top half, below skybox)
    for (int y = horizonY - 1; y >= horizonY / 2; y--) {
        // Calculate ceiling distance factor - darker closer to horizon, lighter at top
        double t = 1.0 - (static_cast<double>(horizonY - y) / (horizonY / 2.0));
        
        // Calculate distance for the Z-buffer
        double distanceMultiplier = 1.5;
        double distance = m_renderDistance * distanceMultiplier;
        
        // Get ceiling tile size (perspective effect)
        int perspectiveCeilingTileSize = static_cast<int>(40 + 20 * t);
        
        for (int x = 0; x < m_screenWidth; x++) {
            // Direction from center of screen
            double dirX = (x - m_screenWidth / 2.0) / (m_screenWidth / 2.0);
            
            // Adjust coordinates based on perspective
            int ceilingX = static_cast<int>(x + dirX * (1.0 - t) * m_screenWidth * 0.8);
            int ceilingY = static_cast<int>(y - (1.0 - t) * m_screenHeight * 0.4);
            
            // Get the DOOM-like ceiling color
            Color ceilingColor = getDoomCeilingColor(ceilingX, ceilingY, perspectiveCeilingTileSize);
            
            // Apply distance fog effect
            double fogFactor = t * 0.8 + 0.2;
            ceilingColor = Color::lerp(Color(5, 2, 0), ceilingColor, fogFactor);
            
            // Set in the Z-buffer
            int index = y * m_screenWidth + x;
            
            // Only draw the ceiling if no wall spans have been drawn here
            if (index >= 0 && index < m_screenWidth * m_screenHeight) {
                if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                    m_zBuffer[index] = distance;
                    setPixel(x, y, ceilingColor);
                }
            }
        }
    }

    // Draw floor (bottom half)
    for (int y = horizonY; y < m_screenHeight; y++) {
        // Calculate floor distance factor - darker farther away, lighter closer
        double t = 1.0 - (static_cast<double>(y - horizonY) / horizonY);
        
        // Calculate distance for the Z-buffer
        // Fixed distance for floor to ensure it's always behind walls
        double distanceMultiplier = 1.5; // Ensure floor is further than walls
        double distance = m_renderDistance * distanceMultiplier;
        
        // Get floor tile size (larger tiles in the distance, smaller when close)
        // This creates the perspective effect for the floor texture
        int perspectiveFloorTileSize = static_cast<int>(40 + 20 * t); // Tile size changes with distance
        
        for (int x = 0; x < m_screenWidth; x++) {
            // Calculate floor coordinates with perspective correction
            // This creates the 3D floor effect as if looking down at floor tiles
            
            // Direction from center of screen
            double dirX = (x - m_screenWidth / 2.0) / (m_screenWidth / 2.0);
            
            // Adjust coordinates based on perspective (closer to horizon = further away)
            int floorX = static_cast<int>(x + dirX * (1.0 - t) * m_screenWidth * 0.8);
            int floorY = static_cast<int>(y + (1.0 - t) * m_screenHeight * 0.4);
            
            // Get the DOOM-like floor color with pentagram pattern
            Color floorColor = getDoomFloorColor(floorX, floorY, perspectiveFloorTileSize);
            
            // Apply distance fog effect
            double fogFactor = t * 0.8 + 0.2; // Prevent complete darkness
            floorColor = Color::lerp(Color(10, 5, 5), floorColor, fogFactor);
            
            // Set in the Z-buffer
            int index = y * m_screenWidth + x;
            
            // Only draw the floor if no wall spans have been drawn here
            // This ensures walls always appear on top of the floor
            if (index >= 0 && index < m_screenWidth * m_screenHeight) {
                if (m_zBuffer[index] == std::numeric_limits<double>::max()) {
                    m_zBuffer[index] = distance;
                    setPixel(x, y, floorColor);
                }
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
    Color baseFloorColor(40, 20, 20);     // Dark brown-red
    Color floorBorderColor(60, 30, 30);   // Slightly lighter
    Color pentagramColor(120, 0, 0);      // Blood red

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
            return pentagramColor;
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
    Color baseCeilingColor(30, 10, 5);      // Dark red-brown
    Color ceilingPatternColor(50, 20, 10);  // Lighter brown
    Color glowingCrackColor(80, 30, 5);     // Orange-red "cracks"
    
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
    if (seed < 30) {  // 30% of tiles have cracks
        // Calculate crack pattern (diagonal cracks)
        double diag1 = fabs(localX - localY);
        double diag2 = fabs(localX - (1.0 - localY));
        
        // If near diagonal and seed-dependent pattern matches
        if ((diag1 < 0.05 || diag2 < 0.05) && (seed % 3 == (x * y) % 3)) {
            return glowingCrackColor;
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
    
    // Base colors for various wall types
    Color stoneColor(70, 60, 50);        // Stone base
    Color metalColor(80, 80, 90);        // Metal base
    Color techColor(60, 70, 80);         // Tech base
    Color demonicColor(80, 40, 40);      // Demonic base
    Color accentColor(120, 30, 20);      // Accent color (red)
    Color darkAccent(30, 20, 20);        // Dark accent
    
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
            
            // Panel edge detection (darker edges)
            int edgeWidth = 2;
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
                baseColor = Color(60, 60, 70); // Darker edge
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
            
            // Brick edge detection
            int mortarWidth = 1;
            bool isMortar = (effectiveX % brickWidth < mortarWidth) || 
                           (effectiveX % brickWidth >= brickWidth - mortarWidth) ||
                           (texY % brickHeight < mortarWidth) ||
                           (texY % brickHeight >= brickHeight - mortarWidth);
            
            // Crack pattern (seeded by position)
            int seed = (col * 1234 + row * 5678) % 100;
            bool hasCrack = (seed < 15); // 15% chance for a crack
            
            // Determine crack pattern
            bool isOnCrack = false;
            if (hasCrack) {
                int crackX = brickWidth / 2 + (seed % 3) - 1;
                int crackY = brickHeight / 2 + ((seed / 3) % 3) - 1;
                int localX = effectiveX % brickWidth;
                int localY = texY % brickHeight;
                
                int dx = localX - crackX;
                int dy = localY - crackY;
                
                // Check if point is on a jagged line crack
                isOnCrack = (abs(dx) <= 1 && abs(dy) <= 4) || 
                            (abs(dx) <= 2 && abs(dy) <= 2);
            }
            
            // Color selection
            if (isMortar) {
                baseColor = darkAccent; // Dark mortar
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
            
            // Grid lines
            bool isGridLine = (texX % gridSize <= 1) || (texY % gridSize <= 1);
            
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
                baseColor = Color(100, 110, 120); // Light circuit line
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
            double lineWidth = 2.0;
            
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
                    
                    // Simple rune patterns (box with cross)
                    int dx = abs(texX - runeX);
                    int dy = abs(texY - runeY);
                    
                    if ((dx < 3 && dy < 6) || (dx < 6 && dy < 3)) {
                        onRune = true;
                    }
                }
            }
            
            // Color selection
            if (onPentagram) {
                baseColor = accentColor; // Red pentagram
            } else if (onRune) {
                baseColor = Color(150, 30, 10); // Brighter rune
            } else {
                // Alternate slightly darker/lighter background
                baseColor = isDarker ? 
                    Color(std::max(0, static_cast<int>(demonicColor.r) - 10),
                          std::max(0, static_cast<int>(demonicColor.g) - 5),
                          std::max(0, static_cast<int>(demonicColor.b) - 5)) : 
                    demonicColor;
            }
            break;
        }
        
        case 4: { // Hellish flesh wall (veins and organic texture)
            // Base color with organic-looking noise
            int noiseX = texX / 4;
            int noiseY = texY / 4;
            int noise = ((noiseX * 7) + (noiseY * 19)) % 10;
            
            // Veins (curved lines)
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
                
                if (veinDist < 2.0) {
                    onVein = true;
                }
            }
            
            // Blood spots (small circles)
            bool inBloodSpot = false;
            
            // Create several blood spots
            for (int spot = 0; spot < 5; spot++) {
                int spotX = (spot * 37) % 64;
                int spotY = (spot * 23) % 64;
                int spotRadius = 3 + (spot % 3);
                
                int dx = texX - spotX;
                int dy = texY - spotY;
                if (dx*dx + dy*dy < spotRadius*spotRadius) {
                    inBloodSpot = true;
                }
            }
            
            // Color selection
            if (inBloodSpot) {
                baseColor = Color(120, 20, 20); // Dark red blood
            } else if (onVein) {
                baseColor = Color(140, 30, 30); // Brighter vein
            } else {
                // Flesh texture with noise
                baseColor = Color(
                    std::min(255, std::max(0, 100 - noise * 2)),  // Reddish base
                    std::min(255, std::max(0, 50 - noise)),
                    std::min(255, std::max(0, 50 - noise))
                );
            }
            break;
        }
        
        default:
            baseColor = Color(200, 200, 200); // Light gray (fallback)
    }
    
    // Apply distance-based darkening
    double fogFactor = 1.0 - std::min(1.0, distance / 20.0);
    return Color::lerp(Color(0, 0, 0), baseColor, fogFactor);
} 