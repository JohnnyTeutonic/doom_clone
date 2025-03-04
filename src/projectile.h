#pragma once

#include <vector>
#include "utils.h"
#include "map.h"
#include "sprite.h"
// Forward declare Map to avoid circular dependency
class Map;
class TextureManager;

// Different types of projectiles
enum class ProjectileType {
    Bullet,
    Rocket,
    Plasma,
    Grenade  // New type for bouncing projectiles
};

// Material properties for different surfaces
struct MaterialProperties {
    double friction;      // Friction coefficient (0-1)
    double restitution;   // Bounciness (0-1)
    bool canBounce;       // Whether this material allows bouncing
};

class Projectile {
public:
    Projectile(int id, const Vec2& position, const Vec2& direction, double speed, double damage);
    
    void update(double deltaTime, const Map& map);
    bool isActive() const { return m_active; }
    bool hasCollided() const { return m_hasCollided; }
    const Vec2& getPosition() const { return m_position; }
    const Vec2& getDirection() const { return m_direction; }
    const Vec2& getVelocity() const { return m_velocity; }
    double getDamage() const { return m_damage; }
    double getLifetime() const { return m_lifetime; }
    int getId() const { return m_id; }
    int getTextureId() const { return m_textureId; }
    ProjectileType getType() const { return m_type; }
    
    // Physics setters
    void setGravity(double gravity) { m_gravity = gravity; }
    void setAirResistance(double resistance) { m_airResistance = resistance; }
    void setMass(double mass) { m_mass = mass; }
    void setBounciness(double bounciness) { m_bounciness = bounciness; }
    void setMaxBounces(int maxBounces) { m_maxBounces = maxBounces; }
    void setUsePhysics(bool usePhysics) { m_usePhysics = usePhysics; }
    void setVelocity(const Vec2& velocity) { m_velocity = velocity; }

private:
    int m_id;
    Vec2 m_position;
    Vec2 m_direction;     // Normalized direction vector
    Vec2 m_velocity;      // Velocity vector (direction * speed)
    Vec2 m_acceleration;  // Acceleration vector
    double m_speed;       // Initial speed
    double m_damage;
    double m_lifetime;
    double m_maxLifetime;
    bool m_active;
    bool m_hasCollided;
    int m_textureId;
    ProjectileType m_type;
    
    // Physics properties
    double m_gravity;         // Gravity strength (negative for downward)
    double m_airResistance;   // Air resistance coefficient
    double m_mass;            // Mass of projectile
    double m_bounciness;      // Restitution coefficient (0-1)
    int m_bounceCount;        // Current bounce count
    int m_maxBounces;         // Maximum number of bounces before deactivating
    bool m_usePhysics;        // Whether to use physics simulation
    
    // Helper methods for physics
    void applyPhysics(double deltaTime, const Map& map);
    void handleCollision(const Vec2& normal, const Map& map);
    MaterialProperties getMaterialProperties(CellType cellType) const;

    friend class ProjectileManager;
};

// ProjectileManager to handle all projectiles
class ProjectileManager {
public:
    ProjectileManager();
    ~ProjectileManager();
    
    void update(double deltaTime, const Map& map);
    void setSpriteManager(SpriteManager* spriteManager);
    
    int createProjectile(const Vec2& position, const Vec2& direction, ProjectileType type, double speed, double damage);
    void setBulletTexture(int textureId) { m_bulletTextureId = textureId; }
    void setRocketTexture(int textureId) { m_rocketTextureId = textureId; }
    void setPlasmaTexture(int textureId) { m_plasmaTextureId = textureId; }
    void setGrenadeTexture(int textureId) { m_grenadeTextureId = textureId; }
    void setDefaultBulletTexture(int textureId) { m_defaultBulletTexture = textureId; }
    
    // Getters for texture IDs
    int getBulletTextureId() const { return m_bulletTextureId; }
    int getRocketTextureId() const { return m_rocketTextureId; }
    int getPlasmaTextureId() const { return m_plasmaTextureId; }
    int getGrenadeTextureId() const { return m_grenadeTextureId; }
    int getDefaultBulletTextureId() const { return m_defaultBulletTexture; }
    
    const std::vector<Projectile*>& getActiveProjectiles() const { return m_activeProjectiles; }
    Projectile* getProjectile(size_t index);
    int getActiveCount() const;

private:
    int m_nextId;
    int m_bulletTextureId;
    int m_rocketTextureId;
    int m_plasmaTextureId;
    int m_grenadeTextureId;
    int m_defaultBulletTexture;
    SpriteManager* m_spriteManager;
    std::vector<Projectile*> m_activeProjectiles;
};
