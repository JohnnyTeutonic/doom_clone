#include "TextureManager.h"
#include <iostream>
#include <cmath>

// TextureManager constructor
TextureManager::TextureManager(SDL_Renderer* renderer) :
    m_renderer(renderer)
#ifdef ENABLE_CUDA
    , m_deviceTextureObjects(nullptr)
#endif
{
    // Load default textures
    loadDefaultTextures();
}

// TextureManager destructor
TextureManager::~TextureManager() {
#ifdef ENABLE_CUDA
    // Free CUDA texture objects
    freeCUDATextureObjects();
#endif

    // Clear texture storage
    m_textures.clear();
    m_nameToId.clear();
}

// Load default textures
void TextureManager::loadDefaultTextures() {
    // Create a default white texture (useful for simple color multiplication)
    createSolidTexture(64, 64, Color(255, 255, 255));
    
    // Create default textures for debugging/placeholders
    createSolidTexture(64, 64, Color(255, 0, 0));    // Red
    createSolidTexture(64, 64, Color(0, 255, 0));    // Green
    createSolidTexture(64, 64, Color(0, 0, 255));    // Blue
    createSolidTexture(64, 64, Color(255, 255, 0));  // Yellow
    createSolidTexture(64, 64, Color(255, 0, 255));  // Magenta
    createSolidTexture(64, 64, Color(0, 255, 255));  // Cyan
    
    // Create a default checkerboard pattern
    createPatternTexture(64, 64, Color(255, 255, 255), Color(0, 0, 0), 8);
    
    // Add DOOM-style textures
    createDoomWallTexture(Color(139, 69, 19), Color(101, 67, 33));  // Brown wall
    createDoomFloorTexture(Color(80, 80, 80), Color(40, 40, 40));   // Gray floor
    
    // Create pentagram for doom ambiance
    createPentagramTexture(true);   // Wall version
    createPentagramTexture(false);  // Floor version
    
    std::cout << "Loaded " << m_textures.size() << " default textures" << std::endl;
}

// Load texture from file
int TextureManager::loadTexture(const std::string& path) {
    // Check if texture is already loaded
    if (m_nameToId.find(path) != m_nameToId.end()) {
        return m_nameToId[path];
    }
    
    // Create new texture
    std::unique_ptr<Texture> texture = std::make_unique<Texture>();
    
    // Load from file
    if (!texture->loadFromFile(path, m_renderer)) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return -1;
    }
    
    // Get texture ID
    int id = static_cast<int>(m_textures.size());
    
    // Add to mapping
    m_nameToId[path] = id;
    
    // Add to storage
    m_textures.push_back(std::move(texture));
    
    return id;
}

// Create solid color texture
int TextureManager::createSolidTexture(int width, int height, const Color& color) {
    // Create name for this texture
    std::string name = "solid_" + std::to_string(color.r) + "_" + 
                     std::to_string(color.g) + "_" + 
                     std::to_string(color.b);
    
    // Check if texture is already created
    if (m_nameToId.find(name) != m_nameToId.end()) {
        return m_nameToId[name];
    }
    
    // Create new texture
    std::unique_ptr<Texture> texture = std::make_unique<Texture>();
    
    // Create solid color
    if (!texture->createSolid(width, height, color, m_renderer)) {
        std::cerr << "Failed to create solid texture" << std::endl;
        return -1;
    }
    
    // Get texture ID
    int id = static_cast<int>(m_textures.size());
    
    // Add to mapping
    m_nameToId[name] = id;
    
    // Add to storage
    m_textures.push_back(std::move(texture));
    
    return id;
}

// Create checker pattern texture
int TextureManager::createPatternTexture(int width, int height, const Color& color1, const Color& color2, int cellSize) {
    // Create name for this texture
    std::string name = "pattern_" + std::to_string(color1.r) + "_" + 
                      std::to_string(color1.g) + "_" + 
                      std::to_string(color1.b) + "_" +
                      std::to_string(color2.r) + "_" + 
                      std::to_string(color2.g) + "_" + 
                      std::to_string(color2.b) + "_" +
                      std::to_string(cellSize);
    
    // Check if texture is already created
    if (m_nameToId.find(name) != m_nameToId.end()) {
        return m_nameToId[name];
    }
    
    // Create new texture
    std::unique_ptr<Texture> texture = std::make_unique<Texture>();
    
    // Create pattern
    if (!texture->createPattern(width, height, color1, color2, cellSize, m_renderer)) {
        std::cerr << "Failed to create pattern texture" << std::endl;
        return -1;
    }
    
    // Get texture ID
    int id = static_cast<int>(m_textures.size());
    
    // Add to mapping
    m_nameToId[name] = id;
    
    // Add to storage
    m_textures.push_back(std::move(texture));
    
    return id;
}

