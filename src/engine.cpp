#include "engine.h"
#include <iostream>
#include <random>
#include <cmath>

Engine::Engine(int screenWidth, int screenHeight)
    : m_window(nullptr)
    , m_sdlRenderer(nullptr)
    , m_renderer(nullptr)
    , m_textureManager(nullptr)
    , m_spriteManager(nullptr)
    , m_projectileManager(nullptr)
    , m_gameState(GameState::MainMenu)
    , m_running(false)
    , m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_lastFrameTime(0)
    , m_deltaTime(0.0)
    , m_weaponRecoil(0.0)
    , m_flashIntensity(0.0)
    , m_weaponRecoilRecovery(5.0)
    , m_flashDecay(3.0)
    , m_fullscreen(false)
    , m_targetFPS(60)
    , m_frameTime(1.0 / 60.0)
    , m_wallTexture(-1)
    , m_floorTexture(-1)
    , m_ceilingTexture(-1)
    , m_enemyTexture(-1)
    , m_impTexture(-1)
    , m_weaponTexture(-1)
    , m_bulletTexture(-1)
    , m_machineGunTexture(-1)
    , m_currentWeaponTexture(-1)
    , m_notificationTimer(0.0)
    , m_font(nullptr)
    , m_notificationTexture(nullptr)
    , m_audioSystem(nullptr)
    , m_musicEnabled(true)
    , m_prevMouseLeftDown(false)
{
    // Initialize random seed
    srand(static_cast<unsigned int>(time(nullptr)));
}

Engine::~Engine() {
    shutdown();
    
    // Clean up resources
    if (m_projectileManager) delete m_projectileManager;
    if (m_spriteManager) delete m_spriteManager;
    if (m_textureManager) delete m_textureManager;
    if (m_audioSystem) delete m_audioSystem;
    
    // Clean up font resources
    if (m_notificationTexture) SDL_DestroyTexture(m_notificationTexture);
    if (m_font) TTF_CloseFont(m_font);
    
    // Clean up SDL
    if (m_sdlRenderer) SDL_DestroyRenderer(m_sdlRenderer);
    if (m_window) SDL_DestroyWindow(m_window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    
    std::cout << "Engine destroyed" << std::endl;
}

bool Engine::init(int screenWidth, int screenHeight, bool fullscreen, int targetFPS) {
    std::cout << "=============================================================" << std::endl;
    std::cout << "Initializing engine..." << std::endl;
    
    // Store configuration
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_fullscreen = fullscreen;
    m_targetFPS = targetFPS;
    m_frameTime = 1.0 / targetFPS;
    
    // Initialize SDL with explicit keyboard support
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "SDL initialized successfully" << std::endl;

    // Initialize SDL_image with WebP support
    int imgFlags = IMG_INIT_WEBP;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "SDL_image could not initialize with WebP support! SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }
    std::cout << "SDL_image initialized with WebP support" << std::endl;
    
    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! SDL_ttf Error: " << TTF_GetError() << std::endl;
        return false;
    }
    std::cout << "SDL_ttf initialized successfully" << std::endl;
    
    // Load font with larger size for DOOM-style appearance
    m_font = TTF_OpenFont("assets/fonts/Doom2016Text-GOlBq.ttf", 36);  // Increased size for better visibility
    if (!m_font) {
        std::cerr << "Failed to load DOOM font! SDL_ttf Error: " << TTF_GetError() << std::endl;
        // Try system fonts as fallback
        m_font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 36);
        if (!m_font) {
            std::cerr << "Failed to load fallback font! SDL_ttf Error: " << TTF_GetError() << std::endl;
            return false;
        }
        std::cout << "Using fallback font" << std::endl;
    }
    
    // Set font style for bold, crisp text
    TTF_SetFontStyle(m_font, TTF_STYLE_BOLD);
    TTF_SetFontOutline(m_font, 1);  // Add outline for better visibility
    std::cout << "Font loaded and configured successfully" << std::endl;
    
    // Create window
    m_window = SDL_CreateWindow("Doom Clone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                m_screenWidth, m_screenHeight, SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "Window created successfully" << std::endl;
    
    // Create renderer
    m_sdlRenderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_sdlRenderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "SDL Renderer created successfully: " << m_sdlRenderer << std::endl;
    
    // Initialize texture manager with our SDL renderer
    m_textureManager = new TextureManager(m_sdlRenderer);
    std::cout << "Texture manager created: " << m_textureManager << std::endl;
    
    // Initialize renderer
    m_renderer = new Renderer();
    m_renderer->init(m_screenWidth, m_screenHeight, m_fullscreen);
    m_renderer->setSDLRenderer(m_sdlRenderer);
    m_renderer->setTextureManager(m_textureManager);
    m_renderer->setEngine(this);  // Set the engine reference
    std::cout << "Renderer initialized: " << m_renderer << std::endl;
    
    // Create sprite manager
    m_spriteManager = new SpriteManager(m_textureManager);
    std::cout << "Sprite manager created: " << m_spriteManager << std::endl;
    
    // Connect sprite manager to renderer
    m_renderer->setSpriteManager(m_spriteManager);
    std::cout << "Connected sprite manager to renderer" << std::endl;
    
    // Create projectile manager
    m_projectileManager = new ProjectileManager();
    std::cout << "Projectile manager created: " << m_projectileManager << std::endl;
    
    // Connect projectile manager to renderer
    m_renderer->setProjectileManager(m_projectileManager);
    std::cout << "Connected projectile manager to renderer" << std::endl;
    
    // Connect sprite manager to projectile manager for collision detection
    m_projectileManager->setSpriteManager(m_spriteManager);
    std::cout << "Connected sprite manager to projectile manager" << std::endl;
    
    // Load all game assets
    if (!loadAssets()) {
        std::cerr << "Failed to load game assets!" << std::endl;
        return false;
    }
    std::cout << "Game assets loaded successfully" << std::endl;
    
    // Initialize the map
    m_map = Map(40, 40);  // Create map with doubled size (40x40 instead of 20x20)
    
    // Initialize player with position in the middle of the larger map
    m_player.init(20.0, 20.0, 1.0, 0.0);  // x, y, dirX, dirY (center of 40x40 map)
    std::cout << "Player initialized at position (20.0, 20.0)" << std::endl;
    
    // Connect player to projectile manager
    m_player.setProjectileManager(m_projectileManager);
    std::cout << "Connected player to projectile manager" << std::endl;
    
    // Setup map and player
    setupMap();
    setupPlayer();
    std::cout << "Map and player setup completed" << std::endl;
    
    // Initialize timers
    m_lastFrameTime = SDL_GetTicks();
    
    // Set initial game state
    m_gameState = GameState::Playing;
    m_running = true;
    
    // Initialize audio system
    m_audioSystem = new AudioSystem();
    if (!m_audioSystem->init()) {
        std::cerr << "Failed to initialize audio system!" << std::endl;
        // Continue anyway, audio is not critical
    } else {
        std::cout << "Audio system initialized: " << m_audioSystem << std::endl;
        
        // Load and play the background music
        std::string musicPath = "assets/music/M_E1M1.mid";
        if (m_audioSystem->loadMusic(musicPath)) {
            if (m_musicEnabled) {
                m_audioSystem->playMusic(true); // Loop the music
            }
        } else {
            std::cerr << "Failed to load music: " << musicPath << std::endl;
        }
    }
    
    std::cout << "Engine initialization complete!" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    return true;
}

void Engine::run() {
    if (!m_running) {
        std::cerr << "Cannot run engine - not initialized!" << std::endl;
        return;
    }
    
    std::cout << "Starting main game loop..." << std::endl;
    
    // Main game loop
    while (m_running) {
        // Calculate delta time
        Uint32 currentTime = SDL_GetTicks();
        m_deltaTime = (currentTime - m_lastFrameTime) / 1000.0;
        m_lastFrameTime = currentTime;
        
        // Cap delta time to avoid large jumps
        if (m_deltaTime > 0.1) m_deltaTime = 0.1;
        
        // Process input
        processInput();
        
        // Update game state
        update();
        
        // Render
        render();
        
        // Small delay to avoid 100% CPU usage
        SDL_Delay(1);
    }
    
    std::cout << "Main game loop ended" << std::endl;
}

void Engine::shutdown() {
    std::cout << "Shutting down engine..." << std::endl;
    m_renderer->cleanup();
}

void Engine::setState(GameState state) {
    m_gameState = state;
    
    // Handle state transitions
    switch (m_gameState) {
        case GameState::MainMenu:
            std::cout << "Entering main menu" << std::endl;
            break;
            
        case GameState::Playing:
            std::cout << "Starting gameplay" << std::endl;
            break;
            
        case GameState::Paused:
            std::cout << "Game paused" << std::endl;
            break;
            
        case GameState::GameOver:
            std::cout << "Game over" << std::endl;
            break;
            
        case GameState::Victory:
            std::cout << "Victory!" << std::endl;
            break;
    }
}

void Engine::toggleFullscreen() {
    // TODO: Implement fullscreen toggle
    std::cout << "Fullscreen toggle not implemented yet" << std::endl;
}

void Engine::restartGame() {
    std::cout << "Restarting game..." << std::endl;
    
    // Reset player
    m_player.init(m_map.getWidth() / 2.0, m_map.getHeight() / 2.0, 1.0, 0.0);
    m_player.setHealth(100.0);
    m_player.setAmmo(50);
    
    // Reset game state
    setState(GameState::Playing);
}

