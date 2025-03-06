#include "engine.h"
#include <iostream>
#include <random>
#include <cmath>

Engine::Engine(int screenWidth, int screenHeight)
    : m_window(nullptr)
    , m_sdlRenderer(nullptr)
    , m_renderer(nullptr)
    , m_cudaRenderer(nullptr)
    , m_textureManager(nullptr)
    , m_spriteManager(nullptr)
    , m_projectileManager(nullptr)
    , m_audioSystem(nullptr)
    , m_gameState(GameState::MainMenu)
    , m_running(false)
    , m_musicEnabled(true)
    , m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_lastFrameTime(0)
    , m_deltaTime(0.0)
    , m_weaponRecoil(0.0)
    , m_flashIntensity(0.0)
    , m_weaponRecoilRecovery(10.0)
    , m_flashDecay(4.0)
    , m_fullscreen(false)
    , m_targetFPS(60)
    , m_frameTime(1.0 / 60.0)
    , m_notificationText("")
    , m_notificationDuration(0.0)
    , m_notificationTimer(0.0)
    , m_notificationTexture(nullptr)
    , m_prevMouseLeftDown(false)
    , m_useCuda(false)
    , m_rocketLauncherTexture(-1)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Check if CUDA is available
    m_useCuda = CudaRenderer::isCudaAvailable();
    
    if (m_useCuda) {
        std::cout << "CUDA is available, using CUDA renderer" << std::endl;
    } else {
        std::cout << "CUDA is not available, using CPU renderer" << std::endl;
    }
    
    // We'll initialize the managers in the init method after creating the SDL renderer
    m_renderer = new Renderer();
    
    // Initialize projectile manager
    m_projectileManager = new ProjectileManager();
    
    // Initialize input handler
    m_inputHandler.init();
    
    // Initialize map with default size
    m_map = Map(50, 50);
    
    // Initialize notification system
    m_notificationText = "";
    m_notificationTimer = 0.0;
    m_notificationDuration = 0.0;
    m_notificationTexture = nullptr;
    
    // Initialize key state tracking
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        m_prevKeyboardState[static_cast<SDL_Scancode>(i)] = false;
    }
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
    m_renderer->init(m_screenWidth, m_screenHeight, m_fullscreen);
    m_renderer->setSDLRenderer(m_sdlRenderer);
    m_renderer->setTextureManager(m_textureManager);
    m_renderer->setSpriteManager(m_spriteManager);
    m_renderer->setProjectileManager(m_projectileManager);
    m_renderer->setEngine(this);  // Set the engine reference
    std::cout << "Renderer initialized: " << m_renderer << std::endl;
    
    // Initialize CUDA renderer if available
    if (m_useCuda) {
        m_cudaRenderer = new CudaRenderer();
        if (!m_cudaRenderer->init(screenWidth, screenHeight, m_sdlRenderer, m_textureManager)) {
            std::cerr << "Failed to initialize CUDA renderer, falling back to CPU renderer" << std::endl;
            m_useCuda = false;
            delete m_cudaRenderer;
            m_cudaRenderer = nullptr;
        } else {
            // Disable muzzle flash when using CUDA to prevent the yellow circle
            m_renderer->toggleMuzzleFlash(); // This will enable it since it's disabled by default
        }
    } else {
        // Enable muzzle flash for CPU renderer
        m_renderer->toggleMuzzleFlash();
    }
    
    // Create sprite manager
    if (m_spriteManager) {
        delete m_spriteManager;
    }
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
    
    // Set running flag
    m_running = true;
    
    // Initialize audio system
    if (!m_audioSystem) {
        m_audioSystem = new AudioSystem();
        if (!m_audioSystem->init()) {
            std::cerr << "Failed to initialize audio system!" << std::endl;
            // Continue anyway, audio is not critical
        } else {
            std::cout << "Audio system initialized: " << m_audioSystem << std::endl;
            
            // Check if we're running in WSL
            bool isWSL = false;
            #ifdef __linux__
            FILE* fp = fopen("/proc/version", "r");
            if (fp) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), fp)) {
                    if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                        isWSL = true;
                    }
                }
                fclose(fp);
            }
            #endif
            
            if (isWSL) {
                // For WSL, use PulseAudio for best MIDI quality
                if (!m_audioSystem->isPulseAudioEnabled()) {
                    std::cout << "Configuring PulseAudio for WSL..." << std::endl;
                    if (m_audioSystem->configurePulseAudio(true)) {
                        std::cout << "PulseAudio configured successfully!" << std::endl;
                    } else {
                        std::cout << "PulseAudio configuration failed, will use default WSL audio" << std::endl;
                        // Apply WSL-specific configuration as fallback
                        m_audioSystem->configureTimidityForWSL();
                    }
                }
            } else {
                // On native Windows, use native MIDI
                m_audioSystem->forceNativeMidi(true);
            }
        }
    }
    
    // Load all game assets (including sounds)
    if (!loadAssets()) {
        std::cerr << "Failed to load game assets!" << std::endl;
        return false;
    }
    std::cout << "Game assets loaded successfully" << std::endl;
    
    // Load and play the background music
    if (m_audioSystem) {
        std::string musicPath = "assets/music/M_E1M1.mid";
        if (m_audioSystem->loadMusic(musicPath)) {
            // Configure MIDI quality with higher frequency for better sound
            m_audioSystem->configureMidiQuality(48000);  // Higher frequency for better MIDI synthesis
            
            // Display MIDI backend information
            std::string midiInfo = m_audioSystem->getMidiBackendInfo();
            std::cout << "[ENGINE] " << midiInfo << std::endl;
            
            // Show a notification about which MIDI backend is being used
            showNotification(midiInfo, 5.0);  // Show for 5 seconds
            
            if (m_musicEnabled) {
                m_audioSystem->playMusic(true); // Loop the music
            }
        } else {
            std::cerr << "Failed to load music: " << musicPath << std::endl;
        }
    }
    
    std::cout << "Engine initialization complete!" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    // Set up menu
    m_menuItems = {
        "Play Game",
        "Save Game",
        "Load Game",
        "Exit"
    };
    m_menuSelection = 0;
    
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
    
    // Clean up renderer
    if (m_renderer) {
        m_renderer->cleanup();
        delete m_renderer;
        m_renderer = nullptr;
    }
    
    // Clean up CUDA renderer
    if (m_cudaRenderer) {
        m_cudaRenderer->cleanup();
        delete m_cudaRenderer;
        m_cudaRenderer = nullptr;
    }
    
    // Clean up texture manager
    if (m_textureManager) {
        delete m_textureManager;
        m_textureManager = nullptr;
    }
    
    // Clean up sprite manager
    if (m_spriteManager) {
        delete m_spriteManager;
        m_spriteManager = nullptr;
    }
    
    // Clean up projectile manager
    if (m_projectileManager) {
        delete m_projectileManager;
        m_projectileManager = nullptr;
    }
    
    // Clean up audio system
    if (m_audioSystem) {
        m_audioSystem->cleanup();
        delete m_audioSystem;
        m_audioSystem = nullptr;
    }
    
    // Clean up SDL resources
    if (m_sdlRenderer) {
        SDL_DestroyRenderer(m_sdlRenderer);
        m_sdlRenderer = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
    // Clean up notification texture
    if (m_notificationTexture) {
        SDL_DestroyTexture(m_notificationTexture);
        m_notificationTexture = nullptr;
    }
    
    // Quit SDL
    SDL_Quit();
    
    std::cout << "Engine shutdown complete" << std::endl;
}

