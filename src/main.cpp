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
#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

// Create a simple test map
std::shared_ptr<Map> createTestMap() {
    auto map = std::make_shared<Map>();
    
    // Create a main sector for our complex map (ground floor)
    auto mainSector = std::make_shared<Sector>();
    mainSector->setFloorHeight(0.0);
    mainSector->setCeilingHeight(6.0);  // Double ceiling height for more space
    mainSector->setFloorTextureId(1);
    mainSector->setCeilingTextureId(2);
    
    // Generate walls for complex map layout
    std::vector<Wall> doomWalls = map->generateDoomMap(Vec2(10.0, 10.0));  // Start at (10,10) instead of (5,5) for more room
    
    // Convert the walls to shared pointers and add to the sector
    for (const auto& wall : doomWalls) {
        auto wallPtr = std::make_shared<Wall>(
            wall.getStart(), 
            wall.getEnd(), 
            wall.getTextureId()
        );
        mainSector->addWall(wallPtr);
    }
    
    // Add the ground floor sector to the map
    map->addSector(mainSector);
    
    // ========================
    // CREATE STAIRCASE SECTOR
    // ========================
    
    // Create a staircase sector that connects to the main floor
    double stairStartX = 10.0 + 12.0;  // Start stairs in the east corridor (doubled dimensions)
    double stairStartY = 10.0;
    double stairWidth = 6.0;  // Double stair width
    double stairLength = 16.0;  // Double stair length
    
    // Create a sector for the staircase
    auto stairSector = std::make_shared<Sector>();
    stairSector->setFloorHeight(0.0);       // Start at ground level
    stairSector->setCeilingHeight(12.0);     // Higher ceiling to accommodate stairs (doubled)
    stairSector->setFloorTextureId(3);      // Different texture for stairs
    stairSector->setCeilingTextureId(2);
    
    // Create walls for the staircase sector
    auto stairWall1 = std::make_shared<Wall>(
        Vec2(stairStartX, stairStartY - stairWidth/2),
        Vec2(stairStartX + stairLength, stairStartY - stairWidth/2),
        3
    );
    
    auto stairWall2 = std::make_shared<Wall>(
        Vec2(stairStartX + stairLength, stairStartY - stairWidth/2),
        Vec2(stairStartX + stairLength, stairStartY + stairWidth/2),
        3
    );
    
    auto stairWall3 = std::make_shared<Wall>(
        Vec2(stairStartX + stairLength, stairStartY + stairWidth/2),
        Vec2(stairStartX, stairStartY + stairWidth/2),
        3
    );
    
    auto stairWall4 = std::make_shared<Wall>(
        Vec2(stairStartX, stairStartY + stairWidth/2),
        Vec2(stairStartX, stairStartY - stairWidth/2),
        3
    );
    
    // Set the walls as portals to connect sectors
    stairWall4->setType(WallType::PORTAL);
    stairWall4->setAdjoiningSector(mainSector.get());
    
    // Find the matching wall in the main sector to make a portal
    // Look for the wall in the main sector that corresponds to where we're attaching the stairs
    bool foundPortalWall = false;
    for (const auto& wall : mainSector->getWalls()) {
        // Check if this wall is close to our desired connection point
        // We need to check if the wall is roughly at the right position and orientation
        Vec2 wallStart = wall->getStart();
        Vec2 wallEnd = wall->getEnd();
        
        // Check if this is a vertical wall near our stair entrance
        bool isVertical = std::abs(wallStart.x - wallEnd.x) < 0.1;
        bool isAtXPosition = std::abs(wallStart.x - stairStartX) < 1.0;  // Increased tolerance
        bool isInYRange = (wallStart.y <= stairStartY + stairWidth/2 + 1.0) && 
                          (wallEnd.y >= stairStartY - stairWidth/2 - 1.0);
        
        if (isVertical && isAtXPosition && isInYRange) {
            // Make this wall a portal to the stair sector
            wall->setType(WallType::PORTAL);
            wall->setAdjoiningSector(stairSector.get());
            foundPortalWall = true;
            break;
        }
    }
    
    // If we couldn't find a matching wall, create one
    if (!foundPortalWall) {
        // Create a custom portal in the eastern corridor
        auto portalWall = std::make_shared<Wall>(
            Vec2(stairStartX, stairStartY - stairWidth/2),
            Vec2(stairStartX, stairStartY + stairWidth/2),
            2
        );
        portalWall->setType(WallType::PORTAL);
        portalWall->setAdjoiningSector(stairSector.get());
        mainSector->addWall(portalWall);
    }
    
    // Add clear visual markers to indicate the staircase entrance
    // Add a pair of pillars on each side of the staircase entrance
    double markerSize = 1.0;  // Double marker size
    double markerOffset = stairWidth/2 + 0.6;
    
    // Left pillar (using a distinctive texture)
    auto leftMarker1 = std::make_shared<Wall>(
        Vec2(stairStartX - 0.6, stairStartY - markerOffset - markerSize),
        Vec2(stairStartX + 0.6, stairStartY - markerOffset - markerSize),
        0
    );
    auto leftMarker2 = std::make_shared<Wall>(
        Vec2(stairStartX + 0.6, stairStartY - markerOffset - markerSize),
        Vec2(stairStartX + 0.6, stairStartY - markerOffset),
        0
    );
    auto leftMarker3 = std::make_shared<Wall>(
        Vec2(stairStartX + 0.6, stairStartY - markerOffset),
        Vec2(stairStartX - 0.6, stairStartY - markerOffset),
        0
    );
    auto leftMarker4 = std::make_shared<Wall>(
        Vec2(stairStartX - 0.6, stairStartY - markerOffset),
        Vec2(stairStartX - 0.6, stairStartY - markerOffset - markerSize),
        0
    );
    
    // Right pillar (using a distinctive texture)
    auto rightMarker1 = std::make_shared<Wall>(
        Vec2(stairStartX - 0.6, stairStartY + markerOffset),
        Vec2(stairStartX + 0.6, stairStartY + markerOffset),
        0
    );
    auto rightMarker2 = std::make_shared<Wall>(
        Vec2(stairStartX + 0.6, stairStartY + markerOffset),
        Vec2(stairStartX + 0.6, stairStartY + markerOffset + markerSize),
        0
    );
    auto rightMarker3 = std::make_shared<Wall>(
        Vec2(stairStartX + 0.6, stairStartY + markerOffset + markerSize),
        Vec2(stairStartX - 0.6, stairStartY + markerOffset + markerSize),
        0
    );
    auto rightMarker4 = std::make_shared<Wall>(
        Vec2(stairStartX - 0.6, stairStartY + markerOffset + markerSize),
        Vec2(stairStartX - 0.6, stairStartY + markerOffset),
        0
    );
    
    // Add directional arrows on the floor pointing to the stairs
    // Create a series of small walls forming an arrow shape
    auto arrow1 = std::make_shared<Wall>(
        Vec2(stairStartX - 5.0, stairStartY - 1.6),  // Double the arrow size
        Vec2(stairStartX - 1.6, stairStartY),
        2
    );
    auto arrow2 = std::make_shared<Wall>(
        Vec2(stairStartX - 1.6, stairStartY),
        Vec2(stairStartX - 5.0, stairStartY + 1.6),
        2
    );
    
    // Add all markers to the main sector
    mainSector->addWall(leftMarker1);
    mainSector->addWall(leftMarker2);
    mainSector->addWall(leftMarker3);
    mainSector->addWall(leftMarker4);
    mainSector->addWall(rightMarker1);
    mainSector->addWall(rightMarker2);
    mainSector->addWall(rightMarker3);
    mainSector->addWall(rightMarker4);
    mainSector->addWall(arrow1);
    mainSector->addWall(arrow2);
    
    // Add a sign at the staircase entrance
    double signHeight = 1.6;  // Double sign height
    double signWidth = 3.0;   // Double sign width
    auto signWall = std::make_shared<Wall>(
        Vec2(stairStartX - 1.6, stairStartY - signWidth/2),
        Vec2(stairStartX - 1.6, stairStartY + signWidth/2),
        0
    );
    signWall->setHeight(signHeight);
    signWall->setBottomOffset(2.4);  // Position the sign above eye level (doubled)
    mainSector->addWall(signWall);
    
    // Add walls to stair sector
    stairSector->addWall(stairWall1);
    stairSector->addWall(stairWall2);
    stairSector->addWall(stairWall3);
    stairSector->addWall(stairWall4);
    
    // Create stair steps inside the staircase
    int numSteps = 8;
    double stepLength = stairLength / numSteps;
    double stepHeight = 6.0 / numSteps;  // Upper floor at height 6.0 (doubled)
    
    for (int i = 0; i < numSteps; i++) {
        double stepX = stairStartX + i * stepLength;
        
        // Create a wall representing the vertical rise of the step
        auto stepRiser = std::make_shared<Wall>(
            Vec2(stepX + stepLength, stairStartY - stairWidth/2 + 0.2),
            Vec2(stepX + stepLength, stairStartY + stairWidth/2 - 0.2),
            3
        );
        
        // Set properties for this step
        stepRiser->setHeight(stepHeight);
        stepRiser->setBottomOffset(i * stepHeight);
        
        // Add the step to the stair sector
        stairSector->addWall(stepRiser);
    }
    
    // Add stair sector to map
    map->addSector(stairSector);
    
    // ========================
    // CREATE UPPER ROOM SECTOR
    // ========================
    
    // Create a distinctive upper room that connects to the staircase
    double upperRoomHeight = 6.0;  // Double room height
    double upperFloorLevel = 6.0;  // Height of the upper floor (doubled)
    
    // Create a sector for the upper room
    auto upperRoomSector = std::make_shared<Sector>();
    upperRoomSector->setFloorHeight(upperFloorLevel);  // Higher floor level
    upperRoomSector->setCeilingHeight(upperFloorLevel + upperRoomHeight);
    upperRoomSector->setFloorTextureId(4);  // Distinctive floor texture
    upperRoomSector->setCeilingTextureId(0);  // Different ceiling texture
    
    // Define dimensions for the upper room
    double upperRoomSize = 24.0;  // Double size
    double upperRoomX = stairStartX + stairLength + upperRoomSize/2;
    double upperRoomY = stairStartY;
    
    // Create walls for the hexagonal upper room
    std::vector<Vec2> hexPoints;
    int numSides = 6;
    for (int i = 0; i < numSides; i++) {
        double angle = i * 2.0 * 3.14159265358979323846 / numSides;
        hexPoints.push_back(Vec2(
            upperRoomX + upperRoomSize * cos(angle),
            upperRoomY + upperRoomSize * sin(angle)
        ));
    }
    
    // Create the walls connecting the points
    std::vector<std::shared_ptr<Wall>> upperRoomWalls;
    for (int i = 0; i < numSides; i++) {
        int nextIdx = (i + 1) % numSides;
        
        // Skip the segment where the staircase connects
        if (i != 4) {  // Adjust this index based on your hexagon orientation
            upperRoomWalls.push_back(std::make_shared<Wall>(
                hexPoints[i],
                hexPoints[nextIdx],
                0  // Different texture for upper room
            ));
        }
    }
    
    // Create the portal connecting the staircase to the upper room
    auto upperRoomEntrance = std::make_shared<Wall>(
        hexPoints[4],
        hexPoints[5],
        0
    );
    upperRoomEntrance->setType(WallType::PORTAL);
    upperRoomEntrance->setAdjoiningSector(stairSector.get());
    
    // Connect staircase to upper room
    stairWall2->setType(WallType::PORTAL);
    stairWall2->setAdjoiningSector(upperRoomSector.get());
    
    // Add walls to upper room sector
    for (const auto& wall : upperRoomWalls) {
        upperRoomSector->addWall(wall);
    }
    upperRoomSector->addWall(upperRoomEntrance);
    
    // Add decorative features to upper room
    // Create a central structure in the upper room
    double pillarSize = 2.0;  // Double pillar size
    for (int i = 0; i < 3; i++) {
        double radius = upperRoomSize * 0.3;
        double angle = i * 2.0 * 3.14159265358979323846 / 3;
        Vec2 pillarCenter(
            upperRoomX + radius * cos(angle),
            upperRoomY + radius * sin(angle)
        );
        
        auto pillar1 = std::make_shared<Wall>(
            Vec2(pillarCenter.x - pillarSize, pillarCenter.y - pillarSize),
            Vec2(pillarCenter.x + pillarSize, pillarCenter.y - pillarSize),
            2  // Different texture for pillars
        );
        auto pillar2 = std::make_shared<Wall>(
            Vec2(pillarCenter.x + pillarSize, pillarCenter.y - pillarSize),
            Vec2(pillarCenter.x + pillarSize, pillarCenter.y + pillarSize),
            2
        );
        auto pillar3 = std::make_shared<Wall>(
            Vec2(pillarCenter.x + pillarSize, pillarCenter.y + pillarSize),
            Vec2(pillarCenter.x - pillarSize, pillarCenter.y + pillarSize),
            2
        );
        auto pillar4 = std::make_shared<Wall>(
            Vec2(pillarCenter.x - pillarSize, pillarCenter.y + pillarSize),
            Vec2(pillarCenter.x - pillarSize, pillarCenter.y - pillarSize),
            2
        );
        
        upperRoomSector->addWall(pillar1);
        upperRoomSector->addWall(pillar2);
        upperRoomSector->addWall(pillar3);
        upperRoomSector->addWall(pillar4);
    }
    
    // Add upper room sector to map
    map->addSector(upperRoomSector);
    
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
    player->init(Vec2(10.0, 10.0), 0.0, map.get());
    
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
                    case SDLK_LEFT:
                        rotateLeft = true;
                        break;
                    case SDLK_e:
                    case SDLK_RIGHT:
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
                    case SDLK_1:
                        // Select chainsaw weapon
                        player->selectWeapon(WeaponType::CHAINSAW);
                        std::cout << "DEBUG: Chainsaw weapon selected!" << std::endl;
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
                    case SDLK_LEFT:
                        rotateLeft = false;
                        break;
                    case SDLK_e:
                    case SDLK_RIGHT:
                        rotateRight = false;
                        break;
                    case SDLK_LCTRL:
                        crouch = false;
                        break;
                    default:
                        break;
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                // We're now using SDL_GetRelativeMouseState directly, so don't process the events here
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
        mouseX = latestMouseX;  // Use only the latest mouse movement
        mouseY = latestMouseY;
        
        // Process player input using the standard input processing system
        player->processInput(
            moveForward, moveBackward, moveLeft, moveRight,
            rotateLeft, rotateRight, jump, crouch,
            mouseX, mouseY, true
        );
        
        // Update player
        player->update(deltaTime);
        
        // Update camera
        camera->update(deltaTime);
        
        // Always select the chainsaw weapon for testing
        player->selectWeapon(WeaponType::CHAINSAW);
        
        // Start rendering frame
        renderer->beginFrame();
        
        // Render the map
        renderer->renderMap(map.get(), camera.get());
        
        // Render HUD with weapon
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
        
        // End the frame to update the screen
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