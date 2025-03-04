#include "player.h"
#include "projectile.h"
#include <iostream>
#include <algorithm> // for std::clamp

Player::Player() 
    : m_position(0, 0)
    , m_direction(1, 0)
    , m_plane(0, 0.66)  // FOV is approximately 2 * atan(0.66/1.0) = 66 degrees
    , m_moveSpeed(3.0)
    , m_rotSpeed(2.5)
    , m_health(100)
    , m_ammo(50)
    , m_verticalAngle(0.0)
    , m_verticalLookSpeed(1.5)
    , m_maxVerticalAngle(M_PI / 4.0)  // 45 degrees up/down
    , m_projectileManager(nullptr)
    , m_currentWeapon(WeaponType::Pistol)
    , m_weaponDamage(30.0)
    , m_weaponCooldown(0.5)
    , m_timeSinceLastShot(0.0)
{
}

void Player::init(double x, double y, double dirX, double dirY) {
    m_position = Vec2(x, y);
    m_direction = Vec2(dirX, dirY).normalized();
    
    // Set camera plane perpendicular to direction (for 66 degree FOV)
    m_plane = Vec2(-m_direction.y, m_direction.x) * 0.66;
    
    // Reset vertical angle
    m_verticalAngle = 0.0;
    
    // Reset weapon state
    m_currentWeapon = WeaponType::Pistol;
    m_weaponDamage = 30.0;
    m_weaponCooldown = 0.5;
    m_timeSinceLastShot = 0.0;
}

void Player::update(double deltaTime, const Map& map) {
    // Update weapon cooldown
    if (m_timeSinceLastShot < m_weaponCooldown) {
        m_timeSinceLastShot += deltaTime;
    }
}

void Player::moveForward(double deltaTime, const Map& map) {
    // Calculate new position
    double newX = m_position.x + m_direction.x * m_moveSpeed * deltaTime;
    double newY = m_position.y + m_direction.y * m_moveSpeed * deltaTime;
    
    // Check for valid position with collision detection
    if (map.isValidPosition(newX, m_position.y)) {
        m_position.x = newX;
    }
    if (map.isValidPosition(m_position.x, newY)) {
        m_position.y = newY;
    }
    
    // Check for stairs and stair steps to update elevation and camera angle
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    CellType currentCell = map.getCell(currentX, currentY);
    
    // Handle stairs and stair steps
    if (map.isStairs(currentX, currentY) || map.isStairStep(currentX, currentY)) {
        float currentStepHeight = map.getStepHeight(currentX, currentY);
        
        // Look ahead to see if we should change vertical angle
        int lookAheadX = static_cast<int>(m_position.x + m_direction.x * 0.5);
        int lookAheadY = static_cast<int>(m_position.y + m_direction.y * 0.5);
        CellType lookAheadCell = map.getCell(lookAheadX, lookAheadY);
        
        // Only adjust vertical angle when moving between different step heights
        if (map.isStairStep(lookAheadX, lookAheadY) || map.isStairs(lookAheadX, lookAheadY)) {
            float lookAheadStepHeight = map.getStepHeight(lookAheadX, lookAheadY);
            
            // Calculate a smooth vertical angle based on step height difference
            if (lookAheadStepHeight > currentStepHeight) {
                // Going up
                m_verticalAngle = (lookAheadStepHeight - currentStepHeight) * m_maxVerticalAngle * 0.6;
            } else if (lookAheadStepHeight < currentStepHeight) {
                // Going down
                m_verticalAngle = (lookAheadStepHeight - currentStepHeight) * m_maxVerticalAngle * 0.6;
            }
        }
    } else {
        // When not on stairs, gradually return vertical angle to zero
        if (m_verticalAngle > 0.01) {
            m_verticalAngle -= m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle < 0) m_verticalAngle = 0;
        } else if (m_verticalAngle < -0.01) {
            m_verticalAngle += m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle > 0) m_verticalAngle = 0;
        }
    }
}

