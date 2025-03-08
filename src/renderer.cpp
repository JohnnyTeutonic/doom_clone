#include "renderer.h"
#include "engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <SDL2/SDL_ttf.h>

Renderer::Renderer()
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_screenWidth(800)
    , m_screenHeight(600)
    , m_fullscreen(false)
    , m_textureManager(nullptr)
    , m_spriteManager(nullptr)
    , m_projectileManager(nullptr)
    , m_engine(nullptr)
    , m_showFPS(true)
    , m_showMinimap(true)
    , m_showWeapon(true)
    , m_showCeilings(true)  // Initialize ceiling rendering to on by default
    , m_lightingEnabled(true)
    , m_muzzleFlashEnabled(false)  // Disable muzzle flash by default
    , m_performanceLevel(PerformanceLevel::Medium)
    , m_frameCount(0)
    , m_fpsTimer(0.0)
    , m_fps(0.0)
{
    // Initialize lighting system
    m_lightingSystem.setEnabled(m_lightingEnabled);
    
    // Initialize z-buffer
    m_zBuffer.resize(m_screenWidth, std::numeric_limits<double>::max());
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::init(int width, int height, bool fullscreen) {
    m_screenWidth = width;
    m_screenHeight = height;
    
    // Initialize Z-buffer
    m_zBuffer.resize(width, 10.0);
    
    // Initialize SDL_ttf for text rendering
    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        return false;
    }
    
    // Initialize lighting system with default medium performance settings
    setPerformanceLevel(PerformanceLevel::Medium);
    
    std::cout << "Renderer initialized with dimensions: " << width << "x" << height << std::endl;
    return true;
}

void Renderer::setSDLRenderer(SDL_Renderer* renderer) {
    m_renderer = renderer;
    std::cout << "SDL Renderer set in Renderer class: " << m_renderer << std::endl;
}

void Renderer::cleanup() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
    TTF_Quit();
    SDL_Quit();
}

void Renderer::render(const Map& map, const Player& player, double deltaTime, double recoil, double flashIntensity) {
    // Clear screen
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    
    // Ensure zBuffer is the right size
    if (m_zBuffer.size() != m_screenWidth) {
        m_zBuffer.resize(m_screenWidth, 10.0);  // Initialize with far distance
    }
    
    // Render the 3D view
    renderView(map, player);
    
    // Render sprites if we have a sprite manager
    if (m_spriteManager) {
        renderSprites(map, player);
    }
    
    // Render projectiles if we have a projectile manager
    if (m_projectileManager) {
        renderProjectiles(player);
    }
    
    // Render minimap if enabled
    if (m_showMinimap) {
        renderMinimap(map, player, *m_projectileManager);
    }
    
    // Render HUD
    renderHUD(player);
    
    // Update FPS counter
    m_frameCount++;
    m_fpsTimer += deltaTime;
    if (m_fpsTimer >= 1.0) {
        m_fps = m_frameCount / m_fpsTimer;
        m_frameCount = 0;
        m_fpsTimer = 0.0;
    }
    
    // Show FPS if enabled
    if (m_showFPS) {
        std::stringstream ss;
        ss << "FPS: " << static_cast<int>(m_fps);
        renderText(ss.str(), 10, 10, Color::White());
    }
}