void Engine::setState(GameState state) {
    m_gameState = state;
    
    // Handle state transitions
    switch (m_gameState) {
        case GameState::MainMenu:
            std::cout << "Entering main menu" << std::endl;
            // Reset menu selection when entering main menu
            m_menuSelection = 0;
            break;
            
        case GameState::Playing:
            std::cout << "Starting gameplay" << std::endl;
            break;
            
        case GameState::Paused:
            std::cout << "Game paused" << std::endl;
            // Reset menu selection when entering pause menu
            m_menuSelection = 0;
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
    // Update input handler at the beginning of input processing
    // This is critical for ensuring key state changes are properly detected
    m_inputHandler.update();
    
    // Handle SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let the input handler process the event first
        m_inputHandler.handleEvent(event);
        
        // Handle quit events
        if (event.type == SDL_QUIT) {
            m_running = false;
        }
        
        // Handle key events for special functions
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    // Toggle menu
                    if (m_gameState == GameState::Playing) {
                        setState(GameState::Paused);
                    } else if (m_gameState == GameState::Paused) {
                        setState(GameState::Playing);
                    } else if (m_gameState == GameState::MainMenu) {
                        m_running = false;
                    }
                    break;
                    
                case SDLK_r:
                    if (m_gameState == GameState::GameOver) {
                        restartGame();
                    }
                    break;
                    
                case SDLK_F9:  // Test sound effects with F9 key
                    testSoundEffects();
                    break;
            }
        }
    }
    
    // Handle input based on game state
    switch (m_gameState) {
        case GameState::MainMenu:
            handleMainMenuInput();
            break;
            
        case GameState::Playing:
            handlePlayingInput();
            break;
            
        case GameState::Paused:
            handlePausedInput();
            break;
            
        case GameState::GameOver:
        case GameState::Victory:
            // Get keyboard state
            {
                int numKeys;
                const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
                
                // Check for Enter key to return to main menu
                if (keyboardState[SDL_SCANCODE_RETURN]) {
                    setState(GameState::MainMenu);
                }
            }
            break;
    }
    
    // Handle global input actions
    if (m_inputHandler.isActionTriggered(InputAction::Quit, m_gameState)) {
        m_running = false;
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::ToggleMusic, m_gameState)) {
        toggleMusic();
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::IncreaseMusicVolume, m_gameState)) {
        int volume = m_audioSystem->getMusicVolume() + 10;
        setMusicVolume(volume);
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::DecreaseMusicVolume, m_gameState)) {
        int volume = m_audioSystem->getMusicVolume() - 10;
        setMusicVolume(volume);
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::IncreaseSfxVolume, m_gameState)) {
        int volume = m_audioSystem->getSfxVolume() + 10;
        setSfxVolume(volume);
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::DecreaseSfxVolume, m_gameState)) {
        int volume = m_audioSystem->getSfxVolume() - 10;
        setSfxVolume(volume);
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::EnhanceMidiQuality, m_gameState)) {
        enhanceMidiQuality();
    }
    
    // Debug actions
    if (m_inputHandler.isActionTriggered(InputAction::TestSound, m_gameState)) {
        testSoundEffects();
    }
    
    if (m_inputHandler.isActionTriggered(InputAction::TestWeapons, m_gameState)) {
        testWeapons();
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
        
        // Update sector visibility based on player position
        m_map.updateVisibility(m_player.getPosition());
    }
    
    // Note: Input handler is now updated at the beginning of processInput()
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
    // Debug check to ensure weapon texture wasn't reset unexpectedly
    static int lastWeaponTexture = -1;
    if (lastWeaponTexture != m_currentWeaponTexture) {
        std::cout << "Weapon texture changed from " << lastWeaponTexture << " to " << m_currentWeaponTexture << std::endl;
        lastWeaponTexture = m_currentWeaponTexture;
    }
    
    // Clear screen
    SDL_SetRenderDrawColor(m_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_sdlRenderer);
    
    // Render based on game state
    switch (m_gameState) {
        case GameState::MainMenu:
            // Render the main menu
            renderMainMenu();
            break;
            
        case GameState::Playing:
            // Render the game
            if (m_useCuda && m_cudaRenderer) {
                // Get the ray-casting buffer from CUDA without rendering it directly
                // This returns a frame buffer that we can render ourselves
                m_cudaRenderer->generateFrame(m_map, m_player);
                
                // Now manually render the frame buffer to the screen
                // This gives us complete control over the rendering order
                m_cudaRenderer->blitFrameBuffer();
                
                // Now render all UI elements on top
                if (m_renderer->isShowingWeapon()) {
                    
                    // Verify we're using the right texture
                    int textureToUse = m_currentWeaponTexture;
                    
                    // Safety check - if somehow m_currentWeaponTexture is invalid, use a fallback
                    if (textureToUse != m_weaponTexture && 
                        textureToUse != m_machineGunTexture && 
                        textureToUse != m_rocketLauncherTexture) {
                        std::cout << "WARNING: Invalid current weapon texture ID! Defaulting to pistol." << std::endl;
                        textureToUse = m_weaponTexture;
                        m_currentWeaponTexture = m_weaponTexture; // Fix the variable too
                    }
                    
                    // When using CUDA, pass 0.0 for flashIntensity to avoid the muzzle flash effect
                    m_renderer->renderWeapon(m_player, m_weaponRecoil, 0.0, textureToUse);
                }
                
                // Render sprites
                m_renderer->renderSprites(m_map, m_player);
                
                // Render projectiles
                m_renderer->renderProjectiles(m_player);
                
                // Render UI elements
                m_renderer->renderUI(m_player);
                
                // Render notification if active
                renderNotification();
            } else {
                // Use CPU renderer
                m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
                if (m_renderer->isShowingWeapon()) {
                    std::cout << "CPU mode: About to render weapon with texture ID: " << m_currentWeaponTexture << std::endl;
                    std::cout << "  Pistol ID: " << m_weaponTexture << std::endl;
                    std::cout << "  Machine Gun ID: " << m_machineGunTexture << std::endl;
                    std::cout << "  Rocket Launcher ID: " << m_rocketLauncherTexture << std::endl;
                    
                    // Verify we're using the right texture
                    int textureToUse = m_currentWeaponTexture;
                    
                    // Safety check - if somehow m_currentWeaponTexture is invalid, use a fallback
                    if (textureToUse != m_weaponTexture && 
                        textureToUse != m_machineGunTexture && 
                        textureToUse != m_rocketLauncherTexture) {
                        std::cout << "WARNING: Invalid current weapon texture ID! Defaulting to pistol." << std::endl;
                        textureToUse = m_weaponTexture;
                        m_currentWeaponTexture = m_weaponTexture; // Fix the variable too
                    }
                    
                    // When using CUDA, pass 0.0 for flashIntensity to avoid the muzzle flash effect
                    m_renderer->renderWeapon(m_player, m_weaponRecoil, 0.0, textureToUse);
                }
                
                // Render notification if active
                renderNotification();
            }
            break;
            
        case GameState::Paused:
            // Render pause overlay
            renderPauseOverlay();
            break;
            
        case GameState::GameOver:
            // TODO: Render game over screen
            break;
            
        case GameState::Victory:
            // TODO: Render victory screen
            break;
    }
    
    // Present the renderer - we do this ONCE at the end of the frame
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
    m_rocketLauncherTexture = -1;
    
    // Load sound effects first
    if (m_audioSystem) {
        std::cout << "Loading weapon sound effects..." << std::endl;
        
        // Path to weapon sounds
        std::string soundPath = "assets/sounds/weapons/";
        
        // Check if directory exists
        std::cout << "Checking sound directory: " << soundPath << std::endl;
        
        // Check each sound file before loading
        std::vector<std::string> requiredSounds = {
            "dspistol.wav",
            "dsplasma.wav",
            "dsrlaunc.wav"
        };
        
        bool allFilesExist = true;
        for (const auto& sound : requiredSounds) {
            std::string fullPath = soundPath + sound;
            FILE* file = fopen(fullPath.c_str(), "rb");
            if (file) {
                std::cout << "Found sound file: " << fullPath << std::endl;
                fclose(file);
            } else {
                std::cerr << "Missing sound file: " << fullPath << std::endl;
                allFilesExist = false;
            }
        }
        
        if (!allFilesExist) {
            std::cerr << "WARNING: Some sound files are missing!" << std::endl;
        }
        
        // Load pistol sound
        if (!m_audioSystem->loadSoundEffect("pistol_fire", soundPath + "dspistol.wav")) {
            std::cerr << "Failed to load pistol sound effect!" << std::endl;
        }
        
        // Load plasma/machine gun sound
        if (!m_audioSystem->loadSoundEffect("machinegun_fire", soundPath + "dsplasma.wav")) {
            std::cerr << "Failed to load machine gun sound effect!" << std::endl;
        }
        
        // Load rocket launcher sound
        if (!m_audioSystem->loadSoundEffect("rocket_fire", soundPath + "dsrlaunc.wav")) {
            std::cerr << "Failed to load rocket launcher sound effect!" << std::endl;
        }
        
        // Load weapon switch sound - fall back to pistol sound if not available
        if (!m_audioSystem->loadSoundEffect("weapon_switch", soundPath + "dspistol.wav")) {
            std::cerr << "Failed to load weapon switch sound effect, using pistol sound as fallback" << std::endl;
        }
        
        std::cout << "Weapon sound effects loading completed" << std::endl;
    }
    
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
                int noise = (rand() % 10) - 5; // Reduced noise for more consistent look
                
                // Create an authentic DOOM-like floor pattern (FLOOR7_2 inspired)
                bool isMainTile = false;
                bool isTileEdge = false;
                
                // Create octagonal tile pattern
                int tileX = x % 32;
                int tileY = y % 32;
                
                // Octagon edges
                if ((tileX == 0 || tileX == 31 || tileY == 0 || tileY == 31) || 
                    (tileX == 8 && tileY < 24 && tileY > 7) ||
                    (tileX == 23 && tileY < 24 && tileY > 7) ||
                    (tileY == 8 && tileX < 24 && tileX > 7) ||
                    (tileY == 23 && tileX < 24 && tileX > 7)) {
                    isTileEdge = true;
                }
                
                // Diamond pattern in center
                bool isDiamondPattern = 
                    ((tileX + tileY >= 16 - 4 && tileX + tileY <= 16 + 4) || 
                     (tileX - tileY <= 4 && tileX - tileY >= -4)) &&
                    (tileX > 8 && tileX < 23 && tileY > 8 && tileY < 23);
                
                if (isDiamondPattern) {
                    // Diamond pattern (brownish)
                    pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, 
                        120 + noise, 100 + noise, 80 + noise);
                } else if (isTileEdge) {
                    // Dark grout/edge (dark gray)
                    pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, 
                        50 + noise, 50 + noise, 50 + noise);
                } else {
                    // Base tile color (grayish tan like DOOM's FLOOR7_2)
                    pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, 
                        100 + noise, 90 + noise, 75 + noise);
                }
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
                int noise = (rand() % 8) - 4; // Reduced noise for more consistent look
                
                // Create a DOOM-like ceiling pattern (FLAT1 / CEIL3_5 inspired)
                bool isSquare = false;
                
                // Position within the repeating pattern (16x16)
                int patternX = x % 16;
                int patternY = y % 16;
                
                // Create main grid lines
                bool isHorizontalLine = (patternY == 0 || patternY == 15);
                bool isVerticalLine = (patternX == 0 || patternX == 15);
                bool isInteriorLine = (patternX == 8 || patternY == 8);
                
                // Small squares in a pattern
                bool isSmallSquare = ((patternX >= 3 && patternX <= 5) && (patternY >= 3 && patternY <= 5)) || 
                                     ((patternX >= 3 && patternX <= 5) && (patternY >= 10 && patternY <= 12)) ||
                                     ((patternX >= 10 && patternX <= 12) && (patternY >= 3 && patternY <= 5)) ||
                                     ((patternX >= 10 && patternX <= 12) && (patternY >= 10 && patternY <= 12));
                
                // Set colors based on pattern (using DOOM's typical grayish-blue ceiling palette)
                if (isHorizontalLine || isVerticalLine) {
                    // Darker border lines
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        60 + noise, 60 + noise, 70 + noise);
                } else if (isInteriorLine) {
                    // Slightly lighter interior lines
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        75 + noise, 75 + noise, 85 + noise);
                } else if (isSmallSquare) {
                    // Light gray accent squares
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        110 + noise, 110 + noise, 120 + noise);
                } else {
                    // Base bluish-gray color
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        90 + noise, 90 + noise, 105 + noise);
                }
            }
        }
        SDL_UnlockSurface(ceilingSurface);
        m_ceilingTexture = m_textureManager->createTextureFromSurface(ceilingSurface);
        SDL_FreeSurface(ceilingSurface);
    } else {
        m_ceilingTexture = m_textureManager->createSolidTexture(64, 64, Color(64, 64, 96));
    }
    std::cout << "Ceiling texture ID: " << m_ceilingTexture << std::endl;
    
    // NOTE: Alternative DOOM-like textures are available from TextureManager:
    // Floor options: 5 (gray stone), 6 (green marble)
    // Ceiling options: 7 (brown grid), 8 (metal panels)
    
    // Use DOOM-like textures from the TextureManager (initialized in TextureManager::initDefaultTextures)
    // These will override the procedurally generated textures above
    m_floorTexture = 6;    // Use green marble floor (FLOOR4_8 style)
    m_ceilingTexture = 8;  // Use metal panel ceiling (FLAT23 style)
    std::cout << "Using DOOM-like textures for floor and ceiling" << std::endl;
    
    // Create bullet texture
    std::cout << "Creating bullet texture..." << std::endl;
    
    // Create a surface for the bullet texture
    SDL_Surface* bulletSurface = SDL_CreateRGBSurface(0, 16, 16, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (bulletSurface) {
        SDL_LockSurface(bulletSurface);
        Uint32* pixels = (Uint32*)bulletSurface->pixels;
        Uint32 bulletColor = SDL_MapRGBA(bulletSurface->format, 255, 220, 50, 255);
        
        // Create bullet gradient
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                double distFromCenter = sqrt(pow(x - 8, 2) + pow(y - 8, 2));
                
                if (distFromCenter <= 6) {
                    // Create a gradient effect
                    double alpha = 1.0 - (distFromCenter / 6.0);
                    pixels[y * 16 + x] = SDL_MapRGBA(bulletSurface->format, 
                                                     255, 
                                                     static_cast<Uint8>(120 + 100 * alpha), 
                                                     static_cast<Uint8>(50 * alpha),
                                                     255);
                }
                else {
                    // Outside radius - transparent
                    pixels[y * 16 + x] = SDL_MapRGBA(bulletSurface->format, 0, 0, 0, 0);
                }
            }
        }
        
        SDL_UnlockSurface(bulletSurface);
        m_bulletTexture = m_textureManager->createTextureFromSurface(bulletSurface);
        SDL_FreeSurface(bulletSurface);
    } else {
        m_bulletTexture = m_textureManager->createSolidTexture(16, 16, Color(255, 220, 50));
    }
    std::cout << "Bullet texture ID: " << m_bulletTexture << std::endl;
    
    // Create a rocket texture
    SDL_Surface* rocketSurface = SDL_CreateRGBSurface(0, 32, 16, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    int rocketTextureId = -1;
    if (rocketSurface) {
        SDL_LockSurface(rocketSurface);
        Uint32* pixels = (Uint32*)rocketSurface->pixels;
        
        // Create a rocket shape with red/orange gradient and smoke trail
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 32; x++) {
                // Rocket body (right half)
                if (x >= 16) {
                    // Rocket head (tip)
                    if (x > 26) {
                        double dist = sqrt(pow(x - 26, 2) + pow(y - 8, 2));
                        if (dist < 6) {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 
                                                           255, 
                                                           static_cast<Uint8>(50 + (16 - x)*10), 
                                                           0, 
                                                           255);
                        } else {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 0, 0, 0, 0);
                        }
                    }
                    // Rocket body
                    else {
                        double dist = abs(y - 8);
                        if (dist < 4) {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 
                                                           200, 
                                                           static_cast<Uint8>(50 + (x - 16)*5), 
                                                           0, 
                                                           255);
                        } else {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 0, 0, 0, 0);
                        }
                    }
                }
                // Smoke/fire trail (left half)
                else {
                    double distFromCenter = sqrt(pow(x - 16, 2) + pow(y - 8, 2));
                    double alpha = 1.0 - (x / 16.0) - (distFromCenter / 16.0);
                    
                    if (alpha > 0) {
                        // Fire
                        if (distFromCenter < 3 && x > 8) {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 
                                                           255, 
                                                           static_cast<Uint8>(128 * alpha), 
                                                           0, 
                                                           static_cast<Uint8>(255 * alpha));
                        }
                        // Smoke
                        else {
                            pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 
                                                           static_cast<Uint8>(100 * alpha), 
                                                           static_cast<Uint8>(100 * alpha), 
                                                           static_cast<Uint8>(100 * alpha), 
                                                           static_cast<Uint8>(200 * alpha));
                        }
                    } else {
                        pixels[y * 32 + x] = SDL_MapRGBA(rocketSurface->format, 0, 0, 0, 0);
                    }
                }
            }
        }
        
        SDL_UnlockSurface(rocketSurface);
        rocketTextureId = m_textureManager->createTextureFromSurface(rocketSurface);
        SDL_FreeSurface(rocketSurface);
        std::cout << "Created rocket texture (ID " << rocketTextureId << ")" << std::endl;
    } else {
        rocketTextureId = m_textureManager->createSolidTexture(32, 16, Color(255, 100, 0));
        std::cout << "Created fallback rocket texture (ID " << rocketTextureId << ")" << std::endl;
    }
    
    // Create a plasma projectile texture
    SDL_Surface* plasmaSurface = SDL_CreateRGBSurface(0, 32, 32, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    m_plasmaTexture = -1;
    if (plasmaSurface) {
        SDL_LockSurface(plasmaSurface);
        Uint32* pixels = (Uint32*)plasmaSurface->pixels;
        
        // Parameters for the plasma ball
        int centerX = 16;
        int centerY = 16;
        double plasmaRadius = 12.0;
        
        // Define colors for the plasma effect
        Uint32 plasmaCore = SDL_MapRGBA(plasmaSurface->format, 200, 220, 255, 255);  // Bright blue-white core
        Uint32 plasmaEdge = SDL_MapRGBA(plasmaSurface->format, 50, 130, 255, 200);   // Blue edge
        
        // Create a circular plasma effect with glow
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                // Set all pixels transparent by default
                pixels[y * 32 + x] = SDL_MapRGBA(plasmaSurface->format, 0, 0, 0, 0);
                
                // Calculate distance from center
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                
                // Draw plasma with glow effect
                if (distFromCenter <= plasmaRadius) {
                    double normalizedDist = distFromCenter / plasmaRadius;
                    
                    if (normalizedDist < 0.6) {
                        // Inner core (brightest)
                        pixels[y * 32 + x] = plasmaCore;
                    } else {
                        // Outer glow (fading)
                        double alpha = 1.0 - ((normalizedDist - 0.6) / 0.4);
                        Uint8 r, g, b, a;
                        SDL_GetRGBA(plasmaEdge, plasmaSurface->format, &r, &g, &b, &a);
                        a = static_cast<Uint8>(a * alpha);
                        pixels[y * 32 + x] = SDL_MapRGBA(plasmaSurface->format, r, g, b, a);
                    }
                    
                    // Add slight pulsing effect variation
                    int variation = (x + y) % 3;
                    if (variation == 0 && normalizedDist < 0.4) {
                        pixels[y * 32 + x] = SDL_MapRGBA(plasmaSurface->format, 200, 230, 255, 255);
                    }
                }
            }
        }
        
        SDL_UnlockSurface(plasmaSurface);
        m_plasmaTexture = m_textureManager->createTextureFromSurface(plasmaSurface);
        SDL_FreeSurface(plasmaSurface);
    } else {
        m_plasmaTexture = m_textureManager->createSolidTexture(32, 32, Color(50, 150, 255));
    }
    std::cout << "Plasma texture ID: " << m_plasmaTexture << std::endl;
    
    // Set the projectile textures in the projectile manager
    if (m_projectileManager) {
        m_projectileManager->setBulletTexture(m_bulletTexture);
        m_projectileManager->setDefaultBulletTexture(m_bulletTexture);
        m_projectileManager->setPlasmaTexture(m_plasmaTexture);
        
        // Set the rocket texture properly
        m_projectileManager->setRocketTexture(rocketTextureId);
        std::cout << "Set rocket projectile texture ID: " << rocketTextureId << std::endl;
    }
    
    // Create enemy texture
    std::cout << "Creating enemy texture..." << std::endl;
    
    // Create an array of enemy textures for animation frames
    const int enemyFrameCount = 4;
    m_enemyTextureFrames.resize(enemyFrameCount);
    
    // Create a surface for the enemy sprite sheet
    SDL_Surface* enemySurface = SDL_CreateRGBSurface(0, 64, 64, 32, 
                                                    0xFF000000,  // Red mask
                                                    0x00FF0000,  // Green mask
                                                    0x0000FF00,  // Blue mask
                                                    0x000000FF); // Alpha mask - important for transparency
    if (enemySurface) {
        // Lock surface for direct pixel access
        SDL_LockSurface(enemySurface);
        
        // Create a basic enemy texture (red with eyes)
        Uint32* pixels = static_cast<Uint32*>(enemySurface->pixels);
        for (int y = 0; y < enemySurface->h; y++) {
            for (int x = 0; x < enemySurface->w; x++) {
                // Default color (red body)
                Uint32 color = SDL_MapRGBA(enemySurface->format, 180, 0, 0, 255);
                
                // Calculate distance from center for smoother edges
                double centerX = enemySurface->w / 2.0;
                double centerY = enemySurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = enemySurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter > radius) {
                    // Outside the circle - transparent
                    color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
                } else {
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 25) && 
                        ((x >= 15 && x <= 25) || (x >= 38 && x <= 48))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 15 && x <= 25) ? 20 : 43;
                        double eyeCenterY = 20;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 5) {
                            color = SDL_MapRGBA(enemySurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (black)
                    if ((y >= 35 && y <= 45) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 32;
                        double mouthCenterY = 40;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 255);
                        }
                    }
                }
                
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
                // Default color (red body)
                Uint32 color = SDL_MapRGBA(enemySurface->format, 180, 0, 0, 255);
                
                // Calculate distance from center for smoother edges
                double centerX = enemySurface->w / 2.0;
                double centerY = enemySurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = enemySurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter > radius) {
                    // Outside the circle - transparent
                    color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
                } else {
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 25) && 
                        ((x >= 15 && x <= 25) || (x >= 38 && x <= 48))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 15 && x <= 25) ? 20 : 43;
                        double eyeCenterY = 20;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 5) {
                            color = SDL_MapRGBA(enemySurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (black)
                    if ((y >= 35 && y <= 45) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 32;
                        double mouthCenterY = 40;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 255);
                        }
                    }
                }
                
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
                // Default color (red body)
                Uint32 color = SDL_MapRGBA(enemySurface->format, 180, 0, 0, 255);
                
                // Calculate distance from center for smoother edges
                double centerX = enemySurface->w / 2.0;
                double centerY = enemySurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = enemySurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter > radius) {
                    // Outside the circle - transparent
                    color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
                } else {
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 25) && 
                        ((x >= 15 && x <= 25) || (x >= 38 && x <= 48))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 15 && x <= 25) ? 20 : 43;
                        double eyeCenterY = 20;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 5) {
                            color = SDL_MapRGBA(enemySurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (black)
                    if ((y >= 35 && y <= 45) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 32;
                        double mouthCenterY = 40;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 255);
                        }
                    }
                }
                
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
                // Default color (red body)
                Uint32 color = SDL_MapRGBA(enemySurface->format, 180, 0, 0, 255);
                
                // Calculate distance from center for smoother edges
                double centerX = enemySurface->w / 2.0;
                double centerY = enemySurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = enemySurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter > radius) {
                    // Outside the circle - transparent
                    color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
                } else {
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 25) && 
                        ((x >= 15 && x <= 25) || (x >= 38 && x <= 48))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 15 && x <= 25) ? 20 : 43;
                        double eyeCenterY = 20;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 5) {
                            color = SDL_MapRGBA(enemySurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (black)
                    if ((y >= 35 && y <= 45) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 32;
                        double mouthCenterY = 40;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 255);
                        }
                    }
                }
                
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
    
    // Reset texture IDs to ensure they're unique
    m_weaponTexture = -1;
    m_machineGunTexture = -1;
    m_rocketLauncherTexture = -1;
    
    // Load shotgun
    SDL_Surface* tempSurface = IMG_Load((assetsPath + "shotgun.webp").c_str());
    if (tempSurface) {
        // Set black as the transparent color
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
                
        // Create texture from surface
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, tempSurface);
        if (texture) {
            // Set blend mode to allow transparency
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            
            // Add texture to manager
            m_weaponTexture = m_textureManager->addTexture(texture);
            
            std::cout << "Added shotgun texture with ID: " << m_weaponTexture << std::endl;
        }
        SDL_FreeSurface(tempSurface);
    }
    if (m_weaponTexture < 0) {
        m_weaponTexture = m_textureManager->createSolidTexture(256, 256, Color(128, 128, 128));
        std::cout << "Created fallback shotgun texture with ID: " << m_weaponTexture << std::endl;
    }
    std::cout << "Weapon texture ID: " << m_weaponTexture << std::endl;
    
    // Load machine gun - use a different path just to be sure
    tempSurface = IMG_Load((assetsPath + "machine_gun.png").c_str());
    if (tempSurface) {
        // Set black as the transparent color
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
                
        // Create texture from surface - create a new texture, don't reuse
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, tempSurface);
        if (texture) {
            // Set blend mode to allow transparency
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            
            // Manually check the texture is not null
            if (texture == nullptr) {
                std::cerr << "ERROR: Machine gun texture is NULL after SDL_CreateTextureFromSurface" << std::endl;
            } else {
                // Force this to be a different texture ID than the shotgun
                m_machineGunTexture = m_textureManager->addTexture(texture);
                std::cout << "Added machine gun texture with ID: " << m_machineGunTexture << std::endl;
            }
        }
        SDL_FreeSurface(tempSurface);
    }
    if (m_machineGunTexture < 0) {
        // Make sure this creates a different solid texture than the shotgun
        m_machineGunTexture = m_textureManager->createSolidTexture(256, 256, Color(100, 100, 200)); // Different color
        std::cout << "Created fallback machine gun texture with ID: " << m_machineGunTexture << std::endl;
    }
    std::cout << "Machine gun texture ID: " << m_machineGunTexture << std::endl;
    
    // Load rocket launcher - make sure we get a unique texture
    tempSurface = IMG_Load((assetsPath + "rocket_launcher.png").c_str());
    if (tempSurface) {
        // Set black as the transparent color
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
                
        // Create texture from surface - with a unique SDL_Texture
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_sdlRenderer, tempSurface);
        if (texture) {
            // Set blend mode to allow transparency
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            
            // Manually check the texture is not null
            if (texture == nullptr) {
                std::cerr << "ERROR: Rocket launcher texture is NULL after SDL_CreateTextureFromSurface" << std::endl;
            } else {
                // Force this to be a different texture ID than the other weapons
                m_rocketLauncherTexture = m_textureManager->addTexture(texture);
                std::cout << "Added rocket launcher texture with ID: " << m_rocketLauncherTexture << std::endl;
            }
        }
        SDL_FreeSurface(tempSurface);
    }
    if (m_rocketLauncherTexture < 0) {
        // Make sure this creates a different solid texture
        m_rocketLauncherTexture = m_textureManager->createSolidTexture(256, 256, Color(255, 120, 120)); // Distinctly different color
        std::cout << "Created fallback rocket launcher texture with ID: " << m_rocketLauncherTexture << std::endl;
    }
    std::cout << "Rocket launcher texture ID: " << m_rocketLauncherTexture << std::endl;
    
    // Verify all weapons have different texture IDs
    if (m_weaponTexture == m_machineGunTexture || m_weaponTexture == m_rocketLauncherTexture || m_machineGunTexture == m_rocketLauncherTexture) {
        std::cerr << "ERROR: Weapon textures have duplicate IDs!" << std::endl;
        // Force them to be different if they're duplicates
        if (m_machineGunTexture == m_weaponTexture) {
            m_machineGunTexture = m_textureManager->createSolidTexture(256, 256, Color(0, 200, 0));
            std::cout << "Fixed duplicate: New machine gun texture ID: " << m_machineGunTexture << std::endl;
        }
        if (m_rocketLauncherTexture == m_weaponTexture) {
            m_rocketLauncherTexture = m_textureManager->createSolidTexture(256, 256, Color(200, 0, 0));
            std::cout << "Fixed duplicate: New rocket launcher texture ID: " << m_rocketLauncherTexture << std::endl;
        }
        if (m_rocketLauncherTexture == m_machineGunTexture) {
            m_rocketLauncherTexture = m_textureManager->createSolidTexture(256, 256, Color(0, 0, 200));
            std::cout << "Fixed duplicate: New rocket launcher texture ID: " << m_rocketLauncherTexture << std::endl;
        }
    }
    
    // Set initial weapon texture
    m_currentWeaponTexture = m_weaponTexture;
    
    // Debug printout of all weapon texture IDs
    std::cout << "\n=============================================" << std::endl;
    std::cout << "WEAPON TEXTURE IDs:" << std::endl;
    std::cout << "Pistol/Shotgun (m_weaponTexture): " << m_weaponTexture << std::endl;
    std::cout << "Machine Gun (m_machineGunTexture): " << m_machineGunTexture << std::endl;
    std::cout << "Rocket Launcher (m_rocketLauncherTexture): " << m_rocketLauncherTexture << std::endl;
    std::cout << "Current weapon texture: " << m_currentWeaponTexture << std::endl;
    std::cout << "=============================================\n" << std::endl;
    
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
                  m_machineGunTexture >= 0 && m_rocketLauncherTexture >= 0 && !m_wallTextureVariations.empty();
    
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
    SDL_Surface* impSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 
                                                  0xFF000000,  // Red mask
                                                  0x00FF0000,  // Green mask
                                                  0x0000FF00,  // Blue mask
                                                  0x000000FF); // Alpha mask - important for transparency
    if (impSurface) {
        SDL_LockSurface(impSurface);
        
        // Create the Imp texture (brownish with spikes)
        Uint32* pixels = static_cast<Uint32*>(impSurface->pixels);
        for (int y = 0; y < impSurface->h; y++) {
            for (int x = 0; x < impSurface->w; x++) {
                // Default color (transparent)
                Uint32 color = SDL_MapRGBA(impSurface->format, 0, 0, 0, 0);
                
                // Calculate distance from center for smoother edges
                double centerX = impSurface->w / 2.0;
                double centerY = impSurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = impSurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter <= radius) {
                    // Inside the circle - brown body
                    color = SDL_MapRGBA(impSurface->format, 139, 69, 19, 255);
                    
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 20) && 
                        ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 20 && x <= 25) ? 22.5 : 40.5;
                        double eyeCenterY = 17.5;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 3) {
                            color = SDL_MapRGBA(impSurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (dark red)
                    if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 31.5;
                        double mouthCenterY = 32.5;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(impSurface->format, 139, 0, 0, 255);
                        }
                    }
                    
                    // Add spikes on head (lighter brown)
                    if (y < 15 && y > 5) {
                        // Calculate spike pattern
                        double spikeX = x % 8;
                        if (spikeX < 4 && y < 15 - spikeX) {
                            color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
                        }
                    }
                    
                    // Add spikes on shoulders
                    if ((y >= 20 && y <= 25) && 
                        ((x <= 15) || (x >= 49))) {
                        color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
                    }
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
                // Default color (transparent)
                Uint32 color = SDL_MapRGBA(impSurface->format, 0, 0, 0, 0);
                
                // Calculate distance from center for smoother edges
                double centerX = impSurface->w / 2.0;
                double centerY = impSurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = impSurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter <= radius) {
                    // Inside the circle - brown body
                    color = SDL_MapRGBA(impSurface->format, 139, 69, 19, 255);
                    
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 20) && 
                        ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 20 && x <= 25) ? 22.5 : 40.5;
                        double eyeCenterY = 17.5;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 3) {
                            color = SDL_MapRGBA(impSurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (dark red)
                    if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 31.5;
                        double mouthCenterY = 32.5;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(impSurface->format, 139, 0, 0, 255);
                        }
                    }
                    
                    // Add spikes on head (lighter brown)
                    if (y < 15 && y > 5) {
                        // Calculate spike pattern
                        double spikeX = x % 8;
                        if (spikeX < 4 && y < 15 - spikeX) {
                            color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
                        }
                    }
                    
                    // Add spikes on shoulders
                    if ((y >= 20 && y <= 25) && 
                        ((x <= 15) || (x >= 49))) {
                        color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
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
                // Default color (transparent)
                Uint32 color = SDL_MapRGBA(impSurface->format, 0, 0, 0, 0);
                
                // Calculate distance from center for smoother edges
                double centerX = impSurface->w / 2.0;
                double centerY = impSurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = impSurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter <= radius) {
                    // Inside the circle - brown body
                    color = SDL_MapRGBA(impSurface->format, 139, 69, 19, 255);
                    
                    // Add eyes (yellow)
                    if ((y >= 15 && y <= 20) && 
                        ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 20 && x <= 25) ? 22.5 : 40.5;
                        double eyeCenterY = 17.5;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 3) {
                            color = SDL_MapRGBA(impSurface->format, 255, 255, 0, 255);
                        }
                    }
                    
                    // Add mouth (dark red)
                    if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 31.5;
                        double mouthCenterY = 32.5;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(impSurface->format, 139, 0, 0, 255);
                        }
                    }
                    
                    // Add spikes on head (lighter brown)
                    if (y < 15 && y > 5) {
                        // Calculate spike pattern
                        double spikeX = x % 8;
                        if (spikeX < 4 && y < 15 - spikeX) {
                            color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
                        }
                    }
                    
                    // Add spikes on shoulders
                    if ((y >= 20 && y <= 25) && 
                        ((x <= 15) || (x >= 49))) {
                        color = SDL_MapRGBA(impSurface->format, 205, 133, 63, 255);
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
                // Default color (transparent)
                Uint32 color = SDL_MapRGBA(impSurface->format, 0, 0, 0, 0);
                
                // Calculate distance from center for smoother edges
                double centerX = impSurface->w / 2.0;
                double centerY = impSurface->h / 2.0;
                double distFromCenter = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
                double radius = impSurface->w / 2.0 - 2.0;
                
                // Create a circular shape with smooth edges
                if (distFromCenter <= radius) {
                    // Inside the circle - brown body
                    color = SDL_MapRGBA(impSurface->format, 139, 69, 19, 255);
                    
                    // Add eyes (red for attack)
                    if ((y >= 15 && y <= 20) && 
                        ((x >= 20 && x <= 25) || (x >= 38 && x <= 43))) {
                        
                        // Calculate distance from eye center for smooth eyes
                        double eyeCenterX = (x >= 20 && x <= 25) ? 22.5 : 40.5;
                        double eyeCenterY = 17.5;
                        double eyeDist = sqrt(pow(x - eyeCenterX, 2) + pow(y - eyeCenterY, 2));
                        
                        if (eyeDist < 3) {
                            color = SDL_MapRGBA(impSurface->format, 255, 0, 0, 255);
                        }
                    }
                    
                    // Add mouth (bright red for attack)
                    if ((y >= 30 && y <= 35) && (x >= 25 && x <= 38)) {
                        // Calculate distance from mouth center for smooth mouth
                        double mouthCenterX = 31.5;
                        double mouthCenterY = 32.5;
                        double mouthDist = sqrt(pow(x - mouthCenterX, 2) + pow(y - mouthCenterY, 2));
                        
                        if (mouthDist < 6) {
                            color = SDL_MapRGBA(impSurface->format, 255, 0, 0, 255);
                        }
                    }
                    
                    // Add spikes on head (lighter brown)
                    if (y < 15 && y > 5) {
                        // Calculate spike pattern
                        double spikeX = x % 8;
                        if (spikeX < 4 && y < 15 - spikeX) {
                            color = SDL_MapRGB(impSurface->format, 205, 133, 63);
                        }
                    }
                    
                    // Add spikes on shoulders
                    if ((y >= 20 && y <= 25) && 
                        ((x <= 15) || (x >= 49))) {
                        color = SDL_MapRGB(impSurface->format, 205, 133, 63);
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
    
    // Set the engine pointer in the map
    m_map.setEngine(this);
    
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Define the second level area
    int secondLevelStartX = m_map.getWidth() / 2;
    int secondLevelStartY = 0;
    int secondLevelWidth = m_map.getWidth() / 2;
    int secondLevelHeight = m_map.getHeight() / 2;
    
    // Create the second level with proper walls and floor
    for (int x = secondLevelStartX; x < secondLevelStartX + secondLevelWidth; x++) {
        for (int y = secondLevelStartY; y < secondLevelStartY + secondLevelHeight; y++) {  // Added missing brace
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
        }  // Added missing brace
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
        Light northLight = Light::createFlickeringLight(
            Vec2(stair1EndX, stair1EndY + 1),
            Color(255, 204, 153),  // Warm light
            1.15f,                 // Intensity increased by 15%
            9.2f,                  // Radius increased by 15%
            1.0f,                  // Flicker speed
            0.3f                   // Flicker amount
        );
        m_renderer->getLightingSystem().addLight(northLight);
        
        // Light at the east staircase entrance
        Light eastLight = Light::createFlickeringLight(
            Vec2(stair2EndX - 1, stair2EndY),
            Color(153, 204, 255),  // Cool light
            1.15f,               // Intensity increased by 15%
            9.2f,                // Radius increased by 15%
            1.2f,                // Flicker speed
            0.25f                // Flicker amount
        );
        m_renderer->getLightingSystem().addLight(eastLight);
        
        // Light in the central chamber
        Light centerLight = Light::createPulsingLight(
            Vec2(secondLevelStartX + secondLevelWidth/2, secondLevelStartY + secondLevelHeight/2),
            Color(204, 102, 230),  // Purple light
            1.15f,                 // Intensity increased by 15%
            11.5f,                 // Radius increased by 15%
            0.5f                   // Pulse speed
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
                
                // Randomly choose a light type
                int lightType = rand() % 3;
                Light randomLight;
                
                switch (lightType) {
                    case 0:
                        randomLight = Light::createFlickeringLight(
                            Vec2(lightX, lightY),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            5.75f,                 // Radius increased by 15%
                            1.0f + (rand() % 100) / 100.0f,  // Random flicker speed
                            0.2f + (rand() % 100) / 500.0f   // Random flicker amount
                        );
                        break;
                        
                    case 1:
                        randomLight = Light::createPulsingLight(
                            Vec2(lightX, lightY),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            5.75f,                 // Radius increased by 15%
                            0.3f + (rand() % 100) / 200.0f   // Random pulse speed
                        );
                        break;
                        
                    case 2:
                    default:
                        randomLight = Light::createGlowLight(
                            Vec2(lightX, lightY),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            5.75f                  // Radius increased by 15%
                        );
                        break;
                }
                
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
                    // Bottom-left quadrant: change from random walls to grid pattern similar to top-left
                    if ((x % 8 == 0 || y % 8 == 0) && (x % 8 != 4 && y % 8 != 4)) {
                        createWall = true;
                    }
                }
                else {
                    // Bottom-right quadrant: change from circular pattern to grid pattern with larger spacing
                    if ((x % 10 == 0 || y % 10 == 0) && (x % 10 != 5 && y % 10 != 5)) {
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
        Light impAreaLight = Light::createGlowLight(
            Vec2(impAreaCenterX, impAreaCenterY),
            Color(255, 50, 0),  // Lava-red light for the Imp area
            1.3f,               // Higher intensity
            12.0f               // Larger radius
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
                
                // Randomly choose a light type
                int lightType = rand() % 4;
                Light groundLight;
                
                switch (lightType) {
                    case 0:
                        groundLight = Light::createPointLight(
                            Vec2(x, y),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            6.0f                   // Radius
                        );
                        break;
                        
                    case 1:
                        groundLight = Light::createFlickeringLight(
                            Vec2(x, y),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            6.0f,                  // Radius
                            1.0f + (rand() % 100) / 100.0f,  // Random flicker speed
                            0.3f + (rand() % 100) / 500.0f   // Random flicker amount
                        );
                        break;
                        
                    case 2:
                        groundLight = Light::createPulsingLight(
                            Vec2(x, y),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            6.0f,                  // Radius
                            0.3f + (rand() % 100) / 200.0f   // Random pulse speed
                        );
                        break;
                        
                    case 3:
                    default:
                        groundLight = Light::createStrobeLight(
                            Vec2(x, y),
                            Color(r * 255, g * 255, b * 255),
                            0.92f,                 // Intensity increased by 15%
                            6.0f,                  // Radius
                            0.2f + (rand() % 100) / 200.0f   // Random strobe speed
                        );
                        break;
                }
                
                m_renderer->getLightingSystem().addLight(groundLight);
            }
        }
        
        // Add lights at staircase entrances
        Light stair1Light = Light::createPulsingLight(
            Vec2(stair1StartX, stair1StartY),
            Color(255, 204, 153),  // Warm light
            1.0f,                  // Intensity
            8.0f,                  // Radius
            0.5f                   // Pulse speed
        );
        m_renderer->getLightingSystem().addLight(stair1Light);
        
        Light stair2Light = Light::createPulsingLight(
            Vec2(stair2StartX, stair2StartY),
            Color(153, 204, 255),  // Cool light
            1.0f,                  // Intensity
            8.0f,                  // Radius
            0.5f                   // Pulse speed
        );
        m_renderer->getLightingSystem().addLight(stair2Light);
    }
    
    // Create sectors for visibility culling
    m_map.createSectors();
    
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
                    double size = 0.7; // Reduced from 0.9 to make Imps smaller
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
                            imp->setMaxHealth(150.0);
                            imp->setHealth(150.0);
                            
                            // Randomly assign a movement type
                            ImpMovementType movementType = static_cast<ImpMovementType>(rand() % 3);
                            imp->setImpMovementType(movementType);
                        }
                    }
                } else {
                    // Create a regular enemy sprite
                    double size = 0.6; // Reduced from 0.8 to make enemies smaller
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
    // Set player starting position
    m_player.init(m_map.getWidth() / 4, m_map.getHeight() / 4, 1.0, 0.0);
    
    // Set up player references
    m_player.setProjectileManager(m_projectileManager);
    m_player.setSpriteManager(m_spriteManager);
    
    // Set player stats
    m_player.setHealth(100.0);
    m_player.setAmmo(50);
    m_player.setGrenades(3);
    
    // Set player movement speeds
    m_player.setMoveSpeed(5.0);
    m_player.setRotSpeed(3.0);
    m_player.setVerticalLookSpeed(2.0);
    
    // Set initial weapon
    m_player.setCurrentWeapon(WeaponType::Pistol);
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
    m_inputHandler.bindKey(SDL_SCANCODE_SPACE, InputAction::Jump);  // Changed from Fire to Jump
    m_inputHandler.bindKey(SDL_SCANCODE_LCTRL, InputAction::Fire);  // Add left control as the new fire button
    m_inputHandler.bindKey(SDL_SCANCODE_R, InputAction::Reload);
    m_inputHandler.bindKey(SDL_SCANCODE_ESCAPE, InputAction::Menu);
    m_inputHandler.bindKey(SDL_SCANCODE_Q, InputAction::Quit);
    m_inputHandler.bindKey(SDL_SCANCODE_F1, InputAction::ToggleFPS);
    m_inputHandler.bindKey(SDL_SCANCODE_F2, InputAction::ToggleMinimap);
    m_inputHandler.bindKey(SDL_SCANCODE_F3, InputAction::ToggleWeapon);
    m_inputHandler.bindKey(SDL_SCANCODE_F4, InputAction::ToggleCeilings);
    
    // Menu navigation keys
    m_inputHandler.bindKey(SDL_SCANCODE_UP, InputAction::MenuUp);
    m_inputHandler.bindKey(SDL_SCANCODE_DOWN, InputAction::MenuDown);
    m_inputHandler.bindKey(SDL_SCANCODE_RETURN, InputAction::MenuSelect);
    
    // Audio control keys
    m_inputHandler.bindKey(SDL_SCANCODE_M, InputAction::ToggleMusic);
    m_inputHandler.bindKey(SDL_SCANCODE_PAGEUP, InputAction::IncreaseMusicVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_PAGEDOWN, InputAction::DecreaseMusicVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_HOME, InputAction::IncreaseSfxVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_END, InputAction::DecreaseSfxVolume);
    m_inputHandler.bindKey(SDL_SCANCODE_F10, InputAction::EnhanceMidiQuality);  // F10 for enhancing MIDI quality
    
    // Weapon keys
    m_inputHandler.bindKey(SDL_SCANCODE_1, InputAction::Weapon1);
    m_inputHandler.bindKey(SDL_SCANCODE_2, InputAction::Weapon2);
    m_inputHandler.bindKey(SDL_SCANCODE_3, InputAction::Weapon3);
    m_inputHandler.bindKey(SDL_SCANCODE_4, InputAction::Weapon4);
    m_inputHandler.bindKey(SDL_SCANCODE_5, InputAction::Weapon5);
    m_inputHandler.bindKey(SDL_SCANCODE_6, InputAction::Weapon6);
    m_inputHandler.bindKey(SDL_SCANCODE_7, InputAction::Weapon7);
    
    // Debug keys
    m_inputHandler.bindKey(SDL_SCANCODE_F9, InputAction::TestSound);
    m_inputHandler.bindKey(SDL_SCANCODE_F8, InputAction::TestWeapons);
    
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

void Engine::enhanceMidiQuality() {
    if (!m_audioSystem) return;
    
    std::cout << "Attempting to enhance MIDI quality..." << std::endl;
    
    // Detect if we're running in WSL
    bool isWSL = false;
    #ifdef __linux__
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "microsoft") || strstr(buffer, "Microsoft")) {
                isWSL = true;
            }
        }
        fclose(fp);
    }
    #endif
    
    if (isWSL) {
        // In WSL, prioritize PulseAudio for best quality
        showNotification("Configuring PulseAudio for optimal MIDI playback...", 3.0);
        
        if (m_audioSystem->configurePulseAudio(true)) {
            showNotification("PulseAudio configured successfully!", 2.0);
            
            // After PulseAudio is configured, try to set up a soundfont for even better quality
            showNotification("Installing high-quality MIDI soundfont...", 5.0);
            if (m_audioSystem->installSoundFontForWSL()) {
                showNotification("MIDI quality enhanced with high-quality soundfont!", 5.0);
            }
        } else {
            // If PulseAudio fails, try the Timidity approach as fallback
            showNotification("PulseAudio config failed, trying Timidity...", 2.0);
            if (m_audioSystem->configureTimidityForWSL()) {
                showNotification("MIDI quality enhanced with Timidity configuration!", 3.0);
            } else {
                showNotification("Failed to enhance MIDI quality. See console for details.", 5.0);
            }
        }
    } else {
        // On Windows, configure for best native MIDI quality
        showNotification("Configuring MIDI quality settings...", 2.0);
        if (m_audioSystem->configureMidiQuality(48000)) {
            showNotification("MIDI quality settings applied!", 2.0);
        } else {
            showNotification("Failed to enhance MIDI quality. See console for details.", 5.0);
        }
    }
    
    // Display current MIDI backend information
    std::string backendInfo = m_audioSystem->getMidiBackendInfo();
    std::cout << "Current MIDI backend: " << backendInfo << std::endl;
    showNotification(backendInfo, 5.0);
}

