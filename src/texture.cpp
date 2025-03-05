#include "texture.h"
#include <cstring>
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
    std::cout << "Attempting to load texture from file: " << filename << std::endl;
    
    // Check if file exists
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        std::cerr << "File not found: " << filename << std::endl;
        return false;
    }
    fclose(file);
    
    // Load image using SDL_image
    SDL_Surface* surface = IMG_Load(filename.c_str());
    if (!surface) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        std::cerr << "SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }
    std::cout << "Successfully loaded surface from " << filename << std::endl;
    std::cout << "Surface details - Width: " << surface->w << ", Height: " << surface->h 
              << ", Format: " << SDL_GetPixelFormatName(surface->format->format) << std::endl;
    
    // Convert surface to RGBA format for consistent handling
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    
    if (!rgbaSurface) {
        std::cerr << "Failed to convert surface to RGBA: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "Successfully converted surface to RGBA format" << std::endl;
    
    // Create streaming texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        rgbaSurface->w,
        rgbaSurface->h
    );
    
    if (!texture) {
        std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(rgbaSurface);
        return false;
    }
    std::cout << "Successfully created texture from surface" << std::endl;
    
    // Set blending mode to none (fully opaque)
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    
    // Get dimensions
    m_width = rgbaSurface->w;
    m_height = rgbaSurface->h;
    
    // Lock texture to update pixels
    void* pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) < 0) {
        std::cerr << "Failed to lock texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(rgbaSurface);
        return false;
    }
    
    // Copy surface pixels to texture
    std::memcpy(pixels, rgbaSurface->pixels, rgbaSurface->pitch * rgbaSurface->h);
    SDL_UnlockTexture(texture);
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Extract pixel data for raycasting
    m_pixels.resize(m_width * m_height);
    
    SDL_LockSurface(rgbaSurface);
    Uint32* surfacePixels = static_cast<Uint32*>(rgbaSurface->pixels);
    
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            int index = y * m_width + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(surfacePixels[index], rgbaSurface->format, &r, &g, &b, &a);
            m_pixels[index] = Color(r, g, b, a);
        }
    }
    
    SDL_UnlockSurface(rgbaSurface);
    SDL_FreeSurface(rgbaSurface);
    
    return true;
}

bool Texture::createSolid(int width, int height, const Color& color, SDL_Renderer* renderer) {
    m_width = width;
    m_height = height;
    
    // Create an SDL texture with streaming access
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );
    
    if (!texture) {
        std::cerr << "Failed to create solid texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set blending mode to none (fully opaque)
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    
    // Lock texture and fill with color
    void* pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) < 0) {
        std::cerr << "Failed to lock texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return false;
    }
    
    // Fill texture with solid color
    Uint32* texturePixels = static_cast<Uint32*>(pixels);
    Uint32 pixelColor = SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), 
                                   color.r, color.g, color.b, color.a);
    
    for (int i = 0; i < width * height; i++) {
        texturePixels[i] = pixelColor;
    }
    
    SDL_UnlockTexture(texture);
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Set pixel data for raycasting
    m_pixels.resize(width * height, color);
    
    return true;
}