void Renderer::renderView(const Map& map, const Player& player) {
    // Clear the z-buffer
    clearZBuffer();
    
    // Get player position, direction, and vertical angles
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    
    // Use the combined vertical offset value that includes look angle, world effects, and jump height
    double totalVerticalOffset = player.getVerticalOffset();
    
    // Calculate screen-space vertical offset
    int screenSpaceOffset = static_cast<int>(totalVerticalOffset * m_screenHeight / 2);
    
    // Get player's elevation level (determine from map cell)
    int playerX = static_cast<int>(pos.x);
    int playerY = static_cast<int>(pos.y);
    int playerElevation = map.getCellElevation(playerX, playerY);
    float playerStepHeight = map.getStepHeight(playerX, playerY);
    
    // For each vertical strip of the screen
    for (int x = 0; x < m_screenWidth; x++) {
        // Calculate ray position and direction
        double cameraX = 2.0 * x / static_cast<double>(m_screenWidth) - 1.0;
        Vec2 rayDir = dir + plane * cameraX;
        
        // Apply vertical offset to wall and sprite rendering
        int effectiveVerticalOffset = screenSpaceOffset;
        
        // Calculate which box of the map we're in
        Vec2 mapPos(static_cast<int>(pos.x), static_cast<int>(pos.y));
        
        // Length of ray from current position to next x or y-side
        Vec2 deltaDist(
            std::abs(rayDir.x) < 1e-8 ? 1e8 : std::abs(1.0 / rayDir.x),
            std::abs(rayDir.y) < 1e-8 ? 1e8 : std::abs(1.0 / rayDir.y)
        );
        
        // Calculate step and initial sideDist
        Vec2 step;
        Vec2 sideDist;
        
        if (rayDir.x < 0) {
            step.x = -1;
            sideDist.x = (pos.x - mapPos.x) * deltaDist.x;
        } else {
            step.x = 1;
            sideDist.x = (mapPos.x + 1.0 - pos.x) * deltaDist.x;
        }
        
        if (rayDir.y < 0) {
            step.y = -1;
            sideDist.y = (pos.y - mapPos.y) * deltaDist.y;
        } else {
            step.y = 1;
            sideDist.y = (mapPos.y + 1.0 - pos.y) * deltaDist.y;
        }
        
        // Perform DDA
        bool hit = false;
        int side;
        bool isElevatedWall = false;
        bool isStairs = false;
        bool isStairStep = false;
        float stepHeight = 0.0f;
        int rayElevation = playerElevation; // Start at player's elevation
        
        while (!hit) {
            // Jump to next map square
            if (sideDist.x < sideDist.y) {
                sideDist.x += deltaDist.x;
                mapPos.x += step.x;
                side = 0;
            } else {
                sideDist.y += deltaDist.y;
                mapPos.y += step.y;
                side = 1;
            }
            
            // Check if ray has hit a wall
            CellType cellType = map.getCell(mapPos.x, mapPos.y);
            int cellElevation = map.getCellElevation(mapPos.x, mapPos.y);
            
            // Skip cells in invisible sectors (sector culling)
            int cellSector = map.getSectorAt(mapPos.x, mapPos.y);
            if (cellSector >= 0 && !map.isSectorVisible(cellSector)) {
                continue;
            }
            
            // Check stair steps
            if (cellType == CellType::StairStep1 || 
                cellType == CellType::StairStep2 || 
                cellType == CellType::StairStep3) {
                
                isStairStep = true;
                stepHeight = map.getStepHeight(mapPos.x, mapPos.y);
                hit = true;
            }
            // Check if we've hit stairs
            else if (cellType == CellType::Stairs) {
                isStairs = true;
                hit = true;
            }
            // Check if we've hit a wall or elevated wall
            else if (cellType == CellType::Wall) {
                // Regular walls are hit regardless of elevation
                hit = true;
            }
            else if (cellType == CellType::ElevatedWall) {
                // Elevated walls are hit only when we're at the same elevation
                if (rayElevation == cellElevation) {
                    hit = true;
                    isElevatedWall = true;
                }
            }
            else if (cellType == CellType::Door) {
                hit = true;
            }
            
            // Update ray's elevation if we find stairs
            if ((cellType == CellType::Stairs || isStairStep) && !hit) {
                rayElevation = cellElevation;
            }
        }
        
        // Calculate distance projected on camera direction
        double perpWallDist;
        if (side == 0) {
            perpWallDist = (mapPos.x - pos.x + (1 - step.x) / 2) / rayDir.x;
        } else {
            perpWallDist = (mapPos.y - pos.y + (1 - step.y) / 2) / rayDir.y;
        }
        
        // Save distance for sprite rendering
        m_zBuffer[x] = perpWallDist;
        
        // Calculate wall height
        int lineHeight = static_cast<int>(m_screenHeight / perpWallDist);
        
        // Apply height adjustment for elevated walls and stairs
        int heightOffset = 0;
        
        if (isStairStep) {
            // For stair steps, use the step height to create a smooth transition
            float relativeHeight = map.getStepHeight(mapPos.x, mapPos.y) - playerStepHeight;
            heightOffset = static_cast<int>(relativeHeight * m_screenHeight / 3);
        }
        else if (isElevatedWall && playerElevation == 0) {
            // Looking up at an elevated wall from ground level
            heightOffset = m_screenHeight / 3; // Move the wall higher
        }
        else if (!isElevatedWall && !isStairs && playerElevation == 1) {
            // Looking down at a ground wall from elevated level
            heightOffset = -m_screenHeight / 3; // Move the wall lower
        }
        
        // Calculate drawing boundaries with vertical offset
        int drawStart = -lineHeight / 2 + m_screenHeight / 2 + effectiveVerticalOffset + heightOffset;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + m_screenHeight / 2 + effectiveVerticalOffset + heightOffset;
        if (drawEnd >= m_screenHeight) drawEnd = m_screenHeight - 1;

        // Get wall texture or render special stair graphics
        if (isStairs || isStairStep) {
            // Handle stair rendering
            if (isStairStep) {
                // Get step height from map for proper visualization
                float stepHeight = map.getStepHeight(mapPos.x, mapPos.y);
                
                // Use different colors based on height to create a 3D effect
                int baseR = 210 + static_cast<int>(stepHeight * 45);  // More red as steps go up
                int baseG = 210 + static_cast<int>((1.0f - stepHeight) * 45); // More green as steps go down
                int baseB = 255;  // Keep blue constant
                
                // Create 3D stair effect with two colors
                int midPoint = drawStart + (drawEnd - drawStart) / 2;
                
                // Draw top half of the step (horizontal surface)
                SDL_SetRenderDrawColor(m_renderer, baseR, baseG, baseB, 255);
                SDL_RenderDrawLine(m_renderer, x, drawStart, x, midPoint);
                
                // Draw bottom half of the step (vertical riser) in slightly darker color
                SDL_SetRenderDrawColor(m_renderer, baseR * 0.8, baseG * 0.8, baseB * 0.8, 255);
                SDL_RenderDrawLine(m_renderer, x, midPoint+1, x, drawEnd);
                
                // Add horizontal lines to show step edges
                int stepSpacing = (drawEnd - drawStart) / 8;
                if (stepSpacing < 3) stepSpacing = 3;
                
                for (int y = drawStart; y < midPoint; y += stepSpacing) {
                    // Add horizontal line for the step edge
                    SDL_SetRenderDrawColor(m_renderer, 50, 50, 150, 255); // Dark blue edge
                    SDL_RenderDrawLine(m_renderer, x, y, x, y+1);
                }
                
                // Add a shadow at the edge between horizontal and vertical parts
                SDL_SetRenderDrawColor(m_renderer, 50, 50, 100, 255); // Dark shadow
                SDL_RenderDrawLine(m_renderer, x, midPoint, x, midPoint+1);
            }
            else {
                // Render stair entry/exit point with a special pattern
                for (int y = drawStart; y < drawEnd; y++) {
                    // Calculate pattern based on position
                    int patternY = (y - drawStart) % 10;
                    int patternX = (x + patternY) % 10;
                    
                    if (x % 5 <= patternX) {
                        // Golden/yellow for entry points
                        SDL_SetRenderDrawColor(m_renderer, 255, 215, 0, 255);
                    } else {
                        // Dark brown for the background
                        SDL_SetRenderDrawColor(m_renderer, 139, 69, 19, 255);
                    }
                    
                    SDL_RenderDrawPoint(m_renderer, x, y);
                }
            }
            
            continue; // Skip regular texture rendering
        }
        else {
            // Regular wall texture rendering
            int texNum = map.getWallTexture(mapPos.x, mapPos.y);
            const Texture* wallTexture = m_textureManager->getTexture(texNum);
            
            if (!wallTexture) {
                // Fallback to solid color if texture not found
                SDL_SetRenderDrawColor(m_renderer, 128, 128, 128, 255);
                SDL_RenderDrawLine(m_renderer, x, drawStart, x, drawEnd);
                continue;
            }
                
            // Calculate texture coordinates
            double wallX;  // Where exactly the wall was hit
            if (!side) {
                wallX = pos.y + perpWallDist * rayDir.y;
            } else {
                wallX = pos.x + perpWallDist * rayDir.x;
            }
            wallX -= floor(wallX);  // Normalize to [0,1]
            
            // Calculate surface normal for lighting
            Vec2 normal = calculateSurfaceNormal(side, rayDir);
            
            // Calculate world position of the wall hit
            Vec2 wallPos = pos + rayDir * perpWallDist;
            
            // Performance optimization: only calculate lighting once per wall segment
            // if the distance is large enough (reduces calculation per pixel)
            Color lighting;
            if (perpWallDist > 4.0) {
                // For distant walls, calculate lighting once per column
                lighting = m_lightingSystem.calculateLighting(wallPos, normal, pos);
                
                // Draw the textured wall column with single lighting value
                for (int y = drawStart; y < drawEnd; y++) {
                    // Calculate texture Y coordinate
                    double texY = (y - drawStart) / static_cast<double>(drawEnd - drawStart);
                    
                    // Get pixel color from texture
                    Color color = wallTexture->getPixelNormalized(wallX, texY);
                    
                    // Apply lighting to the color
                    color = color * lighting;
                    
                    // Draw the pixel
                    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderDrawPoint(m_renderer, x, y);
                }
            }
            else {
                // For nearby walls, calculate lighting at intervals to maintain quality
                const int lightingInterval = 8; // Calculate lighting every N pixels
                
                for (int y = drawStart; y < drawEnd; y++) {
                    // Calculate texture Y coordinate
                    double texY = (y - drawStart) / static_cast<double>(drawEnd - drawStart);
                    
                    // Get pixel color from texture
                    Color color = wallTexture->getPixelNormalized(wallX, texY);
                    
                    // Calculate lighting only at intervals to improve performance
                    if (y % lightingInterval == 0 || y == drawStart) {
                        lighting = m_lightingSystem.calculateLighting(wallPos, normal, pos);
                    }
                    
                    // Apply lighting to the color
                    color = color * lighting;
                    
                    // Draw the pixel
                    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderDrawPoint(m_renderer, x, y);
                }
            }
        }
    }
    
    // Floor and ceiling casting
    if (m_textureManager) {
        const Texture* floorTexture = m_textureManager->getTexture(m_engine->getFloorTexture());
        const Texture* ceilingTexture = m_textureManager->getTexture(m_engine->getCeilingTexture());
        
        if (floorTexture && ceilingTexture) {
            // Performance optimization: Render floor/ceiling at lower resolution
            // Skip rows based on distance from horizon
            int rowSkip = 1; // Start with rendering every row
            
            // For each horizontal line on the screen from the middle down to the bottom
            for (int y = m_screenHeight / 2 + screenSpaceOffset; y < m_screenHeight; y += rowSkip) {
                // Increase row skipping as we get further from horizon
                if (y > m_screenHeight / 2 + screenSpaceOffset + 50) rowSkip = 2;
                if (y > m_screenHeight / 2 + screenSpaceOffset + 100) rowSkip = 4;
                
                // Calculate the ray direction for this row
                // Current y position compared to the center of the screen (horizon)
                float posZ = 0.5 * m_screenHeight; // Player's view height
                float rowDistance = posZ / (y - m_screenHeight / 2 - screenSpaceOffset);
                
                // Calculate the real world step vector we have to add for each x
                float floorStepX = rowDistance * (2.0 * plane.x) / m_screenWidth;
                float floorStepY = rowDistance * (2.0 * plane.y) / m_screenWidth;
                
                // Calculate the leftmost ray position
                float floorX = pos.x + rowDistance * (dir.x - plane.x);
                float floorY = pos.y + rowDistance * (dir.y - plane.y);
                
                // Performance optimization: reduce resolution for distant floors/ceilings
                int step = 1;
                if (rowDistance > 3.0) step = 2;  // Medium distance
                if (rowDistance > 6.0) step = 4;  // Far distance
                if (rowDistance > 10.0) step = 8; // Very far distance
                
                // For each pixel in the horizontal line
                for (int x = 0; x < m_screenWidth; x += step) {
                    // Get the map cell coordinates
                    int cellX = static_cast<int>(floorX);
                    int cellY = static_cast<int>(floorY);
                    
                    // Get the texture coordinates
                    float tx = (floorX - cellX) * floorTexture->getWidth();
                    float ty = (floorY - cellY) * floorTexture->getHeight();
                    
                    // Use distance-based lighting approximation for better performance
                    double distFactor = std::min(1.0, 10.0 / rowDistance);
                    Color floorLighting(
                        static_cast<Uint8>(128 * distFactor + 127),
                        static_cast<Uint8>(128 * distFactor + 127),
                        static_cast<Uint8>(128 * distFactor + 127)
                    );
                    
                    // Only use full lighting calculation for nearby surfaces
                    if (rowDistance < 5.0) {
                        Vec2 floorPos(cellX + 0.5, cellY + 0.5);
                        Vec2 normal = m_normalDown; // Floor normal points up
                        floorLighting = m_lightingSystem.calculateLighting(floorPos, normal, pos);
                    }
                    
                    // Get floor and ceiling colors
                    Color floorColor = floorTexture->getPixel(tx, ty);
                    Color ceilingColor = ceilingTexture->getPixel(tx, ty);
                    
                    // Apply lighting
                    floorColor = floorColor * floorLighting;
                    ceilingColor = ceilingColor * floorLighting; // Use same lighting for ceiling
                    
                    // Draw floor pixels for this step
                    SDL_SetRenderDrawColor(m_renderer, floorColor.r, floorColor.g, floorColor.b, floorColor.a);
                    for (int i = 0; i < step && x + i < m_screenWidth; i++) {
                        SDL_RenderDrawPoint(m_renderer, x + i, y);
                        
                        // Fill in skipped rows for smoother appearance
                        for (int j = 1; j < rowSkip && y + j < m_screenHeight; j++) {
                            SDL_RenderDrawPoint(m_renderer, x + i, y + j);
                        }
                    }
                    
                    // Draw ceiling pixels for this step only if ceiling rendering is enabled
                    if (m_showCeilings) {
                        int ceilingY = m_screenHeight - y - 1 + 2 * screenSpaceOffset;
                        if (ceilingY >= 0 && ceilingY < m_screenHeight) {
                            SDL_SetRenderDrawColor(m_renderer, ceilingColor.r, ceilingColor.g, ceilingColor.b, ceilingColor.a);
                            for (int i = 0; i < step && x + i < m_screenWidth; i++) {
                                SDL_RenderDrawPoint(m_renderer, x + i, ceilingY);
                                
                                // Fill in skipped rows for smoother appearance
                                for (int j = 1; j < rowSkip && ceilingY - j >= 0; j++) {
                                    SDL_RenderDrawPoint(m_renderer, x + i, ceilingY - j);
                                }
                            }
                        }
                    }
                    
                    // Move to next position
                    floorX += floorStepX * step;
                    floorY += floorStepY * step;
                }
            }
        }
    }
}

