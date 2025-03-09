#include "projectile.h"
#include "map.h"
#include "texture.h"
#include <iostream>
#include <cmath>

Projectile::Projectile(int id, const Vec2& position, const Vec2& direction, double speed, double damage)
    : m_id(id)
    , m_position(position)
    , m_direction(direction.normalized())  // Ensure direction is normalized
    , m_velocity(m_direction * speed)      // Initialize velocity vector
    , m_acceleration(0, 0)                 // Initialize acceleration
    , m_speed(speed)
    , m_damage(damage)
    , m_lifetime(0.0)
    , m_maxLifetime(2.0)
    , m_active(true)
    , m_hasCollided(false)
    , m_collisionTime(0.0)
    , m_collisionDebugOutput(true)
    , m_textureId(0)
    , m_type(ProjectileType::Bullet)
    , m_size(0.1)          // Default size
    , m_color(255, 255, 255) // Default color
    , m_explosionRadius(0.0)
    , m_explosionDamage(0.0)
    , m_gravity(0.0)
    , m_airResistance(0.0)
    , m_mass(1.0)
    , m_bounciness(0.0)
    , m_bounceCount(0)
    , m_maxBounces(0)     // Default: no bouncing
    , m_usePhysics(false) // Default: use simple movement
    , m_bounceFactor(0.0) // Default: no bounce
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
    if (m_hasCollided) {
        // For rockets, create explosion after a short visibility period
        if (m_type == ProjectileType::Rocket) {
            // Debug output for rocket collision
            if (m_collisionDebugOutput) {
                std::cout << "Rocket ID " << m_id << " collided and waiting for explosion" << std::endl;
                m_collisionDebugOutput = false; // Only output once
            }
            
            // Allow a brief visibility time after collision before explosion
            if (m_collisionTime > 0.2) { // 200ms visibility after collision
                std::cout << "Rocket ID " << m_id << " creating explosion effect" << std::endl;
                // Create explosion effect here if we had a reference to ProjectileManager
                m_active = false; // Deactivate after brief visibility
            } else {
                m_collisionTime += deltaTime;
            }
        }
        return;
    }
    
    // Store original position
    Vec2 oldPos = m_position;
    
    if (m_usePhysics) {
        // Use advanced physics simulation
        applyPhysics(deltaTime, map);
    } else {
        // Use simple movement (original behavior)
        m_position.x += m_direction.x * m_speed * deltaTime;
        m_position.y += m_direction.y * m_speed * deltaTime;
        
        // Check for map boundaries
        if (m_position.x < 0 || m_position.x >= map.getWidth() ||
            m_position.y < 0 || m_position.y >= map.getHeight()) {
            // Keep active for visibility but stop moving
            m_position = oldPos;
            m_speed = 0;
            m_hasCollided = true;
            m_collisionDebugOutput = true;
            std::cout << "Projectile ID " << m_id << " collided with map boundary" << std::endl;
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
            m_collisionDebugOutput = true;
            std::cout << "Projectile ID " << m_id << " collided with wall at (" 
                      << mapX << ", " << mapY << ")" << std::endl;
            return;
        }
    }
}

void Projectile::applyPhysics(double deltaTime, const Map& map) {
    // Apply gravity if enabled
    if (m_gravity != 0.0) {
        m_acceleration.y = m_gravity;
    }
    
    // Apply air resistance if enabled
    if (m_airResistance > 0.0) {
        // Air resistance is proportional to velocity squared and in opposite direction
        double speedSquared = m_velocity.lengthSquared();
        if (speedSquared > 0.0001) {
            Vec2 dragForce = m_velocity.normalized() * (-m_airResistance * speedSquared);
            Vec2 dragAccel = dragForce / m_mass;
            m_acceleration = m_acceleration + dragAccel;
        }
    }
    
    // Update velocity based on acceleration
    m_velocity = m_velocity + m_acceleration * deltaTime;
    
    // Store old position for collision detection
    Vec2 oldPos = m_position;
    
    // Update position based on velocity
    m_position = m_position + m_velocity * deltaTime;
    
    // Update direction to match velocity (for rendering)
    if (m_velocity.lengthSquared() > 0.0001) {
        m_direction = m_velocity.normalized();
    }
    
    // Check for map boundaries
    if (m_position.x < 0 || m_position.x >= map.getWidth() ||
        m_position.y < 0 || m_position.y >= map.getHeight()) {
        
        // Calculate normal vector for boundary collision
        Vec2 normal(0, 0);
        if (m_position.x < 0) normal.x = 1;
        else if (m_position.x >= map.getWidth()) normal.x = -1;
        
        if (m_position.y < 0) normal.y = 1;
        else if (m_position.y >= map.getHeight()) normal.y = -1;
        
        // Clamp position to boundaries
        m_position.x = std::max(0.0, std::min(static_cast<double>(map.getWidth() - 0.01), m_position.x));
        m_position.y = std::max(0.0, std::min(static_cast<double>(map.getHeight() - 0.01), m_position.y));
        
        // Handle collision with boundary
        handleCollision(normal, map);
        return;
    }
    
    // Check for wall collision
    int mapX = static_cast<int>(m_position.x);
    int mapY = static_cast<int>(m_position.y);
    
    if (map.isSolid(mapX, mapY)) {
        // Calculate which wall was hit and the normal vector
        double fracX = m_position.x - mapX;
        double fracY = m_position.y - mapY;
        
        // Determine which side of the cell we hit
        Vec2 normal(0, 0);
        
        // Coming from left or right
        if (oldPos.x < mapX && fracX < 0.1) normal.x = -1;
        else if (oldPos.x > mapX + 1 && fracX > 0.9) normal.x = 1;
        
        // Coming from top or bottom
        if (oldPos.y < mapY && fracY < 0.1) normal.y = -1;
        else if (oldPos.y > mapY + 1 && fracY > 0.9) normal.y = 1;
        
        // If we couldn't determine the normal, use a default
        if (normal.x == 0 && normal.y == 0) {
            // Calculate the normal based on which side we're closest to
            if (fracX < 0.5 && fracX < fracY && fracX < 1 - fracY) normal.x = -1;
            else if (fracX > 0.5 && fracX > fracY && fracX > 1 - fracY) normal.x = 1;
            else if (fracY < 0.5) normal.y = -1;
            else normal.y = 1;
        }
        
        // Move back to previous position
        m_position = oldPos;
        
        // Handle the collision
        handleCollision(normal, map);
    }
}