bool Texture::createCheckerboard(int width, int height, const Color& color1, const Color& color2, int cellSize, SDL_Renderer* renderer) {
    m_width = width;
    m_height = height;
    
    // Create an SDL texture with streaming access
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );
    
    if (!texture) {
        std::cerr << "Failed to create checkerboard texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set blending mode to none (fully opaque)
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    
    // Lock texture to access pixels
    void* pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) < 0) {
        std::cerr << "Failed to lock texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return false;
    }
    
    // Create pixel format for color mapping
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    Uint32 pixel1 = SDL_MapRGBA(format, color1.r, color1.g, color1.b, color1.a);
    Uint32 pixel2 = SDL_MapRGBA(format, color2.r, color2.g, color2.b, color2.a);
    
    // Fill texture with checkerboard pattern
    Uint32* texturePixels = static_cast<Uint32*>(pixels);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            texturePixels[y * width + x] = isEven ? pixel1 : pixel2;
        }
    }
    
    SDL_FreeFormat(format);
    SDL_UnlockTexture(texture);
    
    // Store the texture
    m_sdlTexture.reset(texture);
    
    // Set pixel data for raycasting
    m_pixels.resize(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool isEven = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            m_pixels[y * width + x] = isEven ? color1 : color2;
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
    auto texture = std::make_unique<Texture>();
    if (!texture->loadFromFile(filename, m_renderer)) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        std::cerr << "Will use a procedurally generated fallback texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

int TextureManager::createSolidTexture(int width, int height, const Color& color) {
    auto texture = std::make_unique<Texture>();
    if (!texture->createSolid(width, height, color, m_renderer)) {
        std::cerr << "Failed to create solid texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

int TextureManager::createCheckerboardTexture(int width, int height, const Color& color1, const Color& color2, int cellSize) {
    auto texture = std::make_unique<Texture>();
    if (!texture->createCheckerboard(width, height, color1, color2, cellSize, m_renderer)) {
        std::cerr << "Failed to create checkerboard texture" << std::endl;
        return -1;
    }
    
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

int TextureManager::createTextureFromSurface(SDL_Surface* surface) {
    if (!surface || !m_renderer) return -1;
    
    // Create a new texture
    auto texture = std::make_unique<Texture>();
    
    // Create SDL texture from surface
    SDL_Texture* sdlTexture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (!sdlTexture) {
        std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // Set the texture blend mode to enable alpha blending
    SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_BLEND);
    
    // Get surface dimensions
    texture->m_width = surface->w;
    texture->m_height = surface->h;
    
    // Lock surface to read pixels
    SDL_LockSurface(surface);
    
    // Create pixel data
    texture->m_pixels.resize(texture->m_width * texture->m_height);
    
    Uint32* surfacePixels = static_cast<Uint32*>(surface->pixels);
    for (int i = 0; i < texture->m_width * texture->m_height; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(surfacePixels[i], surface->format, &r, &g, &b, &a);
        texture->m_pixels[i] = Color(r, g, b, a);
    }
    
    SDL_UnlockSurface(surface);
    
    // Store the SDL texture
    texture->m_sdlTexture.reset(sdlTexture);
    
    // Add texture to the manager
    m_textures.push_back(std::move(texture));
    return static_cast<int>(m_textures.size() - 1);
}

const Texture* TextureManager::getTexture(int id) const {
    if (id < 0 || id >= static_cast<int>(m_textures.size())) {
        std::cout << "TextureManager::getTexture - Invalid texture ID: " << id << ", max ID: " << (m_textures.size() - 1) << std::endl;
        return nullptr;
    }
    
    return m_textures[id].get();
}

SDL_Texture* TextureManager::getSDLTexture(int id) const {
    const Texture* texture = getTexture(id);
    if (!texture) {
        std::cout << "TextureManager::getSDLTexture - Texture not found for ID: " << id << std::endl;
        return nullptr;
    }
    
    SDL_Texture* sdlTexture = texture->getSDLTexture();
    if (!sdlTexture) {
        std::cout << "TextureManager::getSDLTexture - SDL_Texture is null for ID: " << id << std::endl;
    }
    
    return sdlTexture;
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

// Helper function to create a DOOM-style wall texture
SDL_Surface* createDoomWallTexture(int width, int height) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (!surface) return nullptr;
    
    SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;
    
    // Create a DOOM-style wall pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Base color (dark gray)
            Uint8 r = 100, g = 100, b = 100;
            
            // Add some variation based on position
            int variation = ((x ^ y) % 16) - 8;
            r = std::clamp(r + variation, 0, 255);
            g = std::clamp(g + variation, 0, 255);
            b = std::clamp(b + variation, 0, 255);
            
            // Add darker edges for a brick-like pattern
            if (x % 16 == 0 || y % 16 == 0) {
                r = r * 2 / 3;
                g = g * 2 / 3;
                b = b * 2 / 3;
            }
            
            pixels[y * width + x] = SDL_MapRGB(surface->format, r, g, b);
        }
    }
    
    SDL_UnlockSurface(surface);
    return surface;
}