void Renderer::renderSprites(const Map& map, const Player& player) {
    if (!m_spriteManager || !m_textureManager) {
        std::cerr << "ERROR: SpriteManager or TextureManager is null in renderSprites!" << std::endl;
        return;
    }
    
    // Get player position and direction
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    
    // Use the combined vertical offset
    double totalVerticalOffset = player.getVerticalOffset();
    int verticalOffsetPixels = static_cast<int>(totalVerticalOffset * m_screenHeight / 2);
    
    // Get all active sprites
    std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
    if (sprites.empty()) {
        // Debug message only if we expect sprites
        std::cout << "No active sprites to render" << std::endl;
        return;
    }
    
    std::cout << "Rendering " << sprites.size() << " active sprites" << std::endl;
    
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
    
    std::cout << "Sprite types: " << impCount << " imps, " << enemyCount << " enemies, " 
              << itemCount << " items, " << otherCount << " other" << std::endl;
    
    // Sort sprites by distance (furthest first for proper alpha blending)
    std::sort(sprites.begin(), sprites.end(), [&pos](const Sprite* a, const Sprite* b) {
        double distA = (a->getPosition() - pos).lengthSquared();
        double distB = (b->getPosition() - pos).lengthSquared();
        return distA > distB;  // Sort in descending order (furthest first)
    });
    
    // For each sprite
    int renderedCount = 0;
    for (const Sprite* sprite : sprites) {
        if (!sprite->isVisible() || !sprite->isActive()) {
            continue;
        }
        
        // Skip sprites in invisible sectors (sector culling)
        int spriteSectorId = map.getSectorAt(sprite->getPosition().x, sprite->getPosition().y);
        if (spriteSectorId >= 0 && !map.isSectorVisible(spriteSectorId)) {
            continue;
        }
        
        // Translate sprite position relative to player
        Vec2 spritePos = sprite->getPosition() - pos;
        
        // Transform sprite with the inverse camera matrix
        double invDet = 1.0 / (plane.x * dir.y - dir.x * plane.y);
        double transformX = invDet * (dir.y * spritePos.x - dir.x * spritePos.y);
        double transformY = invDet * (-plane.y * spritePos.x + plane.x * spritePos.y);
        
        // Skip if behind player or too far
        if (transformY <= 0.1 || transformY > 20.0) {
            std::cout << "  Skipping sprite at position (" << sprite->getPosition().x << ", " << sprite->getPosition().y << ")" << std::endl;
            continue;
        }
        
        // Calculate screen position
        int spriteScreenX = static_cast<int>((m_screenWidth / 2) * (1.0 + transformX / transformY));
        
        // Calculate sprite size on screen
        int spriteSize = static_cast<int>(m_screenHeight / transformY);
        spriteSize = std::min(std::max(spriteSize, 4), 20);  // Clamp size between 4 and 20 pixels
        
        // Calculate drawing boundaries
        int drawStartX = spriteScreenX - spriteSize / 2;
        int drawEndX = spriteScreenX + spriteSize / 2;
        int drawStartY = m_screenHeight / 2 - spriteSize / 2;
        int drawEndY = m_screenHeight / 2 + spriteSize / 2;
        
        // Get the texture for this sprite
        int textureId = sprite->getTextureId();
        std::cout << "  Sprite texture ID: " << textureId << std::endl;
        
        const Texture* texture = m_textureManager->getTexture(textureId);
        if (!texture) {
            std::cerr << "ERROR: Invalid texture ID " << textureId << " for sprite!" << std::endl;
            continue;
        }
        
        // Draw the sprite
        SDL_Texture* sdlTexture = texture->getSDLTexture();
        if (!sdlTexture) {
            std::cerr << "ERROR: Null SDL_Texture for texture ID " << textureId << "!" << std::endl;
            continue;
        }
        
        // Ensure texture blend mode is set to BLEND for proper transparency
        SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_BLEND);
        
        // Set up source and destination rectangles
        SDL_Rect srcRect = {0, 0, texture->getWidth(), texture->getHeight()};
        
        // For animated sprites, use the current frame
        if (sprite->getCurrentFrame() > 0) {
            srcRect.x = sprite->getCurrentFrame() * texture->getWidth();
        }
        
        SDL_Rect dstRect = {drawStartX, drawStartY, drawEndX - drawStartX, drawEndY - drawStartY};
        
        // Render the sprite
        if (SDL_RenderCopy(m_renderer, sdlTexture, &srcRect, &dstRect) != 0) {
            std::cerr << "ERROR: Failed to render sprite: " << SDL_GetError() << std::endl;
        } else {
            renderedCount++;
        }
    }
    
    std::cout << "Successfully rendered " << renderedCount << " sprites" << std::endl;
}

