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
    , m_map(20, 20)
    , m_gameState(GameState::MainMenu)
    , m_running(false)
    , m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
    , m_fullscreen(false)
    , m_targetFPS(60)
    , m_frameTime(1.0 / 60.0)
    , m_deltaTime(0.0)
    , m_wallTexture(-1)
    , m_floorTexture(-1)
    , m_ceilingTexture(-1)
    , m_enemyTexture(-1)
    , m_weaponTexture(-1)
    , m_bulletTexture(-1)
    , m_machineGunTexture(-1)
    , m_currentWeaponTexture(-1)
    , m_notificationText("")
    , m_notificationTimer(0.0)
    , m_weaponRecoil(0.0)
    , m_weaponRecoilRecovery(5.0)
    , m_flashIntensity(0.0)
    , m_flashDecay(5.0)
    , m_lastFrameTime(0)
    , m_font(nullptr)
    , m_notificationTexture(nullptr)
    , m_notificationRect{}
{
    std::cout << "Engine created with resolution " << screenWidth << "x" << screenHeight << std::endl;
}

Engine::~Engine() {
    shutdown();
    
    // Clean up resources
    if (m_projectileManager) delete m_projectileManager;
    if (m_spriteManager) delete m_spriteManager;
    if (m_textureManager) delete m_textureManager;
    if (m_renderer) delete m_renderer;
    
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
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
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
    
    // Set up map
    m_map = Map(20, 20);  // Create map with default size
    std::cout << "Map created with size: " << m_map.getWidth() << "x" << m_map.getHeight() << std::endl;
    
    // Initialize player with position and direction
    m_player.init(8.0, 8.0, 1.0, 0.0);  // x, y, dirX, dirY
    std::cout << "Player initialized at position (8.0, 8.0)" << std::endl;
    
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
    // Handle SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
        }
        else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    m_running = false;
                    break;
                case SDLK_2:  // Switch to machine gun
                    if (m_currentWeaponTexture != m_machineGunTexture) {
                        m_currentWeaponTexture = m_machineGunTexture;
                        m_notificationText = "Switched to machine gun";
                        m_notificationTimer = 3.0;  // Show for 3 seconds
                        std::cout << "Switched to machine gun" << std::endl;
                    }
                    break;
                case SDLK_1:  // Switch back to default weapon
                    if (m_currentWeaponTexture != m_weaponTexture) {
                        m_currentWeaponTexture = m_weaponTexture;
                        m_notificationText = "Switched to shotgun";
                        m_notificationTimer = 3.0;  // Show for 3 seconds
                        std::cout << "Switched to shotgun" << std::endl;
                    }
                    break;
                case SDLK_SPACE:
                    // Manual firing test (independent of player state)
                    std::cout << "SPACE key pressed - Manual firing test" << std::endl;
                    if (m_projectileManager) {
                        Vec2 bulletPos = m_player.getPosition();
                        Vec2 bulletDir = m_player.getDirection();
                        
                        // Move bullet in front of player
                        bulletPos.x += bulletDir.x * 1.0;
                        bulletPos.y += bulletDir.y * 1.0;
                        
                        int bulletId = m_projectileManager->createProjectile(
                            bulletPos, 
                            bulletDir, 
                            ProjectileType::Bullet,
                            15.0,  // Speed
                            30.0   // Damage
                        );
                        
                        std::cout << "Created manual test bullet with ID " << bulletId 
                                  << " at (" << bulletPos.x << ", " << bulletPos.y << ")" << std::endl;
                    } else {
                        std::cerr << "ERROR: Could not create manual test bullet - Projectile manager is null!" << std::endl;
                    }
                    break;
            }
        }
    }
    
    // Get keyboard state
    const Uint8* keyState = SDL_GetKeyboardState(NULL);
    
    // Handle movement
    double moveSpeed = 3.0 * m_deltaTime;
    double rotSpeed = 2.0 * m_deltaTime;
    
    if (keyState[SDL_SCANCODE_W]) {
        m_player.moveForward(moveSpeed, m_map);
    }
    if (keyState[SDL_SCANCODE_S]) {
        m_player.moveBackward(moveSpeed, m_map);
    }
    if (keyState[SDL_SCANCODE_D]) {
        m_player.rotateLeft(rotSpeed);
    }
    if (keyState[SDL_SCANCODE_A]) {
        m_player.rotateRight(rotSpeed);
    }
    
    // Normal fire through player object
    if (keyState[SDL_SCANCODE_SPACE]) {
        std::cout << "Trying to fire through player object..." << std::endl;
        
        // Check if player has a project manager
        if (m_player.getProjectileManager()) {
            std::cout << "Player has projectile manager: " << m_player.getProjectileManager() << std::endl;
            bool fireSuccess = m_player.fire();
            std::cout << "Player fire result: " << (fireSuccess ? "SUCCESS" : "FAILED") << std::endl;
        } else {
            std::cerr << "ERROR: Player has no projectile manager! Reconnecting..." << std::endl;
            m_player.setProjectileManager(m_projectileManager);
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
        
        // Update flickering lights - optimized version
        if (m_renderer) {
            static float flickerTimer = 0.0f;
            flickerTimer += m_deltaTime;
            
            // Only update every few frames to reduce CPU usage
            static int frameSkip = 0;
            frameSkip = (frameSkip + 1) % 2;  // Update every 2 frames
            if (frameSkip == 0) {
                LightingSystem& lighting = m_renderer->getLightingSystem();
                
                // Pre-calculate common values used by all lights
                float fastFlickerBase = sin(flickerTimer * 15.0f) * 0.2f;
                float mediumFlickerBase = sin(flickerTimer * 7.0f) * 0.15f;
                float slowFlickerBase = sin(flickerTimer * 3.0f) * 0.1f;
                
                // Update each point light's intensity
                for (size_t i = 0; i < lighting.getLightCount(); ++i) {
                    Light& light = lighting.getLightAt(i);
                    
                    if (light.type == LightType::Point) {
                        // Use light's position to create variation without expensive calculations
                        float positionOffset = (light.position.x + light.position.y) * 0.1f;
                        
                        // Combine pre-calculated flicker values with position offset
                        float flickerAmount = fastFlickerBase + 
                                           mediumFlickerBase * cos(positionOffset) + 
                                           slowFlickerBase * sin(positionOffset);
                        
                        // Add slight randomness (less frequently)
                        static int randomSkip = 0;
                        if (++randomSkip >= 10) {  // Only add randomness every 10 updates
                            flickerAmount += (rand() % 10) * 0.01f - 0.05f;
                            randomSkip = 0;
                        }
                        
                        // Apply the flicker effect
                        float baseIntensity = light.intensity;
                        light.intensity = std::max(0.1f, std::min(1.0f, baseIntensity + flickerAmount));
                        
                        // Vary radius less frequently and with less intensity
                        if (randomSkip == 0) {  // Only update radius every 10 updates
                            float radiusVariation = sin(flickerTimer + positionOffset) * 0.2f;
                            light.radius = std::max(1.0f, static_cast<float>(light.radius + radiusVariation));
                        }
                    }
                }
            }
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
    std::cout << "Creating bullet texture..." << std::endl;
    m_bulletTexture = m_textureManager->createSolidTexture(32, 32, Color(255, 255, 0));
    std::cout << "Bullet texture ID: " << m_bulletTexture << std::endl;
    
    // Create enemy texture
    std::cout << "Creating enemy texture..." << std::endl;
    SDL_Surface* enemySurface = SDL_CreateRGBSurface(0, 32, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (enemySurface) {
        SDL_LockSurface(enemySurface);
        Uint32* pixels = (Uint32*)enemySurface->pixels;
        
        // Fill with transparent color first
        Uint32 transparent = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
        for (int i = 0; i < 32 * 64; i++) {
            pixels[i] = transparent;
        }
        
        // Draw enemy shape (simplified for stability)
        for (int y = 10; y < 54; y++) {
            for (int x = 8; x < 24; x++) {
                pixels[y * 32 + x] = SDL_MapRGBA(enemySurface->format, 200, 0, 0, 255);
            }
        }
        
        SDL_UnlockSurface(enemySurface);
        m_enemyTexture = m_textureManager->createTextureFromSurface(enemySurface);
        SDL_FreeSurface(enemySurface);
    } else {
        m_enemyTexture = m_textureManager->createSolidTexture(32, 64, Color(255, 0, 0));
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
    
    // Set bullet texture in projectile manager
    if (m_projectileManager && m_bulletTexture >= 0) {
        m_projectileManager->setDefaultBulletTexture(m_bulletTexture);
        std::cout << "Set default bullet texture ID: " << m_bulletTexture << std::endl;
    }
    
    // Store texture IDs for wall variations
    m_wallTextureVariations.clear();
    if (m_wallTexture >= 0) m_wallTextureVariations.push_back(m_wallTexture);  // Bloody stone
    if (runeTextureId >= 0) m_wallTextureVariations.push_back(runeTextureId);  // Runes
    if (fleshTextureId >= 0) m_wallTextureVariations.push_back(fleshTextureId);  // Flesh
    if (metalTextureId >= 0) m_wallTextureVariations.push_back(metalTextureId);  // Metal
    
    // Verify all required textures were created
    bool success = m_wallTexture >= 0 && m_floorTexture >= 0 && m_ceilingTexture >= 0 && 
                  m_bulletTexture >= 0 && m_enemyTexture >= 0 && m_weaponTexture >= 0 && 
                  m_machineGunTexture >= 0 && !m_wallTextureVariations.empty();
    
    if (!success) {
        std::cerr << "Failed to create one or more required textures!" << std::endl;
        return false;
    }
    
    std::cout << "All textures loaded successfully" << std::endl;
    return true;
}

void Engine::setupMap() {
    std::cout << "Setting up game map..." << std::endl;
    
    // Create a more interesting map with varied wall textures
    for (int y = 0; y < m_map.getHeight(); y++) {
        for (int x = 0; x < m_map.getWidth(); x++) {
            // Create walls around the perimeter
            if (x == 0 || y == 0 || x == m_map.getWidth() - 1 || y == m_map.getHeight() - 1) {
                m_map.setCell(x, y, CellType::Wall);
                
                // Use metal texture for perimeter walls (last texture in variations)
                if (!m_wallTextureVariations.empty()) {
                    m_map.setWallTexture(x, y, m_wallTextureVariations.back());
                } else {
                    m_map.setWallTexture(x, y, m_wallTexture);  // Fallback to default
                }
            } else {
                // Add some walls in the interior with different patterns
                if ((x % 5 == 0 || y % 5 == 0) && rand() % 3 == 0) {
                    m_map.setCell(x, y, CellType::Wall);
                    
                    // Choose wall texture based on position and randomness
                    if (!m_wallTextureVariations.empty()) {
                        int textureChoice = rand() % 100;
                        int selectedTexture;
                        
                        if (x % 7 == 0 && y % 7 == 0) {
                            // Create "ritual circles" with rune walls (second texture)
                            selectedTexture = m_wallTextureVariations.size() > 1 ? 
                                m_wallTextureVariations[1] : m_wallTextureVariations[0];
                        }
                        else if (textureChoice < 40) {
                            // 40% chance for bloody stone (first texture)
                            selectedTexture = m_wallTextureVariations[0];
                        }
                        else if (textureChoice < 70 && m_wallTextureVariations.size() > 2) {
                            // 30% chance for flesh walls (third texture)
                            selectedTexture = m_wallTextureVariations[2];
                        }
                        else if (textureChoice < 90 && m_wallTextureVariations.size() > 1) {
                            // 20% chance for rune walls (second texture)
                            selectedTexture = m_wallTextureVariations[1];
                        }
                        else {
                            // 10% chance for metal walls (last texture) or fallback to first
                            selectedTexture = m_wallTextureVariations.back();
                        }
                        
                        m_map.setWallTexture(x, y, selectedTexture);
                        
                        // Create clusters of similar textures
                        if (x > 0 && y > 0 && x < m_map.getWidth() - 1 && y < m_map.getHeight() - 1) {
                            // Check adjacent walls and match textures sometimes
                            if (m_map.getCell(x-1, y) == CellType::Wall && rand() % 3 == 0) {
                                m_map.setWallTexture(x, y, m_map.getWallTexture(x-1, y));
                            }
                            if (m_map.getCell(x, y-1) == CellType::Wall && rand() % 3 == 0) {
                                m_map.setWallTexture(x, y, m_map.getWallTexture(x, y-1));
                            }
                        }
                    } else {
                        // Fallback to default wall texture if no variations available
                        m_map.setWallTexture(x, y, m_wallTexture);
                    }
                } else {
                    m_map.setCell(x, y, CellType::Empty);
                    
                    // Chance to spawn an enemy in empty cells
                    if (rand() % 20 == 0 && m_spriteManager && m_enemyTexture >= 0) {
                        // Ensure we're not spawning too close to the player start position
                        double distToCenter = std::sqrt(
                            std::pow(x - m_map.getWidth() / 2.0, 2) +
                            std::pow(y - m_map.getHeight() / 2.0, 2)
                        );
                        
                        if (distToCenter > 5.0) {  // Don't spawn too close to player
                            // Add enemy sprite
                            int spriteId = m_spriteManager->addSprite(
                                x + 0.5,  // Center in the cell
                                y + 0.5,
                                1.0,      // Size
                                m_enemyTexture,
                                SpriteType::Enemy
                            );
                            
                            // Add a pulsing red light for the enemy
                            if (spriteId >= 0 && m_renderer) {
                                Light enemyLight = Light::createPointLight(
                                    Vec2(x + 0.5, y + 0.5),  // Position
                                    Color(255, 0, 0),        // Red light
                                    0.7,                     // Higher intensity
                                    4.0                      // Larger radius
                                );
                                m_renderer->getLightingSystem().addLight(enemyLight);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Set up lighting
    if (m_renderer) {
        LightingSystem& lighting = m_renderer->getLightingSystem();
        
        // Set darker ambient lighting for more dramatic effect
        lighting.setAmbientColor(Color(32, 32, 48));  // Darker bluish tone
        lighting.setAmbientIntensity(0.15);           // Lower ambient intensity
        
        // Add main directional light (moonlight)
        Light moonlight = Light::createDirectionalLight(
            Vec2(-0.5, -0.7),           // Direction
            Color(120, 120, 180),       // Cool bluish tone
            0.4                         // Moderate intensity
        );
        lighting.addLight(moonlight);
        
        // Add flickering lights throughout the map
        for (int y = 2; y < m_map.getHeight() - 2; y += 4) {
            for (int x = 2; x < m_map.getWidth() - 2; x += 4) {
                // Only place lights near walls
                bool nearWall = false;
                for (int dy = -1; dy <= 1 && !nearWall; dy++) {
                    for (int dx = -1; dx <= 1 && !nearWall; dx++) {
                        if (m_map.getCell(x + dx, y + dy) == CellType::Wall) {
                            nearWall = true;
                        }
                    }
                }
                
                if (nearWall && rand() % 2 == 0) {
                    // Create a flickering light with random color variation
                    Color baseColor;
                    switch (rand() % 3) {
                        case 0: // Warm orange
                            baseColor = Color(255, 147, 41);
                            break;
                        case 1: // Cool blue
                            baseColor = Color(41, 169, 255);
                            break;
                        case 2: // Eerie green
                            baseColor = Color(41, 255, 147);
                            break;
                    }
                    
                    Light flickeringLight = Light::createPointLight(
                        Vec2(x + 0.5, y + 0.5),
                        baseColor,
                        0.8 + (rand() % 20) / 100.0,  // Random base intensity
                        6.0 + (rand() % 20) / 10.0    // Random radius
                    );
                    lighting.addLight(flickeringLight);
                }
            }
        }
        
        // Add some larger area lights at key positions
        std::vector<Vec2> keyPositions = {
            Vec2(m_map.getWidth() / 4, m_map.getHeight() / 4),
            Vec2(3 * m_map.getWidth() / 4, m_map.getHeight() / 4),
            Vec2(m_map.getWidth() / 4, 3 * m_map.getHeight() / 4),
            Vec2(3 * m_map.getWidth() / 4, 3 * m_map.getHeight() / 4),
            Vec2(m_map.getWidth() / 2, m_map.getHeight() / 2)
        };
        
        for (const Vec2& pos : keyPositions) {
            // Create a large, intense light with a unique color
            Color lightColor(
                128 + rand() % 128,
                128 + rand() % 128,
                128 + rand() % 128
            );
            
            Light areaLight = Light::createPointLight(
                pos,
                lightColor,
                1.0,    // Full intensity
                10.0    // Large radius
            );
            lighting.addLight(areaLight);
        }
        
        // Add some small, subtle accent lights
        for (int i = 0; i < 10; i++) {
            int x = 2 + rand() % (m_map.getWidth() - 4);
            int y = 2 + rand() % (m_map.getHeight() - 4);
            
            if (m_map.getCell(x, y) == CellType::Empty) {
                Light accentLight = Light::createPointLight(
                    Vec2(x + 0.5, y + 0.5),
                    Color(
                        50 + rand() % 50,
                        50 + rand() % 50,
                        50 + rand() % 50
                    ),
                    0.3 + (rand() % 20) / 100.0,  // Low intensity
                    3.0 + (rand() % 20) / 10.0    // Small radius
                );
                lighting.addLight(accentLight);
            }
        }
    }
    
    std::cout << "Map setup complete with enhanced lighting" << std::endl;
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
    
    // Enable mouse capture for looking around
    m_inputHandler.setMouseCapture(true);
} 