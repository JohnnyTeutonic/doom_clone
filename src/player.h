#ifndef PLAYER_H
#define PLAYER_H

#include "utils.h"
#include "map.h"

// Forward declarations
class ProjectileManager;

// Different types of weapons
enum class WeaponType {
    Pistol,
    Shotgun,
    RocketLauncher,
    PlasmaGun
};

class Player {
private:
    Vec2 m_position;      // Player position
    Vec2 m_direction;     // Player view direction (normalized)
    Vec2 m_plane;         // Camera plane (perpendicular to direction)
    double m_moveSpeed;   // Movement speed
    double m_rotSpeed;    // Rotation speed
    double m_health;      // Player health
    int m_ammo;           // Ammo count
    
    // Weapon system
    WeaponType m_currentWeapon;
    double m_weaponDamage;
    double m_weaponCooldown;
    double m_timeSinceLastShot;
    
    // Reference to projectile manager (not owned)
    ProjectileManager* m_projectileManager;
    
public:
    Player();
    
    // Initialize player in specific position and direction
    void init(double x, double y, double dirX, double dirY);
    
    // Update player state
    void update(double deltaTime, const Map& map);
    
    // Movement
    void moveForward(double deltaTime, const Map& map);
    void moveBackward(double deltaTime, const Map& map);
    void strafeLeft(double deltaTime, const Map& map);
    void strafeRight(double deltaTime, const Map& map);
    
    // Rotation
    void rotateLeft(double deltaTime);
    void rotateRight(double deltaTime);
    
    // Weapon/combat
    bool fire();
    void reload();
    void takeDamage(double amount);
    double getWeaponDamage() const { return m_weaponDamage; }
    WeaponType getCurrentWeapon() const { return m_currentWeapon; }
    void setCurrentWeapon(WeaponType weapon);
    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    const Vec2& getDirection() const { return m_direction; }
    const Vec2& getPlane() const { return m_plane; }
    double getHealth() const { return m_health; }
    int getAmmo() const { return m_ammo; }
    ProjectileManager* getProjectileManager() const { return m_projectileManager; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setDirection(const Vec2& direction) { m_direction = direction.normalized(); }
    void setMoveSpeed(double speed) { m_moveSpeed = speed; }
    void setRotSpeed(double speed) { m_rotSpeed = speed; }
    void setHealth(double health) { m_health = health; }
    void setAmmo(int ammo) { m_ammo = ammo; }
    void setProjectileManager(ProjectileManager* manager) { m_projectileManager = manager; }
    
    // Teleport player to a new position (e.g., for level changes)
    void teleport(double x, double y);
};

#endif // PLAYER_H 