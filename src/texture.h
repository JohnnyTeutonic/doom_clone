#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <vector>
#include <memory>
#include <SDL2/SDL.h>
#include "utils.h"
#include <iostream>

// Forward declarations
class TextureManager;

// Helper function to create a DOOM-style wall texture
SDL_Surface* createDoomWallTexture(int width, int height);

// Helper function to create a DOOM-style flat texture (for floors/ceilings)
SDL_Surface* createDoomFlatTexture(int width, int height, bool isFloor);

class Texture {
private:
    int m_width;
    int m_height;
    std::vector<Color> m_pixels;
    std::unique_ptr<SDL_Texture, void(*)(SDL_Texture*)> m_sdlTexture;
    
    // Make TextureManager a friend so it can access private members
    friend class TextureManager;
    
public:
    Texture();
    ~Texture();
    
    // Delete copy constructor and assignment operator
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    // Add move constructor and assignment operator
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    
    // Load texture from file
    bool loadFromFile(const std::string& filename, SDL_Renderer* renderer);
    
    // Create a solid color texture
    bool createSolid(int width, int height, const Color& color, SDL_Renderer* renderer);
    
    // Create a checkerboard pattern texture
    bool createCheckerboard(int width, int height, const Color& color1, const Color& color2, int cellSize, SDL_Renderer* renderer);
    
    // Getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Get a pixel at specific coordinates
    Color getPixel(int x, int y) const;
    
    // Get a pixel at normalized coordinates (0.0 to 1.0)
    Color getPixelNormalized(double u, double v) const;
    
    // Get raw pixel data for CUDA processing
    const uint32_t* getPixelData() const;
    
    // Get the SDL texture
    SDL_Texture* getSDLTexture() const { return m_sdlTexture.get(); }
};

// Texture manager class to load and store textures
class TextureManager {
private:
    SDL_Renderer* m_renderer;
    std::vector<std::unique_ptr<Texture>> m_textures;
    
public:
    explicit TextureManager(SDL_Renderer* renderer);
    ~TextureManager();
    
    // Load a texture from file
    int loadTexture(const std::string& filename);
    
    // Create a solid color texture
    int createSolidTexture(int width, int height, const Color& color);
    
    // Create a checkerboard pattern texture
    int createCheckerboardTexture(int width, int height, const Color& color1, const Color& color2, int cellSize);
    
    // Create a texture from an SDL surface
    int createTextureFromSurface(SDL_Surface* surface);
    
    // Add an existing SDL texture
    int addTexture(SDL_Texture* sdlTexture) {
        if (!sdlTexture) {
            std::cout << "TextureManager::addTexture - SDL_Texture is null" << std::endl;
            return -1;
        }
        
        auto texture = std::make_unique<Texture>();
        
        // Get texture dimensions
        int width, height;
        SDL_QueryTexture(sdlTexture, nullptr, nullptr, &width, &height);
        texture->m_width = width;
        texture->m_height = height;
        
        // Store the SDL texture
        texture->m_sdlTexture.reset(sdlTexture);
        
        // Add texture to the manager
        m_textures.push_back(std::move(texture));
        int id = static_cast<int>(m_textures.size() - 1);
        std::cout << "TextureManager::addTexture - Added texture with ID: " << id << ", dimensions: " << width << "x" << height << std::endl;
        return id;
    }
    
    // Access a texture by ID
    const Texture* getTexture(int id) const;
    
    // Get the number of textures in the manager
    int getTextureCount() const { return static_cast<int>(m_textures.size()); }
    
    // Access an SDL texture directly by ID
    SDL_Texture* getSDLTexture(int id) const;
    
    // Initialize default textures
    void initDefaultTextures();
};

#endif // TEXTURE_H 