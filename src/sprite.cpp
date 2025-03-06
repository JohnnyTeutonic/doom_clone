#include "sprite.h"
#include "texture.h"
#include "player.h"
#include <algorithm>
#include <iostream>

// Initialize static instance
SpriteManager* SpriteManager::s_instance = nullptr;

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
    , m_itemType(ItemType::None)  // Default item type
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
    const double ATTACK_DISTANCE = 3.0;
    const double MINIMUM_DISTANCE = 2.5; // Already increased from 1.2 to 2.5
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
            // Move towards player at increased speed, but stop at attack distance
            {
                // Update direction to face player
                m_direction = toPlayer.normalized();
                
                // Only move closer if we're outside the attack distance
                if (distToPlayer > ATTACK_DISTANCE) {
                    // Calculate how far to move this frame
                    double moveDistance = std::min(m_moveSpeed * deltaTime, distToPlayer - ATTACK_DISTANCE);
                    
                    // Move towards player
                    Vec2 newPos = m_position + m_direction * moveDistance;
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
            // Attack the player while maintaining optimal attack distance
            {
                // Face the player
                m_direction = toPlayer.normalized();
                
                // Calculate optimal attack distance
                double optimalDistance = (ATTACK_DISTANCE + MINIMUM_DISTANCE) / 2.0;
                double distanceDiff = distToPlayer - optimalDistance;
                
                // If we're not at the optimal distance, adjust position
                if (std::abs(distanceDiff) > 0.3) { // Add a small tolerance
                    // Move towards or away from player to maintain optimal distance
                    Vec2 moveDir = distanceDiff > 0 ? m_direction : -m_direction;
                    double moveSpeed = std::min(std::abs(distanceDiff), m_moveSpeed * deltaTime);
                    
                    Vec2 newPos = m_position + moveDir * moveSpeed;
                    if (canMoveTo(newPos, map)) {
                        m_position = newPos;
                    } else {
                        // If blocked, try strafing sideways
                        Vec2 strafeDir(-m_direction.y, m_direction.x);
                        Vec2 strafePos = m_position + strafeDir * m_moveSpeed * deltaTime;
                        
                        if (canMoveTo(strafePos, map)) {
                            m_position = strafePos;
                        } else {
                            // Try the other strafe direction
                            strafeDir = Vec2(m_direction.y, -m_direction.x);
                            strafePos = m_position + strafeDir * m_moveSpeed * deltaTime;
                            
                            if (canMoveTo(strafePos, map)) {
                                m_position = strafePos;
                            }
                        }
                    }
                }
                
                // Use the fourth animation frame (attack) if available
                if (m_isAnimated && m_frameCount > 3) {
                    m_currentFrame = 3;
                }
            }
            break;
            
        case EnemyState::Maintain:
            // Back away from player to maintain minimum distance
            {
                // Direction is away from player
                m_direction = (m_position - playerPos).normalized();
                
                // Calculate target position that's at least MINIMUM_DISTANCE away from player
                double currentDist = distToPlayer;
                double targetDist = MINIMUM_DISTANCE + 0.5; // Add a small buffer
                double moveDistance = std::min(m_moveSpeed * 2.5 * deltaTime, targetDist - currentDist);
                
                // Move away from player more quickly
                Vec2 newPos = m_position + m_direction * moveDistance;
                
                // Check if we can move there
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If we can't back up directly, try moving laterally at an angle
                    // Try multiple angles to find a clear path
                    bool foundPath = false;
                    
                    for (int i = 1; i <= 4; i++) {
                        // Try increasingly wider angles (±30°, ±60°, ±90°, ±120°)
                        double angle = (i * 30.0) * M_PI / 180.0;
                        
                        // Try right turn
                        Vec2 rightDir = m_direction;
                        rightDir.rotate(angle);
                        Vec2 rightPos = m_position + rightDir * m_moveSpeed * 2.0 * deltaTime;
                        
                        if (canMoveTo(rightPos, map)) {
                            m_position = rightPos;
                            foundPath = true;
                            break;
                        }
                        
                        // Try left turn
                        Vec2 leftDir = m_direction;
                        leftDir.rotate(-angle);
                        Vec2 leftPos = m_position + leftDir * m_moveSpeed * 2.0 * deltaTime;
                        
                        if (canMoveTo(leftPos, map)) {
                            m_position = leftPos;
                            foundPath = true;
                            break;
                        }
                    }
                    
                    // If we still can't move, try a random direction as a last resort
                    if (!foundPath) {
                        changeDirection(map);
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
    
    // Reduce health by damage amount
    m_health -= damage;
    std::cout << "Sprite took " << damage << " damage. Health: " << m_health << "/" << m_maxHealth << std::endl;
    
    // Check if sprite is dead
    if (m_health <= 0) {
        m_health = 0;
        m_isDying = true;
        m_deathTimer = 1.0; // 1 second death animation
        std::cout << "Sprite is dying!" << std::endl;
    } 
    // If it's an Imp and not dead, trigger pain state
    else if (m_type == SpriteType::ImpEnemy) {
        // Reset animation timer to show pain frame briefly
        m_animationTimer = 0.0;
        
        // Briefly pause movement by resetting move timer
        m_moveTimer = 0.0;
        
        // 25% chance to enter retreat state when damaged
        if (rand() % 100 < 25) {
            // Set a short movement duration to make it retreat briefly
            m_moveDuration = 0.5 + (rand() % 10) / 10.0; // 0.5-1.5 seconds
        }
    }
}

void Sprite::updateDeathAnimation(double deltaTime) {
    if (!m_isDying) return;
    
    m_deathTimer -= deltaTime;
    
    // Use appropriate death animation frame
    if (m_type == SpriteType::ImpEnemy && m_isAnimated && m_frameCount > 3) {
        // For Imp, use the attack frame (frame 3) as death frame
        m_currentFrame = 3;
        
        // Fade out by adjusting size
        m_size = m_size * (m_deathTimer);
    } else {
        // For other sprites, use the last frame if animated
        if (m_isAnimated && m_frameCount > 0) {
            m_currentFrame = m_frameCount - 1;
        }
        
        // Fade out by adjusting size
        m_size = m_size * (m_deathTimer);
    }
    
    // When timer expires, deactivate the sprite
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
    
    // Define distance thresholds based on original Doom
    const double SIGHT_DISTANCE = 12.0;     // Distance at which Imp notices player
    const double ATTACK_DISTANCE = 3.0;     // Distance at which Imp can attack
    const double MELEE_DISTANCE = 1.2;      // Distance for melee attack
    const double FIREBALL_COOLDOWN = 2.0;   // Time between fireball attacks
    
    // Imp behavior state machine based on original Doom
    enum class ImpState {
        Idle,           // Standing still
        Wander,         // Random movement
        Chase,          // Pursuing player
        Attack,         // Attacking player
        Pain,           // Taking damage
        Retreat         // Moving away from player
    };
    
    // Determine current state
    ImpState state;
    
    if (m_health < m_maxHealth * 0.3) {
        // Low health - occasionally retreat
        if (rand() % 100 < 30) {
            state = ImpState::Retreat;
        } else if (distToPlayer < ATTACK_DISTANCE) {
            state = ImpState::Attack;
        } else if (distToPlayer < SIGHT_DISTANCE) {
            state = ImpState::Chase;
        } else {
            state = ImpState::Wander;
        }
    } else if (distToPlayer < MELEE_DISTANCE) {
        // Very close - melee attack
        state = ImpState::Attack;
    } else if (distToPlayer < ATTACK_DISTANCE && m_specialMoveTimer >= FIREBALL_COOLDOWN) {
        // Within attack range and cooldown expired
        state = ImpState::Attack;
    } else if (distToPlayer < SIGHT_DISTANCE) {
        // Within sight range - chase
        state = ImpState::Chase;
    } else if (m_moveTimer >= m_moveDuration) {
        // Time to change wandering direction
        state = ImpState::Idle;
    } else {
        // Continue wandering
        state = ImpState::Wander;
    }
    
    // Handle behavior based on state
    switch (state) {
        case ImpState::Idle:
            // Stand still briefly, then start wandering
            if (m_isAnimated) {
                m_currentFrame = 0; // Standing frame
            }
            
            // After a short pause, transition to wandering
            if (m_moveTimer > 1.0) {
                m_moveTimer = 0.0;
                m_moveDuration = 2.0 + (rand() % 30) / 10.0; // 2-5 seconds
                
                // Choose a random direction
                double angle = (rand() % 628) / 100.0; // 0-2π
                m_direction = Vec2(cos(angle), sin(angle));
            }
            break;
            
        case ImpState::Wander:
            // Wander in current direction
            {
                // Move in current direction
                Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.5 * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // Hit a wall, change direction
                    changeDirection(map);
                }
                
                // Use walking animation frames
                if (m_isAnimated && m_frameCount > 2) {
                    m_animationTimer += deltaTime;
                    if (m_animationTimer >= 0.25) { // 4 frames per second
                        m_currentFrame = (m_currentFrame == 0) ? 1 : ((m_currentFrame == 1) ? 2 : 0);
                        m_animationTimer = 0.0;
                    }
                }
            }
            break;
            
        case ImpState::Chase:
            // Chase the player with the classic Doom zig-zag pattern
            {
                // Base direction is towards player
                Vec2 baseDirection = toPlayer.normalized();
                
                // Classic Doom imps don't move in a straight line - they zig-zag
                // This makes them harder to hit and more menacing
                double zigZagFactor = sin(m_moveTimer * 3.0) * 0.5;
                Vec2 perpDirection(-baseDirection.y, baseDirection.x);
                Vec2 zigzagDir = baseDirection + perpDirection * zigZagFactor;
                zigzagDir = zigzagDir.normalized();
                
                // Set the direction
                m_direction = zigzagDir;
                
                // Move in the zigzag direction
                Vec2 newPos = m_position + m_direction * m_moveSpeed * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If blocked, try moving directly towards player
                    newPos = m_position + baseDirection * m_moveSpeed * deltaTime;
                    if (canMoveTo(newPos, map)) {
                        m_position = newPos;
                    } else {
                        // If still blocked, try to find a way around
                        changeDirection(map);
                    }
                }
                
                // Use walking animation frames
                if (m_isAnimated && m_frameCount > 2) {
                    m_animationTimer += deltaTime;
                    if (m_animationTimer >= 0.25) { // 4 frames per second
                        m_currentFrame = (m_currentFrame == 0) ? 1 : ((m_currentFrame == 1) ? 2 : 0);
                        m_animationTimer = 0.0;
                    }
                }
            }
            break;
            
        case ImpState::Attack:
            // Attack the player
            {
                // Face the player
                m_direction = toPlayer.normalized();
                
                // Use attack animation frame
                if (m_isAnimated && m_frameCount > 3) {
                    m_currentFrame = 3;
                }
                
                // If in melee range, perform melee attack
                if (distToPlayer < MELEE_DISTANCE) {
                    // Melee attack logic would go here
                    // For now, just reset the special move timer
                    m_specialMoveTimer = 0.0;
                } 
                // Otherwise, if cooldown expired, perform ranged attack
                else if (m_specialMoveTimer >= FIREBALL_COOLDOWN) {
                    // Ranged attack logic would go here
                    // Reset the special move timer
                    m_specialMoveTimer = 0.0;
                }
            }
            break;
            
        case ImpState::Retreat:
            // Move away from player
            {
                // Direction away from player
                Vec2 awayDir = (m_position - playerPos).normalized();
                
                // Add some randomness to retreat direction
                double angle = (rand() % 60 - 30) * 3.14159 / 180.0; // ±30 degrees
                Vec2 retreatDir = Vec2(
                    awayDir.x * cos(angle) - awayDir.y * sin(angle),
                    awayDir.x * sin(angle) + awayDir.y * cos(angle)
                );
                
                // Set direction
                m_direction = retreatDir;
                
                // Move in retreat direction
                Vec2 newPos = m_position + m_direction * m_moveSpeed * 0.7 * deltaTime;
                if (canMoveTo(newPos, map)) {
                    m_position = newPos;
                } else {
                    // If blocked, try a different angle
                    changeDirection(map);
                }
                
                // Use walking animation frames but faster
                if (m_isAnimated && m_frameCount > 2) {
                    m_animationTimer += deltaTime * 1.5; // Faster animation
                    if (m_animationTimer >= 0.25) {
                        m_currentFrame = (m_currentFrame == 0) ? 1 : ((m_currentFrame == 1) ? 2 : 0);
                        m_animationTimer = 0.0;
                    }
                }
            }
            break;
            
        case ImpState::Pain:
            // Pain state - briefly pause movement
            if (m_isAnimated) {
                m_currentFrame = 0; // Use standing frame for pain
            }
            break;
    }
}

void Sprite::applyItemEffect(Player* player) {
    if (!player || m_type != SpriteType::Item) return;
    
    // Apply effect based on item type
    switch (m_itemType) {
        case ItemType::HealthSmall:
            player->setHealth(player->getHealth() + 10);
            std::cout << "Picked up small health pack (+10 health)" << std::endl;
            break;
            
        case ItemType::HealthMedium:
            player->setHealth(player->getHealth() + 25);
            std::cout << "Picked up medikit (+25 health)" << std::endl;
            break;
            
        case ItemType::HealthLarge:
            player->setHealth(player->getHealth() + 100);
            std::cout << "Picked up soulsphere (+100 health)" << std::endl;
            break;
            
        case ItemType::ArmorSmall:
            player->addArmor(5);
            std::cout << "Picked up armor bonus (+5 armor)" << std::endl;
            break;
            
        case ItemType::ArmorMedium:
            player->setArmor(100);
            std::cout << "Picked up green armor (100 armor)" << std::endl;
            break;
            
        case ItemType::ArmorLarge:
            player->setArmor(200);
            std::cout << "Picked up blue armor (200 armor)" << std::endl;
            break;
            
        case ItemType::AmmoSmall:
            player->setAmmo(player->getAmmo() + 5);
            std::cout << "Picked up small ammo pack (+5 ammo)" << std::endl;
            break;
            
        case ItemType::AmmoMedium:
            player->setAmmo(player->getAmmo() + 20);
            std::cout << "Picked up medium ammo pack (+20 ammo)" << std::endl;
            break;
            
        case ItemType::AmmoLarge:
            player->setAmmo(player->getAmmo() + 100);
            std::cout << "Picked up large ammo pack (+100 ammo)" << std::endl;
            break;
            
        case ItemType::WeaponShotgun:
            player->setCurrentWeapon(WeaponType::Shotgun);
            player->setAmmo(player->getAmmo() + 8);
            std::cout << "Picked up shotgun" << std::endl;
            break;
            
        case ItemType::WeaponChainsaw:
            player->setCurrentWeapon(WeaponType::Chainsaw);
            std::cout << "Picked up chainsaw" << std::endl;
            break;
            
        case ItemType::WeaponRocket:
            player->setCurrentWeapon(WeaponType::RocketLauncher);
            player->setAmmo(player->getAmmo() + 2);
            std::cout << "Picked up rocket launcher" << std::endl;
            break;
            
        case ItemType::WeaponPlasma:
            player->setCurrentWeapon(WeaponType::PlasmaGun);
            player->setAmmo(player->getAmmo() + 40);
            std::cout << "Picked up plasma gun" << std::endl;
            break;
            
        case ItemType::PowerupBerserk:
            player->activatePowerUp(PowerUpType::Berserk, 30.0);
            std::cout << "Picked up berserk pack (30 seconds)" << std::endl;
            break;
            
        case ItemType::PowerupInvulnerability:
            player->activatePowerUp(PowerUpType::Invulnerability, 30.0);
            std::cout << "Picked up invulnerability (30 seconds)" << std::endl;
            break;
            
        default:
            std::cout << "Picked up unknown item" << std::endl;
            break;
    }
    
    // Deactivate the item after it's picked up
    m_isActive = false;
}

// SpriteManager implementation
SpriteManager::SpriteManager(const TextureManager* textureManager)
    : m_textureManager(textureManager)
{
    // Set the singleton instance
    std::cout << "Creating SpriteManager instance: " << this << std::endl;
    if (s_instance != nullptr) {
        std::cout << "WARNING: Overwriting existing SpriteManager instance: " << s_instance << " with " << this << std::endl;
    }
    s_instance = this;
    std::cout << "Set SpriteManager singleton to: " << s_instance << std::endl;
}

SpriteManager::~SpriteManager()
{
    // Clear the singleton instance if it's this instance
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

int SpriteManager::addSprite(double x, double y, double size, int textureId, SpriteType type) {
    // Check if texture manager exists
    if (!m_textureManager) {
        std::cerr << "ERROR: TextureManager is null in SpriteManager::addSprite!" << std::endl;
        return -1;
    }
    
    // Check if texture exists
    const Texture* texture = m_textureManager->getTexture(textureId);
    if (!texture) {
        std::cerr << "ERROR: Invalid texture ID " << textureId << " in SpriteManager::addSprite!" << std::endl;
        return -1; // Invalid texture ID
    }
    
    // Check if SDL texture exists
    SDL_Texture* sdlTexture = texture->getSDLTexture();
    if (!sdlTexture) {
        std::cerr << "ERROR: Texture ID " << textureId << " has null SDL_Texture in SpriteManager::addSprite!" << std::endl;
        return -1; // Invalid SDL texture
    }
    
    // Create the sprite
    m_sprites.emplace_back(x, y, size, textureId, type);
    
    // Ensure the sprite is visible and active
    int spriteId = static_cast<int>(m_sprites.size() - 1);
    m_sprites[spriteId].setVisible(true);
    m_sprites[spriteId].setActive(true);
    
    std::cout << "DEBUG: Added sprite with ID " << spriteId << ", type " << static_cast<int>(type) 
              << ", texture ID " << textureId << " at position (" << x << ", " << y << ")" << std::endl;
    
    return spriteId;
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