void Projectile::handleCollision(const Vec2& normal, const Map& map) {
    // Get material properties for the cell we hit
    CellType cellType = map.getCell(static_cast<int>(m_position.x), static_cast<int>(m_position.y));
    MaterialProperties material = getMaterialProperties(cellType);
    
    // If we can't bounce or have reached max bounces, stop the projectile
    if (!material.canBounce || m_bounceCount >= m_maxBounces) {
        m_velocity = Vec2(0, 0);
        m_acceleration = Vec2(0, 0);
        m_hasCollided = true;
        return;
    }
    
    // Calculate reflection vector: v' = v - 2(v·n)n
    double dotProduct = m_velocity.x * normal.x + m_velocity.y * normal.y;
    Vec2 reflection;
    reflection.x = m_velocity.x - 2 * dotProduct * normal.x;
    reflection.y = m_velocity.y - 2 * dotProduct * normal.y;
    
    // Apply bounciness (restitution)
    double bounceFactor = m_bounciness * material.restitution;
    m_velocity = reflection * bounceFactor;
    
    // Apply friction to the tangential component
    Vec2 tangent(-normal.y, normal.x);  // Perpendicular to normal
    double tangentDot = m_velocity.x * tangent.x + m_velocity.y * tangent.y;
    Vec2 tangentVel = tangent * tangentDot;
    
    // Apply friction
    tangentVel = tangentVel * (1.0 - material.friction);
    
    // Recombine normal and tangential components
    double normalDot = m_velocity.x * normal.x + m_velocity.y * normal.y;
    Vec2 normalVel = normal * normalDot;
    m_velocity = normalVel + tangentVel;
    
    // Increment bounce count
    m_bounceCount++;
    
    // If velocity is very low after bounce, stop the projectile
    if (m_velocity.lengthSquared() < 0.1) {
        m_velocity = Vec2(0, 0);
        m_acceleration = Vec2(0, 0);
        m_hasCollided = true;
    }
}

MaterialProperties Projectile::getMaterialProperties(CellType cellType) const {
    MaterialProperties props;
    
    // Default properties
    props.friction = 0.2;
    props.restitution = 0.5;
    props.canBounce = false;
    
    // Customize based on cell type
    switch (cellType) {
        case CellType::Wall:
        case CellType::ElevatedWall:
            props.friction = 0.3;
            props.restitution = 0.4;
            props.canBounce = true;
            break;
            
        case CellType::Floor:
        case CellType::ElevatedFloor:
            props.friction = 0.2;
            props.restitution = 0.6;
            props.canBounce = true;
            break;
            
        case CellType::Door:
            props.friction = 0.4;
            props.restitution = 0.3;
            props.canBounce = true;
            break;
            
        default:
            // Use defaults
            break;
    }
    
    return props;
}