void Engine::processInput() {
    // Process SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
        }
        
        // Let the input handler process the event
        m_inputHandler.processEvent(event);
    }
    
    // WSL2 workaround: Get keyboard state directly
    int numKeys;
    const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
    
    // Process input actions
    if (m_gameState == GameState::Playing) {
        // Movement - use direct keyboard state for better compatibility with WSL2
        if (keyboardState[SDL_SCANCODE_W]) {
            m_player.moveForward(m_deltaTime, m_map);
        }
        if (keyboardState[SDL_SCANCODE_S]) {
            m_player.moveBackward(m_deltaTime, m_map);
        }
        if (keyboardState[SDL_SCANCODE_A]) {
            m_player.strafeLeft(m_deltaTime, m_map);
        }
        if (keyboardState[SDL_SCANCODE_D]) {
            m_player.strafeRight(m_deltaTime, m_map);
        }
        if (keyboardState[SDL_SCANCODE_LEFT]) {
            m_player.rotateLeft(m_deltaTime);
        }
        if (keyboardState[SDL_SCANCODE_RIGHT]) {
            m_player.rotateRight(m_deltaTime);
        }
        
        // Vertical looking - use direct keyboard state for up and down arrow keys
        if (keyboardState[SDL_SCANCODE_UP]) {
            m_player.lookUp(m_deltaTime);
        }
        if (keyboardState[SDL_SCANCODE_DOWN]) {
            m_player.lookDown(m_deltaTime);
        }
        
        // Shooting - use direct keyboard state for Space
        bool shouldFire = false;
        
        // Check for spacebar firing
        if (keyboardState[SDL_SCANCODE_SPACE] && !m_prevKeyboardState[SDL_SCANCODE_SPACE]) {
            shouldFire = true;
            m_prevKeyboardState[SDL_SCANCODE_SPACE] = true;
        } else if (!keyboardState[SDL_SCANCODE_SPACE]) {
            m_prevKeyboardState[SDL_SCANCODE_SPACE] = false;
        }
        
        // Check for left mouse button firing
        if (m_inputHandler.isLeftMouseDown() && !m_prevMouseLeftDown) {
            shouldFire = true;
            m_prevMouseLeftDown = true;
        } else if (!m_inputHandler.isLeftMouseDown()) {
            m_prevMouseLeftDown = false;
        }
        
        // Fire weapon if either input was triggered
        if (shouldFire) {
            if (m_player.fire()) {
                // Apply recoil effect
                m_weaponRecoil = 0.1;
                
                // Apply muzzle flash effect
                m_flashIntensity = 1.0;
            }
        }
        
        // Reload - use direct keyboard state for R
        if (keyboardState[SDL_SCANCODE_R] && !m_prevKeyboardState[SDL_SCANCODE_R]) {
            m_player.reload();
            m_prevKeyboardState[SDL_SCANCODE_R] = true;
        } else if (!keyboardState[SDL_SCANCODE_R]) {
            m_prevKeyboardState[SDL_SCANCODE_R] = false;
        }
        
        // Toggle lighting - use direct keyboard state for L
        if (keyboardState[SDL_SCANCODE_L] && !m_prevKeyboardState[SDL_SCANCODE_L]) {
            if (m_renderer) {
                m_renderer->toggleLighting();
                showNotification("Lighting toggled", 2.0);
            }
            m_prevKeyboardState[SDL_SCANCODE_L] = true;
        } else if (!keyboardState[SDL_SCANCODE_L]) {
            m_prevKeyboardState[SDL_SCANCODE_L] = false;
        }
        
        // Toggle displays - use direct keyboard state for function keys
        if (keyboardState[SDL_SCANCODE_F1] && !m_prevKeyboardState[SDL_SCANCODE_F1]) {
            m_renderer->toggleFPS();
            m_prevKeyboardState[SDL_SCANCODE_F1] = true;
        } else if (!keyboardState[SDL_SCANCODE_F1]) {
            m_prevKeyboardState[SDL_SCANCODE_F1] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_F2] && !m_prevKeyboardState[SDL_SCANCODE_F2]) {
            m_renderer->toggleMinimap();
            m_prevKeyboardState[SDL_SCANCODE_F2] = true;
        } else if (!keyboardState[SDL_SCANCODE_F2]) {
            m_prevKeyboardState[SDL_SCANCODE_F2] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_F3] && !m_prevKeyboardState[SDL_SCANCODE_F3]) {
            m_renderer->toggleWeapon();
            m_prevKeyboardState[SDL_SCANCODE_F3] = true;
        } else if (!keyboardState[SDL_SCANCODE_F3]) {
            m_prevKeyboardState[SDL_SCANCODE_F3] = false;
        }
        
        // Weapon switching - use direct keyboard state for 1-5
        if (keyboardState[SDL_SCANCODE_1] && !m_prevKeyboardState[SDL_SCANCODE_1]) {
            m_player.setCurrentWeapon(WeaponType::Pistol);
            showNotification("Switched to pistol", 2.0);
            m_prevKeyboardState[SDL_SCANCODE_1] = true;
        } else if (!keyboardState[SDL_SCANCODE_1]) {
            m_prevKeyboardState[SDL_SCANCODE_1] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_2] && !m_prevKeyboardState[SDL_SCANCODE_2]) {
            m_player.setCurrentWeapon(WeaponType::Shotgun);
            showNotification("Switched to shotgun", 2.0);
            m_prevKeyboardState[SDL_SCANCODE_2] = true;
        } else if (!keyboardState[SDL_SCANCODE_2]) {
            m_prevKeyboardState[SDL_SCANCODE_2] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_3] && !m_prevKeyboardState[SDL_SCANCODE_3]) {
            m_player.setCurrentWeapon(WeaponType::RocketLauncher);
            showNotification("Switched to rocket launcher", 2.0);
            m_prevKeyboardState[SDL_SCANCODE_3] = true;
        } else if (!keyboardState[SDL_SCANCODE_3]) {
            m_prevKeyboardState[SDL_SCANCODE_3] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_4] && !m_prevKeyboardState[SDL_SCANCODE_4]) {
            m_player.setCurrentWeapon(WeaponType::PlasmaGun);
            showNotification("Switched to plasma gun", 2.0);
            m_prevKeyboardState[SDL_SCANCODE_4] = true;
        } else if (!keyboardState[SDL_SCANCODE_4]) {
            m_prevKeyboardState[SDL_SCANCODE_4] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_5] && !m_prevKeyboardState[SDL_SCANCODE_5]) {
            m_player.setCurrentWeapon(WeaponType::GrenadeLauncher);
            showNotification("Switched to grenade launcher", 2.0);
            m_prevKeyboardState[SDL_SCANCODE_5] = true;
        } else if (!keyboardState[SDL_SCANCODE_5]) {
            m_prevKeyboardState[SDL_SCANCODE_5] = false;
        }
        
        // Throw grenade directly with G key
        if (keyboardState[SDL_SCANCODE_G] && !m_prevKeyboardState[SDL_SCANCODE_G]) {
            if (m_player.throwGrenade()) {
                // Apply recoil effect
                m_weaponRecoil = 0.15;
            }
            m_prevKeyboardState[SDL_SCANCODE_G] = true;
        } else if (!keyboardState[SDL_SCANCODE_G]) {
            m_prevKeyboardState[SDL_SCANCODE_G] = false;
        }
        
        // Audio controls - use direct keyboard state for better compatibility with WSL2
        if (keyboardState[SDL_SCANCODE_M] && !m_prevKeyboardState[SDL_SCANCODE_M]) {
            toggleMusic();
            m_prevKeyboardState[SDL_SCANCODE_M] = true;
        } else if (!keyboardState[SDL_SCANCODE_M]) {
            m_prevKeyboardState[SDL_SCANCODE_M] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_PAGEUP] && !m_prevKeyboardState[SDL_SCANCODE_PAGEUP]) {
            if (m_audioSystem) {
                int currentVolume = m_audioSystem->getMusicVolume();
                setMusicVolume(currentVolume + 8); // Increase by ~6% (8/128)
            }
            m_prevKeyboardState[SDL_SCANCODE_PAGEUP] = true;
        } else if (!keyboardState[SDL_SCANCODE_PAGEUP]) {
            m_prevKeyboardState[SDL_SCANCODE_PAGEUP] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_PAGEDOWN] && !m_prevKeyboardState[SDL_SCANCODE_PAGEDOWN]) {
            if (m_audioSystem) {
                int currentVolume = m_audioSystem->getMusicVolume();
                setMusicVolume(currentVolume - 8); // Decrease by ~6% (8/128)
            }
            m_prevKeyboardState[SDL_SCANCODE_PAGEDOWN] = true;
        } else if (!keyboardState[SDL_SCANCODE_PAGEDOWN]) {
            m_prevKeyboardState[SDL_SCANCODE_PAGEDOWN] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_HOME] && !m_prevKeyboardState[SDL_SCANCODE_HOME]) {
            if (m_audioSystem) {
                int currentVolume = m_audioSystem->getSfxVolume();
                setSfxVolume(currentVolume + 8); // Increase by ~6% (8/128)
            }
            m_prevKeyboardState[SDL_SCANCODE_HOME] = true;
        } else if (!keyboardState[SDL_SCANCODE_HOME]) {
            m_prevKeyboardState[SDL_SCANCODE_HOME] = false;
        }
        
        if (keyboardState[SDL_SCANCODE_END] && !m_prevKeyboardState[SDL_SCANCODE_END]) {
            if (m_audioSystem) {
                int currentVolume = m_audioSystem->getSfxVolume();
                setSfxVolume(currentVolume - 8); // Decrease by ~6% (8/128)
            }
            m_prevKeyboardState[SDL_SCANCODE_END] = true;
        } else if (!keyboardState[SDL_SCANCODE_END]) {
            m_prevKeyboardState[SDL_SCANCODE_END] = false;
        }
    }
    
    // Global actions (work in any state)
    if (keyboardState[SDL_SCANCODE_ESCAPE] && !m_prevKeyboardState[SDL_SCANCODE_ESCAPE]) {
        if (m_gameState == GameState::Playing) {
            setState(GameState::Paused);
        } else if (m_gameState == GameState::Paused) {
            setState(GameState::Playing);
        }
        m_prevKeyboardState[SDL_SCANCODE_ESCAPE] = true;
    } else if (!keyboardState[SDL_SCANCODE_ESCAPE]) {
        m_prevKeyboardState[SDL_SCANCODE_ESCAPE] = false;
    }
    
    if (keyboardState[SDL_SCANCODE_Q] && !m_prevKeyboardState[SDL_SCANCODE_Q]) {
        m_running = false;
        m_prevKeyboardState[SDL_SCANCODE_Q] = true;
    } else if (!keyboardState[SDL_SCANCODE_Q]) {
        m_prevKeyboardState[SDL_SCANCODE_Q] = false;
    }
    
    // Process mouse movement for camera rotation
    int mouseX, mouseY;
    m_inputHandler.getMouseMotion(mouseX, mouseY);
    
    if (mouseX != 0) {
        // Use the existing rotation methods with the mouse input
        if (mouseX > 0) {
            m_player.rotateLeft(m_deltaTime * mouseX * 0.01);
        } else {
            m_player.rotateRight(m_deltaTime * -mouseX * 0.01);
        }
    }
    
    if (mouseY != 0) {
        // Use the existing look methods with the mouse input
        if (mouseY > 0) {
            m_player.lookDown(m_deltaTime * mouseY * 0.01);
        } else {
            m_player.lookUp(m_deltaTime * -mouseY * 0.01);
        }
    }
}

