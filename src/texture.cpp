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
    // Check if texture is loaded
    if (m_width == 0 || m_height == 0) {
        return Color(0, 0, 0);
    }
    
    // Convert normalized coordinates to pixel coordinates
    int x = static_cast<int>(u * m_width) % m_width;
    int y = static_cast<int>(v * m_height) % m_height;
    
    // Handle negative coordinates
    if (x < 0) x += m_width;
    if (y < 0) y += m_height;
    
    return getPixel(x, y);
}

// Implementation of getPixelData
const uint32_t* Texture::getPixelData() const {
    // Create a static buffer to hold the converted data
    // This is not thread-safe but works for our purposes
    static std::vector<uint32_t> buffer;
    
    // Resize the buffer if necessary
    buffer.resize(m_width * m_height);
    
    // Convert Color pixels to uint32_t ARGB format
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            Color color = getPixel(x, y);
            buffer[y * m_width + x] = (0xFF << 24) | (color.r << 16) | (color.g << 8) | color.b;
        }
    }
    
    return buffer.data();
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
    
    // Create DOOM-style wall texture
    SDL_Surface* doomWallSurface = createDoomWallTexture(64, 64);
    if (doomWallSurface) {
        createTextureFromSurface(doomWallSurface);
        SDL_FreeSurface(doomWallSurface);
    }
    
    // Create DOOM-style floor textures - multiple variations
    
    // Floor texture 1 - FLOOR7_2 style (tan with octagon pattern)
    SDL_Surface* doomFloorSurface1 = createDoomFlatTexture(64, 64, true);
    if (doomFloorSurface1) {
        createTextureFromSurface(doomFloorSurface1);
        SDL_FreeSurface(doomFloorSurface1);
    }
    
    // Floor texture 2 - Green marble style (FLOOR4_8 inspired)
    SDL_Surface* floorSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (floorSurface) {
        SDL_LockSurface(floorSurface);
        Uint32* pixels = (Uint32*)floorSurface->pixels;
        
        // Create FLOOR1_6 style texture (gray stone)
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 15) - 7;
                
                // Add crack patterns
                int px = x % 32;
                int py = y % 32;
                bool isCrack = ((px + py) % 13 == 0) || ((px * py) % 29 == 0);
                
                if (isCrack) {
                    // Darker crack color
                    pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, 
                        50 + noise, 50 + noise, 50 + noise);
                } else {
                    // Base stone gray color
                    int variation = ((x + y) % 7) * 3;
                    pixels[y * 64 + x] = SDL_MapRGB(floorSurface->format, 
                        90 + variation + noise, 90 + variation + noise, 90 + variation + noise);
                }
            }
        }
        SDL_UnlockSurface(floorSurface);
        createTextureFromSurface(floorSurface);
        SDL_FreeSurface(floorSurface);
    }
    
    // Create DOOM-style ceiling textures
    
    // Ceiling texture 1 - CEIL3_5 style (blue grid pattern)
    SDL_Surface* doomCeilingSurface1 = createDoomFlatTexture(64, 64, false);
    if (doomCeilingSurface1) {
        createTextureFromSurface(doomCeilingSurface1);
        SDL_FreeSurface(doomCeilingSurface1);
    }
    
    // Ceiling texture 2 - FLAT23 style (metal panels)
    SDL_Surface* ceilingSurface = SDL_CreateRGBSurface(0, 64, 64, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (ceilingSurface) {
        SDL_LockSurface(ceilingSurface);
        Uint32* pixels = (Uint32*)ceilingSurface->pixels;
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int noise = (rand() % 8) - 4;
                
                // Create a metal panel pattern
                int panelX = x % 16;
                int panelY = y % 16;
                
                // Panel edges
                bool isPanelEdge = (panelX == 0 || panelX == 15 || panelY == 0 || panelY == 15);
                
                // Metal bolts in corners
                bool isBolt = ((panelX <= 2 || panelX >= 13) && (panelY <= 2 || panelY >= 13));
                
                // Panel surface features
                bool isFeature = ((panelX + panelY) % 7 == 0);
                
                if (isPanelEdge) {
                    // Dark panel edge
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        60 + noise, 60 + noise, 65 + noise);
                } else if (isBolt) {
                    // Metal bolts
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        70 + noise, 70 + noise, 75 + noise);
                } else if (isFeature) {
                    // Surface feature (scratch or dent)
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        90 + noise, 90 + noise, 95 + noise);
                } else {
                    // Base metal panel color
                    pixels[y * 64 + x] = SDL_MapRGB(ceilingSurface->format, 
                        80 + noise, 80 + noise, 85 + noise);
                }
            }
        }
        SDL_UnlockSurface(ceilingSurface);
        createTextureFromSurface(ceilingSurface);
        SDL_FreeSurface(ceilingSurface);
    }
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

// Helper function to create a DOOM-style flat texture (for floors/ceilings)
SDL_Surface* createDoomFlatTexture(int width, int height, bool isFloor) {
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (!surface) {
        std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;
    
    // Seed for consistent noise pattern
    srand(isFloor ? 42 : 137);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int noise = (rand() % 10) - 5;
            
            if (isFloor) {
                // FLOOR4_8 inspired texture (green marble-like pattern)
                int patternX = x % 32;
                int patternY = y % 32;
                
                // Create marble-like swirls
                int swirl = ((patternX * patternX + patternY * patternY) % 32) / 2;
                
                // Determine if this is a vein in the marble
                bool isVein = (((patternX + patternY + swirl) % 16) < 3) || 
                             (((patternX * patternY + swirl) % 24) < 2);
                
                // Color the pixel based on the pattern
                if (isVein) {
                    // Dark green veins
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        40 + noise, 60 + noise, 40 + noise);
                } else {
                    // Base marble color (greenish)
                    int variation = swirl % 4 * 5;
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        70 + variation + noise, 90 + variation + noise, 70 + variation + noise);
                }
            } else {
                // CEIL1_2 inspired texture (brown with lines)
                int patternX = x % 64;
                int patternY = y % 64;
                
                // Create grid pattern with thin lines
                bool isLine = ((patternX % 16 == 0) || (patternY % 16 == 0));
                bool isSecondaryLine = ((patternX % 8 == 0 && patternX % 16 != 0) || 
                                       (patternY % 8 == 0 && patternY % 16 != 0));
                
                // Small details in a 4x4 grid
                int detailX = (patternX / 4) % 4;
                int detailY = (patternY / 4) % 4;
                bool isDetail = ((detailX == 0 || detailX == 3) && (detailY == 0 || detailY == 3));
                
                // Color the pixel based on the pattern
                if (isLine) {
                    // Darker brown lines
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        90 + noise, 70 + noise, 50 + noise);
                } else if (isSecondaryLine) {
                    // Slightly darker than base for secondary lines
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        110 + noise, 90 + noise, 70 + noise);
                } else if (isDetail) {
                    // Small details
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        130 + noise, 110 + noise, 90 + noise);
                } else {
                    // Base brown color
                    pixels[y * width + x] = SDL_MapRGB(surface->format, 
                        120 + noise, 100 + noise, 80 + noise);
                }
            }
        }
    }
    
    SDL_UnlockSurface(surface);
    return surface;
}