void Renderer::renderMinimap(const Map& map, const Player& player, ProjectileManager& projectileManager) {
    // Get player position
    double playerX = player.getPosition().x;
    double playerY = player.getPosition().y;
    
    // Determine player's current elevation level
    int playerElevation = map.getCellElevation(static_cast<int>(playerX), static_cast<int>(playerY));
    
    // Set minimap size and position
    int mapSize = 200; // Increased size for better visibility
    int padding = 10;
    int mapX = padding;
    int mapY = padding;
    
    // Draw minimap background
    SDL_Rect mapRect = {mapX, mapY, mapSize, mapSize};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200); // Dark background
    SDL_RenderFillRect(m_renderer, &mapRect);
    
    // Draw minimap border
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255); // White border
    SDL_RenderDrawRect(m_renderer, &mapRect);
    
    // Draw minimap title based on current level
    std::string levelTitle = (playerElevation == 0) ? "Ground Level" : "Level 2";
    renderText(levelTitle, mapX + 5, mapY - 20, {255, 255, 255, 255});
    
    // Calculate cell size based on map dimensions
    int cellSize = mapSize / std::max(map.getWidth(), map.getHeight());
    
    // First pass: Draw base cells
    for (int x = 0; x < map.getWidth(); x++) {
        for (int y = 0; y < map.getHeight(); y++) {
            CellType cellType = map.getCell(x, y);
            int cellElevation = map.getCellElevation(x, y);
            
            // Only draw cells on the current elevation level (except for stairs)
            if (cellElevation != playerElevation && 
                !map.isStairs(x, y) && !map.isStairStep(x, y)) {
                continue;
            }
            
            // Calculate cell position on minimap
            SDL_Rect cellRect = {
                mapX + x * cellSize,
                mapY + y * cellSize,
                cellSize,
                cellSize
            };
            
            // Choose color based on cell type and elevation
            int r = 0, g = 0, b = 0;
            
            switch (cellType) {
                case CellType::Wall:
                case CellType::ElevatedWall:
                    // Different colors for walls based on elevation
                    if (cellElevation == 0) {
                        r = 100; g = 100; b = 100; // Dark gray for ground level walls
                    } else {
                        r = 150; g = 150; b = 200; // Light blue for second level walls
                    }
                    break;
                    
                case CellType::Floor:
                case CellType::ElevatedFloor:
                    // Different colors for floors based on elevation
                    if (cellElevation == 0) {
                        r = 50; g = 50; b = 50; // Dark gray for ground level
                    } else {
                        r = 80; g = 100; b = 150; // Light blue for second level
                    }
                    break;
                    
                case CellType::Door:
                    r = 150; g = 75; b = 0; // Brown for doors
                    break;
                    
                case CellType::Item:
                    r = 0; g = 255; b = 0; // Green for items
                    break;
                    
                case CellType::Enemy:
                    r = 255; g = 0; b = 0; // Red for enemies
                    break;
                    
                case CellType::Stairs:
                    r = 255; g = 200; b = 0; // Bright yellow-orange for stair entry/exit
                    break;
                    
                case CellType::StairStep1:
                case CellType::StairStep2:
                case CellType::StairStep3:
                    // Blue-white gradient for stair steps
                    int stepIntensity = 180;
                    
                    switch (cellType) {
                        case CellType::StairStep1:
                            stepIntensity = 180;
                            break;
                        case CellType::StairStep2:
                            stepIntensity = 210;
                            break;
                        case CellType::StairStep3:
                            stepIntensity = 240;
                            break;
                        default:
                            break;
                    }
                    
                    r = stepIntensity;
                    g = stepIntensity;
                    b = 255;
                    break;
            }
            
            // Draw the cell
            SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
            SDL_RenderFillRect(m_renderer, &cellRect);
        }
    }
    
    // Second pass: Add stair direction indicators
    for (int x = 0; x < map.getWidth(); x++) {
        for (int y = 0; y < map.getHeight(); y++) {
            if (map.isStairs(x, y)) {
                // Only show stairs on or connected to the current level
                int stairElevation = map.getCellElevation(x, y);
                bool isConnectedToCurrentLevel = false;
                
                // Check neighboring cells to see if this stair connects to current level
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < map.getWidth() && ny >= 0 && ny < map.getHeight()) {
                            if (map.getCellElevation(nx, ny) == playerElevation) {
                                isConnectedToCurrentLevel = true;
                                break;
                            }
                        }
                    }
                    if (isConnectedToCurrentLevel) break;
                }
                
                if (stairElevation == playerElevation || isConnectedToCurrentLevel) {
                    // Calculate cell position on minimap
                    SDL_Rect cellRect = {
                        mapX + x * cellSize,
                        mapY + y * cellSize,
                        cellSize,
                        cellSize
                    };
                    
                    // Determine stair direction by checking neighboring cells
                    int direction = -1; // -1 = unknown, 0 = N, 1 = E, 2 = S, 3 = W
                    
                    // Check neighboring cells to determine stair direction
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dy = -1; dy <= 1; dy++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < map.getWidth() && ny >= 0 && ny < map.getHeight()) {
                                if (map.isStairStep(nx, ny)) {
                                    // Found a stair step, determine direction
                                    if (dy < 0) direction = 0; // North
                                    else if (dx > 0) direction = 1; // East
                                    else if (dy > 0) direction = 2; // South
                                    else if (dx < 0) direction = 3; // West
                                    break;
                                }
                            }
                        }
                        if (direction >= 0) break;
                    }
                    
                    // Draw direction indicator if found
                    if (direction >= 0) {
                        // Set color for direction indicator
                        SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
                        
                        // Draw arrow
                        DrawArrow(cellRect, direction);
                        
                        // Add up/down marker to indicate elevation change
                        int targetElevation = -1;
                        
                        // Check neighboring cells to determine target elevation
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dy = -1; dy <= 1; dy++) {
                                if (dx == 0 && dy == 0) continue;
                                
                                int nx = x + dx;
                                int ny = y + dy;
                                
                                if (nx >= 0 && nx < map.getWidth() && ny >= 0 && ny < map.getHeight()) {
                                    int neighborElevation = map.getCellElevation(nx, ny);
                                    if (neighborElevation != stairElevation) {
                                        targetElevation = neighborElevation;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Draw up/down marker
                        if (targetElevation > stairElevation) {
                            // Up marker
                            renderText("↑", mapX + x * cellSize + cellSize/4, mapY + y * cellSize, {255, 255, 0, 255});
                        } else if (targetElevation < stairElevation) {
                            // Down marker
                            renderText("↓", mapX + x * cellSize + cellSize/4, mapY + y * cellSize, {255, 255, 0, 255});
                        }
                    }
                }
            }
        }
    }
    
    // Draw player position and direction
    int playerMapX = mapX + static_cast<int>(playerX * cellSize);
    int playerMapY = mapY + static_cast<int>(playerY * cellSize);
    
    // Draw player direction line
    int dirLineLength = cellSize * 2;
    int playerDirEndX = playerMapX + static_cast<int>(player.getDirection().x * dirLineLength);
    int playerDirEndY = playerMapY + static_cast<int>(player.getDirection().y * dirLineLength);
    
    SDL_SetRenderDrawColor(m_renderer, 0, 255, 255, 255); // Cyan for player direction
    SDL_RenderDrawLine(m_renderer, playerMapX, playerMapY, playerDirEndX, playerDirEndY);
    
    // Draw player position
    SDL_Rect playerRect = {
        playerMapX - cellSize/4,
        playerMapY - cellSize/4,
        cellSize/2,
        cellSize/2
    };
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255); // Yellow for player
    SDL_RenderFillRect(m_renderer, &playerRect);
    
    // Draw projectiles
    const std::vector<Projectile*>& projectiles = projectileManager.getActiveProjectiles();
    for (const auto& projectile : projectiles) {
        // Only show projectiles on the current level
        int projectileX = static_cast<int>(projectile->getPosition().x);
        int projectileY = static_cast<int>(projectile->getPosition().y);
        int projectileElevation = map.getCellElevation(projectileX, projectileY);
        
        if (projectileElevation == playerElevation) {
            int projMapX = mapX + static_cast<int>(projectile->getPosition().x * cellSize);
            int projMapY = mapY + static_cast<int>(projectile->getPosition().y * cellSize);
            
            SDL_Rect projRect = {
                projMapX - cellSize/8,
                projMapY - cellSize/8,
                cellSize/4,
                cellSize/4
            };
            
            // Different colors for different projectile types
            switch (projectile->getType()) {
                case ProjectileType::Bullet:
                    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255); // White for bullets
                    break;
                case ProjectileType::Rocket:
                    SDL_SetRenderDrawColor(m_renderer, 255, 100, 0, 255); // Orange for rockets
                    break;
                case ProjectileType::Plasma:
                    SDL_SetRenderDrawColor(m_renderer, 0, 255, 255, 255); // Cyan for plasma
                    break;
            }
            
            SDL_RenderFillRect(m_renderer, &projRect);
        }
    }
}

