#include "renderer.h"
#include "map.h"
#include "utils.h"
#include "player.h"
#include <iostream>
#include <limits>

// Forward declarations for texture functions
Color getDoomFloorColor(int x, int y, int floorTileSize);
Color getDoomCeilingColor(int x, int y, int tileSize);
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
    
    // Process visible walls to generate spans
    processVisibleWalls(map, camera, visibleWalls);
    
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
            
            // Calculate texture U coordinate
            double u = t;
            
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
    
    // Draw each wall span
    for (const auto& span : m_wallSpans) {
        // Skip portal walls for this simplified version
        if (span.isPortal) continue;
        
        // Clamp Y values to screen bounds
        int y1 = std::max(0, span.y1);
        int y2 = std::min(m_screenHeight - 1, span.y2);
        
        // Calculate wall color based on texture and lighting
        // Here we're using a simplified approach with basic colors
        Color wallColor;
        
        // Determine color based on texture ID for this simplified version
        switch (span.textureId % 5) {
            case 0: wallColor = Color(150, 100, 80); break;  // Brown
            case 1: wallColor = Color(120, 120, 120); break; // Gray
            case 2: wallColor = Color(80, 100, 150); break;  // Blue-gray
            case 3: wallColor = Color(140, 80, 80); break;   // Red-brown
            case 4: wallColor = Color(80, 140, 80); break;   // Green
            default: wallColor = Color(200, 200, 200); break; // Light gray
        }
        
        // Adjust color based on distance for simple fog effect
        double fogFactor = 1.0 - std::min(1.0, span.distance / 20.0);
        Color foggedColor = Color::lerp(Colors::BLACK, wallColor, fogFactor);
        
        // Apply lighting
        double lightLevel = span.lightLevel;
        Color finalColor = Color::lerp(Color(0, 0, 0), foggedColor, lightLevel);
        
        // Draw the vertical strip
        for (int y = y1; y <= y2; y++) {
            int index = y * m_screenWidth + span.x;
            
            // Always draw walls, they should override the floor
            // as we've already sorted them by distance
            if (index >= 0 && index < m_screenWidth * m_screenHeight) {
                m_zBuffer[index] = span.distance;
                setPixel(span.x, y, finalColor);
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