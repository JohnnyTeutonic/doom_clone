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
    , m_textureManager(nullptr)
    , m_spriteManager(nullptr)
    , m_projectileManager(nullptr)
    , m_showFPS(true)
    , m_showMinimap(true)
    , m_showWeapon(true)
    , m_performanceLevel(PerformanceLevel::Medium)
    , m_lightingEnabled(true)
    , m_frameCount(0)
    , m_fpsTimer(0.0)
    , m_fps(0.0)
{
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
        renderMinimap(map, player);
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
    // For each vertical strip of the screen
    for (int x = 0; x < m_screenWidth; x++) {
        // Calculate ray position and direction
        double cameraX = 2 * x / static_cast<double>(m_screenWidth) - 1;
        Vec2 rayDir = player.getDirection() + player.getPlane() * cameraX;

        // Initialize DDA algorithm variables
        Vec2 mapPos = Vec2(static_cast<int>(player.getPosition().x), static_cast<int>(player.getPosition().y));
        Vec2 deltaDist = Vec2(std::abs(1.0 / rayDir.x), std::abs(1.0 / rayDir.y));
        Vec2 sideDist;
        Vec2 step;

        // Calculate step and initial sideDist
        if (rayDir.x < 0) {
            step.x = -1;
            sideDist.x = (player.getPosition().x - mapPos.x) * deltaDist.x;
        } else {
            step.x = 1;
            sideDist.x = (mapPos.x + 1.0 - player.getPosition().x) * deltaDist.x;
        }
        if (rayDir.y < 0) {
            step.y = -1;
            sideDist.y = (player.getPosition().y - mapPos.y) * deltaDist.y;
        } else {
            step.y = 1;
            sideDist.y = (mapPos.y + 1.0 - player.getPosition().y) * deltaDist.y;
        }

        // Perform DDA
        bool hit = false;
        bool side = false; // NS or EW wall hit
        while (!hit) {
            // Jump to next map square
            if (sideDist.x < sideDist.y) {
                sideDist.x += deltaDist.x;
                mapPos.x += step.x;
                side = false;
            } else {
                sideDist.y += deltaDist.y;
                mapPos.y += step.y;
                side = true;
            }

            // Check if ray has hit a wall
            if (map.isSolid(mapPos.x, mapPos.y)) {
                hit = true;
            }
        }

        if (hit) {
            // Calculate distance to wall
            double perpWallDist;
            if (!side) {
                perpWallDist = (mapPos.x - player.getPosition().x + (1 - step.x) / 2) / rayDir.x;
            } else {
                perpWallDist = (mapPos.y - player.getPosition().y + (1 - step.y) / 2) / rayDir.y;
            }

            // Store distance in zBuffer
            m_zBuffer[x] = perpWallDist;

            // Calculate wall height
            int lineHeight = static_cast<int>(m_screenHeight / perpWallDist);
            int drawStart = -lineHeight / 2 + m_screenHeight / 2;
            if (drawStart < 0) drawStart = 0;
            int drawEnd = lineHeight / 2 + m_screenHeight / 2;
            if (drawEnd >= m_screenHeight) drawEnd = m_screenHeight - 1;

            // Get wall texture
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
                wallX = player.getPosition().y + perpWallDist * rayDir.y;
            } else {
                wallX = player.getPosition().x + perpWallDist * rayDir.x;
            }
            wallX -= floor(wallX);  // Normalize to [0,1]
            
            // Calculate surface normal for lighting
            Vec2 normal = calculateSurfaceNormal(side, rayDir);
            
            // Calculate world position of the wall hit
            Vec2 wallPos = player.getPosition() + rayDir * perpWallDist;
            
            // Performance optimization: only calculate lighting once per wall segment
            // if the distance is large enough (reduces calculation per pixel)
            Color lighting;
            if (perpWallDist > 4.0) {
                // For distant walls, calculate lighting once per column
                lighting = m_lightingSystem.calculateLighting(wallPos, normal, player.getPosition());
                
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
                        lighting = m_lightingSystem.calculateLighting(wallPos, normal, player.getPosition());
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
    
    // Get player position
    const Vec2& pos = player.getPosition();
    const Vec2& dir = player.getDirection();
    const Vec2& plane = player.getPlane();
    
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
        
        // Calculate drawing boundaries
        int drawStartY = -spriteHeight / 2 + m_screenHeight / 2;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = spriteHeight / 2 + m_screenHeight / 2;
        if (drawEndY >= m_screenHeight) drawEndY = m_screenHeight - 1;
        
        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        if (drawStartX < 0) drawStartX = 0;
        int drawEndX = spriteWidth / 2 + spriteScreenX;
        if (drawEndX >= m_screenWidth) drawEndX = m_screenWidth - 1;
        
        // Get the sprite texture
        const Texture* texture = m_textureManager->getTexture(sprite->getTextureId());
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
            Color lighting = m_lightingSystem.calculateLighting(sprite->getPosition(), spriteNormal, player.getPosition());
            
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

void Renderer::renderMinimap(const Map& map, const Player& player) {
    // Minimap size and position
    int mapSize = 150;
    int mapX = m_screenWidth - mapSize - 10;
    int mapY = 10;
    int cellSize = mapSize / std::max(map.getWidth(), map.getHeight());
    
    // Draw background
    SDL_Rect mapRect = { mapX, mapY, mapSize, mapSize };
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 192);
    SDL_RenderFillRect(m_renderer, &mapRect);
    
    // Draw border
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_renderer, &mapRect);
    
    // Draw map cells
    for (int y = 0; y < map.getHeight(); y++) {
        for (int x = 0; x < map.getWidth(); x++) {
            SDL_Rect cellRect = { 
                mapX + x * cellSize, 
                mapY + y * cellSize, 
                cellSize, 
                cellSize 
            };
            
            CellType cell = map.getCell(x, y);
            
            switch (cell) {
                case CellType::Wall:
                    SDL_SetRenderDrawColor(m_renderer, 128, 128, 128, 255);
                    SDL_RenderFillRect(m_renderer, &cellRect);
                    break;
                case CellType::Door:
                    SDL_SetRenderDrawColor(m_renderer, 139, 69, 19, 255);
                    SDL_RenderFillRect(m_renderer, &cellRect);
                    break;
                case CellType::Item:
                    SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 255);
                    SDL_RenderFillRect(m_renderer, &cellRect);
                    break;
                case CellType::Enemy:
                    SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
                    SDL_RenderFillRect(m_renderer, &cellRect);
                    break;
                default:
                    break;
            }
        }
    }
    
    // Draw player
    int playerX = mapX + static_cast<int>(player.getPosition().x * cellSize);
    int playerY = mapY + static_cast<int>(player.getPosition().y * cellSize);
    int playerSize = cellSize / 2;
    
    SDL_Rect playerRect = { 
        playerX - playerSize / 2, 
        playerY - playerSize / 2, 
        playerSize, 
        playerSize 
    };
    
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255);
    SDL_RenderFillRect(m_renderer, &playerRect);
    
    // Draw player direction
    int dirX = playerX + static_cast<int>(player.getDirection().x * cellSize * 2);
    int dirY = playerY + static_cast<int>(player.getDirection().y * cellSize * 2);
    
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255);
    SDL_RenderDrawLine(m_renderer, playerX, playerY, dirX, dirY);
    
    // Draw projectiles on minimap
    if (m_projectileManager) {
        std::vector<Projectile*> projectiles = m_projectileManager->getActiveProjectiles();
        
        for (const Projectile* projectile : projectiles) {
            if (!projectile) continue;
            
            // Calculate projectile position on minimap
            int projX = mapX + static_cast<int>(projectile->getPosition().x * cellSize);
            int projY = mapY + static_cast<int>(projectile->getPosition().y * cellSize);
            
            // Draw projectile (bright yellow dot)
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 0, 255);
            SDL_Rect projRect = { projX - 2, projY - 2, 4, 4 };
            SDL_RenderFillRect(m_renderer, &projRect);
            
            // Draw projectile direction
            int projDirX = projX + static_cast<int>(projectile->getDirection().x * cellSize);
            int projDirY = projY + static_cast<int>(projectile->getDirection().y * cellSize);
            SDL_RenderDrawLine(m_renderer, projX, projY, projDirX, projDirY);
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
    
    // Render the weapon texture
    std::cout << "Rendering weapon texture (ID " << weaponTextureId << ")" << std::endl;
    
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
    
    // Debug output
    std::cout << "Rendering " << projectiles.size() << " active projectiles" << std::endl;
    
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