void Renderer::renderHUD(const Player& player) {
    // Draw health bar
    int healthBarWidth = 200;
    int healthBarHeight = 20;
    int healthBarX = 10;
    int healthBarY = m_screenHeight - healthBarHeight - 10;
    
    // Background
    SDL_Rect healthBarBg = { healthBarX, healthBarY, healthBarWidth, healthBarHeight };
    SDL_SetRenderDrawColor(m_renderer, 64, 64, 64, 255);
    SDL_RenderFillRect(m_renderer, &healthBarBg);
    
    // Health level
    int healthWidth = static_cast<int>(player.getHealth() / 100.0 * healthBarWidth);
    SDL_Rect healthBar = { healthBarX, healthBarY, healthWidth, healthBarHeight };
    
    // Color based on health level
    if (player.getHealth() > 70) {
        SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);
    } else if (player.getHealth() > 30) {
        SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
    }
    
    SDL_RenderFillRect(m_renderer, &healthBar);
    
    // Border
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_renderer, &healthBarBg);
    
    // Health text
    std::stringstream ss;
    ss << "Health: " << static_cast<int>(player.getHealth());
    renderText(ss.str(), healthBarX + 10, healthBarY + 3, Color::White());
    
    // Ammo counter
    ss.str("");
    ss << "Ammo: " << player.getAmmo();
    renderText(ss.str(), healthBarX + healthBarWidth + 20, healthBarY + 3, Color::White());
}

void Renderer::renderWeapon(const Player& player, double recoil, double flashIntensity, int weaponTextureId) {
    // Check if we have a texture manager
    if (!m_textureManager) {
        std::cout << "No texture manager available for weapon rendering" << std::endl;
        return;
    }
        
    // Get the weapon texture using the passed texture ID
    const Texture* weaponTexture = m_textureManager->getTexture(weaponTextureId);
    
    // If we couldn't find the texture, fall back to the simple rectangle method
    if (!weaponTexture) {
        std::cout << "No weapon texture found (ID " << weaponTextureId << "), using fallback rectangle" << std::endl;
        // Simple weapon rendering - just a placeholder
        int weaponWidth = 100;
        int weaponHeight = 150;
        int weaponX = (m_screenWidth - weaponWidth) / 2;
        int weaponY = m_screenHeight - weaponHeight;
        
        // Apply recoil effect
        int recoilY = static_cast<int>(recoil * 20);  // Scale recoil to pixels
        
        SDL_Rect weaponRect = { 
            weaponX, 
            weaponY + recoilY, 
            weaponWidth, 
            weaponHeight 
        };
        
        // Draw a simple gun shape
        SDL_SetRenderDrawColor(m_renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(m_renderer, &weaponRect);
        
        // Gun barrel
        SDL_Rect barrelRect = { 
            weaponX + weaponWidth / 2 - 10, 
            weaponY + recoilY - 20, 
            20, 
            50 
        };
        
        SDL_SetRenderDrawColor(m_renderer, 80, 80, 80, 255);
        SDL_RenderFillRect(m_renderer, &barrelRect);
        return;
    }
        
    // Calculate the weapon size (maintain aspect ratio)
    float aspectRatio = static_cast<float>(weaponTexture->getWidth()) / weaponTexture->getHeight();
    int weaponHeight = m_screenHeight / 2;  // Take up half the screen height
    int weaponWidth = static_cast<int>(weaponHeight * aspectRatio);
    
    // Center the weapon at the bottom of the screen
    int weaponX = (m_screenWidth - weaponWidth) / 2;
    
    // Special adjustment for the rocket launcher
    // Move it right by 15% of its width to properly center it
    weaponX += static_cast<int>(weaponWidth * 0.15f); 
    
    // Position weapon at bottom of screen
    int weaponY = m_screenHeight - weaponHeight;
    
    // Add debug output to check current texture
    std::cout << "Rendering weapon with textureID: " << weaponTextureId 
              << ", aspect ratio: " << aspectRatio << std::endl;
    
    // Specific vertical adjustment for rocket launcher
    bool isRocketLauncher = false;
    
    // Check if this is the rocket launcher by texture ID
    // Updated to include textureID 17 which is the actual rocket launcher ID
    if (weaponTextureId == 17 || (weaponTextureId >= 7 && weaponTextureId <= 10)) {
        isRocketLauncher = true;
        std::cout << "ROCKET LAUNCHER DETECTED - applying vertical adjustment" << std::endl;
    }
    
    // Alternative detection method using aspect ratio as a backup
    // Updated to include the aspect ratio around 1.34 which is the rocket launcher's ratio
    if (!isRocketLauncher && (aspectRatio < 0.9f || (aspectRatio > 1.3f && aspectRatio < 1.4f) || aspectRatio > 1.5f)) {
        isRocketLauncher = true;
        std::cout << "ROCKET LAUNCHER DETECTED by aspect ratio - applying vertical adjustment" << std::endl;
    }
    
    // Apply a much more significant adjustment
    if (isRocketLauncher) {
        // Move the rocket launcher much lower - 40% further down for better positioning
        int rocketAdjustment = static_cast<int>(weaponHeight * 0.40f);
        weaponY += rocketAdjustment;
        std::cout << "Adjusted rocket Y position by " << rocketAdjustment << " pixels" << std::endl;
    }
    
    // Apply recoil effect
    int recoilY = static_cast<int>(recoil * 20);  // Scale recoil to pixels
    
    // Create a destination rectangle for the weapon
    SDL_Rect destRect = {
        weaponX,
        weaponY + recoilY,
        weaponWidth,
        weaponHeight
    };
    
    // Get the SDL texture from the Texture object
    SDL_Texture* sdlTexture = weaponTexture->getSDLTexture();
    if (sdlTexture) {
        
        // CRITICAL: Save the entire renderer state
        SDL_Renderer* renderer = m_renderer;
        
        // Save blend mode
        SDL_BlendMode oldBlendMode;
        SDL_GetTextureBlendMode(sdlTexture, &oldBlendMode);
        
        // Force alpha blending for the weapon
        SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_BLEND);
        
        // Set the alpha value to fully opaque
        SDL_SetTextureAlphaMod(sdlTexture, 255);
        
        // Save current target
        SDL_Texture* currentTarget = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, NULL);  // Ensure we're rendering to the default target
        
        // CRITICAL: Use this more reliable method to render the texture
        // This ensures it's rendered at the correct size and position
        SDL_RenderCopyEx(
            renderer,          // Renderer
            sdlTexture,        // Texture
            NULL,              // Source rectangle (NULL = entire texture)
            &destRect,         // Destination rectangle
            0.0,               // Angle (no rotation)
            NULL,              // Center of rotation (NULL = center of dest rect)
            SDL_FLIP_NONE      // No flipping
        );
        
        // Restore renderer state
        SDL_SetRenderTarget(renderer, currentTarget);
        SDL_SetTextureBlendMode(sdlTexture, oldBlendMode);
    } else {
        std::cout << "Failed to get SDL_Texture from weapon texture" << std::endl;
    }
    
    // Render muzzle flash if needed
    if (flashIntensity > 0.0 && m_muzzleFlashEnabled) {
        renderMuzzleFlash(flashIntensity, recoil);
    }
}

