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
    , m_impMovementType(ImpMovementType::Zigzag)  // Default movement type
    , m_specialMoveTimer(0.0)
    , m_specialMoveCooldown(3.0)  // 3 seconds between special moves
    , m_lastPlayerPos(0, 0)
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

    // Update behavior based on sprite type
    if (m_type == SpriteType::Enemy) {
        updateEnemyBehavior(deltaTime, map, playerPos);
    } else if (m_type == SpriteType::ImpEnemy) {
        updateImpBehavior(deltaTime, map, playerPos);
    }
}

void Sprite::updateEnemyBehavior(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Update movement timer
    m_moveTimer += deltaTime;
    
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Different behavior states based on distance to player
    enum class EnemyState { Idle, Patrol, Chase, Attack, Maintain };
    
    // Define distance thresholds
    const double ATTACK_DISTANCE = 1.5;
    const double MINIMUM_DISTANCE = 1.2; // Minimum distance to maintain from player
    const double CHASE_DISTANCE = 8.0;
    
    // Determine the current state
    EnemyState state;
    if (distToPlayer < MINIMUM_DISTANCE) {
        state = EnemyState::Maintain;  // Too close - back up
    } else if (distToPlayer < ATTACK_DISTANCE) {
        state = EnemyState::Attack;    // Close enough to attack but not too close
    } else if (distToPlayer < CHASE_DISTANCE) {
        state = EnemyState::Chase;     // Within range - chase
    } else if (m_moveTimer < m_moveDuration) {
        state = EnemyState::Patrol;    // Normal patrolling
    } else {
        state = EnemyState::Idle;      // Idle, about to change direction
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
                    m_animationTimer += deltaTime;
                    if (m_animationTimer >= 1.0 / m_animationSpeed) {
                        m_currentFrame = (m_currentFrame + 1) % 2;
                        m_animationTimer = 0.0;
                    }
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
                    m_animationTimer += deltaTime;
                    if (m_animationTimer >= 1.0 / m_animationSpeed) {
                        m_currentFrame = 1 + ((m_currentFrame - 1 + 1) % 2);
                        m_animationTimer = 0.0;
                    }
                }
            }
            break;
            
        case EnemyState::Attack:
            // Attack the player but maintain minimum distance
            // In a real game, this would deal damage to the player
            m_direction = toPlayer.normalized();
            
            // Use the fourth animation frame (attack) if available
            if (m_isAnimated && m_frameCount > 3) {
                m_currentFrame = 3;
            }
            break;
            
        case EnemyState::Maintain:
            // Back away from player to maintain minimum distance
            {
                // Direction is away from player
                m_direction = (m_position - playerPos).normalized();
                
                // Move away from player
                Vec2 newPos = m_position + m_direction * m_moveSpeed * 1.2 * deltaTime;
                
                // Check if we can move there and it's not too far from player
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If we can't back up directly, try moving laterally
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
                
                // Use attack animation frame
                if (m_isAnimated && m_frameCount > 3) {
                    m_currentFrame = 3;
                }
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
    // Add a small buffer around the sprite to prevent getting too close to walls
    const double buffer = 0.2; // Buffer distance from walls
    
    // Check the center and four points around the sprite (like a plus sign)
    if (!map.isValidPosition(newPos.x, newPos.y)) return false;
    
    // Check points at the edge of the sprite's collision radius
    double radius = m_size * 0.4; // Use 40% of sprite size as collision radius
    
    if (!map.isValidPosition(newPos.x + radius, newPos.y)) return false;
    if (!map.isValidPosition(newPos.x - radius, newPos.y)) return false;
    if (!map.isValidPosition(newPos.x, newPos.y + radius)) return false;
    if (!map.isValidPosition(newPos.x, newPos.y - radius)) return false;
    
    return true;
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

void Sprite::updateImpBehavior(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Update movement timer
    m_moveTimer += deltaTime;
    m_specialMoveTimer += deltaTime;
    
    // Store the player's position for tracking
    m_lastPlayerPos = playerPos;
    
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Different behavior based on the Imp's movement type
    switch (m_impMovementType) {
        case ImpMovementType::Zigzag:
            moveZigzag(deltaTime, map, playerPos);
            break;
            
        case ImpMovementType::Teleport:
            moveTeleport(deltaTime, map, playerPos);
            break;
            
        case ImpMovementType::Charge:
            moveCharge(deltaTime, map, playerPos);
            break;
    }
    
    // Attack behavior when close to player
    const double ATTACK_DISTANCE = 1.5;
    if (distToPlayer < ATTACK_DISTANCE) {
        // Face the player
        m_direction = toPlayer.normalized();
        
        // Use the attack animation frame if available
        if (m_isAnimated && m_frameCount > 3) {
            m_currentFrame = 3;
        }
    }
}

void Sprite::moveZigzag(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Define distance thresholds
    const double CHASE_DISTANCE = 10.0;
    
    if (distToPlayer < CHASE_DISTANCE) {
        // In chase range - use zigzag pattern
        
        // Base direction is towards player
        Vec2 baseDirection = toPlayer.normalized();
        
        // Add a perpendicular component that oscillates
        Vec2 perpDirection(-baseDirection.y, baseDirection.x);
        
        // Oscillate between -1 and 1 with period of about 2 seconds
        double oscillation = sin(m_moveTimer * 3.0);
        
        // Combine the base direction with the perpendicular component
        Vec2 zigzagDir = baseDirection + perpDirection * oscillation * 0.7;
        zigzagDir = zigzagDir.normalized();
        
        // Set the direction
        m_direction = zigzagDir;
        
        // Move in the zigzag direction
        Vec2 newPos = m_position + m_direction * m_moveSpeed * 1.2 * deltaTime;
        if (canMoveTo(newPos, map)) {
            m_position = newPos;
        } else {
            // If blocked, try moving directly towards player
            newPos = m_position + baseDirection * m_moveSpeed * deltaTime;
            if (canMoveTo(newPos, map)) {
                m_position = newPos;
            }
        }
        
        // Use animation frames 0-2 for movement
        if (m_isAnimated && m_frameCount > 2) {
            m_currentFrame = (m_currentFrame % 3);
        }
    } else {
        // Outside chase range - patrol normally
        if (m_moveTimer >= m_moveDuration) {
            changeDirection(map);
            m_moveTimer = 0.0;
        }
        
        // Move in current direction
        Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.7 * deltaTime;
        if (canMoveTo(newPos, map)) {
            m_position = newPos;
        } else {
            changeDirection(map);
        }
        
        // Use first animation frame for patrolling
        if (m_isAnimated && m_frameCount > 0) {
            m_currentFrame = 0;
        }
    }
}

void Sprite::moveTeleport(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Define distance thresholds
    const double CHASE_DISTANCE = 12.0;
    const double TELEPORT_DISTANCE = 8.0;
    const double MIN_TELEPORT_DISTANCE = 2.0;
    
    if (distToPlayer < CHASE_DISTANCE) {
        // In chase range
        
        // Check if it's time to teleport
        if (distToPlayer < TELEPORT_DISTANCE && 
            distToPlayer > MIN_TELEPORT_DISTANCE && 
            m_specialMoveTimer >= m_specialMoveCooldown) {
            
            // Try to teleport closer to player
            const int MAX_ATTEMPTS = 10;
            bool teleported = false;
            
            for (int i = 0; i < MAX_ATTEMPTS; i++) {
                // Calculate a random position around the player
                double angle = (rand() % 628) / 100.0; // Random angle 0-2π
                double distance = MIN_TELEPORT_DISTANCE + (rand() % 100) / 100.0 * 2.0; // 2-4 units
                
                Vec2 offset(cos(angle) * distance, sin(angle) * distance);
                Vec2 teleportPos = playerPos + offset;
                
                // Check if we can teleport there
                if (canMoveTo(teleportPos, map)) {
                    // Teleport!
                    m_position = teleportPos;
                    teleported = true;
                    
                    // Face the player after teleporting
                    m_direction = (playerPos - m_position).normalized();
                    
                    // Reset the special move timer
                    m_specialMoveTimer = 0.0;
                    
                    // Use the attack animation frame for teleport
                    if (m_isAnimated && m_frameCount > 3) {
                        m_currentFrame = 3;
                    }
                    
                    break;
                }
            }
            
            // If teleport failed, just move normally
            if (!teleported) {
                // Move towards player
                m_direction = toPlayer.normalized();
                Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                }
            }
        } else {
            // Not teleporting, move towards player
            m_direction = toPlayer.normalized();
            Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
            if (canMoveTo(newPos, map)) {
                m_position = newPos;
            } else {
                // If blocked, try to find a path around obstacles
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
            
            // Use animation frames 0-2 for movement
            if (m_isAnimated && m_frameCount > 2) {
                m_currentFrame = (m_currentFrame % 3);
            }
        }
    } else {
        // Outside chase range - patrol normally
        if (m_moveTimer >= m_moveDuration) {
            changeDirection(map);
            m_moveTimer = 0.0;
        }
        
        // Move in current direction
        Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.7 * deltaTime;
        if (canMoveTo(newPos, map)) {
            m_position = newPos;
        } else {
            changeDirection(map);
        }
        
        // Use first animation frame for patrolling
        if (m_isAnimated && m_frameCount > 0) {
            m_currentFrame = 0;
        }
    }
}

void Sprite::moveCharge(double deltaTime, const Map& map, const Vec2& playerPos) {
    // Calculate distance to player
    Vec2 toPlayer = playerPos - m_position;
    double distToPlayer = toPlayer.length();
    
    // Define distance thresholds
    const double CHASE_DISTANCE = 10.0;
    const double CHARGE_DISTANCE = 6.0;
    const double CHARGE_DURATION = 1.0; // seconds
    
    if (distToPlayer < CHASE_DISTANCE) {
        // In chase range
        
        // Check if it's time to charge
        if (distToPlayer < CHARGE_DISTANCE && m_specialMoveTimer >= m_specialMoveCooldown) {
            // Start a charge attack
            m_direction = toPlayer.normalized();
            
            // Move at 3x speed during charge
            Vec2 newPos = m_position + m_direction * m_moveSpeed * 3.0 * deltaTime;
            if (canMoveTo(newPos, map)) {
                m_position = newPos;
            }
            
            // Use the attack animation frame for charging
            if (m_isAnimated && m_frameCount > 3) {
                m_currentFrame = 3;
            }
            
            // Decrement the charge timer
            m_specialMoveCooldown -= deltaTime;
            
            // If charge is complete, reset the timer
            if (m_specialMoveCooldown <= 0.0) {
                m_specialMoveTimer = 0.0;
                m_specialMoveCooldown = 5.0; // Longer cooldown after a charge
            }
        } else {
            // Not charging, move towards player at normal speed
            m_direction = toPlayer.normalized();
            Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
            if (canMoveTo(newPos, map)) {
                m_position = newPos;
            } else {
                // If blocked, try to find a path around obstacles
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
            
            // Use animation frames 0-2 for movement
            if (m_isAnimated && m_frameCount > 2) {
                m_currentFrame = (m_currentFrame % 3);
            }
        }
    } else {
        // Outside chase range - patrol normally
        if (m_moveTimer >= m_moveDuration) {
            changeDirection(map);
            m_moveTimer = 0.0;
        }
        
        // Move in current direction
        Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.7 * deltaTime;
        if (canMoveTo(newPos, map)) {
            m_position = newPos;
        } else {
            changeDirection(map);
        }
        
        // Use first animation frame for patrolling
        if (m_isAnimated && m_frameCount > 0) {
            m_currentFrame = 0;
        }
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