void Player::moveBackward(double deltaTime, const Map& map) {
    // Calculate new position
    double newX = m_position.x - m_direction.x * m_moveSpeed * deltaTime;
    double newY = m_position.y - m_direction.y * m_moveSpeed * deltaTime;
    
    // Check for valid position with collision detection
    if (map.isValidPosition(newX, m_position.y)) {
        m_position.x = newX;
    }
    if (map.isValidPosition(m_position.x, newY)) {
        m_position.y = newY;
    }
    
    // Check for stairs and stair steps to update elevation and camera angle
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    CellType currentCell = map.getCell(currentX, currentY);
    
    // Handle stairs and stair steps (going backward)
    if (map.isStairs(currentX, currentY) || map.isStairStep(currentX, currentY)) {
        float currentStepHeight = map.getStepHeight(currentX, currentY);
        
        // Look behind to see if we should change vertical angle
        int lookBehindX = static_cast<int>(m_position.x - m_direction.x * 0.5);
        int lookBehindY = static_cast<int>(m_position.y - m_direction.y * 0.5);
        CellType lookBehindCell = map.getCell(lookBehindX, lookBehindY);
        
        // Only adjust vertical angle when moving between different step heights
        if (map.isStairStep(lookBehindX, lookBehindY) || map.isStairs(lookBehindX, lookBehindY)) {
            float lookBehindStepHeight = map.getStepHeight(lookBehindX, lookBehindY);
            
            // Calculate a smooth vertical angle based on step height difference (reversed since going backward)
            if (lookBehindStepHeight > currentStepHeight) {
                // Going down (when moving backward)
                m_verticalAngle = (currentStepHeight - lookBehindStepHeight) * m_maxVerticalAngle * 0.6;
            } else if (lookBehindStepHeight < currentStepHeight) {
                // Going up (when moving backward)
                m_verticalAngle = (currentStepHeight - lookBehindStepHeight) * m_maxVerticalAngle * 0.6;
            }
        }
    } else {
        // When not on stairs, gradually return vertical angle to zero
        if (m_verticalAngle > 0.01) {
            m_verticalAngle -= m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle < 0) m_verticalAngle = 0;
        } else if (m_verticalAngle < -0.01) {
            m_verticalAngle += m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle > 0) m_verticalAngle = 0;
        }
    }
}

void Player::strafeLeft(double deltaTime, const Map& map) {
    // Calculate new position (perpendicular to direction vector)
    double newX = m_position.x - m_plane.x * m_moveSpeed * deltaTime;
    double newY = m_position.y - m_plane.y * m_moveSpeed * deltaTime;
    
    // Check for valid position with collision detection
    if (map.isValidPosition(newX, m_position.y)) {
        m_position.x = newX;
    }
    if (map.isValidPosition(m_position.x, newY)) {
        m_position.y = newY;
    }
    
    // Check if we're on stairs and update elevation accordingly
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    if (map.isStairs(currentX, currentY)) {
        // Handle elevation changes similarly to forward/backward movement
        int stairElevation = map.getCellElevation(currentX, currentY);
        int lookSideX = static_cast<int>(m_position.x - m_plane.x * 0.5);
        int lookSideY = static_cast<int>(m_position.y - m_plane.y * 0.5);
        int lookSideElevation = map.getCellElevation(lookSideX, lookSideY);
        
        if (lookSideElevation != stairElevation) {
            double targetAngle = (lookSideElevation > stairElevation) ? m_maxVerticalAngle / 2 : -m_maxVerticalAngle / 2;
            m_verticalAngle = targetAngle;
        }
    }
}