// Create from SDL surface
int TextureManager::createFromSurface(SDL_Surface* surface) {
    if (surface == nullptr) {
        return -1;
    }
    
    // Create name for this texture (using pointer address as unique identifier)
    std::string name = "surface_" + std::to_string(reinterpret_cast<uintptr_t>(surface));
    
    // Create new texture
    std::unique_ptr<Texture> texture = std::make_unique<Texture>();
    
    // Create from surface
    if (!texture->createFromSurface(surface, m_renderer)) {
        std::cerr << "Failed to create texture from surface" << std::endl;
        return -1;
    }
    
    // Get texture ID
    int id = static_cast<int>(m_textures.size());
    
    // Add to mapping
    m_nameToId[name] = id;
    
    // Add to storage
    m_textures.push_back(std::move(texture));
    
    return id;
}

// Get texture by ID
Texture* TextureManager::getTexture(int id) {
    if (id < 0 || id >= static_cast<int>(m_textures.size())) {
        return nullptr;
    }
    
    return m_textures[id].get();
}

// Get texture by name
Texture* TextureManager::getTexture(const std::string& name) {
    // Check if texture exists
    if (m_nameToId.find(name) == m_nameToId.end()) {
        return nullptr;
    }
    
    return getTexture(m_nameToId[name]);
}

// Create DOOM-style wall texture
int TextureManager::createDoomWallTexture(const Color& baseColor, const Color& trimColor) {
    // Create name for this texture
    std::string name = "doomwall_" + std::to_string(baseColor.r) + "_" + 
                      std::to_string(baseColor.g) + "_" + 
                      std::to_string(baseColor.b) + "_" +
                      std::to_string(trimColor.r) + "_" + 
                      std::to_string(trimColor.g) + "_" + 
                      std::to_string(trimColor.b);
    
    // Check if texture is already created
    if (m_nameToId.find(name) != m_nameToId.end()) {
        return m_nameToId[name];
    }
    
    // Generate DOOM-style wall texture
    SDL_Surface* surface = generateDoomWallTexture(128, 128, baseColor, trimColor);
    if (surface == nullptr) {
        return -1;
    }
    
    // Create texture from surface
    int id = createFromSurface(surface);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    // Add to mapping
    if (id >= 0) {
        m_nameToId[name] = id;
    }
    
    return id;
}

// Create DOOM-style floor texture
int TextureManager::createDoomFloorTexture(const Color& baseColor, const Color& patternColor) {
    // Create name for this texture
    std::string name = "doomfloor_" + std::to_string(baseColor.r) + "_" + 
                      std::to_string(baseColor.g) + "_" + 
                      std::to_string(baseColor.b) + "_" +
                      std::to_string(patternColor.r) + "_" + 
                      std::to_string(patternColor.g) + "_" + 
                      std::to_string(patternColor.b);
    
    // Check if texture is already created
    if (m_nameToId.find(name) != m_nameToId.end()) {
        return m_nameToId[name];
    }
    
    // Generate DOOM-style floor texture
    SDL_Surface* surface = generateDoomFloorTexture(128, 128, baseColor, patternColor);
    if (surface == nullptr) {
        return -1;
    }
    
    // Create texture from surface
    int id = createFromSurface(surface);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    // Add to mapping
    if (id >= 0) {
        m_nameToId[name] = id;
    }
    
    return id;
}

// Create pentagram texture
int TextureManager::createPentagramTexture(bool isWall) {
    // Create name for this texture
    std::string name = isWall ? "pentagram_wall" : "pentagram_floor";
    
    // Check if texture is already created
    if (m_nameToId.find(name) != m_nameToId.end()) {
        return m_nameToId[name];
    }
    
    // Generate pentagram texture
    SDL_Surface* surface = generatePentagramTexture(128, 128, isWall);
    if (surface == nullptr) {
        return -1;
    }
    
    // Create texture from surface
    int id = createFromSurface(surface);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    // Add to mapping
    if (id >= 0) {
        m_nameToId[name] = id;
    }
    
    return id;
}

// Helper for procedural textures
SDL_Surface* TextureManager::createProcedural(int width, int height, std::function<Color(int,int)> pixelGenerator) {
    // Create surface with 32-bit RGBA
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                             0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000);
    if (surface == nullptr) {
        std::cerr << "Unable to create procedural surface: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    // Lock surface for pixel manipulation
    SDL_LockSurface(surface);
    
    // Generate pixels
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Get color for this pixel
            Color color = pixelGenerator(x, y);
            
            // Map to surface format
            uint32_t pixel = SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
            
            // Set pixel
            pixels[y * width + x] = pixel;
        }
    }
    
    // Unlock surface
    SDL_UnlockSurface(surface);
    
    return surface;
}

