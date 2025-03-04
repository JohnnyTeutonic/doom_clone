#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <vector>
#include "utils.h"
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
    Projectile();
    Projectile(double x, double y, double dirX, double dirY, double speed,
               double damage, double maxLifetime, int textureId, ProjectileType type);
    
    void init(double x, double y, double dirX, double dirY, double speed,
              double damage, double maxLifetime, int textureId, ProjectileType type);
    
    void update(double deltaTime, const Map& map);
    
    bool checkMapCollision(const Map& map);
    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    const Vec2& getDirection() const { return m_direction; }
    double getSpeed() const { return m_speed; }
    double getDamage() const { return m_damage; }
    int getTextureId() const { return m_textureId; }
    ProjectileType getType() const;
    bool isActive() const;
    double getLifetime() const;
    bool hasCollided() const;
    
    // Setters
    void setTextureId(int textureId) { m_textureId = textureId; }
    
private:
    friend class ProjectileManager;  // Allow ProjectileManager to access private members
    
    Vec2 m_position;
    Vec2 m_direction;
    double m_speed;
    double m_damage;
    double m_lifetime;
    double m_maxLifetime;
    bool m_active;
    bool m_hasCollided;
    int m_textureId;
    int m_id;
    ProjectileType m_type;
};

// ProjectileManager to handle all projectiles
class ProjectileManager {
public:
    ProjectileManager();
    ~ProjectileManager();
    
    void update(double deltaTime, const Map& map);
    
    // Create a projectile with the given parameters
    int createProjectile(const Vec2& position, const Vec2& direction, 
                          ProjectileType type, double speed, double damage);
    
    // Set texture IDs for different projectile types
    void setBulletTexture(int textureId) { m_bulletTextureId = textureId; }
    void setRocketTexture(int textureId) { m_rocketTextureId = textureId; }
    void setPlasmaTexture(int textureId) { m_plasmaTextureId = textureId; }
    void setDefaultBulletTexture(int textureId) { m_defaultBulletTexture = textureId; }
    
    // Get active projectiles
    std::vector<Projectile*> getActiveProjectiles() const { return m_activeProjectiles; }
    
    // Get a projectile by its index
    Projectile* getProjectile(size_t index);
    
    // Get the number of active projectiles
    int getActiveCount() const;
    
private:
    std::vector<Projectile*> m_activeProjectiles;
    std::vector<Projectile> m_projectiles;  // Keep for compatibility with existing code
    int m_nextId;
    
    // Texture IDs for different projectile types
    int m_bulletTextureId;
    int m_rocketTextureId;
    int m_plasmaTextureId;
    int m_defaultBulletTexture;  // Keep for compatibility
};

#endif // PROJECTILE_H 