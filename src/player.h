#ifndef PLAYER_H
#define PLAYER_H

#include "utils.h"
#include "map.h"

// Forward declarations
class ProjectileManager;
class SpriteManager;

// Different types of weapons
enum class WeaponType {
    Pistol,
    MachineGun,     // Added machine gun
    Shotgun,
    RocketLauncher,
    PlasmaGun,
    GrenadeLauncher,
    Chainsaw,       // New melee weapon
    SuperShotgun,   // Double-barreled shotgun
    BFG9000         // Ultimate area weapon
};

// Power-up types
enum class PowerUpType {
    None,
    Berserk,        // Increases melee damage and turns screen red
    Invulnerability, // Makes player invulnerable for a short time
    RadiationSuit,   // Protects from damaging floors
    Invisibility,    // Makes player partially invisible to enemies
    ComputerMap,     // Reveals the entire map
    LightAmp,        // Increases brightness (night vision)
    MegaSphere       // Full health and armor
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
    
    // Vertical look properties
    double m_verticalAngle;      // Look up/down angle (in radians)
    double m_lookAngle;          // For mouse look only
    double m_verticalLookSpeed;  // Speed of looking up/down
    double m_maxVerticalAngle;   // Maximum up/down look angle (in radians)
    
    // Weapon system
    WeaponType m_currentWeapon;
    double m_weaponDamage;
    double m_weaponCooldown;
    double m_timeSinceLastShot;
    
    // Grenade properties
    int m_grenades;       // Grenade count
    double m_throwPower;  // How hard grenades are thrown
    
    // Reference to managers (not owned)
    ProjectileManager* m_projectileManager;
    SpriteManager* m_spriteManager;
    
    // Armor system
    double m_armor;       // Current armor value
    double m_maxArmor;    // Maximum armor value
    
    // Power-up system
    PowerUpType m_activePowerUp;
    double m_powerUpTimer;
    double m_powerUpDuration;
    
    // Jumping properties
    bool m_isJumping;
    double m_verticalVelocity;
    double m_jumpForce;
    double m_gravity;
    double m_groundLevel;
    double m_jumpHeight;     // Current height of jump (separate from look angle)
    
public:
    Player();
    
    // Initialize player in specific position and direction
    void init(double x, double y, double dirX, double dirY);
    
    // Update player state
    void update(double deltaTime, const Map& map);
    
    // Movement methods
    void moveForward(double deltaTime, const Map& map);
    void moveBackward(double deltaTime, const Map& map);
    void strafeLeft(double deltaTime, const Map& map);
    void strafeRight(double deltaTime, const Map& map);
    
    // Rotation methods
    void rotateLeft(double deltaTime);
    void rotateRight(double deltaTime);
    
    // Look up/down methods
    void lookUp(double deltaTime);
    void lookDown(double deltaTime);
    double getVerticalAngle() const { return m_verticalAngle; }
    
    // Weapon methods
    bool fire();
    bool throwGrenade();  // New method for throwing grenades
    void reload();
    void takeDamage(double amount);
    double getWeaponDamage() const { return m_weaponDamage; }
    WeaponType getCurrentWeapon() const { return m_currentWeapon; }
    void setCurrentWeapon(WeaponType weapon);
    
    // Grenade methods
    int getGrenades() const { return m_grenades; }
    void setGrenades(int count) { m_grenades = count; }
    void addGrenades(int count) { m_grenades += count; }
    double getThrowPower() const { return m_throwPower; }
    void setThrowPower(double power) { m_throwPower = power; }
    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    double getX() const { return m_position.x; }
    double getY() const { return m_position.y; }
    const Vec2& getDirection() const { return m_direction; }
    double getDirX() const { return m_direction.x; }
    double getDirY() const { return m_direction.y; }
    const Vec2& getPlane() const { return m_plane; }
    double getPlaneX() const { return m_plane.x; }
    double getPlaneY() const { return m_plane.y; }
    double getHealth() const { return m_health; }
    int getAmmo() const { return m_ammo; }
    double getJumpHeight() const { return m_jumpHeight; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setDirection(const Vec2& direction) { m_direction = direction.normalized(); }
    void setMoveSpeed(double speed) { m_moveSpeed = speed; }
    void setRotSpeed(double speed) { m_rotSpeed = speed; }
    void setHealth(double health) { m_health = health; }
    void setAmmo(int ammo) { m_ammo = ammo; }
    void setVerticalLookSpeed(double speed) { m_verticalLookSpeed = speed; }
    void setPlane(const Vec2& plane) { m_plane = plane; }
    void setProjectileManager(ProjectileManager* manager) { m_projectileManager = manager; }
    void setSpriteManager(SpriteManager* manager) { m_spriteManager = manager; }
    
    // Teleport player to a new position
    void teleport(double x, double y);
    
    // Armor methods
    double getArmor() const { return m_armor; }
    double getMaxArmor() const { return m_maxArmor; }
    void setArmor(double armor) { m_armor = std::min(armor, m_maxArmor); }
    void addArmor(double amount) { m_armor = std::min(m_armor + amount, m_maxArmor); }
    
    // Power-up methods
    PowerUpType getActivePowerUp() const { return m_activePowerUp; }
    double getPowerUpTimeRemaining() const { return m_powerUpTimer; }
    void activatePowerUp(PowerUpType type, double duration);
    void updatePowerUps(double deltaTime);
    bool hasPowerUp(PowerUpType type) const { return m_activePowerUp == type && m_powerUpTimer > 0; }
    
    // Jump methods
    void jump();
    void updateJump(double deltaTime);
    bool isOnGround() const;

    // Add getter for combined vertical offset
    double getVerticalOffset() const { 
        return m_verticalAngle + m_lookAngle + m_jumpHeight; 
    }

    // Update vertical angle setter to only affect look angle
    void setVerticalAngle(double angle) {
        // Scale for mouse sensitivity and clamp
        double scaledAngle = angle * m_verticalLookSpeed * 0.001;
        // Manual clamping implementation
        if (scaledAngle < -m_maxVerticalAngle) scaledAngle = -m_maxVerticalAngle;
        if (scaledAngle > m_maxVerticalAngle) scaledAngle = m_maxVerticalAngle;
        m_lookAngle = scaledAngle;
    }
};

#endif // PLAYER_H 