void Engine::update() {
    // Only update game logic if in playing state
    if (m_gameState == GameState::Playing) {
        // Update notification timer
        if (m_notificationTimer > 0) {
            m_notificationTimer -= m_deltaTime;
            if (m_notificationTimer < 0) {
                m_notificationTimer = 0;
                m_notificationText = "";
            }
        }
        
        // Update sprites with map and player position
        m_spriteManager->update(m_deltaTime, m_map, m_player.getPosition());
        
        // Update projectiles
        m_projectileManager->update(m_deltaTime, m_map);
        
        // Update player
        m_player.update(m_deltaTime, m_map);
        
        // Update visual effects
        if (m_weaponRecoil > 0) {
            m_weaponRecoil -= m_weaponRecoilRecovery * m_deltaTime;
            if (m_weaponRecoil < 0) m_weaponRecoil = 0;
        }
        
        if (m_flashIntensity > 0) {
            m_flashIntensity -= m_flashDecay * m_deltaTime;
            if (m_flashIntensity < 0) m_flashIntensity = 0;
        }
        
        // Update lighting system - use the new batched system
        if (m_renderer) {
            LightingSystem& lighting = m_renderer->getLightingSystem();
            
            // Use the new optimized method which handles all batching internally
            lighting.updateLights(m_deltaTime);
        }
        
        // Check game over condition
        if (m_player.getHealth() <= 0) {
            setState(GameState::GameOver);
        }
        
        // Check for pause
        const Uint8* keyState = SDL_GetKeyboardState(NULL);
        if (keyState[SDL_SCANCODE_ESCAPE]) {
            setState(GameState::Paused);
        }
    }
    
    // Update input handler at the end of the frame
    m_inputHandler.update();
}

void Engine::renderNotification() {
    if (m_notificationTimer > 0 && !m_notificationText.empty()) {
        // Create new texture only if text has changed
        if (m_notificationTexture == nullptr) {
            // DOOM-style colors: Main color is bright red
            SDL_Color textColor = {255, 50, 50, static_cast<Uint8>(255 * std::min(1.0, m_notificationTimer))};
            
            // First create the main text
            SDL_Surface* textSurface = TTF_RenderText_Blended(m_font, m_notificationText.c_str(), textColor);
            if (textSurface) {
                // Create dark outline effect
                SDL_Surface* outlineSurface = TTF_RenderText_Blended(m_font, m_notificationText.c_str(), 
                    SDL_Color{0, 0, 0, static_cast<Uint8>(255 * std::min(1.0, m_notificationTimer))});
                
                if (outlineSurface) {
                    // Combine the surfaces for outline effect
                    SDL_Rect destRect = { 2, 2, textSurface->w, textSurface->h };
                    SDL_BlitSurface(textSurface, NULL, outlineSurface, &destRect);
                    
                    m_notificationTexture = SDL_CreateTextureFromSurface(m_sdlRenderer, outlineSurface);
                    m_notificationRect.w = outlineSurface->w;
                    m_notificationRect.h = outlineSurface->h;
                    m_notificationRect.x = (m_screenWidth - outlineSurface->w) / 2;  // Center horizontally
                    m_notificationRect.y = 50;  // Position at top of screen like classic DOOM
                    
                    SDL_FreeSurface(outlineSurface);
                }
                SDL_FreeSurface(textSurface);
            }
        }
        
        // Render the notification with fade out
        if (m_notificationTexture) {
            // Add pulsing effect
            float pulse = 0.8f + 0.2f * sin(SDL_GetTicks() * 0.01f);
            SDL_SetTextureAlphaMod(m_notificationTexture, 
                static_cast<Uint8>(255 * std::min(1.0, m_notificationTimer) * pulse));
            SDL_RenderCopy(m_sdlRenderer, m_notificationTexture, NULL, &m_notificationRect);
        }
    } else {
        // Clean up texture if notification is done
        if (m_notificationTexture) {
            SDL_DestroyTexture(m_notificationTexture);
            m_notificationTexture = nullptr;
        }
    }
}

void Engine::render() {
    // Clear the renderer
    SDL_SetRenderDrawColor(m_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_sdlRenderer);
    
    // Render based on game state
    switch (m_gameState) {
        case GameState::Playing:
        case GameState::Paused:
            // Render the 3D view
            m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
            
            // Render weapon if enabled
            if (m_renderer->getShowWeapon()) {
                m_renderer->renderWeapon(m_player, m_weaponRecoil, m_flashIntensity, m_currentWeaponTexture);
            }
            
            // Render notification if active
            renderNotification();
            
            // If paused, render pause overlay
            if (m_gameState == GameState::Paused) {
                // TODO: Render pause overlay
            }
            break;
            
        case GameState::MainMenu:
            // TODO: Render main menu
            break;
            
        case GameState::GameOver:
            // Render the 3D view (darkened)
            m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
            if (m_renderer->getShowWeapon()) {
                m_renderer->renderWeapon(m_player, m_weaponRecoil, m_flashIntensity, m_currentWeaponTexture);
            }
            // TODO: Render game over overlay
            break;
            
        case GameState::Victory:
            // Render the 3D view
            m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
            if (m_renderer->getShowWeapon()) {
                m_renderer->renderWeapon(m_player, m_weaponRecoil, m_flashIntensity, m_currentWeaponTexture);
            }
            // TODO: Render victory overlay
            break;
    }
    
    // Present the renderer
    SDL_RenderPresent(m_sdlRenderer);
}

