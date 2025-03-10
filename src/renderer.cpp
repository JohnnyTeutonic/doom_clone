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
    
    // REVERTING TO BSP-BASED RENDERING:
    // Get visible walls using BSP tree based on camera position
    std::vector<std::shared_ptr<Wall>> visibleWalls = map->getVisibleWalls(camera->getPosition());
    
    // DEBUG: Make sure we actually have walls
    static int frameCount = 0;
    frameCount++;
    
    // Process walls to generate spans
    processVisibleWalls(visibleWalls, camera);
    
    // DEBUG: Verify spans are being created
    if (frameCount % 60 == 0) {  // Print only once per second
        std::cout << "  Generated " << m_wallSpans.size() << " wall spans" << std::endl;
    }
    
    // Render all spans in the correct order:
    // 1. Draw floor first (which checks z-buffer to avoid overwriting walls)
    drawFloorCeilingSpans(camera);
    
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
            span.texU = u; // Set texU as well for newer implementations
            span.distance = distance;
            span.textureId = wall->getTextureId();
            span.isPortal = false; // Simple map doesn't have portals
            span.lightLevel = lightLevel;
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

void Renderer::drawWallSpans() 
{
    // Sort spans by distance (far to near) for proper rendering
    std::sort(m_wallSpans.begin(), m_wallSpans.end(), [](const WallSpan& a, const WallSpan& b) {
        return a.distance > b.distance;
    });
    
    // Counter for debug visualization
    static int frameCounter = 0;
    frameCounter++;
    
    // Debug message to check wall spans
    if (frameCounter % 100 == 0) {
        std::cout << "Drawing " << m_wallSpans.size() << " wall spans" << std::endl;
    }
    
    // Draw each wall span
    for (const auto& span : m_wallSpans) {
        // Skip portal walls for this simplified version
        if (span.isPortal) continue;
        
        // Clamp Y values to screen bounds
        int y1 = std::max(0, span.y1);
        int y2 = std::min(m_screenHeight - 1, span.y2);
        
        // Debug: Force walls to be visible with high contrast textures
        if (y2 <= y1) {
            // Fix degenerate spans
            y2 = y1 + 100; // Ensure wall is visible
        }
        
        // Force texture ID to be a reasonable value
        int textureId = abs(span.textureId) % 4;  // Make sure it's 0-3
        
        // Draw the vertical strip
        for (int y = y1; y <= y2; y++) {
            // Calculate vertical texture coordinate - normalized position on wall
            double texV = static_cast<double>(y - y1) / static_cast<double>(y2 - y1 + 1);
            
            // Calculate horizontal texture coordinate - use span.u but ensure it wraps
            // Force texU to have visible repetition for debugging
            double texU = span.u * 3.0;  // Multiply to create more visible repetition
            
            // Ensure texture coordinates wrap properly
            texU = texU - floor(texU);
            texV = texV - floor(texV);
            
            // Get wall color based on texture ID
            Color wallColor;
            
            // Enhanced debug coloring - make textures super visible
            switch (textureId) {
                case 0: { // Bloody stone wall - exaggerated colors
                    // Stone pattern with bright red stains
                    int stoneSize = 16;
                    int stoneX = static_cast<int>(texU * 64) / stoneSize;
                    int stoneY = static_cast<int>(texV * 64) / stoneSize;
                    
                    // Mortar grid - make it thicker and more visible
                    bool isMortar = (static_cast<int>(texU * 64) % stoneSize < 3) || 
                                   (static_cast<int>(texV * 64) % stoneSize < 3);
                    
                    // Bright blood stains - more common and brighter
                    int seed = (stoneX * 17 + stoneY * 23) % 100;
                    bool isBloodStain = (seed < 40);  // 40% chance for blood (more common)
                    
                    if (isMortar) {
                        wallColor = Color(30, 20, 20);  // Dark mortar
                    } else if (isBloodStain) {
                        // Brighter blood stains
                        wallColor = Color(200, 20, 20);  // Bright red blood
                    } else {
                        // Stone with high contrast
                        wallColor = Color(90, 80, 70);  // Lighter stone
                    }
                    break;
                }
                
                case 1: { // Tech panels - industrial metal look
                    // Enhanced tech panel pattern with metal finish
                    int panelSize = 32;
                    
                    // Panel grid - thicker lines
                    bool isEdge = (static_cast<int>(texU * 64) % panelSize < 4) || 
                                 (static_cast<int>(texV * 64) % panelSize < 4);
                    
                    // Panel division lines (additional detail)
                    bool isSubdivision = ((static_cast<int>(texU * 64) % (panelSize/2) < 2) && 
                                         (static_cast<int>(texU * 64) % panelSize >= 4)) || 
                                        ((static_cast<int>(texV * 64) % (panelSize/2) < 2) && 
                                         (static_cast<int>(texV * 64) % panelSize >= 4));
                    
                    // Add some bolt/rivet details
                    double localU = (texU * 64) / 64.0;
                    double localV = (texV * 64) / 64.0;
                    
                    // Position bolts at panel corners
                    int cornerX = static_cast<int>(texU * 64) / panelSize;
                    int cornerY = static_cast<int>(texV * 64) / panelSize;
                    double cornerLocalU = fmod(texU * 64, panelSize) / panelSize;
                    double cornerLocalV = fmod(texV * 64, panelSize) / panelSize;
                    
                    bool isBolt = (cornerLocalU < 0.15 || cornerLocalU > 0.85) && 
                                 (cornerLocalV < 0.15 || cornerLocalV > 0.85) &&
                                 (cornerLocalU < 0.08 || cornerLocalU > 0.92 || 
                                  cornerLocalV < 0.08 || cornerLocalV > 0.92);
                    
                    // Add some wear to the panels
                    int wear = (cornerX * 13 + cornerY * 7) % 10;
                    
                    if (isEdge) {
                        // Dark metal edge
                        wallColor = Color(70, 70, 80);
                    } else if (isSubdivision) {
                        // Secondary panel divider
                        wallColor = Color(85, 85, 95);
                    } else if (isBolt) {
                        // Bolt/rivet detail
                        wallColor = Color(110, 110, 120);
                    } else {
                        // Base industrial metal panel
                        int variation = ((cornerX * 5 + cornerY * 7) % 10) - 5;
                        wallColor = Color(
                            60 + variation + (wear > 7 ? -15 : 0),
                            65 + variation + (wear > 7 ? -10 : 0),
                            75 + variation + (wear > 7 ? -5 : 0)
                        );
                    }
                    break;
                }
                
                case 2: { // Flesh wall - vibrant reds
                    // Simplified organic pattern with high contrast
                    int noiseX = static_cast<int>(texU * 64) / 4;
                    int noiseY = static_cast<int>(texV * 64) / 4;
                    
                    // More visible veins
                    bool onVein = false;
                    for (int i = 0; i < 3; i++) {
                        double veinY = 0.2 + i * 0.3;
                        double veinThickness = 0.03;
                        if (fabs(texV - veinY) < veinThickness) {
                            onVein = true;
                        }
                    }
                    
                    if (onVein) {
                        wallColor = Color(180, 10, 10);  // Bright red veins
                    } else {
                        // Base flesh color with pattern
                        int pattern = ((noiseX + noiseY) % 3);
                        int baseR = 100 + pattern * 20;
                        int baseG = 30 + pattern * 10;
                        int baseB = 30 + pattern * 10;
                        wallColor = Color(baseR, baseG, baseB);
                    }
                    break;
                }
                
                case 3: { // Hellish metal with runes - high contrast
                    // Grid pattern with strong contrast
                    int gridSize = 16;
                    bool isGrid = (static_cast<int>(texU * 64) % gridSize < 2) || 
                                 (static_cast<int>(texV * 64) % gridSize < 2);
                    
                    // Create visible rune patterns
                    double localU = fmod(texU, 1.0);
                    double localV = fmod(texV, 1.0);
                    
                    // Simple rune shape
                    bool onRune = (fabs(localU - 0.5) < 0.1 && fabs(localV - 0.5) < 0.3) || 
                                  (fabs(localU - 0.5) < 0.3 && fabs(localV - 0.5) < 0.1);
                    
                    if (onRune) {
                        wallColor = Color(220, 80, 0);  // Bright orange-red rune
                    } else if (isGrid) {
                        wallColor = Color(80, 50, 30);  // Rusty edge
                    } else {
                        wallColor = Color(50, 40, 30);  // Dark metal
                    }
                    break;
                }
                
                default:
                    // Fallback - checkered pattern for visibility
                    bool isCheckerDark = ((static_cast<int>(texU * 8) + static_cast<int>(texV * 8)) % 2 == 0);
                    wallColor = isCheckerDark ? Color(40, 40, 40) : Color(160, 160, 160);
            }
            
            // Apply minimal shading to preserve texture visibility
            double fogFactor = std::max(0.7, 1.0 - span.distance / 30.0);
            Color finalColor = Color::lerp(wallColor, Color(0, 0, 0), static_cast<float>(1.0 - fogFactor));
            
            // Set in Z-buffer and draw
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
        
        // Distance factor for fog effect
        double distFactor = std::min(1.0, rowDistance / 20.0);
        
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
            
            // Apply fog effect
            Color foggedFloorColor = Color::lerp(
                floorColor,
                Color(30, 30, 30), // Dark fog color
                distFactor * 0.7
            );
            
            // Set the pixel for the floor
            setPixel(x, y, foggedFloorColor);
            
            // Calculate mirror position for ceiling
            int ceilingY = m_screenHeight - y - 1;
            
            // Only draw ceiling if not already occupied by a wall
            if (m_zBuffer[ceilingY * m_screenWidth + x] == std::numeric_limits<double>::max()) {
                // Get ceiling color - darker variation of floor
                Color ceilingColor = getDoomCeilingColor(texX, texY, tileSize);
                
                // Apply fog effect to ceiling
                Color foggedCeilingColor = Color::lerp(
                    ceilingColor,
                    Color(20, 15, 15), // Darker fog for ceiling
                    distFactor * 0.8
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
    return Color::lerp(Color(0, 0, 0), baseColor, static_cast<float>(1.0 - fogFactor));
} 