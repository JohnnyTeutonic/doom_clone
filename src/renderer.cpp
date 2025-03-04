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
    , m_showFPS(false)
    , m_showMinimap(true)
    , m_showWeapon(true)
    , m_lightingEnabled(true)
    , m_performanceLevel(PerformanceLevel::High)
    , m_frameCount(0)
    , m_fps(0.0)
    , m_fpsTimer(0.0)
{
    // Initialize lighting system
    m_lightingSystem.setEnabled(m_lightingEnabled);
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
        renderSprites(player);
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
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<double>::max());
    
    // Get player position, direction, and vertical angle
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    double verticalAngle = player.getVerticalAngle();  // Get the vertical look angle
    
    // Calculate vertical offset based on vertical angle
    int verticalOffset = static_cast<int>(verticalAngle * m_screenHeight / 2);
    
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
        
        // Apply vertical angle to wall placement
        int drawStart = -lineHeight / 2 + m_screenHeight / 2 + verticalOffset + heightOffset;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + m_screenHeight / 2 + verticalOffset + heightOffset;
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
                SDL_RenderDrawLine(m_renderer, x, midPoint-1, x, midPoint+1);
            }
            else if (isStairs) {
                // For stair entry/exit points, create a distinct pattern
                // Render the entry/exit point with a fancy pattern
                // Create a chevron/arrow pattern pointing up/down
                for (int y = drawStart; y < drawEnd; y++) {
                    // Calculate normalized position in the stair (0.0 to 1.0)
                    float relY = (y - drawStart) / static_cast<float>(drawEnd - drawStart);
                    
                    // Create an arrow/chevron pattern
                    int patternValue = static_cast<int>((relY * 10)) % 10;
                    int patternX = abs(patternValue - 5); // Creates a zigzag from 5->0->5
                    
                    // Arrow pattern is wider in the middle, narrower at ends
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
}

void Renderer::renderSprites(const Player& player) {
    if (!m_spriteManager) return;
    
    // Get player position and vertical angle
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    double verticalAngle = player.getVerticalAngle();  // Get vertical look angle
    
    // Calculate vertical offset based on vertical angle
    int verticalOffset = static_cast<int>(verticalAngle * m_screenHeight / 2);
    
    // Get active sprites
    std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
    
    // Sort sprites by distance (furthest first)
    std::sort(sprites.begin(), sprites.end(), 
        [&pos](const Sprite* a, const Sprite* b) {
            double distA = (a->getPosition() - pos).lengthSquared();
            double distB = (b->getPosition() - pos).lengthSquared();
            return distA > distB;
        }
    );
    
    // Render each sprite
    for (const Sprite* sprite : sprites) {
        // Translate sprite position to relative to camera
        double spriteX = sprite->getPosition().x - pos.x;
        double spriteY = sprite->getPosition().y - pos.y;
        
        // Transform sprite with the inverse camera matrix
        double invDet = 1.0 / (plane.x * dir.y - dir.x * plane.y);
        double transformX = invDet * (dir.y * spriteX - dir.x * spriteY);
        double transformY = invDet * (-plane.y * spriteX + plane.x * spriteY);
        
        // Skip if behind player or too far away (culling)
        if (transformY <= 0.1) continue;
        
        // Optimization: Skip distant sprites when in low performance mode
        if (m_performanceLevel == PerformanceLevel::Low && transformY > 12.0) continue;
        
        // Calculate sprite screen position
        int spriteScreenX = static_cast<int>((m_screenWidth / 2) * (1 + transformX / transformY));
        
        // Calculate sprite height and width on screen
        int spriteHeight = std::abs(static_cast<int>(m_screenHeight / transformY)) * sprite->getSize();
        int spriteWidth = spriteHeight; // Square sprites
        
        // Calculate drawing boundaries with vertical offset applied
        int drawStartY = -spriteHeight / 2 + m_screenHeight / 2 + verticalOffset;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = spriteHeight / 2 + m_screenHeight / 2 + verticalOffset;
        if (drawEndY >= m_screenHeight) drawEndY = m_screenHeight - 1;
        
        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        if (drawStartX < 0) drawStartX = 0;
        int drawEndX = spriteWidth / 2 + spriteScreenX;
        if (drawEndX >= m_screenWidth) drawEndX = m_screenWidth - 1;
        
        // Get the sprite texture based on animation frame
        int textureId = sprite->getTextureId();
        
        // For enemy sprites, check if we need to use a different texture based on animation frame
        if (sprite->getType() == SpriteType::Enemy) {
            // Get the current animation frame
            int currentFrame = sprite->getCurrentFrame();
            
            // Use the engine reference to get texture frames
            if (m_engine && currentFrame >= 0) {
                const std::vector<int>& enemyTextureFrames = m_engine->getEnemyTextureFrames();
                if (!enemyTextureFrames.empty() && currentFrame < static_cast<int>(enemyTextureFrames.size())) {
                    textureId = enemyTextureFrames[currentFrame];
                }
            }
        }
        
        // Get the texture
        const Texture* texture = m_textureManager->getTexture(textureId);
        if (!texture) continue;
        
        // Optimization: Pre-calculate lighting based on distance (fake lighting for sprites)
        // This is much faster than calculating per-pixel lighting for sprites
        double distanceShade = 1.0 - std::min(1.0, transformY / 15.0);
        
        // Adjust shading based on performance level
        if (m_performanceLevel == PerformanceLevel::High) {
            // For high quality, use slightly more accurate lighting calculation
            // Create a normal vector pointing towards the player for more accurate lighting
            Vec2 spriteNormal = (pos - sprite->getPosition()).normalized();
            
            // Pre-calculate one lighting value for the whole sprite
            Color lighting = m_lightingSystem.calculateLighting(sprite->getPosition(), spriteNormal, pos);
            
            // Adjust the distance shading based on this lighting
            distanceShade *= (lighting.r + lighting.g + lighting.b) / (3.0 * 255.0);
        }
        
        // Draw the sprite
        for (int x = drawStartX; x < drawEndX; x++) {
            // Check if sprite is in front of the wall
            if (transformY > 0 && x >= 0 && x < m_screenWidth && transformY < m_zBuffer[x]) {
                // Calculate texture x coordinate
                double texX = (x - (-spriteWidth / 2 + spriteScreenX)) / static_cast<double>(spriteWidth);
                
                // For medium/high quality, determine pixel lighting frequency
                int pixelStride = 1; // Default render every pixel
                if (m_performanceLevel == PerformanceLevel::Low) {
                    pixelStride = transformY < 5.0 ? 1 : 2; // Skip pixels for distant sprites
                }
                
                // Draw vertical stripe
                for (int y = drawStartY; y < drawEndY; y += pixelStride) {
                    // Calculate texture y coordinate
                    double texY = (y - drawStartY) / static_cast<double>(drawEndY - drawStartY);
                    
                    // Get pixel color from texture
                    Color color = texture->getPixelNormalized(texX, texY);
                    
                    // Skip transparent pixels
                    if (color.a < 128) continue;
                    
                    // Apply pre-calculated distance-based shading
                    color = color.withLighting(distanceShade);
                    
                    // Draw the pixel
                    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderDrawPoint(m_renderer, x, y);
                    
                    // Fill in skipped pixels with the same color if using stride > 1
                    if (pixelStride > 1) {
                        for (int i = 1; i < pixelStride && y + i < drawEndY; i++) {
                            SDL_RenderDrawPoint(m_renderer, x, y + i);
                        }
                    }
                }
            }
        }
    }
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
                            if (targetElevation >= 0) break;
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
    int weaponY = m_screenHeight - weaponHeight;
    
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
        // Draw the texture
        SDL_RenderCopy(m_renderer, sdlTexture, NULL, &destRect);
    }
    
    // Render muzzle flash if needed
    if (flashIntensity > 0.0) {
        renderMuzzleFlash(flashIntensity);
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
        
    // Render each projectile
    for (const Projectile* projectile : projectiles) {
        if (!projectile) {
            std::cerr << "Null projectile in active projectiles list!" << std::endl;
            continue;
        }
        
        // Print projectile information
        std::cout << "Rendering bullet at (" << projectile->getPosition().x << ", " 
                  << projectile->getPosition().y << "), active: " << projectile->isActive() 
                  << ", lifetime: " << projectile->getLifetime() << " seconds" << std::endl;
        
        // Calculate projectile position relative to player
        double projX = projectile->getPosition().x - pos.x;
        double projY = projectile->getPosition().y - pos.y;
        
        // Transform with the inverse camera matrix
        double invDet = 1.0 / (plane.x * dir.y - dir.x * plane.y);
        double transformX = invDet * (dir.y * projX - dir.x * projY);
        double transformY = invDet * (-plane.y * projX + plane.x * projY);
        
        // Skip if behind player or too far
        if (transformY <= 0.1) {
            std::cout << "Bullet behind player, skipping" << std::endl;
            continue;
        }
        
        // Calculate screen position
        int screenX = static_cast<int>((m_screenWidth / 2) * (1 + transformX / transformY));
        
        // Calculate bullet size based on distance
        int size = static_cast<int>(m_screenHeight / transformY * 0.05); // Make bullets smaller but still visible
        size = std::max(4, std::min(size, 20)); // Clamp size between 4 and 20 pixels
        
        std::cout << "Bullet screen position: x=" << screenX << ", size=" << size 
                 << ", distance=" << transformY << std::endl;
        
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
                    m_renderer,
                    sdlTexture,
                    NULL,                    // Use the entire source texture
                    &destRect,               // Destination on screen
                    angle,                   // Rotation angle in degrees
                    NULL,                    // Rotate around center
                    SDL_FLIP_NONE            // No flipping
                );
                
                // Add a small glow effect
                if (m_performanceLevel != PerformanceLevel::Low) {
                    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 64);
                    SDL_Rect glowRect = { 
                        centerX - size / 2 - 2, 
                        centerY - size / 2 - 2, 
                        size + 4, 
                        size + 4 
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

void Renderer::renderMuzzleFlash(double intensity) {
    // Skip if intensity is too low
    if (intensity <= 0.01) return;
    
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
        
        // Center the weapon
        weaponX = (m_screenWidth - weaponWidth) / 2;
        weaponY = m_screenHeight - weaponHeight;
        
        // Position the flash at the end of the barrel - adjusted for shotgun.webp
        // This is a rough estimate; adjust based on your texture
        int flashX = weaponX + weaponWidth * 0.8;
        int flashY = weaponY + weaponHeight * 0.3;
        
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