bool Engine::loadAssets() {
    std::cout << "Loading assets..." << std::endl;
    
    // Initialize texture IDs to -1
    m_wallTexture = -1;
    m_floorTexture = -1;
    m_ceilingTexture = -1;
    m_bulletTexture = -1;
    m_enemyTexture = -1;
    m_impTexture = -1;
    m_weaponTexture = -1;
    m_machineGunTexture = -1;
    
    std::string assetsPath = "assets/textures/";
    
    // Create wall textures
    std::cout << "Creating DOOM-style wall textures..." << std::endl;
    
    // Bloody stone texture (ID 0)
    SDL_Surface* bloodyWallSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (bloodyWallSurface) {
        SDL_LockSurface(bloodyWallSurface);
        Uint32* pixels = (Uint32*)bloodyWallSurface->pixels;
        
        // Base stone pattern
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 30) - 15;
                int baseGray = 100 + noise;
                
                if ((x + y) % 8 == 0 || (x - y) % 8 == 0) {
                    baseGray = 60;
                }
                
                if (rand() % 10 == 0) {
                    pixels[y * 64 + x] = SDL_MapRGB(bloodyWallSurface->format, 
                        120 + rand() % 40, 20 + rand() % 20, 20 + rand() % 20);
                } else {
                    pixels[y * 64 + x] = SDL_MapRGB(bloodyWallSurface->format, 
                        baseGray, baseGray, baseGray);
                }
            }
        }
        SDL_UnlockSurface(bloodyWallSurface);
        m_wallTexture = m_textureManager->createTextureFromSurface(bloodyWallSurface);
        SDL_FreeSurface(bloodyWallSurface);
        std::cout << "Created bloody stone texture (ID " << m_wallTexture << ")" << std::endl;
    }
    
    // Demonic runes texture
    SDL_Surface* runeWallSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    int runeTextureId = -1;
    if (runeWallSurface) {
        SDL_LockSurface(runeWallSurface);
        Uint32* pixels = (Uint32*)runeWallSurface->pixels;
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 20) - 10;
                int baseGray = 40 + noise;
                
                bool isRune = false;
                if ((x + y) % 16 < 2 || (x - y) % 16 < 2) {
                    if (rand() % 3 == 0) isRune = true;
                }
                
                if (isRune) {
                    pixels[y * 64 + x] = SDL_MapRGB(runeWallSurface->format,
                        200 + rand() % 55, 50 + rand() % 30, 0);
                } else {
                    pixels[y * 64 + x] = SDL_MapRGB(runeWallSurface->format,
                        baseGray, baseGray, baseGray);
                }
            }
        }
        SDL_UnlockSurface(runeWallSurface);
        runeTextureId = m_textureManager->createTextureFromSurface(runeWallSurface);
        SDL_FreeSurface(runeWallSurface);
        std::cout << "Created rune texture (ID " << runeTextureId << ")" << std::endl;
    }
    
    // Flesh wall texture
    SDL_Surface* fleshWallSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    int fleshTextureId = -1;
    if (fleshWallSurface) {
        SDL_LockSurface(fleshWallSurface);
        Uint32* pixels = (Uint32*)fleshWallSurface->pixels;
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 40) - 20;
                
                bool isVein = false;
                if (((x * x + y * y) % 32 < 4) || ((x - 32) * (x - 32) + (y - 32) * (y - 32)) % 24 < 3) {
                    isVein = true;
                }
                
                if (isVein) {
                    pixels[y * 64 + x] = SDL_MapRGB(fleshWallSurface->format,
                        140 + noise, 20 + noise, 20 + noise);
                } else {
                    pixels[y * 64 + x] = SDL_MapRGB(fleshWallSurface->format,
                        180 + noise, 100 + noise, 100 + noise);
                }
            }
        }
        SDL_UnlockSurface(fleshWallSurface);
        fleshTextureId = m_textureManager->createTextureFromSurface(fleshWallSurface);
        SDL_FreeSurface(fleshWallSurface);
        std::cout << "Created flesh texture (ID " << fleshTextureId << ")" << std::endl;
    }
    
    // Rusty metal texture
    SDL_Surface* metalWallSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    int metalTextureId = -1;
    if (metalWallSurface) {
        SDL_LockSurface(metalWallSurface);
        Uint32* pixels = (Uint32*)metalWallSurface->pixels;
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 30) - 15;
                
                bool isRust = (rand() % 3 == 0);
                if ((x + y) % 8 == 0) isRust = true;
                
                if (isRust) {
                    pixels[y * 64 + x] = SDL_MapRGB(metalWallSurface->format,
                        139 + noise, 69 + noise, 19 + noise);
                } else {
                    pixels[y * 64 + x] = SDL_MapRGB(metalWallSurface->format,
                        160 + noise, 160 + noise, 160 + noise);
                }
            }
        }
        SDL_UnlockSurface(metalWallSurface);
        metalTextureId = m_textureManager->createTextureFromSurface(metalWallSurface);
        SDL_FreeSurface(metalWallSurface);
        std::cout << "Created metal texture (ID " << metalTextureId << ")" << std::endl;
    }
    
    // Create floor texture
    std::cout << "Creating floor texture..." << std::endl;
    SDL_Surface* floorSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (floorSurface) {
        SDL_LockSurface(floorSurface);
        Uint32* pixels = (Uint32*)floorSurface->pixels;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 30) - 15;
                int baseGray = 80 + noise;  // Darker base for floor
                pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, baseGray, baseGray, baseGray);
            }
        }
        SDL_UnlockSurface(floorSurface);
        m_floorTexture = m_textureManager->createTextureFromSurface(floorSurface);
        SDL_FreeSurface(floorSurface);
    } else {
        m_floorTexture = m_textureManager->createSolidTexture(64, 64, Color(32, 32, 64));
    }
    std::cout << "Floor texture ID: " << m_floorTexture << std::endl;
    
    // Create ceiling texture
    std::cout << "Creating ceiling texture..." << std::endl;
    SDL_Surface* ceilingSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (ceilingSurface) {
        SDL_LockSurface(ceilingSurface);
        Uint32* pixels = (Uint32*)ceilingSurface->pixels;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 30) - 15;
                int baseGray = 120 + noise;  // Lighter base for ceiling
                pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, baseGray, baseGray, baseGray);
            }
        }
        SDL_UnlockSurface(ceilingSurface);
        m_ceilingTexture = m_textureManager->createTextureFromSurface(ceilingSurface);
        SDL_FreeSurface(ceilingSurface);
    } else {
        m_ceilingTexture = m_textureManager->createSolidTexture(64, 64, Color(64, 64, 96));
    }
    std::cout << "Ceiling texture ID: " << m_ceilingTexture << std::endl;
    
    // Create bullet texture
    std::cout << "Creating realistic bullet texture..." << std::endl;
    SDL_Surface* bulletSurface = SDL_CreateRGBSurface(0, 32, 32, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (bulletSurface) {
        SDL_LockSurface(bulletSurface);
        Uint32* pixels = (Uint32*)bulletSurface->pixels;
        
        // Colors for the realistic bullet
        Uint32 bulletBase = SDL_MapRGBA(bulletSurface->format, 180, 180, 180, 255);         // Base brass color
        Uint32 bulletTip = SDL_MapRGBA(bulletSurface->format, 100, 100, 100, 255);          // Darker bullet tip
        Uint32 highlight = SDL_MapRGBA(bulletSurface->format, 240, 240, 240, 255);          // Highlight/reflection
        Uint32 shadow = SDL_MapRGBA(bulletSurface->format, 120, 120, 120, 255);             // Shadow
        Uint32 transparent = SDL_MapRGBA(bulletSurface->format, 0, 0, 0, 0);                // Transparent background
        
        // Fill with transparency first
        for (int i = 0; i < 32 * 32; i++) {
            pixels[i] = transparent;
        }
        
        // Draw bullet shape - 3D perspective (bullet flying toward viewer)
        int centerX = 16;
        int centerY = 16;
        int bulletRadius = 12;
        
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                // Calculate distance from center
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                
                // Only draw within circle radius
                if (distFromCenter <= bulletRadius) {
                    // Determine which part of the bullet we're drawing
                    double normalizedDist = distFromCenter / bulletRadius;
                    
                    // The central ~30% is the bullet tip, rest is brass casing
                    if (normalizedDist < 0.3) {
                        // Bullet tip (darker material)
                        pixels[y * 32 + x] = bulletTip;
                        
                        // Add slight texture variation to bullet tip
                        if ((x + y) % 4 == 0) {
                            pixels[y * 32 + x] = SDL_MapRGBA(bulletSurface->format, 90, 90, 90, 255);
                        }
                    } else {
                        // Brass casing
                        pixels[y * 32 + x] = bulletBase;
                        
                        // Create a ring where the casing and tip meet
                        if (normalizedDist > 0.28 && normalizedDist < 0.32) {
                            pixels[y * 32 + x] = shadow;
                        }
                        
                        // Add slight texture variation for realism
                        if ((x + y) % 5 == 0 && normalizedDist > 0.5) {
                            pixels[y * 32 + x] = SDL_MapRGBA(bulletSurface->format, 170, 170, 170, 255);
                        }
                    }
                    
                    // Add highlights based on angle (top-left light source)
                    double angle = atan2(y - centerY, x - centerX);
                    if (angle > -2.5 && angle < -1.0) {
                        // Top-left highlight (reflection)
                        if (normalizedDist > 0.4 && normalizedDist < 0.8) {
                            pixels[y * 32 + x] = highlight;
                        }
                    }
                    
                    // Add bottom-right shadow
                    if (angle > 0.5 && angle < 2.0) {
                        // Bottom-right shadow
                        if (normalizedDist > 0.4) {
                            pixels[y * 32 + x] = shadow;
                        }
                    }
                    
                    // Add a small central reflection dot
                    if (distFromCenter < bulletRadius * 0.15) {
                        pixels[y * 32 + x] = highlight;
                    }
                }
            }
        }
        
        SDL_UnlockSurface(bulletSurface);
        m_bulletTexture = m_textureManager->createTextureFromSurface(bulletSurface);
        SDL_FreeSurface(bulletSurface);
    } else {
        // Fallback to simple solid texture if surface creation fails
        m_bulletTexture = m_textureManager->createSolidTexture(32, 32, Color(255, 255, 0));
    }
    std::cout << "Bullet texture ID: " << m_bulletTexture << std::endl;
    
    // Set the bullet texture in the projectile manager
    if (m_projectileManager) {
        m_projectileManager->setBulletTexture(m_bulletTexture);
        m_projectileManager->setDefaultBulletTexture(m_bulletTexture);
    }
    
    // Create enemy texture
    std::cout << "Creating enemy texture..." << std::endl;
    
    // Create an array of enemy textures for animation frames
    const int enemyFrameCount = 4;
    m_enemyTextureFrames.resize(enemyFrameCount);
    
    // Create a surface for the enemy sprite sheet
    SDL_Surface* enemySurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0, 0, 0, 0);
    if (enemySurface) {
        // Lock surface for direct pixel access
        SDL_LockSurface(enemySurface);
        
        // Create a basic enemy texture (red with eyes)
        Uint32* pixels = static_cast<Uint32*>(enemySurface->pixels);
        for (int y = 0; y < enemySurface->h; y++) {
            for (int x = 0; x < enemySurface->w; x++) {
                // Base color (dark red)
                Uint32 color = SDL_MapRGB(enemySurface->format, 180, 0, 0);
                
                // Add some details (eyes, mouth)
                if ((x >= 15 && x <= 25 && y >= 15 && y <= 25) || 
                    (x >= 38 && x <= 48 && y >= 15 && y <= 25)) {
                    // Eyes (yellow)
                    color = SDL_MapRGB(enemySurface->format, 255, 255, 0);
                }
                else if (x >= 20 && x <= 44 && y >= 40 && y <= 45) {
                    // Mouth (black)
                    color = SDL_MapRGB(enemySurface->format, 0, 0, 0);
                }
                
                // Set the pixel
                pixels[y * enemySurface->w + x] = color;
            }
        }
        
        SDL_UnlockSurface(enemySurface);
        
        // Create the first frame
        m_enemyTextureFrames[0] = m_textureManager->createTextureFromSurface(enemySurface);
        
        // Create frame 2 (slightly different - eyes narrowed)
        SDL_LockSurface(enemySurface);
        pixels = static_cast<Uint32*>(enemySurface->pixels);
        for (int y = 0; y < enemySurface->h; y++) {
            for (int x = 0; x < enemySurface->w; x++) {
                // Base color (dark red)
                Uint32 color = SDL_MapRGB(enemySurface->format, 180, 0, 0);
                
                // Add some details (eyes, mouth)
                if ((x >= 15 && x <= 25 && y >= 18 && y <= 25) || 
                    (x >= 38 && x <= 48 && y >= 18 && y <= 25)) {
                    // Eyes (yellow) - narrowed
                    color = SDL_MapRGB(enemySurface->format, 255, 255, 0);
                }
                else if (x >= 20 && x <= 44 && y >= 40 && y <= 45) {
                    // Mouth (black)
                    color = SDL_MapRGB(enemySurface->format, 0, 0, 0);
                }
                
                // Set the pixel
                pixels[y * enemySurface->w + x] = color;
            }
        }
        SDL_UnlockSurface(enemySurface);
        m_enemyTextureFrames[1] = m_textureManager->createTextureFromSurface(enemySurface);
        
        // Create frame 3 (mouth open)
        SDL_LockSurface(enemySurface);
        pixels = static_cast<Uint32*>(enemySurface->pixels);
        for (int y = 0; y < enemySurface->h; y++) {
            for (int x = 0; x < enemySurface->w; x++) {
                // Base color (dark red)
                Uint32 color = SDL_MapRGB(enemySurface->format, 180, 0, 0);
                
                // Add some details (eyes, mouth)
                if ((x >= 15 && x <= 25 && y >= 15 && y <= 25) || 
                    (x >= 38 && x <= 48 && y >= 15 && y <= 25)) {
                    // Eyes (yellow)
                    color = SDL_MapRGB(enemySurface->format, 255, 255, 0);
                }
                else if (x >= 20 && x <= 44 && y >= 38 && y <= 48) {
                    // Mouth (black) - open wider
                    color = SDL_MapRGB(enemySurface->format, 0, 0, 0);
                }
                
                // Set the pixel
                pixels[y * enemySurface->w + x] = color;
            }
        }
        SDL_UnlockSurface(enemySurface);
        m_enemyTextureFrames[2] = m_textureManager->createTextureFromSurface(enemySurface);
        
        // Create frame 4 (attacking)
        SDL_LockSurface(enemySurface);
        pixels = static_cast<Uint32*>(enemySurface->pixels);
        for (int y = 0; y < enemySurface->h; y++) {
            for (int x = 0; x < enemySurface->w; x++) {
                // Base color (bright red - angry)
                Uint32 color = SDL_MapRGB(enemySurface->format, 255, 0, 0);
                
                // Add some details (eyes, mouth)
                if ((x >= 15 && x <= 25 && y >= 15 && y <= 25) || 
                    (x >= 38 && x <= 48 && y >= 15 && y <= 25)) {
                    // Eyes (bright yellow)
                    color = SDL_MapRGB(enemySurface->format, 255, 255, 0);
                }
                else if (x >= 15 && x <= 49 && y >= 35 && y <= 50) {
                    // Mouth (black) - wide open with teeth
                    color = SDL_MapRGB(enemySurface->format, 0, 0, 0);
                    
                    // Add teeth
                    if ((y == 35 || y == 36) && 
                        ((x >= 20 && x <= 25) || (x >= 30 && x <= 35) || (x >= 40 && x <= 45))) {
                        color = SDL_MapRGB(enemySurface->format, 255, 255, 255);
                    }
                }
                
                // Set the pixel
                pixels[y * enemySurface->w + x] = color;
            }
        }
        SDL_UnlockSurface(enemySurface);
        m_enemyTextureFrames[3] = m_textureManager->createTextureFromSurface(enemySurface);
        
        // Free the surface
        SDL_FreeSurface(enemySurface);
        
        // Set the main enemy texture to the first frame
        m_enemyTexture = m_enemyTextureFrames[0];
    } else {
        // Fallback to a simple solid texture if surface creation fails
        m_enemyTexture = m_textureManager->createSolidTexture(32, 64, Color(255, 0, 0));
        m_enemyTextureFrames.push_back(m_enemyTexture);
    }
    std::cout << "Enemy texture ID: " << m_enemyTexture << std::endl;
    
    // Load weapon textures with transparency
    std::cout << "Loading weapon textures..." << std::endl;
    
    // Load shotgun
    SDL_Surface* tempSurface = IMG_Load((assetsPath + "shotgun.webp").c_str());
    if (tempSurface) {
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, tempSurface);
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            m_weaponTexture = m_textureManager->addTexture(texture);
        }
        SDL_FreeSurface(tempSurface);
    }
    if (m_weaponTexture < 0) {
        m_weaponTexture = m_textureManager->createSolidTexture(256, 256, Color(128, 128, 128));
    }
    std::cout << "Weapon texture ID: " << m_weaponTexture << std::endl;
    
    // Load machine gun
    tempSurface = IMG_Load((assetsPath + "machine_gun.png").c_str());
    if (tempSurface) {
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, tempSurface);
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            m_machineGunTexture = m_textureManager->addTexture(texture);
        }
        SDL_FreeSurface(tempSurface);
    }
    if (m_machineGunTexture < 0) {
        m_machineGunTexture = m_textureManager->createSolidTexture(256, 256, Color(100, 100, 100));
    }
    std::cout << "Machine gun texture ID: " << m_machineGunTexture << std::endl;
    
    // Set initial weapon texture
    m_currentWeaponTexture = m_weaponTexture;
    
    // Create a variety of wall textures for more interesting maps
    std::cout << "Adding wall texture variations to m_wallTextureVariations" << std::endl;
    m_wallTextureVariations.push_back(m_wallTexture);   // 0: Bloody stone (default)
    m_wallTextureVariations.push_back(runeTextureId);   // 1: Rune texture
    m_wallTextureVariations.push_back(fleshTextureId);  // 2: Flesh texture
    m_wallTextureVariations.push_back(metalTextureId);  // 3: Metal texture
    
    // Share texture variations with the renderer for special rendering (stairs, elevated walls)
    if (m_renderer) {
        for (int textureId : m_wallTextureVariations) {
            m_renderer->addWallTextureVariation(textureId);
        }
    }
    
    // Verify all required textures were created
    bool success = m_wallTexture >= 0 && m_floorTexture >= 0 && m_ceilingTexture >= 0 && 
                  m_bulletTexture >= 0 && m_enemyTexture >= 0 && m_weaponTexture >= 0 && 
                  m_machineGunTexture >= 0 && !m_wallTextureVariations.empty();
    
    if (!success) {
        std::cerr << "Failed to create one or more required textures!" << std::endl;
        return false;
    }
    
    std::cout << "All textures loaded successfully" << std::endl;
    
    // Create Imp texture (based on the Doom Imp)
    std::cout << "Creating Imp texture..." << std::endl;
    
    // Create an array of Imp textures for animation frames
    const int impFrameCount = 4;
    m_impTextureFrames.resize(impFrameCount);
    
    // Create a surface for the Imp sprite sheet
    SDL_Surface* impSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0, 0, 0, 0);
    if (impSurface) {
        SDL_LockSurface(impSurface);
        
        // Create the Imp texture (brownish with spikes)
        Uint32* pixels = static_cast<Uint32*>(impSurface->pixels);
        for (int y = 0; y < impSurface->h; y++) {
            for (int x = 0; x < impSurface->w; x++) {
                // Default color (brown body)
                Uint32 color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                
                // Add eyes (yellow)
                if ((y >= 15 && y <= 20) && 
                    ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                    color = SDL_MapRGB(impSurface->format, 255, 255, 0);
                }
                
                // Add mouth (dark red)
                if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                    color = SDL_MapRGB(impSurface->format, 139, 0, 0);
                }
                
                // Add spikes on head (lighter brown)
                if (y < 15 && (x % 8 < 4) && y > 5) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Add spikes on shoulders
                if ((y >= 20 && y <= 25) && 
                    ((x <= 15) || (x >= 49))) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                pixels[y * impSurface->w + x] = color;
            }
        }
        
        SDL_UnlockSurface(impSurface);
        
        // First frame - standing
        m_impTextureFrames[0] = m_textureManager->createTextureFromSurface(impSurface);
        
        // Second frame - walking 1
        SDL_LockSurface(impSurface);
        pixels = static_cast<Uint32*>(impSurface->pixels);
        for (int y = 0; y < impSurface->h; y++) {
            for (int x = 0; x < impSurface->w; x++) {
                // Default color (brown body)
                Uint32 color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                
                // Add eyes (yellow)
                if ((y >= 15 && y <= 20) && 
                    ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                    color = SDL_MapRGB(impSurface->format, 255, 255, 0);
                }
                
                // Add mouth (dark red)
                if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                    color = SDL_MapRGB(impSurface->format, 139, 0, 0);
                }
                
                // Add spikes on head (lighter brown)
                if (y < 15 && (x % 8 < 4) && y > 5) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Add spikes on shoulders
                if ((y >= 20 && y <= 25) && 
                    ((x <= 15) || (x >= 49))) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Modify legs for walking animation
                if (y >= 45) {
                    if (x < 32) {
                        // Left leg forward
                        if (x >= 20 && x <= 30 && y >= 50) {
                            color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                        } else {
                            color = SDL_MapRGB(impSurface->format, 0, 0, 0);
                        }
                    }
                }
                
                pixels[y * impSurface->w + x] = color;
            }
        }
        
        SDL_UnlockSurface(impSurface);
        m_impTextureFrames[1] = m_textureManager->createTextureFromSurface(impSurface);
        
        // Third frame - walking 2
        SDL_LockSurface(impSurface);
        pixels = static_cast<Uint32*>(impSurface->pixels);
        for (int y = 0; y < impSurface->h; y++) {
            for (int x = 0; x < impSurface->w; x++) {
                // Default color (brown body)
                Uint32 color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                
                // Add eyes (yellow)
                if ((y >= 15 && y <= 20) && 
                    ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                    color = SDL_MapRGB(impSurface->format, 255, 255, 0);
                }
                
                // Add mouth (dark red)
                if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                    color = SDL_MapRGB(impSurface->format, 139, 0, 0);
                }
                
                // Add spikes on head (lighter brown)
                if (y < 15 && (x % 8 < 4) && y > 5) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Add spikes on shoulders
                if ((y >= 20 && y <= 25) && 
                    ((x <= 15) || (x >= 49))) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Modify legs for walking animation
                if (y >= 45) {
                    if (x >= 32) {
                        // Right leg forward
                        if (x >= 33 && x <= 43 && y >= 50) {
                            color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                        } else {
                            color = SDL_MapRGB(impSurface->format, 0, 0, 0);
                        }
                    }
                }
                
                pixels[y * impSurface->w + x] = color;
            }
        }
        
        SDL_UnlockSurface(impSurface);
        m_impTextureFrames[2] = m_textureManager->createTextureFromSurface(impSurface);
        
        // Fourth frame - attack
        SDL_LockSurface(impSurface);
        pixels = static_cast<Uint32*>(impSurface->pixels);
        for (int y = 0; y < impSurface->h; y++) {
            for (int x = 0; x < impSurface->w; x++) {
                // Default color (brown body)
                Uint32 color = SDL_MapRGB(impSurface->format, 139, 69, 19);
                
                // Add eyes (red for attack)
                if ((y >= 15 && y <= 20) && 
                    ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                    color = SDL_MapRGB(impSurface->format, 255, 0, 0);
                }
                
                // Add mouth (bright red for attack)
                if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                    color = SDL_MapRGB(impSurface->format, 255, 0, 0);
                }
                
                // Add spikes on head (lighter brown)
                if (y < 15 && (x % 8 < 4) && y > 5) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Add spikes on shoulders
                if ((y >= 20 && y <= 25) && 
                    ((x <= 15) || (x >= 49))) {
                    color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                }
                
                // Add fireball effect
                if ((y >= 25 && y <= 40) && (x >= 15 && x <= 48)) {
                    // Calculate distance from center of fireball
                    int centerX = 32;
                    int centerY = 32;
                    double dist = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                    
                    if (dist < 10) {
                        // Inner fireball (bright orange)
                        color = SDL_MapRGB(impSurface->format, 255, 165, 0);
                    } else if (dist < 15) {
                        // Outer fireball (red)
                        color = SDL_MapRGB(impSurface->format, 255, 0, 0);
                    }
                }
                
                pixels[y * impSurface->w + x] = color;
            }
        }
        
        SDL_UnlockSurface(impSurface);
        m_impTextureFrames[3] = m_textureManager->createTextureFromSurface(impSurface);
        
        // Free the surface
        SDL_FreeSurface(impSurface);
        
        // Set the main Imp texture to the first frame
        m_impTexture = m_impTextureFrames[0];
    } else {
        // Fallback to a simple solid texture if surface creation fails
        m_impTexture = m_textureManager->createSolidTexture(32, 64, Color(139, 69, 19));
        m_impTextureFrames.push_back(m_impTexture);
    }
    
    std::cout << "Imp texture ID: " << m_impTexture << std::endl;
    
    return true;
}

