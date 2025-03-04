#include "sprite.h"
#include "texture.h"
#include <algorithm>
#include <iostream>

// Sprite implementation
Sprite::Sprite(double x, double y, double size, int textureId, SpriteType type)
    : m_position(x, y)
    , m_direction(1, 0)  // Start facing right
    , m_size(size)
    , m_textureId(textureId)
    , m_type(type)
    , m_isVisible(true)
    , m_isActive(true)
    , m_health(100.0)    // Default health
    , m_maxHealth(100.0)
    , m_isDying(false)
    , m_deathTimer(0.0)
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

    // Handle death animation if dying
    if (m_isDying) {
        updateDeathAnimation(deltaTime);
        return;
    }

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
    
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Different behavior states based on distance to player
    enum class EnemyState { Idle, Patrol, Chase, Attack };
    
    // Determine the current state
    EnemyState state;
    if (distToPlayer < 1.5) {
        state = EnemyState::Attack;  // Very close - attack
    } else if (distToPlayer < 8.0) {
        state = EnemyState::Chase;   // Within range - chase
    } else if (m_moveTimer < m_moveDuration) {
        state = EnemyState::Patrol;  // Normal patrolling
    } else {
        state = EnemyState::Idle;    // Idle, about to change direction
    }
    
    // Handle behavior based on state
    switch (state) {
        case EnemyState::Idle:
            // Just stand still briefly, then switch to patrol
            if (m_moveTimer >= m_moveDuration + 1.0) {
                changeDirection(map);
                m_moveTimer = 0.0;
            }
            
            // Use first animation frame for idle
            if (m_isAnimated && m_frameCount > 0) {
                m_currentFrame = 0;
            }
            break;
            
        case EnemyState::Patrol:
            // Move in current direction at normal speed
            {
                Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.5 * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If blocked, change direction
                    changeDirection(map);
                }
                
                // Use first and second animation frames for patrol
                if (m_isAnimated && m_frameCount > 1) {
                    m_currentFrame = (m_currentFrame < 2) ? m_currentFrame : 0;
                }
            }
            break;
            
        case EnemyState::Chase:
            // Move towards player at increased speed
            {
                // Update direction to face player
                m_direction = toPlayer.normalized();
                
                // Move towards player
                Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If blocked, try to find a path around obstacles
                    // Try moving laterally
                    Vec2 lateralDir(-m_direction.y, m_direction.x);
                    Vec2 lateralPos = m_position + lateralDir * m_moveSpeed * deltaTime;
                    
                    if (canMoveTo(lateralPos, map)) {
                        m_position = lateralPos;
                    } else {
                        // Try the other lateral direction
                        lateralDir = Vec2(m_direction.y, -m_direction.x);
                        lateralPos = m_position + lateralDir * m_moveSpeed * deltaTime;
                        
                        if (canMoveTo(lateralPos, map)) {
                            m_position = lateralPos;
                        }
                    }
                }
                
                // Use second and third animation frames for chase
                if (m_isAnimated && m_frameCount > 2) {
                    m_currentFrame = 1 + (m_currentFrame % 2);
                }
            }
            break;
            
        case EnemyState::Attack:
            // Attack the player
            // In a real game, this would deal damage to the player
            // For now, just face the player and use the attack animation
            m_direction = toPlayer.normalized();
            
            // Use the fourth animation frame (attack) if available
            if (m_isAnimated && m_frameCount > 3) {
                m_currentFrame = 3;
            }
            break;
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

void Sprite::takeDamage(double damage) {
    if (m_isDying || !m_isActive) return;
    
    m_health -= damage;
    std::cout << "Sprite took " << damage << " damage. Health: " << m_health << "/" << m_maxHealth << std::endl;
    
    if (m_health <= 0) {
        m_health = 0;
        m_isDying = true;
        m_deathTimer = 1.0; // 1 second death animation
        std::cout << "Sprite is dying!" << std::endl;
    }
}

void Sprite::updateDeathAnimation(double deltaTime) {
    if (!m_isDying) return;
    
    m_deathTimer -= deltaTime;
    
    // Fade out by adjusting size
    m_size = m_size * (m_deathTimer);
    
    if (m_deathTimer <= 0) {
        m_isActive = false;
        m_isVisible = false;
        std::cout << "Sprite death animation complete" << std::endl;
    }
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