void Player::strafeRight(double deltaTime, const Map& map) {
    // Calculate new position (perpendicular to direction vector)
    double newX = m_position.x + m_plane.x * m_moveSpeed * deltaTime;
    double newY = m_position.y + m_plane.y * m_moveSpeed * deltaTime;
    
    // Check for valid position with collision detection
    if (map.isValidPosition(newX, m_position.y)) {
        m_position.x = newX;
    }
    if (map.isValidPosition(m_position.x, newY)) {
        m_position.y = newY;
    }
    
    // Check if we're on stairs and update elevation accordingly
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    if (map.isStairs(currentX, currentY)) {
        // Handle elevation changes similarly to forward/backward movement
        int stairElevation = map.getCellElevation(currentX, currentY);
        int lookSideX = static_cast<int>(m_position.x + m_plane.x * 0.5);
        int lookSideY = static_cast<int>(m_position.y + m_plane.y * 0.5);
        int lookSideElevation = map.getCellElevation(lookSideX, lookSideY);
        
        if (lookSideElevation != stairElevation) {
            double targetAngle = (lookSideElevation > stairElevation) ? m_maxVerticalAngle / 2 : -m_maxVerticalAngle / 2;
            m_verticalAngle = targetAngle;
        }
    }
}

void Player::rotateLeft(double deltaTime) {
    // Rotate direction vector and camera plane
    double rotSpeed = m_rotSpeed * deltaTime;
    double oldDirX = m_direction.x;
    double oldPlaneX = m_plane.x;
    
    // Rotate using a 2D rotation matrix
    double cosRot = cos(rotSpeed);
    double sinRot = sin(rotSpeed);
    
    m_direction.x = m_direction.x * cosRot - m_direction.y * sinRot;
    m_direction.y = oldDirX * sinRot + m_direction.y * cosRot;
    
    m_plane.x = m_plane.x * cosRot - m_plane.y * sinRot;
    m_plane.y = oldPlaneX * sinRot + m_plane.y * cosRot;
}

void Player::rotateRight(double deltaTime) {
    // Rotate direction vector and camera plane
    double rotSpeed = m_rotSpeed * deltaTime;
    double oldDirX = m_direction.x;
    double oldPlaneX = m_plane.x;
    
    // Rotate using a 2D rotation matrix (opposite direction)
    double cosRot = cos(-rotSpeed);
    double sinRot = sin(-rotSpeed);
    
    m_direction.x = m_direction.x * cosRot - m_direction.y * sinRot;
    m_direction.y = oldDirX * sinRot + m_direction.y * cosRot;
    
    m_plane.x = m_plane.x * cosRot - m_plane.y * sinRot;
    m_plane.y = oldPlaneX * sinRot + m_plane.y * cosRot;
}

void Player::lookUp(double deltaTime) {
    // Adjust vertical angle, clamping to prevent looking too far up
    m_verticalAngle += m_verticalLookSpeed * deltaTime;
    if (m_verticalAngle > m_maxVerticalAngle) {
        m_verticalAngle = m_maxVerticalAngle;
    }
}

void Player::lookDown(double deltaTime) {
    // Adjust vertical angle, clamping to prevent looking too far down
    m_verticalAngle -= m_verticalLookSpeed * deltaTime;
    if (m_verticalAngle < -m_maxVerticalAngle) {
        m_verticalAngle = -m_maxVerticalAngle;
    }
}

void Player::setVerticalAngle(double angle) {
    // Clamp the vertical angle to the allowed range
    m_verticalAngle = std::max(-m_maxVerticalAngle, std::min(angle, m_maxVerticalAngle));
}

void Player::setCurrentWeapon(WeaponType weapon) {
    m_currentWeapon = weapon;
    
    // Update weapon properties based on type
    switch (m_currentWeapon) {
        case WeaponType::Pistol:
            m_weaponDamage = 30.0;
            m_weaponCooldown = 0.5;
            break;
            
        case WeaponType::Shotgun:
            m_weaponDamage = 45.0;
            m_weaponCooldown = 0.8;
            break;
            
        case WeaponType::RocketLauncher:
            m_weaponDamage = 100.0;
            m_weaponCooldown = 1.2;
            break;
            
        case WeaponType::PlasmaGun:
            m_weaponDamage = 25.0;
            m_weaponCooldown = 0.2;
            break;
    }
}