void Engine::setupMap() {
    // Reset the map instead of clear
    for (int x = 0; x < m_map.getWidth(); x++) {
        for (int y = 0; y < m_map.getHeight(); y++) {
            m_map.setCell(x, y, CellType::Empty);
            m_map.setCellElevation(x, y, 0);
        }
    }
    
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Define the second level area
    int secondLevelStartX = m_map.getWidth() / 2;
    int secondLevelStartY = 0;
    int secondLevelWidth = m_map.getWidth() / 2;
    int secondLevelHeight = m_map.getHeight() / 2;
    
    // Create the second level with proper walls and floor
    for (int x = secondLevelStartX; x < secondLevelStartX + secondLevelWidth; x++) {
        for (int y = secondLevelStartY; y < secondLevelStartY + secondLevelHeight; y++) {
            // Set all cells in this region to elevated floor
            m_map.setCell(x, y, CellType::Empty); // Use Empty instead of ElevatedFloor
            m_map.setCellElevation(x, y, 1); // Set elevation to 1 (second level)
            
            // Create walls around the perimeter of the second level
            if (x == secondLevelStartX || x == secondLevelStartX + secondLevelWidth - 1 ||
                y == secondLevelStartY || y == secondLevelStartY + secondLevelHeight - 1) {
                
                // Check for stair entrance/exit points
                bool isStairEntrance = false;
                
                // North staircase entrance - about 1/3 from the left
                if (y == secondLevelStartY && 
                    x >= secondLevelStartX + secondLevelWidth/3 - 1 && 
                    x <= secondLevelStartX + secondLevelWidth/3 + 1) {
                    isStairEntrance = true;
                }
                
                // East staircase entrance - about middle
                if (x == secondLevelStartX + secondLevelWidth - 1 && 
                    y >= secondLevelStartY + secondLevelHeight/2 - 1 && 
                    y <= secondLevelStartY + secondLevelHeight/2 + 1) {
                    isStairEntrance = true;
                }
                
                if (!isStairEntrance) {
                    m_map.setCell(x, y, CellType::Wall); // Use Wall instead of ElevatedWall
                    if (!m_wallTextureVariations.empty() && m_wallTextureVariations.size() > 1) {
                        m_map.setWallTexture(x, y, m_wallTextureVariations[1]); // Rune texture for edges
                    }
                }
            }
            // Internal structures for second level
            else {
                // Create some internal wall patterns on the second level
                bool createSecondLevelWall = false;
                
                // Horizontal corridors
                if ((y - secondLevelStartY) % 5 == 0 && x > secondLevelStartX && 
                    x < secondLevelStartX + secondLevelWidth - 1) {
                    if ((x - secondLevelStartX) % 8 != 3 && (x - secondLevelStartX) % 8 != 4) {
                        createSecondLevelWall = true;
                    }
                }
                
                // Vertical corridors
                if ((x - secondLevelStartX) % 8 == 0 && y > secondLevelStartY && 
                    y < secondLevelStartY + secondLevelHeight - 1) {
                    if ((y - secondLevelStartY) % 5 != 2 && (y - secondLevelStartY) % 5 != 3) {
                        createSecondLevelWall = true;
                    }
                }
                
                // Central chamber or boss area
                int centerX = secondLevelStartX + secondLevelWidth / 2;
                int centerY = secondLevelStartY + secondLevelHeight / 2;
                int chamberSize = 5;
                
                if (abs(x - centerX) <= chamberSize && abs(y - centerY) <= chamberSize) {
                    // Inside the central chamber
                    if (abs(x - centerX) == chamberSize || abs(y - centerY) == chamberSize) {
                        // Chamber walls
                        createSecondLevelWall = true;
                        
                        // Add doorways to the chamber
                        if ((x == centerX && abs(y - centerY) == chamberSize) ||
                            (y == centerY && abs(x - centerX) == chamberSize)) {
                            createSecondLevelWall = false;
                        }
                    }
                    
                    // Add some enemies in the central chamber
                    if (!createSecondLevelWall && 
                        abs(x - centerX) < chamberSize - 1 && 
                        abs(y - centerY) < chamberSize - 1) {
                        // Increase spawn chance from 20% to 40% for more enemies
                        if (rand() % 5 <= 1) {
                            // Make all enemies in the central chamber Imps for a more challenging boss area
                            m_map.setCell(x, y, CellType::Enemy);
                            m_map.setCellElevation(x, y, 1); // Keep elevation at level 2
                        }
                    }
                }
                
                if (createSecondLevelWall) {
                    m_map.setCell(x, y, CellType::Wall); // Use Wall instead of ElevatedWall
                    
                    // Use a different texture for internal walls
                    if (!m_wallTextureVariations.empty() && m_wallTextureVariations.size() > 2) {
                        m_map.setWallTexture(x, y, m_wallTextureVariations[2]); // Use third texture if available
                    } else if (!m_wallTextureVariations.empty()) {
                        m_map.setWallTexture(x, y, m_wallTextureVariations[0]); // Use first texture otherwise
                    }
                }
            }
        }
    }
    
    // Create staircases connecting the levels - manually create them instead of using createStaircase
    // First staircase - from bottom left to top right (North entrance)
    int stair1StartX = secondLevelStartX + secondLevelWidth/3 - 5;
    int stair1StartY = secondLevelStartY + secondLevelHeight;
    int stair1EndX = secondLevelStartX + secondLevelWidth/3;
    int stair1EndY = secondLevelStartY;
    
    // Create a diagonal staircase from start to end
    int stairLength = std::max(abs(stair1EndX - stair1StartX), abs(stair1EndY - stair1StartY));
    for (int i = 0; i < stairLength; i++) {
        int x = stair1StartX + (stair1EndX - stair1StartX) * i / stairLength;
        int y = stair1StartY + (stair1EndY - stair1StartY) * i / stairLength;
        
        // Create stair steps
        m_map.setCell(x, y, CellType::Stairs);
        
        // Set step height based on position along the staircase
        float stepHeight = static_cast<float>(i) / stairLength;
        m_map.setStepHeight(x, y, stepHeight);
        
        // Set appropriate elevation
        if (i < stairLength / 2) {
            m_map.setCellElevation(x, y, 0);
        } else {
            m_map.setCellElevation(x, y, 1);
        }
    }
    
    // Second staircase - from right middle to east entrance
    int stair2StartX = secondLevelStartX + secondLevelWidth;
    int stair2StartY = secondLevelStartY + secondLevelHeight/2 + 5;
    int stair2EndX = secondLevelStartX + secondLevelWidth - 1;
    int stair2EndY = secondLevelStartY + secondLevelHeight/2;
    
    // Create a diagonal staircase from start to end
    stairLength = std::max(abs(stair2EndX - stair2StartX), abs(stair2EndY - stair2StartY));
    for (int i = 0; i < stairLength; i++) {
        int x = stair2StartX + (stair2EndX - stair2StartX) * i / stairLength;
        int y = stair2StartY + (stair2EndY - stair2StartY) * i / stairLength;
        
        // Create stair steps
        m_map.setCell(x, y, CellType::Stairs);
        
        // Set step height based on position along the staircase
        float stepHeight = static_cast<float>(i) / stairLength;
        m_map.setStepHeight(x, y, stepHeight);
        
        // Set appropriate elevation
        if (i < stairLength / 2) {
            m_map.setCellElevation(x, y, 0);
        } else {
            m_map.setCellElevation(x, y, 1);
        }
    }
    
    // Add lights at key locations using the lighting system instead of addLight
    if (m_renderer) {
        // Light at the north staircase entrance
        Light northLight = Light::createPointLight(
            Vec2(stair1EndX, stair1EndY + 1),
            Color(255, 204, 153),  // Warm light
            1.15f,                 // Intensity increased by 15%
            9.2f                   // Radius increased by 15%
        );
        m_renderer->getLightingSystem().addLight(northLight);
        
        // Light at the east staircase entrance
        Light eastLight = Light::createPointLight(
            Vec2(stair2EndX - 1, stair2EndY),
            Color(153, 204, 255),  // Cool light
            1.15f,                 // Intensity increased by 15%
            9.2f                   // Radius increased by 15%
        );
        m_renderer->getLightingSystem().addLight(eastLight);
        
        // Light in the central chamber
        Light centerLight = Light::createPointLight(
            Vec2(secondLevelStartX + secondLevelWidth/2, secondLevelStartY + secondLevelHeight/2),
            Color(204, 102, 230),  // Purple light
            1.15f,                 // Intensity increased by 15%
            11.5f                  // Radius increased by 15%
        );
        m_renderer->getLightingSystem().addLight(centerLight);
        
        // Add some additional lights throughout the second level
        for (int i = 0; i < 8; i++) {
            int lightX = secondLevelStartX + rand() % secondLevelWidth;
            int lightY = secondLevelStartY + rand() % secondLevelHeight;
            
            // Only place lights on floor cells (empty cells at elevation 1)
            if (m_map.getCell(lightX, lightY) == CellType::Empty && 
                m_map.getCellElevation(lightX, lightY) == 1) {
                
                float r = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                float g = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                float b = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                
                Light randomLight = Light::createPointLight(
                    Vec2(lightX, lightY),
                    Color(r * 255, g * 255, b * 255),
                    0.92f,                 // Intensity increased by 15%
                    5.75f                  // Radius increased by 15%
                );
                m_renderer->getLightingSystem().addLight(randomLight);
            }
        }
    }
    
    // Create the ground level map
    for (int x = 0; x < m_map.getWidth(); x++) {
        for (int y = 0; y < m_map.getHeight(); y++) {
            // Skip cells that are already part of the second level
            if (x >= secondLevelStartX && x < secondLevelStartX + secondLevelWidth &&
                y >= secondLevelStartY && y < secondLevelStartY + secondLevelHeight) {
                continue;
            }
            
            // Skip cells that are already part of the staircases
            if (m_map.getCell(x, y) == CellType::Stairs) {
                continue;
            }
            
            // Set default cell type to empty
            m_map.setCell(x, y, CellType::Empty); // Use Empty instead of Floor
            m_map.setCellElevation(x, y, 0); // Ground level
            
            // Create walls around the perimeter of the map
            if (x == 0 || x == m_map.getWidth() - 1 || y == 0 || y == m_map.getHeight() - 1) {
                m_map.setCell(x, y, CellType::Wall);
                
                // Use a different texture for perimeter walls
                if (!m_wallTextureVariations.empty()) {
                    m_map.setWallTexture(x, y, m_wallTextureVariations[0]);
                }
            }
            // Create internal walls with a maze-like pattern
            else {
                bool createWall = false;
                
                // Different patterns for different quadrants
                if (x < m_map.getWidth()/2 && y < m_map.getHeight()/2) {
                    // Top-left quadrant: grid pattern
                    if ((x % 8 == 0 || y % 8 == 0) && (x % 8 != 4 && y % 8 != 4)) {
                        createWall = true;
                    }
                }
                else if (x >= m_map.getWidth()/2 && y < m_map.getHeight()/2) {
                    // Top-right quadrant: already handled as second level
                    continue;
                }
                else if (x < m_map.getWidth()/2 && y >= m_map.getHeight()/2) {
                    // Bottom-left quadrant: random walls
                    createWall = rand() % 3 == 0;
                }
                else {
                    // Bottom-right quadrant: circular pattern
                    int centerX = 3 * m_map.getWidth() / 4;
                    int centerY = 3 * m_map.getHeight() / 4;
                    int distance = static_cast<int>(sqrt(pow(x - centerX, 2) + pow(y - centerY, 2)));
                    
                    if (distance % 5 == 0) {
                        createWall = true;
                    }
                }
                
                // Don't create walls near staircase entrances
                bool nearStair1 = (abs(x - stair1StartX) < 3 && abs(y - stair1StartY) < 3);
                bool nearStair2 = (abs(x - stair2StartX) < 3 && abs(y - stair2StartY) < 3);
                
                if (createWall && !nearStair1 && !nearStair2) {
                    m_map.setCell(x, y, CellType::Wall);
                    
                    // Randomly assign wall textures
                    if (!m_wallTextureVariations.empty()) {
                        int textureIndex = rand() % m_wallTextureVariations.size();
                        m_map.setWallTexture(x, y, m_wallTextureVariations[textureIndex]);
                    }
                }
            }
        }
    }
    
    // Add some items and enemies to the ground level
    for (int i = 0; i < 40; i++) {
        int x = rand() % m_map.getWidth();
        int y = rand() % m_map.getHeight();
        
        // Only place items on empty cells at ground level
        if (m_map.getCell(x, y) == CellType::Empty && m_map.getCellElevation(x, y) == 0) {
            // 30% chance for item, 70% chance for enemy (increased enemy ratio)
            if (rand() % 10 < 3) {
                m_map.setCell(x, y, CellType::Item);
            } else {
                m_map.setCell(x, y, CellType::Enemy);
            }
        }
    }
    
    // Create a special area with only Imps in the bottom-right quadrant
    int impAreaCenterX = 3 * m_map.getWidth() / 4;
    int impAreaCenterY = 3 * m_map.getHeight() / 4;
    int impAreaRadius = 5;
    
    // Add a cluster of Imps in this area
    for (int i = 0; i < 8; i++) {
        // Random position within the imp area
        int offsetX = rand() % (impAreaRadius * 2) - impAreaRadius;
        int offsetY = rand() % (impAreaRadius * 2) - impAreaRadius;
        
        int x = impAreaCenterX + offsetX;
        int y = impAreaCenterY + offsetY;
        
        // Make sure the position is valid
        if (x >= 0 && x < m_map.getWidth() && y >= 0 && y < m_map.getHeight()) {
            // Only place on empty cells
            if (m_map.getCell(x, y) == CellType::Empty && m_map.getCellElevation(x, y) == 0) {
                m_map.setCell(x, y, CellType::Enemy);
            }
        }
    }
    
    // Add a special light in the Imp area
    if (m_renderer) {
        Light impAreaLight = Light::createPointLight(
            Vec2(impAreaCenterX, impAreaCenterY),
            Color(255, 100, 0),  // Orange-red light for the Imp area
            1.3f,                // Higher intensity
            12.0f                // Larger radius
        );
        m_renderer->getLightingSystem().addLight(impAreaLight);
    }
    
    // Add lights to the ground level using the lighting system
    if (m_renderer) {
        // Increased from 10 to 15 lights
        for (int i = 0; i < 15; i++) {
            int x = rand() % m_map.getWidth();
            int y = rand() % m_map.getHeight();
            
            // Only place lights on empty cells at ground level
            if (m_map.getCell(x, y) == CellType::Empty && m_map.getCellElevation(x, y) == 0) {
                float r = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                float g = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                float b = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
                
                Light groundLight = Light::createPointLight(
                    Vec2(x, y),
                    Color(r * 255, g * 255, b * 255),
                    0.92f,                 // Intensity increased by 15%
                    6.0f                   // Radius
                );
                m_renderer->getLightingSystem().addLight(groundLight);
            }
        }
        
        // Add lights at staircase entrances
        Light stair1Light = Light::createPointLight(
            Vec2(stair1StartX, stair1StartY),
            Color(255, 204, 153),  // Warm light
            1.0f,                  // Intensity
            8.0f                   // Radius
        );
        m_renderer->getLightingSystem().addLight(stair1Light);
        
        Light stair2Light = Light::createPointLight(
            Vec2(stair2StartX, stair2StartY),
            Color(153, 204, 255),  // Cool light
            1.0f,                  // Intensity
            8.0f                   // Radius
        );
        m_renderer->getLightingSystem().addLight(stair2Light);
    }
    
    // Set player starting position
    m_player.init(m_map.getWidth() / 4, m_map.getHeight() / 4, 1.0, 0.0);
    
    // Create sprites from map cells
    createSpritesFromMap();
}

