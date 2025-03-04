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
    , m_weaponRecoil(0.0)
    , m_weaponRecoilRecovery(5.0)
    , m_flashIntensity(0.0)
    , m_flashDecay(5.0)
    , m_lastFrameTime(0)
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
    
    // Clean up SDL
    if (m_sdlRenderer) SDL_DestroyRenderer(m_sdlRenderer);
    if (m_window) SDL_DestroyWindow(m_window);
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
    
    // Initialize sprite manager
    m_spriteManager = new SpriteManager(m_textureManager);
    std::cout << "Sprite manager created: " << m_spriteManager << std::endl;
    
    // Initialize projectile manager
    m_projectileManager = new ProjectileManager();
    std::cout << "Projectile manager created: " << m_projectileManager << std::endl;
    
    // Set up map
    m_map = Map(20, 20);  // Create map with default size
    std::cout << "Map created with size: " << m_map.getWidth() << "x" << m_map.getHeight() << std::endl;
    
    // Initialize player with position and direction
    m_player.init(8.0, 8.0, 1.0, 0.0);  // x, y, dirX, dirY
    std::cout << "Player initialized at position (8.0, 8.0)" << std::endl;
    
    // Connect player to projectile manager
    m_player.setProjectileManager(m_projectileManager);
    
    // Initialize renderer with all required parameters
    m_renderer = new Renderer();
    if (!m_renderer->init(m_screenWidth, m_screenHeight, m_fullscreen)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }
    
    // Set the SDL renderer in our game renderer
    m_renderer->setSDLRenderer(m_sdlRenderer);
    std::cout << "Game renderer initialized successfully and connected to SDL renderer" << std::endl;
    
    m_renderer->setTextureManager(m_textureManager);
    m_renderer->setSpriteManager(m_spriteManager);
    m_renderer->setProjectileManager(m_projectileManager);
    std::cout << "Managers connected to renderer" << std::endl;
    
    // Initialize input handler
    setupInput();
    std::cout << "Input handler initialized" << std::endl;
    
    // Load assets
    if (!loadAssets()) {
        std::cerr << "Failed to load assets" << std::endl;
        return false;
    }
    std::cout << "Assets loaded successfully" << std::endl;
    
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
                            15.0,  // Fast bullet
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
    if (keyState[SDL_SCANCODE_A]) {
        m_player.rotateLeft(rotSpeed);
    }
    if (keyState[SDL_SCANCODE_D]) {
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

void Engine::render() {
    // Render based on game state
    switch (m_gameState) {
        case GameState::Playing:
        case GameState::Paused:
            // Render the 3D view
            m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
            
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
            // TODO: Render game over overlay
            break;
            
        case GameState::Victory:
            // Render the 3D view
            m_renderer->render(m_map, m_player, m_deltaTime, m_weaponRecoil, m_flashIntensity);
            // TODO: Render victory overlay
            break;
    }
}

bool Engine::loadAssets() {
    std::cout << "Loading assets..." << std::endl;
    
    // Set up textures for walls, floor, ceiling
    std::string assetsPath = "assets/textures/";
    
    // Load wall textures
    m_wallTexture = m_textureManager->loadTexture(assetsPath + "wall1.png");
    if (m_wallTexture < 0) {
        std::cout << "Creating DOOM-style wall texture..." << std::endl;
        SDL_Surface* wallSurface = createDoomWallTexture(64, 64);
        if (wallSurface) {
            m_wallTexture = m_textureManager->createTextureFromSurface(wallSurface);
            SDL_FreeSurface(wallSurface);
        } else {
            std::cout << "Failed to create wall texture, falling back to solid color" << std::endl;
            m_wallTexture = m_textureManager->createSolidTexture(64, 64, Color(128, 128, 128));
        }
    }
    std::cout << "Wall texture ID: " << m_wallTexture << std::endl;
    
    // Load floor texture
    m_floorTexture = m_textureManager->loadTexture(assetsPath + "floor1.png");
    if (m_floorTexture < 0) {
        std::cout << "Creating floor texture..." << std::endl;
        // Create a darker variant of the wall texture for the floor
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
    }
    std::cout << "Floor texture ID: " << m_floorTexture << std::endl;
    
    // Load ceiling texture
    m_ceilingTexture = m_textureManager->loadTexture(assetsPath + "ceiling1.png");
    if (m_ceilingTexture < 0) {
        std::cout << "Creating ceiling texture..." << std::endl;
        // Create a lighter variant of the wall texture for the ceiling
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
    }
    std::cout << "Ceiling texture ID: " << m_ceilingTexture << std::endl;
    
    // Create bullet texture
    m_bulletTexture = m_textureManager->createSolidTexture(32, 32, Color(255, 255, 0));
    std::cout << "Bullet texture ID: " << m_bulletTexture << std::endl;
    
    // Load enemy texture
    m_enemyTexture = m_textureManager->loadTexture(assetsPath + "enemy1.png");
    if (m_enemyTexture < 0) {
        std::cout << "Failed to load enemy texture, creating solid color" << std::endl;
        m_enemyTexture = m_textureManager->createSolidTexture(64, 64, Color(255, 0, 0));
    }
    std::cout << "Enemy texture ID: " << m_enemyTexture << std::endl;
    
    // Set bullet texture in projectile manager
    if (m_projectileManager && m_bulletTexture >= 0) {
        m_projectileManager->setDefaultBulletTexture(m_bulletTexture);
        std::cout << "Set default bullet texture ID: " << m_bulletTexture << std::endl;
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
                std::cout << "#";
            } else {
                // Add some random walls in the interior
                if ((x % 5 == 0 || y % 5 == 0) && rand() % 3 == 0) {
                    m_map.setCell(x, y, CellType::Wall);
                    m_map.setWallTexture(x, y, m_wallTexture);
                    std::cout << "#";
                } else {
                    m_map.setCell(x, y, CellType::Empty);
                    std::cout << ".";
                    
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
                            
                            if (spriteId >= 0) {
                                std::cout << "Spawned enemy at (" << x << ", " << y << ")" << std::endl;
                            }
                        }
                    }
                }
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "Map setup complete" << std::endl;
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