void Engine::testSoundEffects() {
    if (!m_audioSystem) return;
    
    std::cout << "Testing sound effects..." << std::endl;
    m_audioSystem->playSoundEffect("pistol_fire");
    SDL_Delay(500);
    m_audioSystem->playSoundEffect("machinegun_fire");
    SDL_Delay(500);
    m_audioSystem->playSoundEffect("rocket_fire");
    showNotification("Sound effects tested", 2.0);
}

void Engine::testWeapons() {
    std::cout << "Testing all weapons..." << std::endl;
    
    // Store the original weapon
    WeaponType originalWeapon = m_player.getCurrentWeapon();
    
    // Test pistol
    m_player.setCurrentWeapon(WeaponType::Pistol);
    m_currentWeaponTexture = m_weaponTexture;
    showNotification("Testing Pistol", 1.0);
    m_player.fire();
    SDL_Delay(500);
    
    // Test machine gun
    m_player.setCurrentWeapon(WeaponType::MachineGun);
    m_currentWeaponTexture = m_machineGunTexture;
    showNotification("Testing Machine Gun", 1.0);
    m_player.fire();
    SDL_Delay(500);
    
    // Test rocket launcher
    m_player.setCurrentWeapon(WeaponType::RocketLauncher);
    m_currentWeaponTexture = m_rocketLauncherTexture;
    showNotification("Testing Rocket Launcher", 1.0);
    m_player.fire();
    SDL_Delay(500);
    
    // Restore original weapon
    m_player.setCurrentWeapon(originalWeapon);
    
    // Update current weapon texture based on the weapon type
    switch (originalWeapon) {
        case WeaponType::Pistol:
            m_currentWeaponTexture = m_weaponTexture;
            break;
        case WeaponType::MachineGun:
            m_currentWeaponTexture = m_machineGunTexture;
            break;
        case WeaponType::RocketLauncher:
            m_currentWeaponTexture = m_rocketLauncherTexture;
            break;
        default:
            m_currentWeaponTexture = m_weaponTexture;
            break;
    }
    
    showNotification("Weapons test complete", 2.0);
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

void Engine::renderMainMenu() {
    // Set background color to black
    SDL_SetRenderDrawColor(m_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_sdlRenderer);
    
    // Render title
    SDL_Color titleColor = {255, 0, 0, 255}; // Red color for DOOM-style title
    SDL_Surface* titleSurface = TTF_RenderText_Blended(m_font, "DOOM CLONE", titleColor);
    if (titleSurface) {
        SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(m_sdlRenderer, titleSurface);
        if (titleTexture) {
            SDL_Rect titleRect = {
                m_screenWidth / 2 - titleSurface->w / 2,
                m_screenHeight / 4 - titleSurface->h / 2,
                titleSurface->w,
                titleSurface->h
            };
            SDL_RenderCopy(m_sdlRenderer, titleTexture, NULL, &titleRect);
            SDL_DestroyTexture(titleTexture);
        }
        SDL_FreeSurface(titleSurface);
    }
    
    // Render menu options with DOOM-style appearance
    const int menuStartY = m_screenHeight / 2;
    const int menuItemSpacing = 60;
    
    for (size_t i = 0; i < m_menuItems.size(); i++) {
        // Selected item is red, others are gray
        SDL_Color menuColor = (i == m_menuSelection) 
            ? SDL_Color{255, 0, 0, 255}  // Red for selected item
            : SDL_Color{180, 180, 180, 255};  // Light gray for unselected items
        
        // Add a ">" marker for the selected item
        std::string menuText = (i == m_menuSelection) 
            ? "> " + m_menuItems[i]
            : "  " + m_menuItems[i];
            
        SDL_Surface* menuSurface = TTF_RenderText_Blended(m_font, menuText.c_str(), menuColor);
        if (menuSurface) {
            SDL_Texture* menuTexture = SDL_CreateTextureFromSurface(m_sdlRenderer, menuSurface);
            if (menuTexture) {
                SDL_Rect menuRect = {
                    m_screenWidth / 2 - menuSurface->w / 2,
                    menuStartY + i * menuItemSpacing,
                    menuSurface->w,
                    menuSurface->h
                };
                SDL_RenderCopy(m_sdlRenderer, menuTexture, NULL, &menuRect);
                SDL_DestroyTexture(menuTexture);
            }
            SDL_FreeSurface(menuSurface);
        }
    }
}

void Engine::renderPauseOverlay() {
    // Render semi-transparent overlay
    SDL_SetRenderDrawBlendMode(m_sdlRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlRenderer, 0, 0, 0, 180); // Semi-transparent black
    SDL_Rect overlayRect = {0, 0, m_screenWidth, m_screenHeight};
    SDL_RenderFillRect(m_sdlRenderer, &overlayRect);
    
    // Render "PAUSED" text
    SDL_Color titleColor = {255, 0, 0, 255}; // Red color for DOOM-style title
    SDL_Surface* titleSurface = TTF_RenderText_Blended(m_font, "PAUSED", titleColor);
    if (titleSurface) {
        SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(m_sdlRenderer, titleSurface);
        if (titleTexture) {
            SDL_Rect titleRect = {
                m_screenWidth / 2 - titleSurface->w / 2,
                m_screenHeight / 4 - titleSurface->h / 2,
                titleSurface->w,
                titleSurface->h
            };
            SDL_RenderCopy(m_sdlRenderer, titleTexture, NULL, &titleRect);
            SDL_DestroyTexture(titleTexture);
        }
        SDL_FreeSurface(titleSurface);
    }
    
    // Render menu options with DOOM-style appearance
    const int menuStartY = m_screenHeight / 2;
    const int menuItemSpacing = 60;
    
    for (size_t i = 0; i < m_menuItems.size(); i++) {
        // Selected item is red, others are gray
        SDL_Color menuColor = (i == m_menuSelection) 
            ? SDL_Color{255, 0, 0, 255}  // Red for selected item
            : SDL_Color{180, 180, 180, 255};  // Light gray for unselected items
        
        // Add a ">" marker for the selected item
        std::string menuText = (i == m_menuSelection) 
            ? "> " + m_menuItems[i]
            : "  " + m_menuItems[i];
            
        SDL_Surface* menuSurface = TTF_RenderText_Blended(m_font, menuText.c_str(), menuColor);
        if (menuSurface) {
            SDL_Texture* menuTexture = SDL_CreateTextureFromSurface(m_sdlRenderer, menuSurface);
            if (menuTexture) {
                SDL_Rect menuRect = {
                    m_screenWidth / 2 - menuSurface->w / 2,
                    menuStartY + i * menuItemSpacing,
                    menuSurface->w,
                    menuSurface->h
                };
                SDL_RenderCopy(m_sdlRenderer, menuTexture, NULL, &menuRect);
                SDL_DestroyTexture(menuTexture);
            }
            SDL_FreeSurface(menuSurface);
        }
    }
}

void Engine::handlePlayingInput() {
    // Handle mouse movement for camera rotation
    int mouseX, mouseY;
    m_inputHandler.getMouseMotion(mouseX, mouseY);
    
    // Vertical mouse movement (look up/down)
    if (mouseY != 0) {
        // Pass Y movement to player's vertical angle setter
        m_player.setVerticalAngle(static_cast<double>(mouseY));
    }
    
    // Only rotate if there's actual mouse movement
    if (mouseX != 0) {
        // Calculate rotation amount based on mouse movement
        double rotationAmount = static_cast<double>(-mouseX) * 0.003;  // Inverted by negating mouseX
        
        // Get current direction and plane
        double oldDirX = m_player.getDirX();
        double oldDirY = m_player.getDirY();
        double oldPlaneX = m_player.getPlaneX();
        double oldPlaneY = m_player.getPlaneY();
        
        // Rotation matrix
        double cosRot = cos(-rotationAmount);  // Negative because mouseX right should rotate right
        double sinRot = sin(-rotationAmount);
        
        // Update player direction vector
        Vec2 newDir(
            oldDirX * cosRot - oldDirY * sinRot,
            oldDirX * sinRot + oldDirY * cosRot
        );
        
        // Update player camera plane
        Vec2 newPlane(
            oldPlaneX * cosRot - oldPlaneY * sinRot,
            oldPlaneX * sinRot + oldPlaneY * cosRot
        );
        
        // Set the new direction and plane
        m_player.setDirection(newDir);
        m_player.setPlane(newPlane);
    }
    
    // Get keyboard state once for all key checks
    int numKeys;
    const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
    
    // Weapon switching using direct keyboard state checks
    // Check for number keys - store previous state and compare
    static bool prev1Down = false;
    static bool prev2Down = false;
    static bool prev3Down = false;
    
    bool key1Down = keyboardState[SDL_SCANCODE_1] != 0;
    bool key2Down = keyboardState[SDL_SCANCODE_2] != 0;
    bool key3Down = keyboardState[SDL_SCANCODE_3] != 0;
    
    // Weapon 1 (just pressed this frame)
    if (key1Down && !prev1Down) {
        std::cout << "KEY 1 DETECTED - Switching to weapon 1 (Pistol)" << std::endl;
        m_player.setCurrentWeapon(WeaponType::Pistol);
        m_currentWeaponTexture = m_weaponTexture;
        showNotification("Pistol", 1.0);
    }
    
    // Weapon 2 (just pressed this frame)
    if (key2Down && !prev2Down) {
        std::cout << "KEY 2 DETECTED - Switching to weapon 2 (Machine Gun)" << std::endl;
        m_player.setCurrentWeapon(WeaponType::MachineGun);
        m_currentWeaponTexture = m_machineGunTexture;
        showNotification("Machine Gun", 1.0);
    }
    
    // Weapon 3 (just pressed this frame)
    if (key3Down && !prev3Down) {
        std::cout << "KEY 3 DETECTED - Switching to weapon 3 (Rocket Launcher)" << std::endl;
        m_player.setCurrentWeapon(WeaponType::RocketLauncher);
        m_currentWeaponTexture = m_rocketLauncherTexture;
        showNotification("Rocket Launcher", 1.0);
    }
    
    // Update previous key states
    prev1Down = key1Down;
    prev2Down = key2Down;
    prev3Down = key3Down;
    
    // Forward/backward movement with W/S or UP/DOWN
    if (keyboardState[SDL_SCANCODE_W] || keyboardState[SDL_SCANCODE_UP]) {
        m_player.moveForward(m_deltaTime, m_map);
    }
    if (keyboardState[SDL_SCANCODE_S] || keyboardState[SDL_SCANCODE_DOWN]) {
        m_player.moveBackward(m_deltaTime, m_map);
    }
    
    // Strafe left/right with A/D
    if (keyboardState[SDL_SCANCODE_A]) {
        m_player.strafeLeft(m_deltaTime, m_map);
    }
    if (keyboardState[SDL_SCANCODE_D]) {
        m_player.strafeRight(m_deltaTime, m_map);
    }
    
    // Rotation with LEFT/RIGHT arrow keys
    if (keyboardState[SDL_SCANCODE_LEFT]) {
        // Rotate left
        double rotationAmount = 2.0 * m_deltaTime;  // Adjust rotation speed as needed
        double oldDirX = m_player.getDirX();
        double oldDirY = m_player.getDirY();
        double oldPlaneX = m_player.getPlaneX();
        double oldPlaneY = m_player.getPlaneY();
        
        // Rotation matrix
        double cosRot = cos(rotationAmount);
        double sinRot = sin(rotationAmount);
        
        // Update player direction vector and camera plane
        Vec2 newDir(
            oldDirX * cosRot - oldDirY * sinRot,
            oldDirX * sinRot + oldDirY * cosRot
        );
        Vec2 newPlane(
            oldPlaneX * cosRot - oldPlaneY * sinRot,
            oldPlaneX * sinRot + oldPlaneY * cosRot
        );
        
        m_player.setDirection(newDir);
        m_player.setPlane(newPlane);
    }
    if (keyboardState[SDL_SCANCODE_RIGHT]) {
        // Rotate right
        double rotationAmount = -2.0 * m_deltaTime;  // Negative for right rotation
        double oldDirX = m_player.getDirX();
        double oldDirY = m_player.getDirY();
        double oldPlaneX = m_player.getPlaneX();
        double oldPlaneY = m_player.getPlaneY();
        
        // Rotation matrix
        double cosRot = cos(rotationAmount);
        double sinRot = sin(rotationAmount);
        
        // Update player direction vector and camera plane
        Vec2 newDir(
            oldDirX * cosRot - oldDirY * sinRot,
            oldDirX * sinRot + oldDirY * cosRot
        );
        Vec2 newPlane(
            oldPlaneX * cosRot - oldPlaneY * sinRot,
            oldPlaneX * sinRot + oldPlaneY * cosRot
        );
        
        m_player.setDirection(newDir);
        m_player.setPlane(newPlane);
    }
    
    // Jumping - DOOM-style: Jump only on key press, not while held down
    static bool prevSpaceDown = false;
    bool spaceDown = keyboardState[SDL_SCANCODE_SPACE] != 0;
    
    if (spaceDown && !prevSpaceDown) {
        m_player.jump();
    }
    prevSpaceDown = spaceDown;
    
    // Weapon firing
    bool shouldFire = false;
    
    // Check for firing with left control
    static bool prevLCtrlDown = false;
    bool lCtrlDown = keyboardState[SDL_SCANCODE_LCTRL] != 0;
    
    if (lCtrlDown && !prevLCtrlDown) {
        shouldFire = true;
    }
    prevLCtrlDown = lCtrlDown;
    
    // Check for firing action
    if (m_inputHandler.isActionJustPressed(InputAction::Fire, m_gameState)) {
        shouldFire = true;
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
        // Play sound effect FIRST based on the current weapon
        WeaponType currentWeapon = m_player.getCurrentWeapon();
        bool soundPlayed = false;
        
        if (m_audioSystem) {
            // Always play the sound effect first, regardless of whether the weapon fires successfully
            switch (currentWeapon) {
                case WeaponType::Pistol:
                    // Play pistol sound (dspistol.wav)
                    std::cout << "Playing pistol sound effect" << std::endl;
                    soundPlayed = m_audioSystem->playSoundEffect("pistol_fire");
                    break;
                    
                case WeaponType::MachineGun:
                    // Play machine gun sound (dsplasma.wav)
                    std::cout << "Playing machine gun sound effect" << std::endl;
                    soundPlayed = m_audioSystem->playSoundEffect("machinegun_fire");
                    break;
                    
                case WeaponType::RocketLauncher:
                    // Play rocket launcher sound (dsrlaunc.wav)
                    std::cout << "Playing rocket launcher sound effect" << std::endl;
                    soundPlayed = m_audioSystem->playSoundEffect("rocket_fire");
                    break;
                    
                case WeaponType::Shotgun:
                    // Fall back to pistol sound for now
                    soundPlayed = m_audioSystem->playSoundEffect("pistol_fire");
                    break;
                    
                case WeaponType::PlasmaGun:
                    // Use plasma sound (same as machine gun)
                    soundPlayed = m_audioSystem->playSoundEffect("machinegun_fire");
                    break;
                    
                case WeaponType::Chainsaw:
                    // Fall back to pistol sound for now
                    soundPlayed = m_audioSystem->playSoundEffect("pistol_fire");
                    break;
                    
                default:
                    // Default to pistol sound
                    soundPlayed = m_audioSystem->playSoundEffect("pistol_fire");
                    break;
            }
        }
        
        // Apply visual effects regardless of whether the projectile is created
        m_weaponRecoil = 0.1;
        m_flashIntensity = 1.0;
        
        // Now attempt to fire the weapon
        bool fireResult = m_player.fire();
        
        // Log results
        if (soundPlayed) {
            std::cout << "Weapon sound played successfully" << std::endl;
        } else {
            std::cout << "Failed to play weapon sound!" << std::endl;
        }
        
        if (fireResult) {
            std::cout << "Weapon fired successfully!" << std::endl;
        } else {
            std::cout << "Weapon projectile creation failed" << std::endl;
        }
    }
    
    // Reset mouse movement deltas so they don't accumulate
    m_inputHandler.resetMouseRel();
}

void Engine::handleMainMenuInput() {
    // Get direct keyboard state
    int numKeys;
    const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
    
    // Track previous key states for menu navigation
    static bool prevUpDown = false;
    static bool prevDownDown = false;
    static bool prevEnterDown = false;
    
    // Get current key states
    bool upDown = keyboardState[SDL_SCANCODE_UP] != 0;
    bool downDown = keyboardState[SDL_SCANCODE_DOWN] != 0;
    bool enterDown = keyboardState[SDL_SCANCODE_RETURN] != 0;
    
    // Debug output
    std::cout << "Menu input - UP: " << upDown << " (prev: " << prevUpDown << ")"
              << ", DOWN: " << downDown << " (prev: " << prevDownDown << ")"
              << ", ENTER: " << enterDown << " (prev: " << prevEnterDown << ")" << std::endl;
    
    // Check for menu navigation - UP key just pressed
    if (upDown && !prevUpDown) {
        m_menuSelection = (m_menuSelection - 1 + m_menuItems.size()) % m_menuItems.size();
        std::cout << "UP pressed - menu selection: " << m_menuSelection << std::endl;
        
        // Play menu sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_move");
        }
    }
    
    // Check for menu navigation - DOWN key just pressed
    if (downDown && !prevDownDown) {
        m_menuSelection = (m_menuSelection + 1) % m_menuItems.size();
        std::cout << "DOWN pressed - menu selection: " << m_menuSelection << std::endl;
        
        // Play menu sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_move");
        }
    }
    
    // Check for menu selection - ENTER key just pressed
    if (enterDown && !prevEnterDown) {
        std::cout << "ENTER pressed - selecting menu item: " << m_menuSelection << std::endl;
        
        // Play menu select sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_select");
        }
        
        // Handle the selected menu item
        switch (m_menuSelection) {
            case 0: // Play Game
                setState(GameState::Playing);
                break;
                
            case 1: // Save Game
                // TODO: Implement save game functionality
                showNotification("Save Game not implemented yet", 2.0);
                break;
                
            case 2: // Load Game
                // TODO: Implement load game functionality
                showNotification("Load Game not implemented yet", 2.0);
                break;
                
            case 3: // Exit
                m_running = false;
                break;
        }
    }
    
    // Update previous key states
    prevUpDown = upDown;
    prevDownDown = downDown;
    prevEnterDown = enterDown;
}

