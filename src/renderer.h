#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include "utils.h"
#include "map.h"
#include "player.h"
#include "texture.h"
#include "sprite.h"
#include "projectile.h"
#include "lighting.h"

// Forward declare Engine to avoid circular dependency
class Engine;

// Performance settings enumeration
enum class PerformanceLevel {
    Low,      // Minimal lighting, maximum performance
    Medium,   // Balanced lighting and performance
    High      // Full lighting effects (may impact performance)
};

// Forward declarations
class SpriteManager;
class ProjectileManager;

// Renderer class for raycasting
class Renderer {
private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    int m_screenWidth;
    int m_screenHeight;
    bool m_fullscreen;
    
    // Rendering buffers
    std::vector<double> m_zBuffer;  // Depth buffer for sprite rendering
    
    // Managers and references
    TextureManager* m_textureManager;
    SpriteManager* m_spriteManager;
    ProjectileManager* m_projectileManager;
    const Engine* m_engine;  // Reference to the engine for texture frames
    LightingSystem m_lightingSystem;  // New lighting system
    
    // Rendering options
    bool m_showFPS;
    bool m_showMinimap;
    bool m_showWeapon;
    
    // Performance settings
    PerformanceLevel m_performanceLevel;
    bool m_lightingEnabled;
    
    // FPS counter
    int m_frameCount;
    double m_fpsTimer;
    double m_fps;

    // Pre-calculated normals for faster lighting calculation
    const Vec2 m_normalLeft{-1.0, 0.0};
    const Vec2 m_normalRight{1.0, 0.0};
    const Vec2 m_normalUp{0.0, -1.0};
    const Vec2 m_normalDown{0.0, 1.0};
    
    // Wall texture variations for different wall types
    std::vector<int> m_wallTextureVariations;
    
    // Calculate surface normal for lighting - optimized to use pre-calculated normals
    Vec2 calculateSurfaceNormal(bool side, const Vec2& rayDir) const {
        if (side) {
            return rayDir.y > 0 ? m_normalLeft : m_normalRight;  // Hit vertical wall
        } else {
            return rayDir.x > 0 ? m_normalUp : m_normalDown;  // Hit horizontal wall
        }
    }
    
    // Private helper methods
    void DrawArrow(const SDL_Rect& rect, int direction); // 0=N, 1=E, 2=S, 3=W
    
public:
    Renderer();
    ~Renderer();
    
    // Initialize the renderer
    bool init(int width, int height, bool fullscreen);
    
    // Clean up resources
    void cleanup();
    
    // Set SDL renderer
    void setSDLRenderer(SDL_Renderer* renderer);
    
    // Set texture and sprite managers
    void setTextureManager(TextureManager* textureManager) { m_textureManager = textureManager; }
    void setSpriteManager(SpriteManager* spriteManager) { m_spriteManager = spriteManager; }
    void setProjectileManager(ProjectileManager* projectileManager) { m_projectileManager = projectileManager; }
    void setEngine(const Engine* engine) { m_engine = engine; }
    
    // Get managers
    TextureManager* getTextureManager() const { return m_textureManager; }
    SpriteManager* getSpriteManager() const { return m_spriteManager; }
    ProjectileManager* getProjectileManager() const { return m_projectileManager; }
    LightingSystem& getLightingSystem() { return m_lightingSystem; }
    
    // Add wall texture variations
    void addWallTextureVariation(int textureId) { m_wallTextureVariations.push_back(textureId); }
    void clearWallTextureVariations() { m_wallTextureVariations.clear(); }
    const std::vector<int>& getWallTextureVariations() const { return m_wallTextureVariations; }
    
    // Render a frame
    void render(const Map& map, const Player& player, double deltaTime, double recoil = 0.0, double flashIntensity = 0.0);
    
    // Render the 3D view
    void renderView(const Map& map, const Player& player);
    
    // Render sprites
    void renderSprites(const Player& player);
    
    // Render projectiles
    void renderProjectiles(const Player& player);
    
    // Render the minimap
    void renderMinimap(const Map& map, const Player& player, ProjectileManager& projectileManager);
    
    // Overload for backward compatibility
    void renderMinimap(const Map& map, const Player& player) {
        if (m_projectileManager) {
            renderMinimap(map, player, *m_projectileManager);
        }
    }
    
    // Render the HUD (health, ammo, etc.)
    void renderHUD(const Player& player);
    
    // Render the weapon
    void renderWeapon(const Player& player, double recoil = 0.0, double flashIntensity = 0.0, int weaponTextureId = 5);
    
    // Render muzzle flash
    void renderMuzzleFlash(double intensity);
    
    // Render text
    void renderText(const std::string& text, int x, int y, const Color& color);
    
    // Toggle rendering options
    void toggleFPS() { m_showFPS = !m_showFPS; }
    void toggleMinimap() { m_showMinimap = !m_showMinimap; }
    void toggleWeapon() { m_showWeapon = !m_showWeapon; }
    void toggleLighting() { m_lightingEnabled = !m_lightingEnabled; m_lightingSystem.setEnabled(m_lightingEnabled); }
    bool getShowWeapon() const { return m_showWeapon; }
    
    // Performance settings
    void setPerformanceLevel(PerformanceLevel level) {
        m_performanceLevel = level;
        
        // Configure lighting system based on performance level
        switch (level) {
            case PerformanceLevel::Low:
                m_lightingSystem.setCullingEnabled(true);
                m_lightingSystem.setCullDistance(8.0);  // Shorter cull distance
                m_lightingSystem.setUpdateFrequency(5); // Update lighting less frequently
                break;
                
            case PerformanceLevel::Medium:
                m_lightingSystem.setCullingEnabled(true);
                m_lightingSystem.setCullDistance(15.0); // Default cull distance
                m_lightingSystem.setUpdateFrequency(3); // Normal update frequency
                break;
                
            case PerformanceLevel::High:
                m_lightingSystem.setCullingEnabled(true);
                m_lightingSystem.setCullDistance(25.0); // Longer cull distance
                m_lightingSystem.setUpdateFrequency(1); // Update every frame
                break;
        }
    }
    
    PerformanceLevel getPerformanceLevel() const { return m_performanceLevel; }
    
    // Get the SDL renderer
    SDL_Renderer* getSDLRenderer() const { return m_renderer; }
};

#endif // RENDERER_H 