void Renderer::renderText(const std::string& text, int x, int y, const Color& color) {
    // This is a simplified text rendering function
    // In a real game, you would use SDL_ttf to render text properly
    
    // For now, we'll just draw a placeholder
    SDL_Rect textRect = { x, y, static_cast<int>(text.length() * 8), 16 };
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(m_renderer, &textRect);
}

void Renderer::renderProjectiles(const Player& player) {
    if (!m_projectileManager) {
        std::cerr << "No projectile manager in renderProjectiles!" << std::endl;
        return;
    }
    
    if (!m_textureManager) {
        std::cerr << "No texture manager in renderProjectiles!" << std::endl;
        return;
    }
    
    // Get player position and camera plane
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    
    // Get active projectiles
    std::vector<Projectile*> projectiles = m_projectileManager->getActiveProjectiles();
    
    // DEBUG: Output total active projectiles
    std::cout << "Rendering projectiles: " << projectiles.size() << " active projectiles" << std::endl;
    
    // Render each projectile
    for (const Projectile* projectile : projectiles) {
        if (!projectile) {
            std::cerr << "Null projectile in active projectiles list!" << std::endl;
            continue;
        }
        
        // DEBUG: Output projectile type
        std::string typeStr = "Unknown";
        switch (projectile->getType()) {
            case ProjectileType::Bullet: typeStr = "Bullet"; break;
            case ProjectileType::Rocket: typeStr = "Rocket"; break;
            case ProjectileType::Plasma: typeStr = "Plasma"; break;
            case ProjectileType::Grenade: typeStr = "Grenade"; break;
            case ProjectileType::BFG: typeStr = "BFG"; break;
        }
        std::cout << "Processing " << typeStr << " projectile (ID: " << projectile->getId() 
                  << ") at position (" << projectile->getPosition().x << ", " 
                  << projectile->getPosition().y << ")" << std::endl;
        
        // Calculate projectile position relative to player
        double projX = projectile->getPosition().x - pos.x;
        double projY = projectile->getPosition().y - pos.y;
        
        // Transform with the inverse camera matrix
        double invDet = 1.0 / (plane.x * dir.y - dir.x * plane.y);
        double transformX = invDet * (dir.y * projX - dir.x * projY);
        double transformY = invDet * (-plane.y * projX + plane.x * projY);
        
        // Skip if behind player or too far
        if (transformY <= 0.1 || transformY > 20.0) {
            std::cout << "  Skipping " << typeStr << " - behind player or too far (transformY: " << transformY << ")" << std::endl;
            continue;
        }
        
        // Calculate screen position
        int screenX = static_cast<int>((m_screenWidth / 2) * (1 + transformX / transformY));
        
        // Check if projectile is occluded by walls using the zBuffer
        bool isVisible = false;
        // Check if the projectile's center is visible
        if (screenX >= 0 && screenX < m_screenWidth) {
            // If the projectile's distance is less than the wall distance at this x-coordinate, it's visible
            if (transformY < m_zBuffer[screenX]) {
                isVisible = true;
                std::cout << "  " << typeStr << " is visible (distance: " << transformY 
                          << ", zBuffer: " << m_zBuffer[screenX] << ")" << std::endl;
            } else {
                std::cout << "  " << typeStr << " is occluded by wall (distance: " << transformY 
                          << ", zBuffer: " << m_zBuffer[screenX] << ")" << std::endl;
            }
        } else {
            std::cout << "  " << typeStr << " is off-screen (screenX: " << screenX << ")" << std::endl;
        }
        
        // Skip if not visible
        if (!isVisible) {
            continue;
        }
        
        // Calculate bullet size based on distance
        int size = static_cast<int>(m_screenHeight / transformY * 0.05); // Make bullets smaller but still visible
        size = std::max(4, std::min(size, 20)); // Clamp size between 4 and 20 pixels
        
        // Calculate drawing boundaries
        int drawStartY = -size / 2 + m_screenHeight / 2;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = size / 2 + m_screenHeight / 2;
        if (drawEndY >= m_screenHeight) drawEndY = m_screenHeight - 1;
        
        int drawStartX = -size / 2 + screenX;
        if (drawStartX < 0) drawStartX = 0;
        int drawEndX = size / 2 + screenX;
        if (drawEndX >= m_screenWidth) drawEndX = m_screenWidth - 1;
        
        // Try to get the bullet texture based on projectile type
        int textureId = -1;
        switch (projectile->getType()) {
            case ProjectileType::Bullet:
                textureId = m_projectileManager->getBulletTextureId();
                break;
            case ProjectileType::Rocket:
                textureId = m_projectileManager->getRocketTextureId();
                std::cout << "  Using rocket texture from manager: " << textureId << std::endl;
                break;
            case ProjectileType::Plasma:
                textureId = m_projectileManager->getPlasmaTextureId();
                break;
        }
        
        const Texture* bulletTexture = nullptr;
        if (textureId >= 0) {
            bulletTexture = m_textureManager->getTexture(textureId);
        }
        
        // If we have a valid texture, use it
        if (bulletTexture) {
            // Calculate the center of the bullet on screen
            int centerX = (drawStartX + drawEndX) / 2;
            int centerY = (drawStartY + drawEndY) / 2;
            
            // Calculate the size of the bullet
            int bulletWidth = drawEndX - drawStartX;
            int bulletHeight = drawEndY - drawStartY;
            
            // Apply rotation based on bullet direction
            double angle = atan2(projectile->getDirection().y, projectile->getDirection().x) * 180.0 / M_PI;
            
            // Create a rotation effect based on lifetime for spinning bullets
            double spinSpeed = 720.0; // degrees per second
            angle += projectile->getLifetime() * spinSpeed;
            
            // Apply slight trajectory-based scaling for a motion blur effect
            double speedScale = 1.0 + std::min(0.3, projectile->getLifetime() * 0.5);
            
            // Create a destination rectangle
            SDL_Rect destRect = {
                centerX - static_cast<int>(bulletWidth * speedScale / 2),
                centerY - static_cast<int>(bulletHeight * speedScale / 2),
                static_cast<int>(bulletWidth * speedScale),
                static_cast<int>(bulletHeight * speedScale)
            };
            
            // Get the SDL texture
            SDL_Texture* sdlTexture = bulletTexture->getSDLTexture();
            
            if (sdlTexture) {
                // Set the blend mode to allow transparency
                SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_BLEND);
                
                // Draw the rotated texture
                SDL_RenderCopyEx(
                    m_renderer,               // Renderer
                    sdlTexture,               // Texture
                    NULL,                    // Use the entire source texture
                    &destRect,               // Destination on screen
                    angle,                   // Rotation angle in degrees
                    NULL,                    // Rotate around center
                    SDL_FLIP_NONE            // No flipping
                );
                
                // Add a bright outline for better visibility (debug)
                SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255); // Bright red outline
                SDL_RenderDrawRect(m_renderer, &destRect);
                
                // Special handling for rocket projectiles
                if (projectile->getType() == ProjectileType::Rocket) {
                    // Draw a larger, more noticeable outline for rockets
                    SDL_Rect rocketOutline = { 
                        destRect.x - 2, 
                        destRect.y - 2, 
                        destRect.w + 4, 
                        destRect.h + 4 
                    };
                    SDL_SetRenderDrawColor(m_renderer, 255, 165, 0, 255); // Orange outline
                    SDL_RenderDrawRect(m_renderer, &rocketOutline);
                    
                    // Add a second outline
                    SDL_Rect rocketOutline2 = { 
                        destRect.x - 4, 
                        destRect.y - 4, 
                        destRect.w + 8, 
                        destRect.h + 8 
                    };
                    SDL_SetRenderDrawColor(m_renderer, 255, 215, 0, 255); // Gold outline
                    SDL_RenderDrawRect(m_renderer, &rocketOutline2);
                    
                    // Draw a trail behind the rocket
                    int trailLength = 4;
                    for (int i = 1; i <= trailLength; i++) {
                        double trailScale = 0.8 - (i * 0.15); // Gradually smaller
                        SDL_Rect trailRect = {
                            destRect.x - static_cast<int>(projectile->getDirection().x * i * 10),
                            destRect.y - static_cast<int>(projectile->getDirection().y * i * 10),
                            static_cast<int>(destRect.w * trailScale),
                            static_cast<int>(destRect.h * trailScale)
                        };
                        // Gradient from orange to red to fade
                        int alpha = 255 - (i * 50);
                        if (alpha < 0) alpha = 0;
                        SDL_SetRenderDrawColor(m_renderer, 255, 100 - (i * 20), 0, alpha);
                        SDL_RenderDrawRect(m_renderer, &trailRect);
                    }
                }
                
                // Draw a second outline for extra visibility
                SDL_Rect outerRect = { 
                    destRect.x - 1, 
                    destRect.y - 1, 
                    destRect.w + 2, 
                    destRect.h + 2 
                };
                SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255); // Yellow outer outline
                SDL_RenderDrawRect(m_renderer, &outerRect);
                
                // Add a small glow effect
                if (m_performanceLevel != PerformanceLevel::Low) {
                    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 64);
                    SDL_Rect glowRect = { 
                        destRect.x - 2, 
                        destRect.y - 2, 
                        destRect.w + 4, 
                        destRect.h + 4 
                    };
                    SDL_RenderDrawRect(m_renderer, &glowRect);
                }
            }
        }
        // Fallback to procedural drawing if texture not available
        else {
            // Draw a bright bullet sprite (procedural fallback)
            for (int x = drawStartX; x < drawEndX; x++) {
                for (int y = drawStartY; y < drawEndY; y++) {
                    // Calculate distance from center (squared)
                    int centerX = (drawStartX + drawEndX) / 2;
                    int centerY = (drawStartY + drawEndY) / 2;
                    double distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                    
                    // Only draw if within a circle
                    if (distance <= size / 2) {
                        Color color;
                        
                        // Gradient based on distance from center
                        double gradient = 1.0 - (distance / (size / 2));
                        gradient = pow(gradient, 0.5); // Make the gradient more pronounced
                        
                        // Different colors based on projectile type
                        switch (projectile->getType()) {
                            case ProjectileType::Bullet:
                                // Bright yellow-orange bullet with white core
                                color = Color(
                                    255,                          // Red
                                    255 * gradient,               // Green
                                    gradient > 0.8 ? 255 : 0,    // Blue (white core)
                                    255                          // Alpha
                                );
                                break;
                            case ProjectileType::Rocket:
                                // Red-orange rocket with bright core
                                color = Color(
                                    255,                          // Red
                                    100 * gradient,               // Green
                                    gradient > 0.9 ? 200 : 0,    // Blue
                                    255                          // Alpha
                                );
                                break;
                            case ProjectileType::Plasma:
                                // Blue-green plasma with bright core
                                color = Color(
                                    gradient > 0.8 ? 200 : 0,    // Red
                                    200 * gradient,               // Green
                                    255,                         // Blue
                                    255                          // Alpha
                                );
                                break;
                            default:
                                color = Color(255, 255, 255, 255);
                                break;
                        }
                        
                        // Draw the pixel
                        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
                        SDL_RenderDrawPoint(m_renderer, x, y);
                    }
                }
            }
            
            // Draw an outer glow
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 64);
            SDL_Rect glowRect = { 
                (drawStartX + drawEndX) / 2 - size / 2, 
                (drawStartY + drawEndY) / 2 - size / 2, 
                size, 
                size 
            };
            SDL_RenderDrawRect(m_renderer, &glowRect);
        }
    }
}

