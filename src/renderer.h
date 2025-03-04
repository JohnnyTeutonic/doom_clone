#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <vector>
#include "utils.h"
#include "map.h"
#include "player.h"
#include "texture.h"
#include "sprite.h"
#include "projectile.h"

// Renderer class for raycasting
class Renderer {
private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    int m_screenWidth;
    int m_screenHeight;
    
    // Rendering buffers
    std::vector<double> m_zBuffer;  // Depth buffer for sprite rendering
    
    // Texture manager
    TextureManager* m_textureManager;
    
    // Sprite manager
    SpriteManager* m_spriteManager;
    
    // Projectile manager
    ProjectileManager* m_projectileManager;
    
    // Rendering options
    bool m_showFPS;
    bool m_showMinimap;
    bool m_showWeapon;
    
    // FPS counter
    int m_frameCount;
    double m_fpsTimer;
    double m_fps;
    
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
    
    // Get managers
    TextureManager* getTextureManager() const { return m_textureManager; }
    SpriteManager* getSpriteManager() const { return m_spriteManager; }
    ProjectileManager* getProjectileManager() const { return m_projectileManager; }
    
    // Render a frame
    void render(const Map& map, const Player& player, double deltaTime, double recoil = 0.0, double flashIntensity = 0.0);
    
    // Render the 3D view
    void renderView(const Map& map, const Player& player);
    
    // Render sprites
    void renderSprites(const Player& player);
    
    // Render projectiles
    void renderProjectiles(const Player& player);
    
    // Render the minimap
    void renderMinimap(const Map& map, const Player& player);
    
    // Render the HUD (health, ammo, etc.)
    void renderHUD(const Player& player);
    
    // Render the weapon
    void renderWeapon(const Player& player, double recoil = 0.0, double flashIntensity = 0.0);
    
    // Render muzzle flash
    void renderMuzzleFlash(double intensity);
    
    // Render text
    void renderText(const std::string& text, int x, int y, const Color& color);
    
    // Toggle rendering options
    void toggleFPS() { m_showFPS = !m_showFPS; }
    void toggleMinimap() { m_showMinimap = !m_showMinimap; }
    void toggleWeapon() { m_showWeapon = !m_showWeapon; }
    
    // Get the SDL renderer
    SDL_Renderer* getSDLRenderer() const { return m_renderer; }
};

#endif // RENDERER_H 