// Create sprite objects from map cells marked as Enemy or Item
void Engine::createSpritesFromMap() {
    if (!m_spriteManager) return;
    
    // Clear existing sprites first
    m_spriteManager->clearSprites();
    
    // Iterate through the map
    for (int x = 0; x < m_map.getWidth(); x++) {
        for (int y = 0; y < m_map.getHeight(); y++) {
            CellType cellType = m_map.getCell(x, y);
            
            if (cellType == CellType::Enemy) {
                // Randomly decide if this should be a regular enemy or an Imp (1/2 chance for Imp - increased from 1/3)
                bool createImp = (rand() % 2 == 0);
                
                if (createImp && !m_impTextureFrames.empty()) {
                    // Create an Imp enemy sprite
                    double size = 0.9; // Imps are slightly larger
                    int textureId = m_impTexture; // Use the Imp texture
                    int spriteId = m_spriteManager->addSprite(x + 0.5, y + 0.5, size, textureId, SpriteType::ImpEnemy);
                    
                    // Set up animation for the Imp
                    if (spriteId >= 0) {
                        Sprite* imp = m_spriteManager->getSprite(spriteId);
                        if (imp) {
                            // Set up animation with frames at 2 frames per second
                            imp->setAnimated(true, m_impTextureFrames.size(), 2.0);
                            
                            // Set movement properties
                            imp->setMoveSpeed(2.0); // Units per second - faster than regular enemies
                            imp->setTurnSpeed(2.5); // Radians per second
                            
                            // Set health - Imps are tougher
                            imp->setHealth(150.0);
                            
                            // Randomly assign a movement type
                            ImpMovementType movementType = static_cast<ImpMovementType>(rand() % 3);
                            imp->setImpMovementType(movementType);
                        }
                    }
                } else {
                    // Create a regular enemy sprite
                    double size = 0.8; // Standard enemy size
                    int textureId = m_enemyTexture; // Use the enemy texture
                    int spriteId = m_spriteManager->addSprite(x + 0.5, y + 0.5, size, textureId, SpriteType::Enemy);
                    
                    // Set up animation for the enemy
                    if (spriteId >= 0 && !m_enemyTextureFrames.empty()) {
                        Sprite* enemy = m_spriteManager->getSprite(spriteId);
                        if (enemy) {
                            // Set up animation with 4 frames at 2 frames per second
                            enemy->setAnimated(true, m_enemyTextureFrames.size(), 2.0);
                            
                            // Set movement properties
                            enemy->setMoveSpeed(1.5); // Units per second
                            enemy->setTurnSpeed(2.0); // Radians per second
                            
                            // Set health
                            enemy->setHealth(100.0);
                        }
                    }
                }
                
                // Clear the cell so we don't have both a cell and a sprite
                m_map.setCell(x, y, CellType::Empty);
            }
            else if (cellType == CellType::Item) {
                // Create an item sprite
                double size = 0.5; // Items are smaller
                int textureId = 3; // Use item texture (adjust as needed)
                m_spriteManager->addSprite(x + 0.5, y + 0.5, size, textureId, SpriteType::Item);
                
                // Clear the cell so we don't have both a cell and a sprite
                m_map.setCell(x, y, CellType::Empty);
            }
        }
    }
}