void Renderer::renderMuzzleFlash(double intensity, double recoil) {
    // Skip if intensity is too low or muzzle flash is disabled
    if (intensity <= 0.01 || !m_muzzleFlashEnabled) return;
    
    // Calculate flash size based on intensity
    int flashSize = static_cast<int>(30 * intensity);
    if (flashSize <= 0) return;
    
    // Check if we have a texture manager to get the weapon texture
    bool useTexturedWeapon = false;
    const Texture* weaponTexture = nullptr;
    
    if (m_textureManager) {
        // Look for the weapon texture
        for (int i = 0; i < 10; i++) {
            const Texture* texture = m_textureManager->getTexture(i);
            if (texture && texture->getWidth() > 100) { // Assume weapon texture is larger
                weaponTexture = texture;
                useTexturedWeapon = true;
                break;
            }
        }
    }
    
    // Position at the end of the gun barrel
    int weaponWidth = 100;
    int weaponHeight = 150;
    int weaponX = (m_screenWidth - weaponWidth) / 2;
    int weaponY = m_screenHeight - weaponHeight;
    
    // For the textured weapon, we need to adjust the muzzle position
    if (useTexturedWeapon && weaponTexture) {
        // Calculate the weapon size (maintain aspect ratio)
        float aspectRatio = static_cast<float>(weaponTexture->getWidth()) / weaponTexture->getHeight();
        weaponHeight = m_screenHeight / 2;  // Take up half the screen height
        weaponWidth = static_cast<int>(weaponHeight * aspectRatio);
        
        // Center the weapon at the bottom of the screen
        weaponX = (m_screenWidth - weaponWidth) / 2;
        
        // Special adjustment for the rocket launcher
        // Move it right by 15% of its width to properly center it
        weaponX += static_cast<int>(weaponWidth * 0.15f); 
        
        weaponY = m_screenHeight - weaponHeight;
        
        // Determine if this is the rocket launcher based on aspect ratio
        bool isRocketLauncher = false;
        if (aspectRatio < 0.9f || (aspectRatio > 1.3f && aspectRatio < 1.4f) || aspectRatio > 1.5f) {
            isRocketLauncher = true;
            std::cout << "ROCKET LAUNCHER DETECTED in muzzle flash - applying adjustment" << std::endl;
        }
        
        // Apply the same vertical adjustment as in renderWeapon
        if (isRocketLauncher) {
            // Move the rocket launcher much lower - 40% further down
            int rocketAdjustment = static_cast<int>(weaponHeight * 0.40f);
            weaponY += rocketAdjustment;
            std::cout << "Adjusted muzzle flash Y position by " << rocketAdjustment << " pixels" << std::endl;
        }
        
        // Apply recoil effect for flash positioning
        int recoilY = static_cast<int>(recoil * 20);  // Scale recoil to pixels
        
        // Position the flash at the end of the barrel - adjusted for the weapon type
        // For rocket launcher, position it near the top of the weapon
        float flashXRatio, flashYRatio;
        
        if (isRocketLauncher) {
            // Position the flash at the rocket launcher barrel
            flashXRatio = 0.7f;  // 70% from the left edge
            flashYRatio = 0.18f; // Adjusted to be higher up on the rocket launcher (was 0.25f)
        } else {
            // Default positioning for other weapons
            flashXRatio = 0.8f;  // 80% from the left edge
            flashYRatio = 0.3f;  // 30% from the top
        }
        
        int flashX = weaponX + static_cast<int>(weaponWidth * flashXRatio);
        int flashY = weaponY + static_cast<int>(weaponHeight * flashYRatio) + recoilY;
        
        // Draw the flash as a yellow/orange circle
        for (int y = -flashSize; y <= flashSize; y++) {
            for (int x = -flashSize; x <= flashSize; x++) {
                // Calculate distance from center (squared)
                double distSq = (x * x + y * y) / static_cast<double>(flashSize * flashSize);
                
                // Skip if outside the circle
                if (distSq > 1.0) continue;
                
                // Calculate alpha based on distance from center
                int alpha = static_cast<int>(255 * (1.0 - distSq) * intensity);
                if (alpha <= 0) continue;
                
                // Calculate screen coordinates
                int screenX = flashX + x;
                int screenY = flashY + y;
                
                // Skip if off-screen
                if (screenX < 0 || screenX >= m_screenWidth || screenY < 0 || screenY >= m_screenHeight) continue;
                
                // Draw the pixel with a yellow-green color for plasma
                SDL_SetRenderDrawColor(m_renderer, 100, 255, 50, alpha);
                SDL_RenderDrawPoint(m_renderer, screenX, screenY);
            }
        }
    } else {
        // Use the original calculation for simple rectangle weapon
        int flashX = weaponX + weaponWidth / 2 - flashSize / 2;
        int flashY = weaponY - 40 - flashSize / 2;
        
        // Draw the flash as a yellow/orange circle
        for (int y = -flashSize; y <= flashSize; y++) {
            for (int x = -flashSize; x <= flashSize; x++) {
                // Calculate distance from center (squared)
                double distSq = (x * x + y * y) / static_cast<double>(flashSize * flashSize);
                
                // Skip if outside the circle
                if (distSq > 1.0) continue;
                
                // Calculate alpha based on distance from center
                int alpha = static_cast<int>(255 * (1.0 - distSq) * intensity);
                if (alpha <= 0) continue;
                
                // Calculate screen coordinates
                int screenX = flashX + flashSize + x;
                int screenY = flashY + flashSize + y;
                
                // Skip if off-screen
                if (screenX < 0 || screenX >= m_screenWidth || screenY < 0 || screenY >= m_screenHeight) continue;
                
                // Draw the pixel with a yellow-orange color
                SDL_SetRenderDrawColor(m_renderer, 255, 200, 50, alpha);
                SDL_RenderDrawPoint(m_renderer, screenX, screenY);
            }
        }
    }
}

