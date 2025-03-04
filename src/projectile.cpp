#include "projectile.h"
#include "map.h"
#include "texture.h"
#include <iostream>
#include <cmath>

Projectile::Projectile()
    : m_position(0.0, 0.0)
    , m_direction(0.0, 0.0)
    , m_speed(0.0)
    , m_damage(0.0)
    , m_lifetime(0.0)
    , m_maxLifetime(0.0)
    , m_active(false)
    , m_hasCollided(false)
    , m_id(0)
    , m_textureId(0)
    , m_type(ProjectileType::Bullet)
{
}

Projectile::Projectile(double x, double y, double dirX, double dirY, double speed,
                       double damage, double maxLifetime, int textureId, ProjectileType type)
    : m_position(x, y)
    , m_direction(dirX, dirY)
    , m_speed(speed)
    , m_damage(damage)
    , m_lifetime(0.0)
    , m_maxLifetime(maxLifetime)
    , m_active(true)
    , m_hasCollided(false)
    , m_id(0)
    , m_textureId(textureId)
    , m_type(type)
{
}

void Projectile::init(double x, double y, double dirX, double dirY, double speed,
                     double damage, double maxLifetime, int textureId, ProjectileType type) {
    m_position.x = x;
    m_position.y = y;
    m_direction.x = dirX;
    m_direction.y = dirY;
    m_speed = speed;
    m_damage = damage;
    m_lifetime = 0.0;
    m_maxLifetime = maxLifetime;
    m_active = true;
    m_hasCollided = false;
    m_textureId = textureId;
    m_type = type;
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

bool Projectile::checkMapCollision(const Map& map) {
    return map.isSolid(static_cast<int>(m_position.x), static_cast<int>(m_position.y));
}

// ProjectileManager implementation
ProjectileManager::ProjectileManager()
    : m_nextId(0)
    , m_bulletTextureId(0)
    , m_rocketTextureId(0)
    , m_plasmaTextureId(0)
    , m_defaultBulletTexture(0)
{
    std::cout << "ProjectileManager created" << std::endl;
}

ProjectileManager::~ProjectileManager() {
    // Clean up any active projectiles
    for (auto* projectile : m_activeProjectiles) {
        delete projectile;
    }
    m_activeProjectiles.clear();
    
    std::cout << "ProjectileManager destroyed" << std::endl;
}

int ProjectileManager::createProjectile(const Vec2& position, const Vec2& direction, 
                                         ProjectileType type, double speed, double damage) {
    // Create new projectile
    Projectile* projectile = new Projectile();
    
    // Set basic properties
    projectile->m_position = position;
    projectile->m_direction = direction;
    projectile->m_type = type;
    projectile->m_damage = damage;
    projectile->m_active = true;
    projectile->m_id = m_nextId++;
    projectile->m_hasCollided = false;
    projectile->m_lifetime = 0.0;
    
    // Set type-specific properties
    switch (type) {
        case ProjectileType::Bullet:
            projectile->m_speed = speed > 0.0 ? speed : 15.0;
            projectile->m_maxLifetime = 20.0;  // Increased for testing
            projectile->m_textureId = m_bulletTextureId;
            break;
        case ProjectileType::Rocket:
            projectile->m_speed = speed > 0.0 ? speed : 10.0;
            projectile->m_maxLifetime = 30.0;  // Increased for testing
            projectile->m_textureId = m_rocketTextureId;
            break;
        case ProjectileType::Plasma:
            projectile->m_speed = speed > 0.0 ? speed : 12.0;
            projectile->m_maxLifetime = 25.0;  // Increased for testing
            projectile->m_textureId = m_plasmaTextureId;
            break;
    }
    
    // Add to active projectiles
    m_activeProjectiles.push_back(projectile);
    
    std::cout << "Created projectile ID " << projectile->m_id 
              << " at position (" << position.x << ", " << position.y 
              << ") with direction (" << direction.x << ", " << direction.y 
              << "), speed " << projectile->m_speed 
              << ", damage " << damage 
              << ", type " << static_cast<int>(type)
              << ", texture ID " << projectile->m_textureId 
              << std::endl;
    
    return projectile->m_id;
}

void ProjectileManager::update(double deltaTime, const Map& map) {
    std::cout << "ProjectileManager::update - Active projectiles: " << m_activeProjectiles.size() << std::endl;
    
    // Update the old projectiles list for compatibility
    for (auto& projectile : m_projectiles) {
        if (projectile.isActive()) {
            projectile.update(deltaTime, map);
        }
    }
    
    // Update new projectiles
    for (auto it = m_activeProjectiles.begin(); it != m_activeProjectiles.end();) {
        Projectile* projectile = *it;
        
        if (!projectile) {
            std::cerr << "Null projectile in active projectiles list!" << std::endl;
            it = m_activeProjectiles.erase(it);
            continue;
        }
        
        // Log all active projectiles
        std::cout << "Updating projectile at (" << projectile->m_position.x << ", " 
                  << projectile->m_position.y << ")" << std::endl;
        
        // Increment lifetime
        projectile->m_lifetime += deltaTime;
        
        // Delete projectile if it's expired its maximum lifetime
        if (projectile->m_lifetime > projectile->m_maxLifetime) {
            std::cout << "Projectile lifetime expired: " << projectile->m_lifetime << "s" << std::endl;
            delete projectile;
            it = m_activeProjectiles.erase(it);
            continue;
        }
        
        // Don't move the projectile if it's hit something, but keep it visible
        if (projectile->m_hasCollided) {
            std::cout << "Projectile has collided, not moving but still visible" << std::endl;
            ++it;
            continue;
        }
        
        // Store original position for collision checking
        Vec2 oldPos = projectile->m_position;
        
        // Move projectile
        projectile->m_position.x += projectile->m_direction.x * projectile->m_speed * deltaTime;
        projectile->m_position.y += projectile->m_direction.y * projectile->m_speed * deltaTime;
        
        // Boundary check
        if (projectile->m_position.x < 0 || projectile->m_position.x >= map.getWidth() ||
            projectile->m_position.y < 0 || projectile->m_position.y >= map.getHeight()) {
            std::cout << "Projectile out of bounds at (" << projectile->m_position.x << ", " 
                      << projectile->m_position.y << ")" << std::endl;
            projectile->m_position = oldPos;
            projectile->m_hasCollided = true;
            ++it;
            continue;
        }
        
        // Simple collision detection with map
        int mapX = static_cast<int>(projectile->m_position.x);
        int mapY = static_cast<int>(projectile->m_position.y);
        
        if (map.isSolid(mapX, mapY)) {
            std::cout << "Projectile hit wall at (" << mapX << ", " << mapY << ")" << std::endl;
            // For wall collisions, revert position to just before the wall and mark as collided
            projectile->m_position = oldPos;
            projectile->m_hasCollided = true;
            ++it;
            continue;
        }
        
        // If no collision, continue to next projectile
        ++it;
    }
}

Projectile* ProjectileManager::getProjectile(size_t index) {
    if (index < m_activeProjectiles.size()) {
        return m_activeProjectiles[index];
    }
    return nullptr;
}

int ProjectileManager::getActiveCount() const {
    int count = 0;
    for (const auto& projectile : m_projectiles) {
        if (projectile.isActive()) {
            count++;
        }
    }
    // Add count from new projectiles list
    count += m_activeProjectiles.size();
    return count;
}

ProjectileType Projectile::getType() const {
    return m_type;
}

bool Projectile::isActive() const {
    return m_active;
}

double Projectile::getLifetime() const {
    return m_lifetime;
}

bool Projectile::hasCollided() const {
    return m_hasCollided;
} 