bool Player::fire() {
    if (!m_projectileManager) {
        std::cerr << "[FIRE ERROR] Player has no projectile manager!" << std::endl;
        return false;
    }
    
    if (m_ammo <= 0) {
        std::cout << "[FIRE] No ammo left!" << std::endl;
        return false;
    }
    
    if (m_timeSinceLastShot < m_weaponCooldown) {
        std::cout << "[FIRE] Weapon still cooling down!" << std::endl;
        return false;
    }
    
    std::cout << "=======================================" << std::endl;
    std::cout << "[FIRE] Player firing weapon!" << std::endl;
    std::cout << "  Position: (" << m_position.x << ", " << m_position.y << ")" << std::endl;
    std::cout << "  Direction: (" << m_direction.x << ", " << m_direction.y << ")" << std::endl;
    std::cout << "  Current weapon: " << static_cast<int>(m_currentWeapon) << std::endl;
    std::cout << "  Ammo remaining: " << m_ammo << std::endl;
    
    m_ammo--;
    m_timeSinceLastShot = 0.0;  // Reset cooldown
    
    // Starting position directly in front of player
    Vec2 bulletPos = m_position;
    
    // Offset bullet start position significantly forward for visibility
    double offsetDistance = 1.0; // Increased from 0.5 to ensure bullets aren't inside walls
    bulletPos.x += m_direction.x * offsetDistance;
    bulletPos.y += m_direction.y * offsetDistance;
    
    std::cout << "  Bullet start position: (" << bulletPos.x << ", " << bulletPos.y << ")" << std::endl;
    
    // Fire 3 bullets with slight spread for improved visibility
    bool success = false;
    
    // The spread amount controls how much the bullets deviate
    double spreadAmount = 0.1;
    
    std::cout << "  Firing multiple bullets with spread: " << spreadAmount << std::endl;
    
    // Center bullet (no spread)
    bool centerBulletSuccess = m_projectileManager->createProjectile(
        bulletPos,
        m_direction,
        ProjectileType::Bullet,
        15.0,  // Increased speed
        m_weaponDamage
    ) >= 0;
    
    std::cout << "  Center bullet created: " << (centerBulletSuccess ? "SUCCESS" : "FAILED") << std::endl;
    success |= centerBulletSuccess;
    
    // Left spread bullet
    Vec2 leftDir = m_direction;
    leftDir.rotate(-spreadAmount);
    bool leftBulletSuccess = m_projectileManager->createProjectile(
        bulletPos,
        leftDir,
        ProjectileType::Bullet,
        15.0,
        m_weaponDamage
    ) >= 0;
    
    std::cout << "  Left bullet created: " << (leftBulletSuccess ? "SUCCESS" : "FAILED") << std::endl;
    success |= leftBulletSuccess;
    
    // Right spread bullet
    Vec2 rightDir = m_direction;
    rightDir.rotate(spreadAmount);
    bool rightBulletSuccess = m_projectileManager->createProjectile(
        bulletPos,
        rightDir,
        ProjectileType::Bullet,
        15.0,
        m_weaponDamage
    ) >= 0;
    
    std::cout << "  Right bullet created: " << (rightBulletSuccess ? "SUCCESS" : "FAILED") << std::endl;
    success |= rightBulletSuccess;
    
    std::cout << "  Overall firing result: " << (success ? "SUCCESS" : "ALL BULLETS FAILED") << std::endl;
    std::cout << "  Ammo now: " << m_ammo << std::endl;
    std::cout << "=======================================" << std::endl;
    
    return success;
}

void Player::reload() {
    // A very simplified reload mechanism
    m_ammo = 50;  // Reset to max ammo
}

void Player::takeDamage(double amount) {
    m_health -= amount;
    if (m_health < 0) {
        m_health = 0;
    }
}

void Player::teleport(double x, double y) {
    m_position.x = x;
    m_position.y = y;
} 