void Renderer::DrawArrow(const SDL_Rect& rect, int direction) {
    // Calculate center and size
    int centerX = rect.x + rect.w / 2;
    int centerY = rect.y + rect.h / 2;
    int size = std::min(rect.w, rect.h) / 2;
    
    // Define arrow points based on direction
    // 0=North, 1=East, 2=South, 3=West
    switch (direction) {
        case 0: // North
            SDL_RenderDrawLine(m_renderer, centerX, centerY - size, centerX - size/2, centerY + size/2);
            SDL_RenderDrawLine(m_renderer, centerX, centerY - size, centerX + size/2, centerY + size/2);
            SDL_RenderDrawLine(m_renderer, centerX, centerY - size, centerX, centerY + size/2);
            break;
        case 1: // East
            SDL_RenderDrawLine(m_renderer, centerX + size, centerY, centerX - size/2, centerY - size/2);
            SDL_RenderDrawLine(m_renderer, centerX + size, centerY, centerX - size/2, centerY + size/2);
            SDL_RenderDrawLine(m_renderer, centerX + size, centerY, centerX - size/2, centerY);
            break;
        case 2: // South
            SDL_RenderDrawLine(m_renderer, centerX, centerY + size, centerX - size/2, centerY - size/2);
            SDL_RenderDrawLine(m_renderer, centerX, centerY + size, centerX + size/2, centerY - size/2);
            SDL_RenderDrawLine(m_renderer, centerX, centerY + size, centerX, centerY - size/2);
            break;
        case 3: // West
            SDL_RenderDrawLine(m_renderer, centerX - size, centerY, centerX + size/2, centerY - size/2);
            SDL_RenderDrawLine(m_renderer, centerX - size, centerY, centerX + size/2, centerY + size/2);
            SDL_RenderDrawLine(m_renderer, centerX - size, centerY, centerX + size/2, centerY);
            break;
    }
}

void Renderer::renderUI(const Player& player) {
    // Render HUD
    renderHUD(player);
    
    // Render FPS counter if enabled
    if (m_showFPS) {
        // Calculate FPS
        m_frameCount++;
        double currentTime = SDL_GetTicks() / 1000.0;
        if (currentTime - m_fpsTimer >= 1.0) {
            m_fps = m_frameCount / (currentTime - m_fpsTimer);
            m_frameCount = 0;
            m_fpsTimer = currentTime;
        }
        
        // Render FPS text
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(m_fps));
        renderText(fpsText, 10, 10, Color(255, 255, 255, 255));
    }
    
    // Render minimap if enabled
    if (m_showMinimap && m_engine) {
        renderMinimap(m_engine->getMap(), player);
    }
}

// Clear the z-buffer
void Renderer::clearZBuffer() {
    #if defined(__SSE2__) || defined(_MSC_VER)
    optimized::fill_doubles(m_zBuffer.data(), m_zBuffer.data() + m_zBuffer.size(), std::numeric_limits<double>::max());
    #else
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<double>::max());
    #endif
} 