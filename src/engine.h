#ifndef ENGINE_H
#define ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "game_state.h"

// Forward declarations
class Renderer;
class TextureManager;
class SpriteManager;
class ProjectileManager;
class InputHandler;
class Map;
class Player;
class AudioSystem;
class CudaRenderer;

#include "renderer.h"
#include "map.h"
#include "player.h"
#include "texture.h"
#include "sprite.h"
#include "projectile.h"
#include "input.h"
#include "utils.h"
#include "audio.h"
#include "cuda_renderer.h"

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
    const Map& getMap() const { return m_map; }
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
    
    // Enhanced MIDI quality in WSL
    void enhanceMidiQuality();
    
    // Debug function to test sound playback
    void testSoundEffects();
    
    // Test weapon firing
    void testWeapons();
    
    // Notification system
    void showNotification(const std::string& text, double duration);
    
    // Player actions
    void fireWeapon();
    void switchWeapon();
    void useItem();
    
    // Enemy and sprite creation
    int createImpEnemy(double x, double y, double size = 0.7);
    void addAdditionalImps();
    void addRandomImps();
    
    // Audio
    void playSound(const std::string& soundName);
    
private:
    void processInput();
    void update();
    void render();
    bool loadAssets();
    
    // Input handling methods
    void handlePlayingInput();
    void handleMainMenuInput();
    void handlePausedInput();
    void handlePauseMenuInput();
    
    // Window and rendering
    SDL_Window* m_window;
    SDL_Renderer* m_sdlRenderer;
    Renderer* m_renderer;
    CudaRenderer* m_cudaRenderer;
    
    // Game objects
    Map m_map;
    Player m_player;
    TextureManager* m_textureManager;
    SpriteManager* m_spriteManager;
    ProjectileManager* m_projectileManager;
    InputHandler m_inputHandler;
    AudioSystem* m_audioSystem;
    
    // Game state
    GameState m_gameState;
    bool m_running;
    bool m_musicEnabled;  // Added flag for music enabled state
    
    // Menu state
    std::vector<std::string> m_menuItems;
    int m_menuSelection;
    
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
    bool m_useCuda;  // Added CUDA usage flag
    
    // Timing
    Timer m_frameTimer;
    
    // Texture IDs
    int m_wallTexture;
    int m_floorTexture;
    int m_ceilingTexture;
    int m_bulletTexture;
    int m_enemyTexture;
    int m_impTexture;
    int m_weaponTexture;
    int m_machineGunTexture;
    int m_rocketLauncherTexture;
    int m_currentWeaponTexture;
    int m_rocketTexture;
    int m_explosionTexture;
    int m_plasmaTexture;  // New texture for plasma projectiles
    int m_itemTexture;  // Add item texture ID
    int m_ammoBoxTexture; // Texture ID for ammo box
    
    // Animation frames
    std::vector<int> m_enemyTextureFrames;  // Animation frames for enemies
    std::vector<int> m_impTextureFrames;    // Animation frames for Imp
    std::vector<int> m_wallTextureVariations;  // Store different wall texture IDs
    
    // Notification system
    std::string m_notificationText;
    double m_notificationDuration;
    double m_notificationTimer;
    SDL_Texture* m_notificationTexture;
    SDL_Rect m_notificationRect;
    
    // Font handling
    TTF_Font* m_font;
    
    // Mouse state tracking
    bool m_prevMouseLeftDown;
    
    // Keyboard state tracking for WSL2 compatibility
    std::unordered_map<SDL_Scancode, bool> m_prevKeyboardState;
    
    // Private methods
    void setupMap();
    void setupPlayer();
    void setupInput();
    void renderNotification();
    void renderMainMenu();
    void renderPauseOverlay();
};

#endif // ENGINE_H 