#include "texture.h"
#include <iostream>
#include <SDL2/SDL_image.h>

// Texture implementation
Texture::Texture() 
    : m_width(0)
    , m_height(0)
    , m_sdlTexture(nullptr, SDL_DestroyTexture)
{
}

Texture::~Texture() {
    // m_sdlTexture will be automatically destroyed by the unique_ptr
}

// Move constructor
Texture::Texture(Texture&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_pixels(std::move(other.m_pixels))
    , m_sdlTexture(std::move(other.m_sdlTexture))
{
    other.m_width = 0;
    other.m_height = 0;
}

// Move assignment operator
Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_pixels = std::move(other.m_pixels);
        m_sdlTexture = std::move(other.m_sdlTexture);
        
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

bool Texture::loadFromFile(const std::string& filename, SDL_Renderer* renderer) {
    // Load image using SDL_image
    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (!surface) {
        std::cerr << "Failed to load texture: " << filename << ", SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create texture from surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return false;
    }
    
    // Get dimensions
    m_width = surface->w;
    m_height = surface->h;
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Extract pixel data for raycasting
    m_pixels.resize(m_width * m_height);
    
    SDL_LockSurface(surface);
    
    Uint32* surfacePixels = static_cast<Uint32*>(surface->pixels);
    SDL_PixelFormat* format = surface->format;
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            int index = y * m_width + x;
            Uint32 pixel = surfacePixels[index];
            
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
            
            m_pixels[index] = Color(r, g, b, a);
        }
    }
    
    SDL_UnlockSurface(surface);
    SDL_FreeSurface(surface);
    
    return true;
}

bool Texture::createSolid(int width, int height, const Color& color, SDL_Renderer* renderer) {
    m_width = width;
    m_height = height;
    
    // Create an SDL texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width, height
    );
    
    if (!texture) {
        std::cerr << "Failed to create solid texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set the texture as the render target
    SDL_SetRenderTarget(renderer, texture);
    
    // Set the color and fill the texture
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
    
    // Reset render target
    SDL_SetRenderTarget(renderer, nullptr);
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Set pixel data for raycasting
    m_pixels.resize(width * height, color);
    
    return true;
}

bool Texture::createCheckerboard(int width, int height, const Color& color1, const Color& color2, int cellSize, SDL_Renderer* renderer) {
    m_width = width;
    m_height = height;
    
    // Create an SDL texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width, height
    );
    
    if (!texture) {
        std::cerr << "Failed to create checkerboard texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set the texture as the render target
    SDL_SetRenderTarget(renderer, texture);
    
    // Draw the checkerboard pattern
    for (int y = 0; y < height; y += cellSize) {
        for (int x = 0; x < width; x += cellSize) {
            SDL_Rect rect = { x, y, cellSize, cellSize };
            
            bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            const Color& color = isEven ? color1 : color2;
            
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    
    // Reset render target
    SDL_SetRenderTarget(renderer, nullptr);
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Set pixel data for raycasting
    m_pixels.resize(width * height);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = y * width + x;
            bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            m_pixels[index] = isEven ? color1 : color2;
        }
    }
    
    return true;
}

Color Texture::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return Color(); // Return black for out of bounds
    }
    
    return m_pixels[y * m_width + x];
}

Color Texture::getPixelNormalized(double u, double v) const {
    // Normalize coordinates to [0, 1]
    u = u - floor(u);
    v = v - floor(v);
    
    // Convert to pixel coordinates
    int x = static_cast<int>(u * m_width);
    int y = static_cast<int>(v * m_height);
    
    // Handle edge cases
    if (x == m_width) x = m_width - 1;
    if (y == m_height) y = m_height - 1;
    
    return getPixel(x, y);
}

// TextureManager implementation
TextureManager::TextureManager(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
}

TextureManager::~TextureManager() {
    // Textures are automatically cleaned up
}

int TextureManager::loadTexture(const std::string& filename) {
    Texture texture;
    if (!texture.loadFromFile(filename, m_renderer)) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        std::cerr << "Will use a procedurally generated fallback texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

int TextureManager::createSolidTexture(int width, int height, const Color& color) {
    Texture texture;
    if (!texture.createSolid(width, height, color, m_renderer)) {
        std::cerr << "Failed to create solid texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

int TextureManager::createCheckerboardTexture(int width, int height, const Color& color1, const Color& color2, int cellSize) {
    Texture texture;
    if (!texture.createCheckerboard(width, height, color1, color2, cellSize, m_renderer)) {
        std::cerr << "Failed to create checkerboard texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

const Texture* TextureManager::getTexture(int id) const {
    if (id < 0 || id >= static_cast<int>(m_textures.size())) {
        return nullptr;
    }
    
    return &m_textures[id];
}

void TextureManager::initDefaultTextures() {
    // Create default wall textures
    // Brick texture (red)
    createCheckerboardTexture(64, 64, Color(139, 0, 0), Color(205, 85, 85), 16);
    
    // Stone texture (gray)
    createCheckerboardTexture(64, 64, Color(100, 100, 100), Color(169, 169, 169), 16);
    
    // Blue wall texture
    createCheckerboardTexture(64, 64, Color(0, 0, 139), Color(65, 105, 225), 16);
    
    // Green wall texture
    createCheckerboardTexture(64, 64, Color(0, 100, 0), Color(34, 139, 34), 16);
} 