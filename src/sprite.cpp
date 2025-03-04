#include "sprite.h"
#include "texture.h"
#include <algorithm>

// Sprite implementation
Sprite::Sprite(double x, double y, double size, int textureId, SpriteType type)
    : m_position(x, y)
    , m_size(size)
    , m_textureId(textureId)
    , m_type(type)
    , m_isVisible(true)
    , m_isActive(true)
    , m_isAnimated(false)
    , m_frameCount(1)
    , m_currentFrame(0)
    , m_animationSpeed(1.0)
    , m_animationTimer(0.0)
{
}

void Sprite::update(double deltaTime) {
    // Update animation if sprite is animated
    if (m_isAnimated && m_frameCount > 1) {
        m_animationTimer += deltaTime * m_animationSpeed;
        
        // Advance frame if timer exceeds 1.0
        if (m_animationTimer >= 1.0) {
            m_currentFrame = (m_currentFrame + 1) % m_frameCount;
            m_animationTimer -= 1.0;
        }
    }
}

void Sprite::setAnimated(bool animated, int frameCount, double animationSpeed) {
    m_isAnimated = animated;
    m_frameCount = frameCount;
    m_animationSpeed = animationSpeed;
    m_currentFrame = 0;
    m_animationTimer = 0.0;
}

// SpriteManager implementation
SpriteManager::SpriteManager(const TextureManager* textureManager)
    : m_textureManager(textureManager)
{
}

int SpriteManager::addSprite(double x, double y, double size, int textureId, SpriteType type) {
    // Check if texture exists
    if (!m_textureManager->getTexture(textureId)) {
        return -1; // Invalid texture ID
    }
    
    m_sprites.emplace_back(x, y, size, textureId, type);
    return static_cast<int>(m_sprites.size() - 1);
}

void SpriteManager::removeSprite(int id) {
    if (id < 0 || id >= static_cast<int>(m_sprites.size())) {
        return; // Invalid ID
    }
    
    // Mark as inactive instead of actually removing
    // This is more efficient than removing from the vector
    m_sprites[id].setActive(false);
    m_sprites[id].setVisible(false);
}

void SpriteManager::update(double deltaTime) {
    for (auto& sprite : m_sprites) {
        if (sprite.isActive()) {
            sprite.update(deltaTime);
        }
    }
}

Sprite* SpriteManager::getSprite(int id) {
    if (id < 0 || id >= static_cast<int>(m_sprites.size())) {
        return nullptr; // Invalid ID
    }
    
    return &m_sprites[id];
}

std::vector<Sprite*> SpriteManager::getActiveSprites() {
    std::vector<Sprite*> activeSprites;
    
    for (auto& sprite : m_sprites) {
        if (sprite.isActive() && sprite.isVisible()) {
            activeSprites.push_back(&sprite);
        }
    }
    
    return activeSprites;
}

void SpriteManager::sortSpritesByDistance(const Vec2& playerPos) {
    // Use a lambda to sort sprites by distance from player
    std::sort(m_sprites.begin(), m_sprites.end(), 
        [&playerPos](const Sprite& a, const Sprite& b) {
            // Only consider active and visible sprites
            if (!a.isActive() || !a.isVisible()) return false;
            if (!b.isActive() || !b.isVisible()) return true;
            
            // Calculate squared distances
            double distA = (a.getPosition() - playerPos).lengthSquared();
            double distB = (b.getPosition() - playerPos).lengthSquared();
            
            // Sort in descending order (furthest first)
            return distA > distB;
        }
    );
} 