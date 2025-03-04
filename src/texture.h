#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <vector>
#include <memory>
#include <SDL2/SDL.h>
#include "utils.h"

// Forward declarations
class TextureManager;

// Helper function to create a DOOM-style wall texture
SDL_Surface* createDoomWallTexture(int width, int height);

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
    
    // Get a texture by ID
    const Texture* getTexture(int id) const;
    
    // Initialize default textures
    void initDefaultTextures();
};

#endif // TEXTURE_H 