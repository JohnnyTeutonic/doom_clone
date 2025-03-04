#include "sprite.h"
#include "texture.h"
#include <algorithm>

// Sprite implementation
Sprite::Sprite(double x, double y, double size, int textureId, SpriteType type)
    : m_position(x, y)
    , m_direction(1, 0)  // Start facing right
    , m_size(size)
    , m_textureId(textureId)
    , m_type(type)
    , m_isVisible(true)
    , m_isActive(true)
    , m_moveSpeed(2.0)   // Units per second
    , m_turnSpeed(2.0)   // Radians per second
    , m_moveTimer(0.0)
    , m_moveDuration(2.0)  // Change direction every 2 seconds
    , m_isAnimated(false)
    , m_frameCount(1)
    , m_currentFrame(0)
    , m_animationSpeed(1.0)
    , m_animationTimer(0.0)
{
}

void Sprite::update(double deltaTime, const Map& map, const Vec2& playerPos) {
    if (!m_isActive) return;

    // Update animation if sprite is animated
    if (m_isAnimated && m_frameCount > 1) {
        m_animationTimer += deltaTime * m_animationSpeed;
        
        if (m_animationTimer >= 1.0) {
            m_currentFrame = (m_currentFrame + 1) % m_frameCount;
            m_animationTimer -= 1.0;
        }
    }

    // Update enemy behavior if this is an enemy sprite
    if (m_type == SpriteType::Enemy) {
        updateEnemyBehavior(deltaTime, map, playerPos);
    }
}

void Sprite::updateEnemyBehavior(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Update movement timer
    m_moveTimer += deltaTime;
    
    // Change direction periodically or when blocked
    if (m_moveTimer >= m_moveDuration) {
        changeDirection(map);
        m_moveTimer = 0.0;
    }

    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();

    // If player is within range (8 units), move towards them
    if (distToPlayer < 8.0) {
        m_direction = toPlayer.normalized();
    }

    // Try to move in current direction
    Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
    
    // Check if we can move there
    if (canMoveTo(newPos, map)) {
        m_position = newPos;
    } else {
        // If blocked, try to change direction
        changeDirection(map);
    }
}

void Sprite::changeDirection(const Map& map) {
    // Try several random directions until we find one we can move in
    for (int i = 0; i < 8; i++) {
        // Generate random angle between 0 and 2π
        double angle = (rand() % 628) / 100.0;  // 0 to 2π in radians
        Vec2 newDir(cos(angle), sin(angle));
        
        // Test if we can move in this direction
        Vec2 testPos = m_position + newDir * m_moveSpeed * 0.5;  // Test half a second ahead
        if (canMoveTo(testPos, map)) {
            m_direction = newDir;
            return;
        }
    }
    
    // If we couldn't find a valid direction, just stop
    m_direction = Vec2(0, 0);
}

bool Sprite::canMoveTo(const Vec2& newPos, const Map& map) const {
    // Check if the new position is valid (not in a wall)
    return map.isValidPosition(newPos.x, newPos.y);
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

void SpriteManager::update(double deltaTime, const Map& map, const Vec2& playerPos) {
    for (auto& sprite : m_sprites) {
        if (sprite.isActive()) {
            sprite.update(deltaTime, map, playerPos);
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