void Engine::setupPlayer() {
    std::cout << "Setting up player..." << std::endl;
    
    // Initialize player in the middle of the map, facing east
    m_player.init(m_map.getWidth() / 2.0, m_map.getHeight() / 2.0, 1.0, 0.0);
    m_player.setMoveSpeed(3.0);
    m_player.setRotSpeed(3.0);
    m_player.setHealth(100.0);
    m_player.setAmmo(50);
    
    // Connect player to projectile manager
    m_player.setProjectileManager(m_projectileManager);
}

void Engine::setupInput() {
    std::cout << "Setting up input controls..." << std::endl;
    
    // Initialize input handler
    m_inputHandler.init();
    
    // Bind keys to actions
    m_inputHandler.bindKey(SDL_SCANCODE_W, InputAction::MoveForward);
    m_inputHandler.bindKey(SDL_SCANCODE_S, InputAction::MoveBackward);
    m_inputHandler.bindKey(SDL_SCANCODE_A, InputAction::StrafeLeft);
    m_inputHandler.bindKey(SDL_SCANCODE_D, InputAction::StrafeRight);
    m_inputHandler.bindKey(SDL_SCANCODE_LEFT, InputAction::RotateLeft);
    m_inputHandler.bindKey(SDL_SCANCODE_RIGHT, InputAction::RotateRight);
    m_inputHandler.bindKey(SDL_SCANCODE_SPACE, InputAction::Fire);
    m_inputHandler.bindKey(SDL_SCANCODE_R, InputAction::Reload);
    m_inputHandler.bindKey(SDL_SCANCODE_ESCAPE, InputAction::Menu);
    m_inputHandler.bindKey(SDL_SCANCODE_Q, InputAction::Quit);
    m_inputHandler.bindKey(SDL_SCANCODE_F1, InputAction::ToggleFPS);
    m_inputHandler.bindKey(SDL_SCANCODE_F2, InputAction::ToggleMinimap);
    m_inputHandler.bindKey(SDL_SCANCODE_F3, InputAction::ToggleWeapon);
    
    // Audio control keys
    m_inputHandler.bindKey(SDL_SCANCODE_M, InputAction::ToggleMusic);
    m_inputHandler.bindKey(SDL_SCANCODE_PAGEUP, InputAction::IncreaseMusicVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_PAGEDOWN, InputAction::DecreaseMusicVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_HOME, InputAction::IncreaseSfxVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_END, InputAction::DecreaseSfxVolume);
    
    // Enable mouse capture for looking around
    m_inputHandler.setMouseCapture(true);
}

