#include <iostream>
#include "engine.h"

int main(int argc, char* argv[]) {
    // Print welcome message
    std::cout << "Starting Doom Clone..." << std::endl;
    
    // Create and initialize the game engine
    Engine engine;
    if (!engine.init(1024, 768, false, 60)) {
        std::cerr << "Failed to initialize game engine!" << std::endl;
        return 1;
    }
    
    // Run the game
    engine.run();
    
    // Engine will clean up resources in its destructor
    std::cout << "Doom Clone shut down successfully." << std::endl;
    
    return 0;
} 