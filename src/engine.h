#ifndef ENGINE_H
#define ENGINE_H

#include "Common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

// Forward declarations
class Map;
class Player;
class Renderer;
class TextureManager;
class InputHandler;
class AudioSystem;

// Game states
enum class GameState {
    MAIN_MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    QUITTING
};

// Input handling struct
struct InputState {
    bool keyDown[SDL_NUM_SCANCODES];
    bool keyPressed[SDL_NUM_SCANCODES];
    bool keyReleased[SDL_NUM_SCANCODES];
    bool mouseButtonDown[5];
    bool mouseButtonPressed[5];
    bool mouseButtonReleased[5];
    int mouseX, mouseY;
    int mouseDeltaX, mouseDeltaY;
    
    InputState() {
        std::memset(keyDown, 0, sizeof(keyDown));
        std::memset(keyPressed, 0, sizeof(keyPressed));
        std::memset(keyReleased, 0, sizeof(keyReleased));
        std::memset(mouseButtonDown, 0, sizeof(mouseButtonDown));
        std::memset(mouseButtonPressed, 0, sizeof(mouseButtonPressed));
        std::memset(mouseButtonReleased, 0, sizeof(mouseButtonReleased));
        mouseX = mouseY = mouseDeltaX = mouseDeltaY = 0;
    }
};

// Engine configuration
struct EngineConfig {
    int screenWidth;
    int screenHeight;
    bool fullscreen;
    int targetFPS;
    bool vsync;
    bool showFPS;
    float mouseSensitivity;
    float soundVolume;
    float musicVolume;
    
    EngineConfig() : 
        screenWidth(1024),
        screenHeight(768),
        fullscreen(false),
        targetFPS(60),
        vsync(true),
        showFPS(true),
        mouseSensitivity(1.0f),
        soundVolume(0.8f),
        musicVolume(0.5f) {}
};

// Main Engine class - manages game state and subsystems
class Engine {
public:
    Engine();
    ~Engine();
    
    // Initialize the engine
    bool init(const EngineConfig& config = EngineConfig());
    
    // Run the game loop
    void run();
    
    // Shutdown and cleanup
    void shutdown();
    
    // State management
    void setState(GameState state);
    GameState getState() const { return m_state; }
    
    // Quit the game
    void quit() { m_running = false; }
    
    // Get FPS
    int getFPS() const { return m_fps; }
    
    // Toggle fullscreen
    void toggleFullscreen();
    
    // Accessors
    Map* getMap() { return m_map.get(); }
    Player* getPlayer() { return m_player.get(); }
    Renderer* getRenderer() { return m_renderer.get(); }
    TextureManager* getTextureManager() { return m_textureManager.get(); }
    InputState* getInputState() { return &m_inputState; }
    
    // Audio control
    void playSound(const std::string& name, int channel = -1, float volume = 1.0f);
    void playMusic(const std::string& name, bool loop = true);
    void stopMusic();
    void setMusicVolume(float volume);
    void setSoundVolume(float volume);
    
    // Resource directory
    std::string getResourcePath(const std::string& subDir = "") const;
    
    // Notification system
    void showNotification(const std::string& text, float duration = 3.0f);
    
private:
    // Game state
    GameState m_state;
    bool m_running;
    bool m_paused;
    
    // Performance metrics
    int m_fps;
    double m_frameTime;
    
    // Input state
    InputState m_inputState;
    
    // Configuration
    EngineConfig m_config;
    
    // SDL components
    SDL_Window* m_window;
    
    // Subsystems
    std::unique_ptr<Map> m_map;
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<AudioSystem> m_audioSystem;
    
    // Notification system
    struct Notification {
        std::string text;
        float timeRemaining;
    };
    std::vector<Notification> m_notifications;
    
    // Internal methods
    void processInput();
    void update(double deltaTime);
    void render();
    
    // Input handling
    void resetInputState();
    void updateInputState(SDL_Event& event);
    
    // Game initialization
    bool initSDL();
    bool initSubsystems();
    bool loadResources();
    
    // Test map for development
    void createTestMap();
};

#endif // ENGINE_H 