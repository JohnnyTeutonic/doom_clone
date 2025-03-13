#include "TextureManager.h"
#include <iostream>

// Texture constructor
Texture::Texture() :
    m_width(0),
    m_height(0),
    m_texture(nullptr)
{
    // Initialize pixel data as empty
    m_pixels.clear();
}

// Texture destructor
Texture::~Texture()
{
    // Free all resources
    free();
    
#ifdef ENABLE_CUDA
    // CUDA textures are automatically destroyed by CUDATexture destructor
#endif
}

// Free texture resources
void Texture::free()
{
    // Free texture
    if (m_texture != nullptr)
    {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    
    // Clear pixel data
    m_pixels.clear();
    
    // Reset dimensions
    m_width = 0;
    m_height = 0;
}

// Load texture from file
bool Texture::loadFromFile(const std::string& path, SDL_Renderer* renderer)
{
    // Free existing texture
    free();
    
    // Load image
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (surface == nullptr)
    {
        std::cerr << "Failed to load image " << path << ": " << IMG_GetError() << std::endl;
        return false;
    }
    
    // Create texture from surface
    bool success = createFromSurface(surface, renderer);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    return success;
}

// Create texture from SDL surface
bool Texture::createFromSurface(SDL_Surface* surface, SDL_Renderer* renderer)
{
    // Free existing texture
    free();
    
    if (surface == nullptr || renderer == nullptr)
    {
        return false;
    }
    
    // Create texture from surface pixels
    m_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (m_texture == nullptr)
    {
        std::cerr << "Unable to create texture from surface: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Get dimensions
    m_width = surface->w;
    m_height = surface->h;
    
    // Store pixel data
    m_pixels.resize(m_width * m_height);
    
    // Lock surface for pixel manipulation
    SDL_LockSurface(surface);
    
    // Copy pixel data
    memcpy(m_pixels.data(), surface->pixels, m_width * m_height * sizeof(uint32_t));
    
    // Unlock surface
    SDL_UnlockSurface(surface);
    
    return true;
}

// Create solid color texture
bool Texture::createSolid(int width, int height, const Color& color, SDL_Renderer* renderer)
{
    // Free existing texture
    free();
    
    if (width <= 0 || height <= 0 || renderer == nullptr)
    {
        return false;
    }
    
    // Create surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                              0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000);
    if (surface == nullptr)
    {
        std::cerr << "Unable to create surface: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Fill surface with color
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
    
    // Create texture from surface
    bool success = createFromSurface(surface, renderer);
    
    // Free surface
    SDL_FreeSurface(surface);
    
    return success;
}

// Create checkerboard pattern
bool Texture::createPattern(int width, int height, const Color& color1, const Color& color2, 
                           int cellSize, SDL_Renderer* renderer)
{
    // Free existing texture
    free();
    
    if (width <= 0 || height <= 0 || cellSize <= 0 || renderer == nullptr)
    {
        return false;
    }
    
    // Create surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                              0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000);
    if (surface == nullptr)
    {
        std::cerr << "Unable to create surface: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Lock surface for pixel manipulation
    SDL_LockSurface(surface);
    
    // Create checkerboard pattern
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
    uint32_t color1Value = SDL_MapRGBA(surface->format, color1.r, color1.g, color1.b, color1.a);
    uint32_t color2Value = SDL_MapRGBA(surface->format, color2.r, color2.g, color2.b, color2.a);
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int cellX = x / cellSize;
            int cellY = y / cellSize;
            bool isColor1 = (cellX + cellY) % 2 == 0;
            
            pixels[y * width + x] = isColor1 ? color1Value : color2Value;
        }
    }
    
    // Unlock surface
    SDL_UnlockSurface(surface);
    
    // Create texture from surface
    bool success = createFromSurface(surface, renderer);
    
    // Free surface
    SDL_FreeSurface(surface);
    
    return success;
}

// Get pixel color
Color Texture::getPixel(int x, int y) const
{
    // Check bounds
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || m_pixels.empty())
    {
        return Color(0, 0, 0, 0);
    }
    
    // Get pixel
    uint32_t pixel = m_pixels[y * m_width + x];
    
    // Extract components (assuming RGBA format)
    unsigned char r = (pixel >> 16) & 0xFF;
    unsigned char g = (pixel >> 8) & 0xFF;
    unsigned char b = pixel & 0xFF;
    unsigned char a = (pixel >> 24) & 0xFF;
    
    return Color(r, g, b, a);
}

// Get normalized pixel color
Color Texture::getPixelNormalized(float u, float v) const
{
    // Ensure u,v are in range [0,1]
    u = u - floor(u);
    v = v - floor(v);
    
    // Convert to pixel coordinates
    int x = static_cast<int>(u * m_width);
    int y = static_cast<int>(v * m_height);
    
    // Get pixel
    return getPixel(x, y);
}

// Lock texture for pixel manipulation
bool Texture::lockTexture()
{
    // Only lock if pixel data doesn't already exist
    if (!m_pixels.empty())
    {
        return true;
    }
    
    // Make sure texture exists
    if (m_texture == nullptr)
    {
        std::cerr << "Cannot lock null texture!" << std::endl;
        return false;
    }
    
    // Allocate pixel data
    m_pixels.resize(m_width * m_height);
    
    // Lock texture
    void* pixels;
    int pitch;
    if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) != 0)
    {
        std::cerr << "Unable to lock texture: " << SDL_GetError() << std::endl;
        m_pixels.clear();
        return false;
    }
    
    // Copy pixel data
    memcpy(m_pixels.data(), pixels, m_width * m_height * sizeof(uint32_t));
    
    // Unlock texture
    SDL_UnlockTexture(m_texture);
    
    return true;
}

// Unlock texture
void Texture::unlockTexture()
{
    // Nothing to do if there's no pixel data
    if (m_pixels.empty())
    {
        return;
    }
    
    // Update texture with current pixel data
    updateTexture();
    
    // Clear pixel data
    m_pixels.clear();
}

// Update texture with pixel data
void Texture::updateTexture()
{
    if (m_texture == nullptr || m_pixels.empty())
    {
        return;
    }
    
    // Update texture from pixels
    SDL_UpdateTexture(m_texture, nullptr, m_pixels.data(), m_width * sizeof(uint32_t));
}

// Render texture to screen
void Texture::render(SDL_Renderer* renderer, int x, int y, const SDL_Rect* clip)
{
    if (m_texture == nullptr || renderer == nullptr)
    {
        return;
    }
    
    // Set rendering area
    SDL_Rect renderQuad = { x, y, m_width, m_height };
    
    // Set clip rendering dimensions
    if (clip != nullptr)
    {
        renderQuad.w = clip->w;
        renderQuad.h = clip->h;
    }
    
    // Render to screen
    SDL_RenderCopy(renderer, m_texture, clip, &renderQuad);
}

// Render with scaling
void Texture::render(SDL_Renderer* renderer, int x, int y, int width, int height, const SDL_Rect* clip)
{
    if (m_texture == nullptr || renderer == nullptr)
    {
        return;
    }
    
    // Set rendering area
    SDL_Rect renderQuad = { x, y, width, height };
    
    // Render to screen
    SDL_RenderCopy(renderer, m_texture, clip, &renderQuad);
} 