void Engine::handlePausedInput() {
    // Get direct keyboard state
    int numKeys;
    const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
    
    // Track previous key states for menu navigation
    static bool prevUpDown = false;
    static bool prevDownDown = false;
    static bool prevEnterDown = false;
    
    // Get current key states
    bool upDown = keyboardState[SDL_SCANCODE_UP] != 0;
    bool downDown = keyboardState[SDL_SCANCODE_DOWN] != 0;
    bool enterDown = keyboardState[SDL_SCANCODE_RETURN] != 0;
    
    // Debug output
    std::cout << "Pause menu input - UP: " << upDown << " (prev: " << prevUpDown << ")"
              << ", DOWN: " << downDown << " (prev: " << prevDownDown << ")"
              << ", ENTER: " << enterDown << " (prev: " << prevEnterDown << ")" << std::endl;
    
    // Check for menu navigation - UP key just pressed
    if (upDown && !prevUpDown) {
        m_menuSelection = (m_menuSelection - 1 + m_menuItems.size()) % m_menuItems.size();
        std::cout << "UP pressed - menu selection: " << m_menuSelection << std::endl;
        
        // Play menu sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_move");
        }
    }
    
    // Check for menu navigation - DOWN key just pressed
    if (downDown && !prevDownDown) {
        m_menuSelection = (m_menuSelection + 1) % m_menuItems.size();
        std::cout << "DOWN pressed - menu selection: " << m_menuSelection << std::endl;
        
        // Play menu sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_move");
        }
    }
    
    // Check for menu selection - ENTER key just pressed
    if (enterDown && !prevEnterDown) {
        std::cout << "ENTER pressed - selecting menu item: " << m_menuSelection << std::endl;
        
        // Play menu select sound if available
        if (m_audioSystem) {
            m_audioSystem->playSoundEffect("menu_select");
        }
        
        // Handle the selected menu item
        switch (m_menuSelection) {
            case 0: // Play Game
                setState(GameState::Playing);
                break;
                
            case 1: // Save Game
                // TODO: Implement save game functionality
                showNotification("Save Game not implemented yet", 2.0);
                break;
                
            case 2: // Load Game
                // TODO: Implement load game functionality
                showNotification("Load Game not implemented yet", 2.0);
                break;
                
            case 3: // Exit
                m_running = false;
                break;
        }
    }
    
    // Update previous key states
    prevUpDown = upDown;
    prevDownDown = downDown;
    prevEnterDown = enterDown;
}