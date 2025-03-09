#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Texture class for loaded textures
class Texture {
public:
    Texture();
    ~Texture();
    
    // Load from file
    bool loadFromFile(const std::string& path, SDL_Renderer* renderer);
    
    // Create from SDL surface
    bool createFromSurface(SDL_Surface* surface, SDL_Renderer* renderer);
    
    // Create solid color texture
    bool createSolid(int width, int height, const Color& color, SDL_Renderer* renderer);
    
    // Create checkerboard pattern
    bool createPattern(int width, int height, const Color& color1, const Color& color2, 
                      int cellSize, SDL_Renderer* renderer);
    
    // Access dimensions
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Get pixel color (for software rendering)
    Color getPixel(int x, int y) const;
    
    // Get normalized pixel color (u,v in range [0,1])
    Color getPixelNormalized(float u, float v) const;
    
    // Get raw pixel data
    const uint32_t* getPixelData() const { return m_pixels.data(); }
    
    // Render texture to screen
    void render(SDL_Renderer* renderer, int x, int y, const SDL_Rect* clip = nullptr);
    
    // Render with scaling
    void render(SDL_Renderer* renderer, int x, int y, int width, int height, const SDL_Rect* clip = nullptr);
    
    // Get SDL texture
    SDL_Texture* getSDLTexture() const { return m_texture; }
    
private:
    // Texture dimensions
    int m_width;
    int m_height;
    
    // SDL texture
    SDL_Texture* m_texture;
    
    // Pixel data for CPU access
    std::vector<uint32_t> m_pixels;
    
    // Free resources
    void free();
    
    // Lock texture for pixel manipulation
    bool lockTexture();
    
    // Unlock texture
    void unlockTexture();
    
    // Update texture with pixel data
    void updateTexture();
};

// Texture manager for loading and caching textures
class TextureManager {
public:
    TextureManager(SDL_Renderer* renderer);
    ~TextureManager();
    
    // Load texture from file
    int loadTexture(const std::string& path);
    
    // Create solid color texture
    int createSolidTexture(int width, int height, const Color& color);
    
    // Create checker pattern texture
    int createPatternTexture(int width, int height, const Color& color1, const Color& color2, int cellSize);
    
    // Create from SDL surface
    int createFromSurface(SDL_Surface* surface);
    
    // Get texture by ID
    Texture* getTexture(int id);
    
    // Get texture by name
    Texture* getTexture(const std::string& name);
    
    // Get texture count
    int getTextureCount() const { return (int)m_textures.size(); }
    
    // Load default textures
    void loadDefaultTextures();
    
    // Create DOOM-style textures
    int createDoomWallTexture(const Color& baseColor, const Color& trimColor);
    int createDoomFloorTexture(const Color& baseColor, const Color& patternColor);
    
    // Create pentagram texture
    int createPentagramTexture(bool isWall);
    
private:
    // SDL renderer reference
    SDL_Renderer* m_renderer;
    
    // Texture storage
    std::vector<std::unique_ptr<Texture>> m_textures;
    
    // Name to ID mapping
    std::unordered_map<std::string, int> m_nameToId;
    
    // Helpers for procedural textures
    SDL_Surface* createProcedural(int width, int height, 
                               std::function<Color(int,int)> pixelGenerator);
                               
    // Generate DOOM-style wall texture
    SDL_Surface* generateDoomWallTexture(int width, int height, 
                                      const Color& baseColor, 
                                      const Color& trimColor);
                                      
    // Generate DOOM-style floor texture
    SDL_Surface* generateDoomFloorTexture(int width, int height,
                                       const Color& baseColor,
                                       const Color& patternColor);
                                       
    // Generate pentagram texture
    SDL_Surface* generatePentagramTexture(int width, int height, bool isWall);
};

#endif // TEXTURE_MANAGER_H 