// Generate DOOM-style wall texture
SDL_Surface* TextureManager::generateDoomWallTexture(int width, int height, const Color& baseColor, const Color& trimColor) {
    return createProcedural(width, height, [baseColor, trimColor, width, height](int x, int y) {
        // Base color
        Color color = baseColor;
        
        // Add some noise to the base color
        int noise = (x * 7 + y * 13) % 20 - 10;
        color.r = std::min(255, std::max(0, color.r + noise));
        color.g = std::min(255, std::max(0, color.g + noise));
        color.b = std::min(255, std::max(0, color.b + noise));
        
        // Add trim at top and bottom
        if (y < height / 10 || y > height * 9 / 10) {
            color = trimColor;
            
            // Add some noise to the trim color
            int trimNoise = (x * 11 + y * 17) % 15 - 7;
            color.r = std::min(255, std::max(0, color.r + trimNoise));
            color.g = std::min(255, std::max(0, color.g + trimNoise));
            color.b = std::min(255, std::max(0, color.b + trimNoise));
        }
        
        // Add vertical lines for panel effect
        int panelWidth = width / 4;
        if ((x + panelWidth/2) % panelWidth < 3) {
            // Make the line a darker version of the current color
            color.r = std::max(0, color.r - 40);
            color.g = std::max(0, color.g - 40);
            color.b = std::max(0, color.b - 40);
        }
        
        return color;
    });
}

// Generate DOOM-style floor texture
SDL_Surface* TextureManager::generateDoomFloorTexture(int width, int height, const Color& baseColor, const Color& patternColor) {
    return createProcedural(width, height, [baseColor, patternColor, width, height](int x, int y) {
        // Base color
        Color color = baseColor;
        
        // Add some noise to the base color
        int noise = (x * 13 + y * 7) % 15 - 7;
        color.r = std::min(255, std::max(0, color.r + noise));
        color.g = std::min(255, std::max(0, color.g + noise));
        color.b = std::min(255, std::max(0, color.b + noise));
        
        // Create a grid pattern
        int cellSize = width / 8;
        if ((x % cellSize < 2) || (y % cellSize < 2)) {
            color = patternColor;
            
            // Add some noise to the pattern color
            int patternNoise = (x * 19 + y * 23) % 10 - 5;
            color.r = std::min(255, std::max(0, color.r + patternNoise));
            color.g = std::min(255, std::max(0, color.g + patternNoise));
            color.b = std::min(255, std::max(0, color.b + patternNoise));
        }
        
        return color;
    });
}

// Generate pentagram texture
SDL_Surface* TextureManager::generatePentagramTexture(int width, int height, bool isWall) {
    return createProcedural(width, height, [width, height, isWall](int x, int y) {
        // Base color (dark for floor, wall-colored for wall)
        Color color = isWall ? Color(139, 69, 19) : Color(30, 30, 30);
        
        // Add some noise to the base color
        int noise = (x * 5 + y * 9) % 10 - 5;
        color.r = std::min(255, std::max(0, color.r + noise));
        color.g = std::min(255, std::max(0, color.g + noise));
        color.b = std::min(255, std::max(0, color.b + noise));
        
        // Center coordinates
        float cx = width / 2.0f;
        float cy = height / 2.0f;
        
        // Normalized coordinates (-1 to 1)
        float nx = (x - cx) / (width / 2.0f);
        float ny = (y - cy) / (height / 2.0f);
        
        // Convert to polar coordinates
        float radius = std::sqrt(nx*nx + ny*ny);
        float angle = std::atan2(ny, nx);
        if (angle < 0) angle += 2.0f * M_PI;
        
        // Draw pentagram circle
        float circleRadius = 0.8f;
        if (std::abs(radius - circleRadius) < 0.04f) {
            return Color(220, 0, 0);
        }
        
        // Draw pentagram lines
        float points[5][2];
        for (int i = 0; i < 5; ++i) {
            float pointAngle = i * 2.0f * M_PI / 5.0f - M_PI / 2.0f;
            points[i][0] = circleRadius * std::cos(pointAngle);
            points[i][1] = circleRadius * std::sin(pointAngle);
        }
        
        // Connect points to draw the star
        for (int i = 0; i < 5; ++i) {
            int j = (i + 2) % 5;  // Connect to the point 2 positions away
            
            // Line equation: Ax + By + C = 0
            float A = points[j][1] - points[i][1];
            float B = points[i][0] - points[j][0];
            float C = points[j][0] * points[i][1] - points[i][0] * points[j][1];
            
            // Distance from point to line
            float distance = std::abs(A * nx + B * ny + C) / std::sqrt(A*A + B*B);
            
            // Draw the line if close enough
            if (distance < 0.03f) {
                // Check if the point is between the two vertices
                float dot1 = (nx - points[i][0]) * (points[j][0] - points[i][0]) + 
                           (ny - points[i][1]) * (points[j][1] - points[i][1]);
                float dot2 = (nx - points[j][0]) * (points[i][0] - points[j][0]) + 
                           (ny - points[j][1]) * (points[i][1] - points[j][1]);
                
                if (dot1 >= 0 && dot2 >= 0) {
                    return Color(220, 0, 0);
                }
            }
        }
        
        return color;
    });
} 