void Engine::toggleMusic() {
    if (!m_audioSystem) return;
    
    m_musicEnabled = !m_musicEnabled;
    
    if (m_musicEnabled) {
        m_audioSystem->resumeMusic();
    } else {
        m_audioSystem->pauseMusic();
    }
    
    // Show notification
    std::string message = m_musicEnabled ? "Music: On" : "Music: Off";
    showNotification(message, 2.0);
}

void Engine::setMusicVolume(int volume) {
    if (!m_audioSystem) return;
    
    m_audioSystem->setMusicVolume(volume);
    
    // Show notification
    std::string message = "Music Volume: " + std::to_string(volume * 100 / MIX_MAX_VOLUME) + "%";
    showNotification(message, 2.0);
}

void Engine::setSfxVolume(int volume) {
    if (!m_audioSystem) return;
    
    m_audioSystem->setSfxVolume(volume);
    
    // Show notification
    std::string message = "SFX Volume: " + std::to_string(volume * 100 / MIX_MAX_VOLUME) + "%";
    showNotification(message, 2.0);
}

bool Engine::isMusicPlaying() const {
    if (!m_audioSystem) return false;
    return m_audioSystem->isMusicPlaying();
}

void Engine::showNotification(const std::string& text, double duration) {
    m_notificationText = text;
    m_notificationTimer = duration;
    
    // Clean up any existing notification texture
    if (m_notificationTexture) {
        SDL_DestroyTexture(m_notificationTexture);
        m_notificationTexture = nullptr;
    }
} 