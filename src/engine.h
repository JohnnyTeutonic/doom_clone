#ifndef ENGINE_H
#define ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

// Forward declarations
class Renderer;
class TextureManager;
class SpriteManager;
class ProjectileManager;
class InputHandler;
class Map;
class Player;
class AudioSystem;

#include "renderer.h"
#include "map.h"
#include "player.h"
#include "texture.h"
#include "sprite.h"
#include "projectile.h"
#include "input.h"
#include "utils.h"
#include "audio.h"

// Game states
enum class GameState {
    MainMenu,
    Playing,
    Paused,
    GameOver,
    Victory
};

class Engine {
public:
    Engine(int screenWidth = 800, int screenHeight = 600);
    ~Engine();
    
    bool init(int screenWidth, int screenHeight, bool fullscreen, int targetFPS);
    void run();
    void shutdown();
    
    // State management
    void setState(GameState state);
    GameState getState() const { return m_gameState; }
    
    // Utility methods
    void toggleFullscreen();
    void restartGame();
    void quitGame() { m_running = false; }
    
    // Getters for subsystems (for advanced usage)
    Renderer& getRenderer() { return *m_renderer; }
    TextureManager& getTextureManager() { return *m_textureManager; }
    SpriteManager& getSpriteManager() { return *m_spriteManager; }
    InputHandler& getInputHandler() { return m_inputHandler; }
    Map& getMap() { return m_map; }
    Player& getPlayer() { return m_player; }
    
    // Getters for visual effects
    double getWeaponRecoil() const { return m_weaponRecoil; }
    double getFlashIntensity() const { return m_flashIntensity; }
    
    // Texture getters
    int getWallTexture() const { return m_wallTexture; }
    int getFloorTexture() const { return m_floorTexture; }
    int getCeilingTexture() const { return m_ceilingTexture; }
    int getEnemyTexture() const { return m_enemyTexture; }
    const std::vector<int>& getEnemyTextureFrames() const { return m_enemyTextureFrames; }
    int getImpTexture() const { return m_impTexture; }
    const std::vector<int>& getImpTextureFrames() const { return m_impTextureFrames; }
    int getWeaponTexture() const { return m_weaponTexture; }
    
    // Create sprites from map cells
    void createSpritesFromMap();
    
    // Audio control
    void toggleMusic();
    void setMusicVolume(int volume);
    void setSfxVolume(int volume);
    bool isMusicPlaying() const;
    
    // Notification system
    void showNotification(const std::string& text, double duration);
    
private:
    void processInput();
    void update();
    void render();
    bool loadAssets();
    
    // Window and rendering
    SDL_Window* m_window;
    SDL_Renderer* m_sdlRenderer;
    Renderer* m_renderer;
    
    // Game objects
    Map m_map;
    Player m_player;
    TextureManager* m_textureManager;
    SpriteManager* m_spriteManager;
    ProjectileManager* m_projectileManager;
    InputHandler m_inputHandler;
    
    // Game state
    GameState m_gameState;
    bool m_running;
    
    // Timing
    int m_screenWidth;
    int m_screenHeight;
    Uint32 m_lastFrameTime;
    double m_deltaTime;
    
    // Visual effects
    double m_weaponRecoil;
    double m_flashIntensity;
    double m_weaponRecoilRecovery;
    double m_flashDecay;
    
    // Configuration
    bool m_fullscreen;
    int m_targetFPS;
    double m_frameTime;
    
    // Timing
    Timer m_frameTimer;
    
    // Asset IDs
    int m_wallTexture;
    int m_floorTexture;
    int m_ceilingTexture;
    int m_enemyTexture;
    std::vector<int> m_enemyTextureFrames;  // Animation frames for enemies
    int m_impTexture;                       // Texture for Imp enemy
    std::vector<int> m_impTextureFrames;    // Animation frames for Imp
    int m_weaponTexture;
    int m_bulletTexture;
    int m_machineGunTexture;
    int m_currentWeaponTexture;
    std::vector<int> m_wallTextureVariations;  // Store different wall texture IDs
    
    // Notification system
    std::string m_notificationText;
    double m_notificationTimer;
    
    // Font handling
    TTF_Font* m_font;
    SDL_Texture* m_notificationTexture;
    SDL_Rect m_notificationRect;
    
    // Audio system
    AudioSystem* m_audioSystem;
    bool m_musicEnabled;
    
    // Keyboard state tracking for WSL2 compatibility
    std::unordered_map<SDL_Scancode, bool> m_prevKeyboardState;
    
    // Mouse state tracking
    bool m_prevMouseLeftDown;
    
    // Private methods
    void setupMap();
    void setupPlayer();
    void setupInput();
    void renderNotification();
};

#endif // ENGINE_H 