// ProjectileManager implementation
ProjectileManager::ProjectileManager()
    : m_nextId(0)
    , m_bulletTextureId(0)
    , m_rocketTextureId(0)
    , m_plasmaTextureId(0)
    , m_grenadeTextureId(0)
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
    
    // Set properties based on projectile type
    projectile->m_type = type;
    
    switch (type) {
        case ProjectileType::Bullet:
            projectile->m_size = 0.1;
            projectile->m_lifetime = 0.5;  // Shorter lifetime for DOOM-style bullets
            projectile->m_usePhysics = false;  // Bullets use simple physics
            projectile->m_textureId = m_bulletTextureId;  // Use the bullet texture
            projectile->m_color = Color(255, 220, 50);  // Yellow-orange for tracer effect
            break;
            
        case ProjectileType::Rocket:
            projectile->m_size = 0.8;  // Significantly increased size for better visibility
            projectile->m_lifetime = 0.0;  // Start at 0 lifetime
            projectile->m_maxLifetime = 10.0;  // Longer maximum lifetime
            projectile->m_usePhysics = true;   // Rockets use advanced physics
            projectile->m_explosionRadius = 2.0;
            projectile->m_explosionDamage = damage * 0.7;  // Explosion does 70% of direct hit damage
            projectile->m_textureId = m_rocketTextureId;
            projectile->m_color = Color(255, 100, 0);  // Orange for rocket
            break;
            
        case ProjectileType::Plasma:
            projectile->m_size = 0.2;
            projectile->m_lifetime = 1.5;
            projectile->m_usePhysics = false;
            projectile->m_textureId = m_plasmaTextureId;  // Use the plasma texture
            projectile->m_color = Color(80, 180, 255);  // Blue for plasma
            break;
            
        case ProjectileType::Grenade:
            projectile->m_size = 0.25;
            projectile->m_lifetime = 3.0;
            projectile->m_usePhysics = true;
            projectile->m_gravity = 5.0;       // Affected by gravity
            projectile->m_bounceFactor = 0.6;  // Bounces off surfaces
            projectile->m_explosionRadius = 3.0;
            projectile->m_explosionDamage = damage;
            projectile->m_textureId = m_grenadeTextureId;
            projectile->m_color = Color(100, 100, 100);  // Gray for grenade
            break;
            
        case ProjectileType::BFG:
            projectile->m_size = 0.5;          // Large projectile
            projectile->m_lifetime = 4.0;
            projectile->m_usePhysics = false;
            projectile->m_textureId = m_plasmaTextureId;  // Use plasma texture for BFG too
            projectile->m_color = Color(0, 255, 0);  // Green color
            projectile->m_explosionRadius = 5.0;     // Massive explosion radius
            projectile->m_explosionDamage = damage;  // Full damage in explosion
            break;
    }
    
    // If no texture is set, use default
    if (projectile->m_textureId <= 0) {
        if (type == ProjectileType::Plasma || type == ProjectileType::BFG) {
            projectile->m_textureId = m_plasmaTextureId;
        } else {
            projectile->m_textureId = m_defaultBulletTexture;
        }
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
                    projectile->m_velocity = Vec2(0, 0);
                    projectile->m_position = oldPos; // Keep projectile at collision point
                    
                    std::cout << "Projectile hit sprite! Damage: " << projectile->getDamage() << std::endl;
                    break;
                }
            }
        }
        
        // Update lifetime
        projectile->m_lifetime -= deltaTime;
        if (projectile->m_lifetime <= 0) {
            // Handle explosion for rockets, grenades, and BFG
            if (projectile->m_type == ProjectileType::Rocket || 
                projectile->m_type == ProjectileType::Grenade ||
                projectile->m_type == ProjectileType::BFG) {
                createExplosion(projectile->m_position, projectile->m_explosionRadius, projectile->m_explosionDamage);
            }
            
            projectile->m_active = false;
            ++it;
            continue;
        }
        
        // Special behavior for BFG projectile
        if (projectile->m_type == ProjectileType::BFG) {
            // BFG projectiles damage enemies in their path
            if (m_spriteManager) {
                std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
                
                for (Sprite* sprite : sprites) {
                    if (!sprite || sprite->isDying() || !sprite->isActive()) continue;
                    
                    // Calculate distance from sprite to BFG projectile
                    double dist = (sprite->getPosition() - projectile->m_position).length();
                    
                    // BFG damages enemies within a certain radius as it travels
                    if (dist < 2.0) {
                        // Apply a portion of the damage
                        sprite->takeDamage(projectile->m_damage * 0.1 * deltaTime);
                    }
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

void ProjectileManager::createExplosion(const Vec2& position, double radius, double damage) {
    if (!m_spriteManager) return;
    
    std::cout << "Creating explosion at (" << position.x << ", " << position.y << ") with radius " << radius << " and damage " << damage << std::endl;
    
    // Get all sprites
    std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
    
    // Check each sprite for distance to explosion
    for (Sprite* sprite : sprites) {
        if (!sprite || !sprite->isActive() || sprite->isDying()) continue;
        
        // Calculate distance from sprite to explosion center
        double distance = (sprite->getPosition() - position).length();
        
        // Check if sprite is within explosion radius
        if (distance <= radius) {
            // Calculate damage based on distance (more damage closer to center)
            double distanceFactor = 1.0 - (distance / radius); // 1.0 at center, 0.0 at edge
            double explosionDamage = damage * distanceFactor;
            
            // Apply damage to sprite
            sprite->takeDamage(explosionDamage);
            
            std::cout << "Explosion hit sprite at distance " << distance << ", dealing " << explosionDamage << " damage" << std::endl;
        }
    }
    
    // TODO: Add visual effects for explosion
}

