#include <iostream>
#include <memory>
#include <cmath>
#include <ctime>
#include "utils.h"
#include "map.h"
#include "renderer.h"
#include "player.h"

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

// Create a simple test map
std::shared_ptr<Map> createTestMap() {
    auto map = std::make_shared<Map>();
    
    // Create a main sector for our complex map
    auto mainSector = std::make_shared<Sector>();
    mainSector->setFloorHeight(0.0);
    mainSector->setCeilingHeight(3.0);
    mainSector->setFloorTextureId(1);
    mainSector->setCeilingTextureId(2);
    
    // Generate walls for complex map layout
    std::vector<Wall> doomWalls = map->generateDoomMap(Vec2(5.0, 5.0));
    
    // Convert the walls to shared pointers and add to the sector
    for (const auto& wall : doomWalls) {
        auto wallPtr = std::make_shared<Wall>(
            wall.getStart(), 
            wall.getEnd(), 
            wall.getTextureId()
        );
        mainSector->addWall(wallPtr);
    }
    
    // Add the sector to the map
    map->addSector(mainSector);
    
    // Build BSP tree for the map
    map->buildBSPTree();
    
    return map;
}

int main(int argc, char* argv[])
{
    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Print startup message
    std::cout << "DOOM Clone - Sector-Based BSP Renderer" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Parse command line arguments
    bool fullscreen = false;
    int screenWidth = 1280;
    int screenHeight = 720;
    int targetFPS = 60;
    bool vsync = true;
    bool showFPS = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--fullscreen") {
            fullscreen = true;
        } else if (arg == "--windowed") {
            fullscreen = false;
        } else if (arg == "--width" && i + 1 < argc) {
            screenWidth = std::max(640, std::min(3840, std::stoi(argv[++i])));
        } else if (arg == "--height" && i + 1 < argc) {
            screenHeight = std::max(480, std::min(2160, std::stoi(argv[++i])));
        } else if (arg == "--fps" && i + 1 < argc) {
            targetFPS = std::max(30, std::min(240, std::stoi(argv[++i])));
        } else if (arg == "--no-vsync") {
            vsync = false;
        } else if (arg == "--no-fps") {
            showFPS = false;
        } else if (arg == "--help") {
            std::cout << "Available options:" << std::endl;
            std::cout << "  --fullscreen       Run in fullscreen mode" << std::endl;
            std::cout << "  --windowed         Run in windowed mode" << std::endl;
            std::cout << "  --width N          Set window width to N pixels" << std::endl;
            std::cout << "  --height N         Set window height to N pixels" << std::endl;
            std::cout << "  --fps N            Set target FPS to N" << std::endl;
            std::cout << "  --no-vsync         Disable vertical sync" << std::endl;
            std::cout << "  --no-fps           Hide FPS counter" << std::endl;
            std::cout << "  --help             Show this help message" << std::endl;
            return 0;
        }
    }
    
    std::cout << "Screen resolution: " << screenWidth << "x" << screenHeight << std::endl;
    std::cout << "Fullscreen: " << (fullscreen ? "Yes" : "No") << std::endl;
    std::cout << "Target FPS: " << targetFPS << std::endl;
    std::cout << "VSync: " << (vsync ? "Enabled" : "Disabled") << std::endl;
    
    // Create renderer
    auto renderer = std::make_unique<Renderer>();
    
    // Initialize renderer
    if (!renderer->init(screenWidth, screenHeight, fullscreen, vsync)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }
    
    // Create test map
    auto map = createTestMap();
    
    // Create player and camera
    auto player = std::make_unique<Player>();
    auto camera = std::make_unique<Camera>();
    
    // Initialize player at the center of the new map (matching where we generated the walls)
    player->init(Vec2(5.0, 5.0), 0.0, map.get());
    
    // Initialize camera to follow player
    camera->init(player.get());
    
    // Set up frame time tracking
    Uint32 lastFrameTime = SDL_GetTicks();
    Uint32 frameCount = 0;
    Uint32 lastFPSUpdateTime = lastFrameTime;
    double currentFPS = 0.0;
    
    // Calculate frame delay for target FPS
    const Uint32 frameDelay = 1000 / targetFPS;
    
    // Main game loop
    bool running = true;
    SDL_Event event;
    
    // Input state
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool rotateLeft = false;
    bool rotateRight = false;
    bool jump = false;
    bool crouch = false;
    int mouseX = 0, mouseY = 0;
    
    // Set up mouse for relative mode (capturing)
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    // Ensure the first frame doesn't have large mouse movements
    SDL_GetRelativeMouseState(&mouseX, &mouseY);
    mouseX = 0;
    mouseY = 0;
    
    while (running) {
        // Reset one-time input flags
        jump = false;
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_w:
                        moveForward = true;
                        break;
                    case SDLK_s:
                        moveBackward = true;
                        break;
                    case SDLK_a:
                        moveLeft = true;
                        break;
                    case SDLK_d:
                        moveRight = true;
                        break;
                    case SDLK_q:
                        rotateLeft = true;
                        break;
                    case SDLK_e:
                        rotateRight = true;
                        break;
                    case SDLK_SPACE:
                        jump = true;
                        break;
                    case SDLK_LCTRL:
                        crouch = true;
                        break;
                    case SDLK_f:
                        // Toggle fullscreen
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(SDL_GetWindowFromID(event.key.windowID),
                                               fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;
                    default:
                        break;
                }
            } else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        moveForward = false;
                        break;
                    case SDLK_s:
                        moveBackward = false;
                        break;
                    case SDLK_a:
                        moveLeft = false;
                        break;
                    case SDLK_d:
                        moveRight = false;
                        break;
                    case SDLK_q:
                        rotateLeft = false;
                        break;
                    case SDLK_e:
                        rotateRight = false;
                        break;
                    case SDLK_LCTRL:
                        crouch = false;
                        break;
                    default:
                        break;
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                mouseX = event.motion.xrel;
                mouseY = event.motion.yrel;
            }
        }
        
        // Calculate delta time
        Uint32 currentFrameTime = SDL_GetTicks();
        double deltaTime = (currentFrameTime - lastFrameTime) / 1000.0;
        lastFrameTime = currentFrameTime;
        
        // Cap delta time to prevent jumps after pauses/freezes
        deltaTime = std::min(deltaTime, 0.1);
        
        // Get the latest mouse movement even if no event was generated
        // This ensures continuous rotation even when mouse hits screen edge
        int latestMouseX, latestMouseY;
        SDL_GetRelativeMouseState(&latestMouseX, &latestMouseY);
        mouseX += latestMouseX;
        mouseY += latestMouseY;
        
        // Process player input using the standard input processing system
        player->processInput(
            moveForward, moveBackward, moveLeft, moveRight,
            rotateLeft, rotateRight, jump, crouch,
            mouseX, mouseY, true
        );
        
        // Reset mouse deltas
        mouseX = 0;
        mouseY = 0;
        
        // Update player
        player->update(deltaTime);
        
        // Update camera
        camera->update(deltaTime);
        
        // Start rendering frame
        renderer->beginFrame();
        
        // Render the map
        renderer->renderMap(map.get(), camera.get());
        
        // Render HUD
        renderer->renderHUD(player.get());
        
        // Calculate and display FPS
        frameCount++;
        if (currentFrameTime - lastFPSUpdateTime >= 1000) {
            currentFPS = frameCount * 1000.0 / (currentFrameTime - lastFPSUpdateTime);
            frameCount = 0;
            lastFPSUpdateTime = currentFrameTime;
        }
        
        // Display FPS if enabled
        if (showFPS) {
            char fpsText[32];
            sprintf(fpsText, "FPS: %.1f", currentFPS);
            renderer->renderDebugInfo(fpsText, 10, 10);
        }
        
        // Present frame
        renderer->endFrame();
        
        // Cap frame rate
        Uint32 frameTicks = SDL_GetTicks() - currentFrameTime;
        if (frameTicks < frameDelay) {
            SDL_Delay(frameDelay - frameTicks);
        }
    }
    
    // Restore mouse
    SDL_SetRelativeMouseMode(SDL_FALSE);
    
    // Cleanup happens automatically thanks to RAII with unique_ptr
    
    return 0;
} 