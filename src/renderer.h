#ifndef RENDERER_H
#define RENDERER_H

#include "utils.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

// Platform-specific includes for SDL
#ifdef _WIN32
#include <SDL.h>
#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

// Forward declarations
class Map;
class Player;
class Camera;
class Wall;
class Sector;
#include "sprite.h" // Include sprite.h instead of forward declaration
class TextureManager;
class Engine;

// Structure for vertical rendering spans
struct WallSpan {
    int x;                  // Screen x-coordinate
    int y1, y2;             // Top and bottom y-coordinates
    int screenStartY, screenEndY; // Screen-space Y coordinates
    double z1, z2;          // Top and bottom z-coordinates (world space)
    double u;               // Texture u-coordinate (horizontal)
    double texU;            // Texture u-coordinate for newer implementations
    double distance;        // Distance to wall
    int textureId;          // Wall texture ID
    bool isPortal;          // Whether this span is a portal
    bool isFlipped;         // Whether the texture should be horizontally flipped
    double lightLevel;      // Light level for this span
    Sector* sector;         // Parent sector
    Wall* wall;             // Parent wall
    
    WallSpan() : 
        x(0), y1(0), y2(0), screenStartY(0), screenEndY(0), z1(0), z2(0), u(0), texU(0), 
        distance(0), textureId(-1), isPortal(false), 
        isFlipped(false), lightLevel(1.0), sector(nullptr), wall(nullptr) {}
};

struct FloorCeilingSpan {
    int x;                  // Screen x-coordinate
    int y;                  // Screen y-coordinate
    double z;               // World z-coordinate
    double u, v;            // Texture coordinates
    double distance;        // Distance to point
    int textureId;          // Texture ID
    bool isFloor;           // Whether this is a floor (true) or ceiling (false)
    double lightLevel;      // Light level for this span
    Sector* sector;         // Parent sector
    
    FloorCeilingSpan() : 
        x(0), y(0), z(0), u(0), v(0), 
        distance(0), textureId(-1), isFloor(true), 
        lightLevel(1.0), sector(nullptr) {}
};

struct SpriteSpan {
    int x;                  // Screen x-coordinate
    int y1, y2;             // Top and bottom y-coordinates
    double u, v;            // Texture coordinates
    double distance;        // Distance to sprite
    int textureId;          // Sprite texture ID
    Sprite* sprite;         // Parent sprite
    double lightLevel;      // Light level for this span
    
    SpriteSpan() : 
        x(0), y1(0), y2(0), u(0), v(0), 
        distance(0), textureId(-1), sprite(nullptr), lightLevel(1.0) {}
};

// Renderer class
class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // Initialize the renderer
    bool init(int screenWidth, int screenHeight, bool fullscreen, bool vsync);
    
    // Clean up resources
    void shutdown();
    
    // Start a new frame
    void beginFrame();
    
    // End current frame and present
    void endFrame();
    
    // Clear the screen
    void clear(const Color& color = Colors::BLACK);
    
    // Render the current map and view
    void renderMap(Map* map, Camera* camera);
    
    // Get screen dimensions
    int getScreenWidth() const { return m_screenWidth; }
    int getScreenHeight() const { return m_screenHeight; }
    
    // Set references
    void setTextureManager(TextureManager* textureManager) { m_textureManager = textureManager; }
    void setEngine(Engine* engine) { m_engine = engine; }
    
    // Set rendering options
    void setRenderDistance(double distance) { m_renderDistance = distance; }
    void setFogEnabled(bool enabled) { m_fogEnabled = enabled; }
    void setFogColor(const Color& color) { m_fogColor = color; }
    void setFogDistance(double distance) { m_fogDistance = distance; }
    void setFogDensity(double density) { m_fogDensity = density; }
    void setLightingEnabled(bool enabled) { m_lightingEnabled = enabled; }
    void setGammaCorrectionEnabled(bool enabled) { m_gammaCorrectionEnabled = enabled; }
    void setTextureFiltering(bool enabled) { m_textureFiltering = enabled; }
    
    // Screenshot functions
    bool takeScreenshot(const std::string& filename);
    
    // Debug rendering
    void renderDebugInfo(const std::string& text, int x, int y, const Color& color = Colors::WHITE);
    void renderDebugLine(const Vec2& start, const Vec2& end, const Color& color = Colors::WHITE);
    void renderDebugRect(const Rect& rect, const Color& color = Colors::WHITE, bool filled = false);
    void renderDebugCircle(const Vec2& center, double radius, const Color& color = Colors::WHITE, bool filled = false);
    void toggleDebugMode() { m_debugMode = !m_debugMode; }
    bool isDebugModeEnabled() const { return m_debugMode; }
    
    // HUD rendering
    void renderHUD(Player* player);
    
    // Get SDL renderer
    SDL_Renderer* getSDLRenderer() const { return m_renderer; }
    
    // Set background color
    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }
    
private:
    SDL_Window* m_window;             // SDL window
    SDL_Renderer* m_renderer;         // SDL renderer
    SDL_Texture* m_frameBuffer;       // Framebuffer texture
    uint32_t* m_pixelBuffer;          // Pixel buffer
    int m_screenWidth;                // Screen width
    int m_screenHeight;               // Screen height
    double* m_zBuffer;                // Z-buffer for depth testing
    
    // Rendering options
    double m_renderDistance;          // Maximum render distance
    bool m_fogEnabled;                // Whether fog is enabled
    Color m_fogColor;                 // Fog color
    double m_fogDistance;             // Distance at which fog starts
    double m_fogDensity;              // Fog density
    bool m_lightingEnabled;           // Whether lighting is enabled
    bool m_gammaCorrectionEnabled;    // Whether gamma correction is enabled
    bool m_textureFiltering;          // Whether texture filtering is enabled
    bool m_debugMode;                 // Whether debug mode is enabled
    Color m_backgroundColor;          // Background color
    
    // References
    TextureManager* m_textureManager; // Texture manager
    Engine* m_engine;                 // Engine reference
    
    // Rendering spans
    std::vector<WallSpan> m_wallSpans;
    std::vector<FloorCeilingSpan> m_floorCeilingSpans;
    std::vector<SpriteSpan> m_spriteSpans;
    
    // Internal rendering functions
    void renderWalls(Map* map, Camera* camera);
    void renderFloorsCeilings(Map* map, Camera* camera);
    void renderSprites(Map* map, Camera* camera);
    void renderSkybox(Camera* camera);
    
    // Process visible walls for rendering
    void processVisibleWalls(const std::vector<std::shared_ptr<Wall>>& walls, Camera* camera);
    
    // Calculate wall spans
    void calculateWallSpans(Wall* wall, Camera* camera, Sector* sector);
    
    // Calculate floor/ceiling spans
    void calculateFloorCeilingSpans(Sector* sector, Camera* camera);
    
    // Calculate sprite spans
    void calculateSpriteSpans(Sprite* sprite, Camera* camera);
    
    // Draw spans
    void drawWallSpans();
    void drawFloorCeilingSpans(Camera* camera);
    void drawSpriteSpans();
    
    // Utility functions
    Color applyLighting(const Color& color, double lightLevel);
    Color applyFog(const Color& color, double distance);
    double calculateLightLevel(Sector* sector, const Vec2& position, double height);
    
    // Set pixel in framebuffer
    void setPixel(int x, int y, const Color& color);
    
    // Get pixel from texture at uv coordinates
    Color sampleTexture(int textureId, double u, double v, bool filter = false);
    
    // Draw text
    void drawText(const std::string& text, int x, int y, const Color& color);
};

#endif // RENDERER_H 