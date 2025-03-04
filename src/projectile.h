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
    Plasma
};

class Projectile {
public:
    Projectile(int id, const Vec2& position, const Vec2& direction, double speed, double damage);
    
    void update(double deltaTime, const Map& map);
    bool isActive() const { return m_active; }
    bool hasCollided() const { return m_hasCollided; }
    const Vec2& getPosition() const { return m_position; }
    const Vec2& getDirection() const { return m_direction; }
    double getDamage() const { return m_damage; }
    double getLifetime() const { return m_lifetime; }
    int getId() const { return m_id; }
    int getTextureId() const { return m_textureId; }
    ProjectileType getType() const { return m_type; }

private:
    int m_id;
    Vec2 m_position;
    Vec2 m_direction;
    double m_speed;
    double m_damage;
    double m_lifetime;
    double m_maxLifetime;
    bool m_active;
    bool m_hasCollided;
    int m_textureId;
    ProjectileType m_type;

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
    void setDefaultBulletTexture(int textureId) { m_defaultBulletTexture = textureId; }
    
    const std::vector<Projectile*>& getActiveProjectiles() const { return m_activeProjectiles; }
    Projectile* getProjectile(size_t index);
    int getActiveCount() const;

private:
    int m_nextId;
    int m_bulletTextureId;
    int m_rocketTextureId;
    int m_plasmaTextureId;
    int m_defaultBulletTexture;
    SpriteManager* m_spriteManager;
    std::vector<Projectile*> m_activeProjectiles;
};
