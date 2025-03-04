#include "projectile.h"
#include "map.h"
#include "texture.h"
#include <iostream>
#include <cmath>

Projectile::Projectile(int id, const Vec2& position, const Vec2& direction, double speed, double damage)
    : m_id(id)
    , m_position(position)
    , m_direction(direction)
    , m_speed(speed)
    , m_damage(damage)
    , m_lifetime(0.0)
    , m_maxLifetime(5.0)  // 5 seconds default lifetime
    , m_active(true)
    , m_hasCollided(false)
    , m_textureId(-1)  // Default to no texture
    , m_type(ProjectileType::Bullet)  // Default to bullet type
{
}

void Projectile::update(double deltaTime, const Map& map) {
    // If not active, do nothing
    if (!m_active) return;
    
    // Update lifetime
    m_lifetime += deltaTime;
    
    // Check if lifetime expired
    if (m_lifetime > m_maxLifetime) {
        m_active = false;
        return;
    }
    
    // Don't move if collision already happened but keep visible
    if (m_hasCollided) return;
    
    // Store original position
    Vec2 oldPos = m_position;
    
    // Move projectile
    m_position.x += m_direction.x * m_speed * deltaTime;
    m_position.y += m_direction.y * m_speed * deltaTime;
    
    // Check for map boundaries
    if (m_position.x < 0 || m_position.x >= map.getWidth() ||
        m_position.y < 0 || m_position.y >= map.getHeight()) {
        // Keep active for visibility but stop moving
        m_position = oldPos;
        m_speed = 0;
        m_hasCollided = true;
        return;
    }
    
    // Check for wall collision
    int mapX = static_cast<int>(m_position.x);
    int mapY = static_cast<int>(m_position.y);
    
    if (map.isSolid(mapX, mapY)) {
        // Don't immediately deactivate - make bullet hit effect visible for a short time
        m_position = oldPos;
        m_speed = 0;
        m_hasCollided = true;
        return;
    }
}

// ProjectileManager implementation
ProjectileManager::ProjectileManager()
    : m_nextId(0)
    , m_bulletTextureId(0)
    , m_rocketTextureId(0)
    , m_plasmaTextureId(0)
    , m_defaultBulletTexture(0)
    , m_spriteManager(nullptr)
{
    std::cout << "ProjectileManager created" << std::endl;
}

ProjectileManager::~ProjectileManager() {
    // Clean up any remaining active projectiles
    for (auto projectile : m_activeProjectiles) {
        delete projectile;
    }
    m_activeProjectiles.clear();
}

int ProjectileManager::createProjectile(const Vec2& position, const Vec2& direction, ProjectileType type, double speed, double damage) {
    Projectile* projectile = new Projectile(m_nextId++, position, direction, speed, damage);
    
    // Set the projectile type
    projectile->m_type = type;
    
    // Set the texture ID based on projectile type
    switch (type) {
        case ProjectileType::Bullet:
            projectile->m_textureId = m_bulletTextureId;
            break;
        case ProjectileType::Rocket:
            projectile->m_textureId = m_rocketTextureId;
            break;
        case ProjectileType::Plasma:
            projectile->m_textureId = m_plasmaTextureId;
            break;
        default:
            projectile->m_textureId = m_defaultBulletTexture;
            break;
    }
    
    m_activeProjectiles.push_back(projectile);
    return projectile->m_id;
}

void ProjectileManager::setSpriteManager(SpriteManager* spriteManager) {
    m_spriteManager = spriteManager;
}

void ProjectileManager::update(double deltaTime, const Map& map) {
    // Update all active projectiles
    for (auto it = m_activeProjectiles.begin(); it != m_activeProjectiles.end();) {
        Projectile* projectile = *it;
        
        if (!projectile) {
            it = m_activeProjectiles.erase(it);
            continue;
        }
        
        // Store old position for collision detection
        Vec2 oldPos = projectile->getPosition();
        
        // Update projectile
        projectile->update(deltaTime, map);
        
        // Check for sprite collisions if we have a sprite manager
        if (m_spriteManager && !projectile->hasCollided()) {
            std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
            
            for (Sprite* sprite : sprites) {
                if (!sprite || sprite->isDying() || !sprite->isActive()) continue;
                
                // Simple collision check - if projectile is within sprite's radius
                Vec2 spritePos = sprite->getPosition();
                double spriteSize = sprite->getSize() * 0.5; // Half size for radius
                
                // Check both old and new position to prevent fast bullets from passing through
                Vec2 newPos = projectile->getPosition();
                
                // Check if either position is within the sprite's radius
                bool oldPosInRange = (oldPos - spritePos).length() < spriteSize;
                bool newPosInRange = (newPos - spritePos).length() < spriteSize;
                
                if (oldPosInRange || newPosInRange) {
                    // Hit! Apply damage and mark projectile as collided
                    sprite->takeDamage(projectile->getDamage());
                    projectile->m_hasCollided = true;
                    projectile->m_speed = 0;
                    projectile->m_position = oldPos; // Keep projectile at collision point
                    
                    std::cout << "Projectile hit sprite! Damage: " << projectile->getDamage() << std::endl;
                    break;
                }
            }
        }
        
        // Remove inactive projectiles
        if (!projectile->isActive()) {
            delete projectile;
            it = m_activeProjectiles.erase(it);
        } else {
            ++it;
        }
    }
}


int ProjectileManager::getActiveCount() const {
    return static_cast<int>(m_activeProjectiles.size());
}

