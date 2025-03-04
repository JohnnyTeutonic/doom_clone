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
        
        // Update flickering lights
        if (m_renderer) {
            LightingSystem& lighting = m_renderer->getLightingSystem();
            static float flickerTimer = 0.0f;
            flickerTimer += m_deltaTime;
            
            // Update each light's intensity
            for (size_t i = 0; i < lighting.getLightCount(); ++i) {
                Light& light = lighting.getLightAt(i);  // Assuming this method exists to get a modifiable light
                
                if (light.type == LightType::Point) {
                    // Different flicker patterns based on light position
                    float xOffset = light.position.x * 1.23f; // Use position to create varied patterns
                    float yOffset = light.position.y * 0.87f;
                    
                    // Combine multiple sine waves for more organic flickering
                    float fastFlicker = sin(flickerTimer * 15.0f + xOffset) * 0.2f;
                    float mediumFlicker = sin(flickerTimer * 7.0f + yOffset) * 0.15f;
                    float slowFlicker = sin(flickerTimer * 3.0f + xOffset + yOffset) * 0.1f;
                    
                    // Add some random noise for more chaotic effect
                    float noise = (rand() % 100) / 500.0f - 0.1f;
                    
                    // Combine all effects
                    float flickerAmount = fastFlicker + mediumFlicker + slowFlicker + noise;
                    
                    // Apply the flicker effect to the light's base intensity
                    float baseIntensity = light.intensity;
                    light.intensity = std::max(0.1f, std::min(1.0f, baseIntensity + flickerAmount));
                    
                    // Also slightly vary the light's radius
                    float radiusVariation = sin(flickerTimer * 5.0f + xOffset * yOffset) * 0.5f;
                    light.radius = std::max(1.0f, static_cast<float>(light.radius + radiusVariation));
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
    
    // Set up textures for walls, floor, ceiling
    std::string assetsPath = "assets/textures/";
    
    // Create wall texture (ID 0)
    std::cout << "Creating DOOM-style wall texture..." << std::endl;
    SDL_Surface* wallSurface = createDoomWallTexture(64, 64);
    if (wallSurface) {
        m_wallTexture = m_textureManager->createTextureFromSurface(wallSurface);
        SDL_FreeSurface(wallSurface);
    } else {
        std::cout << "Failed to create wall texture, falling back to solid color" << std::endl;
        m_wallTexture = m_textureManager->createSolidTexture(64, 64, Color(128, 128, 128));
    }
    std::cout << "Wall texture ID: " << m_wallTexture << std::endl;
    
    // Create floor texture (ID 1)
    std::cout << "Creating floor texture..." << std::endl;
    SDL_Surface* floorSurface = createDoomWallTexture(64, 64);
    if (floorSurface) {
        // Darken the floor texture
        SDL_LockSurface(floorSurface);
        Uint32* pixels = (Uint32*)floorSurface->pixels;
        for (int i = 0; i < 64 * 64; i++) {
            Uint8 r, g, b;
            SDL_GetRGB(pixels[i], floorSurface->format, &r, &g, &b);
            r = r * 2 / 3;
            g = g * 2 / 3;
            b = b * 2 / 3;
            pixels[i] = SDL_MapRGB(floorSurface->format, r, g, b);
        }
        SDL_UnlockSurface(floorSurface);
        
        m_floorTexture = m_textureManager->createTextureFromSurface(floorSurface);
        SDL_FreeSurface(floorSurface);
    } else {
        std::cout << "Failed to create floor texture, falling back to solid color" << std::endl;
        m_floorTexture = m_textureManager->createSolidTexture(64, 64, Color(32, 32, 64));
    }
    std::cout << "Floor texture ID: " << m_floorTexture << std::endl;
    
    // Create ceiling texture (ID 2)
    std::cout << "Creating ceiling texture..." << std::endl;
    SDL_Surface* ceilingSurface = createDoomWallTexture(64, 64);
    if (ceilingSurface) {
        // Lighten the ceiling texture
        SDL_LockSurface(ceilingSurface);
        Uint32* pixels = (Uint32*)ceilingSurface->pixels;
        for (int i = 0; i < 64 * 64; i++) {
            Uint8 r, g, b;
            SDL_GetRGB(pixels[i], ceilingSurface->format, &r, &g, &b);
            r = std::min(255, r * 3 / 2);
            g = std::min(255, g * 3 / 2);
            b = std::min(255, b * 3 / 2);
            pixels[i] = SDL_MapRGB(ceilingSurface->format, r, g, b);
        }
        SDL_UnlockSurface(ceilingSurface);
        
        m_ceilingTexture = m_textureManager->createTextureFromSurface(ceilingSurface);
        SDL_FreeSurface(ceilingSurface);
    } else {
        std::cout << "Failed to create ceiling texture, falling back to solid color" << std::endl;
        m_ceilingTexture = m_textureManager->createSolidTexture(64, 64, Color(64, 64, 96));
    }
    std::cout << "Ceiling texture ID: " << m_ceilingTexture << std::endl;
    
    // Create bullet texture (ID 3)
    std::cout << "Creating bullet texture..." << std::endl;
    m_bulletTexture = m_textureManager->createSolidTexture(32, 32, Color(255, 255, 0));
    std::cout << "Bullet texture ID: " << m_bulletTexture << std::endl;
    
    // Create enemy texture (ID 4)
    std::cout << "Creating enemy texture..." << std::endl;
    SDL_Surface* enemySurface = SDL_CreateRGBSurface(0, 32, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (enemySurface) {
        SDL_LockSurface(enemySurface);
        Uint32* pixels = (Uint32*)enemySurface->pixels;
        
        // Colors for the demon
        Uint32 darkRed = SDL_MapRGBA(enemySurface->format, 139, 0, 0, 255);      // Dark red for body
        Uint32 lightRed = SDL_MapRGBA(enemySurface->format, 220, 20, 20, 255);   // Lighter red for highlights
        Uint32 brown = SDL_MapRGBA(enemySurface->format, 139, 69, 19, 255);      // Brown for horns
        Uint32 yellow = SDL_MapRGBA(enemySurface->format, 255, 255, 0, 255);     // Yellow for eyes
        Uint32 black = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 255);          // Black for details
        
        // Fill with transparent color first
        Uint32 transparent = SDL_MapRGBA(enemySurface->format, 0, 0, 0, 0);
        for (int i = 0; i < 32 * 64; i++) {
            pixels[i] = transparent;
        }
        
        // Draw humanoid shape (narrower body)
        for (int y = 15; y < 60; y++) {
            int width = 12;  // Base body width
            // Wider at shoulders (y=20), narrower at waist (y=40)
            if (y < 25) width = 16;  // Shoulders
            else if (y > 40) width = 14;  // Legs
            
            int startX = (32 - width) / 2;
            for (int x = startX; x < startX + width; x++) {
                pixels[y * 32 + x] = darkRed;
            }
        }
        
        // Draw horns (smaller and more pointed)
        for (int y = 0; y < 15; y++) {
            for (int x = 8; x < 13; x++) {
                if (x - 8 <= y/2) pixels[y * 32 + x] = brown;
            }
            for (int x = 19; x < 24; x++) {
                if (24 - x <= y/2) pixels[y * 32 + x] = brown;
            }
        }
        
        // Draw eyes (yellow circles with black centers)
        for (int y = 18; y < 28; y++) {
            for (int x = 8; x < 15; x++) {
                int dx = x - 11;
                int dy = y - 23;
                if (dx*dx + dy*dy < 9) {
                    pixels[y * 32 + x] = yellow;
                    if (dx*dx + dy*dy < 4) {
                        pixels[y * 32 + x] = black;
                    }
                }
            }
            for (int x = 17; x < 24; x++) {
                int dx = x - 20;
                int dy = y - 23;
                if (dx*dx + dy*dy < 9) {
                    pixels[y * 32 + x] = yellow;
                    if (dx*dx + dy*dy < 4) {
                        pixels[y * 32 + x] = black;
                    }
                }
            }
        }
        
        // Draw mouth (smaller and more defined)
        for (int y = 30; y < 38; y++) {
            for (int x = 10; x < 22; x++) {
                // Main mouth line
                if (y == 34) pixels[y * 32 + x] = black;
                
                // Teeth
                if (y > 34 && y < 37 && (x % 4 < 2)) {
                    pixels[y * 32 + x] = lightRed;
                }
            }
        }
        
        // Add muscle definition with lighter red
        for (int y = 15; y < 60; y++) {
            int width = 12;
            if (y < 25) width = 16;
            else if (y > 40) width = 14;
            
            int startX = (32 - width) / 2;
            for (int x = startX; x < startX + width; x++) {
                if ((x + y) % 6 == 0 && pixels[y * 32 + x] == darkRed) {
                    pixels[y * 32 + x] = lightRed;
                }
            }
        }
        
        SDL_UnlockSurface(enemySurface);
        m_enemyTexture = m_textureManager->createTextureFromSurface(enemySurface);
        SDL_FreeSurface(enemySurface);
    } else {
        std::cout << "Failed to create enemy texture surface, falling back to simple texture" << std::endl;
        m_enemyTexture = m_textureManager->createCheckerboardTexture(32, 64, Color(255, 0, 0), Color(200, 0, 0), 8);
    }
    std::cout << "Enemy texture ID: " << m_enemyTexture << std::endl;
    
    // Load weapon texture (ID 5)
    std::cout << "Loading weapon texture (shotgun.webp)..." << std::endl;
    m_weaponTexture = m_textureManager->loadTexture(assetsPath + "shotgun.webp");
    if (m_weaponTexture < 0) {
        std::cout << "Failed to load weapon texture (shotgun.webp), creating solid color" << std::endl;
        m_weaponTexture = m_textureManager->createSolidTexture(256, 256, Color(128, 128, 128));
    }
    std::cout << "Weapon texture ID: " << m_weaponTexture << std::endl;
    
    // Load machine gun texture (ID 6)
    std::cout << "Loading machine gun texture (machine_gun.png)..." << std::endl;
    m_machineGunTexture = m_textureManager->loadTexture(assetsPath + "machine_gun.png");
    if (m_machineGunTexture < 0) {
        std::cout << "Failed to load machine gun texture, creating solid color" << std::endl;
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
    
    // Verify all textures were created
    if (m_wallTexture < 0 || m_floorTexture < 0 || m_ceilingTexture < 0 || 
        m_bulletTexture < 0 || m_enemyTexture < 0 || m_weaponTexture < 0 || m_machineGunTexture < 0) {
        std::cerr << "Failed to create one or more required textures!" << std::endl;
        return false;
    }
    
    return true;
}

void Engine::setupMap() {
    std::cout << "Setting up game map..." << std::endl;
    
    // Create a simple test map
    for (int y = 0; y < m_map.getHeight(); y++) {
        for (int x = 0; x < m_map.getWidth(); x++) {
            // Create walls around the perimeter
            if (x == 0 || y == 0 || x == m_map.getWidth() - 1 || y == m_map.getHeight() - 1) {
                m_map.setCell(x, y, CellType::Wall);
                m_map.setWallTexture(x, y, m_wallTexture);
            } else {
                // Add some random walls in the interior
                if ((x % 5 == 0 || y % 5 == 0) && rand() % 3 == 0) {
                    m_map.setCell(x, y, CellType::Wall);
                    m_map.setWallTexture(x, y, m_wallTexture);
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
        std::vector<Light> flickeringLights;
        
        // Add torch-like lights along the walls
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