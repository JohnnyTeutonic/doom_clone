#include "player.h"
#include "projectile.h"
#include <iostream>
#include <algorithm> // for std::clamp
#include <cmath>

Player::Player() 
    : m_position(2, 2)
    , m_direction(1, 0)
    , m_plane(0, 0.66)  // FOV is approximately 2 * atan(0.66/1.0) = 66 degrees
    , m_moveSpeed(5.0)
    , m_rotSpeed(3.0)
    , m_health(100.0)
    , m_ammo(10)
    , m_verticalAngle(0.0)  // For stairs and world effects
    , m_lookAngle(0.0)      // For mouse look
    , m_verticalLookSpeed(3.0)
    , m_maxVerticalAngle(M_PI / 4.0)  // 45 degrees up/down
    , m_projectileManager(nullptr)
    , m_spriteManager(nullptr)
    , m_currentWeapon(WeaponType::Pistol)
    , m_weaponDamage(25.0)
    , m_weaponCooldown(0.2)
    , m_timeSinceLastShot(0.0)
    , m_grenades(3)        // Start with 3 grenades
    , m_throwPower(10.0)   // Default throw power
    , m_armor(0.0)         // Start with no armor
    , m_maxArmor(100.0)    // Maximum armor value
    , m_activePowerUp(PowerUpType::None)
    , m_powerUpTimer(0.0)
    , m_powerUpDuration(0.0)
    , m_isJumping(false)
    , m_verticalVelocity(0.0)
    , m_jumpForce(8.0)     // DOOM-style jump force - less bouncy, more controlled
    , m_gravity(20.0)      // DOOM-style gravity - gentler for a more predictable arc
    , m_groundLevel(0.0)   // Ground level reference
    , m_jumpHeight(0.0)    // Initialize jump height
{
}

void Player::init(double x, double y, double dirX, double dirY) {
    m_position = Vec2(x, y);
    m_direction = Vec2(dirX, dirY).normalized();
    
    // Set camera plane perpendicular to direction (for 66 degree FOV)
    m_plane = Vec2(-m_direction.y, m_direction.x) * 0.66;
    
    // Reset angles and jumping state
    m_verticalAngle = 0.0;
    m_lookAngle = 0.0;
    m_verticalVelocity = 0.0;
    m_isJumping = false;
    
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
    
    // Update power-ups
    updatePowerUps(deltaTime);
    
    // Update jumping physics
    updateJump(deltaTime);
    
    // Check for nearby items to pick up
    checkNearbyItems();
    
    // Debug output for jump state
    if (m_isJumping || m_verticalVelocity != 0.0) {
        std::cout << "Jump State - IsJumping: " << m_isJumping 
                  << ", Velocity: " << m_verticalVelocity 
                  << ", Position: " << m_verticalAngle << std::endl;
    }
}

void Player::moveForward(double deltaTime, const Map& map) {
    // Calculate new position
    double newX = m_position.x + m_direction.x * m_moveSpeed * deltaTime;
    double newY = m_position.y + m_direction.y * m_moveSpeed * deltaTime;
    
    // Get current cell data
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    CellType currentCell = map.getCell(currentX, currentY);
    int currentElevation = map.getCellElevation(currentX, currentY);
    
    // Calculate new cell data
    int newCellX = static_cast<int>(newX);
    int newCellY = static_cast<int>(newY);
    CellType newCellType = map.getCell(newCellX, newCellY);
    int newCellElevation = map.getCellElevation(newCellX, newCellY);
    
    // Special handling for stairs transitions
    bool onStairs = (currentCell == CellType::Stairs || currentCell == CellType::StairStep1 || 
                     currentCell == CellType::StairStep2 || currentCell == CellType::StairStep3);
    
    bool movingToStairs = (newCellType == CellType::Stairs || newCellType == CellType::StairStep1 || 
                          newCellType == CellType::StairStep2 || newCellType == CellType::StairStep3);
    
    // Determine if we're crossing a level boundary
    bool crossingElevation = (currentElevation != newCellElevation) &&
                            !(onStairs || movingToStairs);
    
    // Debug output for elevation changes
    if (currentElevation != newCellElevation) {
        std::cout << "Elevation change detected: " << currentElevation << " -> " << newCellElevation 
                  << " (onStairs: " << onStairs << ", movingToStairs: " << movingToStairs << ")" << std::endl;
    }
    
    // Check for valid position with collision detection
    // If we're crossing elevation without stairs, block movement
    if (crossingElevation) {
        // Don't allow crossing elevation without stairs
        std::cout << "Blocked movement across elevation boundary" << std::endl;
    } else {
        // Normal collision detection
        bool validX = map.isValidPosition(newX, m_position.y);
        bool validY = map.isValidPosition(m_position.x, newY);
        
        // Check if the new position maintains the same elevation (unless on stairs)
        if (validX) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newXElevation = map.getCellElevation(static_cast<int>(newX), static_cast<int>(m_position.y));
            if (newXElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(newX), static_cast<int>(m_position.y))) {
                m_position.x = newX;
            } else {
                std::cout << "Blocked X movement due to elevation change" << std::endl;
            }
        }
        
        if (validY) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newYElevation = map.getCellElevation(static_cast<int>(m_position.x), static_cast<int>(newY));
            if (newYElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(m_position.x), static_cast<int>(newY))) {
                m_position.y = newY;
            } else {
                std::cout << "Blocked Y movement due to elevation change" << std::endl;
            }
        }
    }
    
    // Update current position after movement
    currentX = static_cast<int>(m_position.x);
    currentY = static_cast<int>(m_position.y);
    currentCell = map.getCell(currentX, currentY);
    currentElevation = map.getCellElevation(currentX, currentY);
    
    // Handle stair and step transitions
    if (currentCell == CellType::Stairs) {
        // At a stair entry/exit point - determine if going up or down
        int stairElevation = map.getCellElevation(currentX, currentY);
        
        // Look ahead to determine if we're going up or down
        int lookAheadX = static_cast<int>(m_position.x + m_direction.x);
        int lookAheadY = static_cast<int>(m_position.y + m_direction.y);
        int lookAheadElevation = map.getCellElevation(lookAheadX, lookAheadY);
        
        // Update the vertical angle to indicate level change
        if (lookAheadElevation > stairElevation) {
            // Going up
            m_verticalAngle = m_maxVerticalAngle * 0.5;
            std::cout << "Going up stairs" << std::endl;
        } else if (lookAheadElevation < stairElevation) {
            // Going down
            m_verticalAngle = -m_maxVerticalAngle * 0.5;
            std::cout << "Going down stairs" << std::endl;
        }
    }
    else if (map.isStairStep(currentX, currentY)) {
        // On a stair step - update vertical angle based on step height
        float currentStepHeight = map.getStepHeight(currentX, currentY);
        
        // Look ahead to see which direction we're going
        int lookAheadX = static_cast<int>(m_position.x + m_direction.x * 0.5);
        int lookAheadY = static_cast<int>(m_position.y + m_direction.y * 0.5);
        
        if (map.isStairStep(lookAheadX, lookAheadY)) {
            float lookAheadStepHeight = map.getStepHeight(lookAheadX, lookAheadY);
            
            // Smoothly transition based on step height difference
            if (fabs(lookAheadStepHeight - currentStepHeight) > 0.01) {
                float angleFactor = (lookAheadStepHeight - currentStepHeight) * 2.0;
                m_verticalAngle = angleFactor * m_maxVerticalAngle;
            }
        }
    }
    else {
        // Not on stairs - determine angle based on elevation
        int elevation = map.getCellElevation(currentX, currentY);
        
        // Gradually return vertical angle to neutral
        if (m_verticalAngle > 0.01) {
            m_verticalAngle -= m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle < 0) m_verticalAngle = 0;
        } else if (m_verticalAngle < -0.01) {
            m_verticalAngle += m_verticalLookSpeed * deltaTime * 2.0;
            if (m_verticalAngle > 0) m_verticalAngle = 0;
        }
    }
    
    // Debug output for current position and elevation
    std::cout << "Player position: (" << m_position.x << ", " << m_position.y 
              << "), Elevation: " << currentElevation << std::endl;
}

void Player::moveBackward(double deltaTime, const Map& map) {
    // Calculate new position
    double newX = m_position.x - m_direction.x * m_moveSpeed * deltaTime;
    double newY = m_position.y - m_direction.y * m_moveSpeed * deltaTime;
    
    // Get current cell data
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    CellType currentCell = map.getCell(currentX, currentY);
    int currentElevation = map.getCellElevation(currentX, currentY);
    
    // Calculate new cell data
    int newCellX = static_cast<int>(newX);
    int newCellY = static_cast<int>(newY);
    CellType newCellType = map.getCell(newCellX, newCellY);
    int newCellElevation = map.getCellElevation(newCellX, newCellY);
    
    // Special handling for stairs transitions
    bool onStairs = (currentCell == CellType::Stairs || currentCell == CellType::StairStep1 || 
                     currentCell == CellType::StairStep2 || currentCell == CellType::StairStep3);
    
    bool movingToStairs = (newCellType == CellType::Stairs || newCellType == CellType::StairStep1 || 
                          newCellType == CellType::StairStep2 || newCellType == CellType::StairStep3);
    
    // Determine if we're crossing a level boundary
    bool crossingElevation = (currentElevation != newCellElevation) &&
                            !(onStairs || movingToStairs);
    
    // Check for valid position with collision detection
    // If we're crossing elevation without stairs, block movement
    if (crossingElevation) {
        // Don't allow crossing elevation without stairs
        std::cout << "Blocked backward movement across elevation boundary" << std::endl;
    } else {
        // Normal collision detection
        bool validX = map.isValidPosition(newX, m_position.y);
        bool validY = map.isValidPosition(m_position.x, newY);
        
        // Check if the new position maintains the same elevation (unless on stairs)
        if (validX) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newXElevation = map.getCellElevation(static_cast<int>(newX), static_cast<int>(m_position.y));
            if (newXElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(newX), static_cast<int>(m_position.y))) {
                m_position.x = newX;
            } else {
                std::cout << "Blocked backward X movement due to elevation change" << std::endl;
            }
        }
        
        if (validY) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newYElevation = map.getCellElevation(static_cast<int>(m_position.x), static_cast<int>(newY));
            if (newYElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(m_position.x), static_cast<int>(newY))) {
                m_position.y = newY;
            } else {
                std::cout << "Blocked backward Y movement due to elevation change" << std::endl;
            }
        }
    }
    
    // Update current position after movement
    currentX = static_cast<int>(m_position.x);
    currentY = static_cast<int>(m_position.y);
    currentCell = map.getCell(currentX, currentY);
    
    // Handle stair and step transitions
    if (currentCell == CellType::Stairs) {
        // At a stair entry/exit point - determine if going up or down
        int stairElevation = map.getCellElevation(currentX, currentY);
        
        // When moving backward, look behind to determine direction
        int lookBehindX = static_cast<int>(m_position.x - m_direction.x);
        int lookBehindY = static_cast<int>(m_position.y - m_direction.y);
        int lookBehindElevation = map.getCellElevation(lookBehindX, lookBehindY);
        
        // Update the vertical angle to indicate level change (reversed for backward)
        if (lookBehindElevation > stairElevation) {
            // Going down when walking backward
            m_verticalAngle = -m_maxVerticalAngle * 0.5;
            std::cout << "Going down stairs (backward)" << std::endl;
        } else if (lookBehindElevation < stairElevation) {
            // Going up when walking backward
            m_verticalAngle = m_maxVerticalAngle * 0.5;
            std::cout << "Going up stairs (backward)" << std::endl;
        }
    }
    else if (map.isStairStep(currentX, currentY)) {
        // On a stair step - update vertical angle based on step height
        float currentStepHeight = map.getStepHeight(currentX, currentY);
        
        // Look behind to determine direction
        int lookBehindX = static_cast<int>(m_position.x - m_direction.x * 0.5);
        int lookBehindY = static_cast<int>(m_position.y - m_direction.y * 0.5);
        
        if (map.isStairStep(lookBehindX, lookBehindY)) {
            float lookBehindStepHeight = map.getStepHeight(lookBehindX, lookBehindY);
            
            // Reversed angle calculation since going backward
            if (fabs(lookBehindStepHeight - currentStepHeight) > 0.01) {
                float angleFactor = (currentStepHeight - lookBehindStepHeight) * 2.0;
                m_verticalAngle = angleFactor * m_maxVerticalAngle;
            }
        }
    }
    else {
        // Not on stairs - determine angle based on elevation
        int elevation = map.getCellElevation(currentX, currentY);
        
        // Gradually return vertical angle to neutral
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
    
    // Get current cell data
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    int currentElevation = map.getCellElevation(currentX, currentY);
    CellType currentCell = map.getCell(currentX, currentY);
    
    // Calculate new cell data
    int newCellX = static_cast<int>(newX);
    int newCellY = static_cast<int>(newY);
    int newCellElevation = map.getCellElevation(newCellX, newCellY);
    CellType newCellType = map.getCell(newCellX, newCellY);
    
    // Special handling for stairs transitions
    bool onStairs = (currentCell == CellType::Stairs || currentCell == CellType::StairStep1 || 
                     currentCell == CellType::StairStep2 || currentCell == CellType::StairStep3);
    
    bool movingToStairs = (newCellType == CellType::Stairs || newCellType == CellType::StairStep1 || 
                          newCellType == CellType::StairStep2 || newCellType == CellType::StairStep3);
    
    // Determine if we're crossing a level boundary
    bool crossingElevation = (currentElevation != newCellElevation) &&
                            !(onStairs || movingToStairs);
    
    // Check for valid position with collision detection
    if (crossingElevation) {
        // Don't allow crossing elevation without stairs
        std::cout << "Blocked strafe left movement across elevation boundary" << std::endl;
    } else {
        // Normal collision detection
        bool validX = map.isValidPosition(newX, m_position.y);
        bool validY = map.isValidPosition(m_position.x, newY);
        
        // Check if the new position maintains the same elevation (unless on stairs)
        if (validX) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newXElevation = map.getCellElevation(static_cast<int>(newX), static_cast<int>(m_position.y));
            if (newXElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(newX), static_cast<int>(m_position.y))) {
                m_position.x = newX;
            }
        }
        
        if (validY) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newYElevation = map.getCellElevation(static_cast<int>(m_position.x), static_cast<int>(newY));
            if (newYElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(m_position.x), static_cast<int>(newY))) {
                m_position.y = newY;
            }
        }
    }
    
    // Check if we're on stairs and update elevation accordingly
    currentX = static_cast<int>(m_position.x);
    currentY = static_cast<int>(m_position.y);
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
    
    // Get current cell data
    int currentX = static_cast<int>(m_position.x);
    int currentY = static_cast<int>(m_position.y);
    int currentElevation = map.getCellElevation(currentX, currentY);
    CellType currentCell = map.getCell(currentX, currentY);
    
    // Calculate new cell data
    int newCellX = static_cast<int>(newX);
    int newCellY = static_cast<int>(newY);
    int newCellElevation = map.getCellElevation(newCellX, newCellY);
    CellType newCellType = map.getCell(newCellX, newCellY);
    
    // Special handling for stairs transitions
    bool onStairs = (currentCell == CellType::Stairs || currentCell == CellType::StairStep1 || 
                     currentCell == CellType::StairStep2 || currentCell == CellType::StairStep3);
    
    bool movingToStairs = (newCellType == CellType::Stairs || newCellType == CellType::StairStep1 || 
                          newCellType == CellType::StairStep2 || newCellType == CellType::StairStep3);
    
    // Determine if we're crossing a level boundary
    bool crossingElevation = (currentElevation != newCellElevation) &&
                            !(onStairs || movingToStairs);
    
    // Check for valid position with collision detection
    if (crossingElevation) {
        // Don't allow crossing elevation without stairs
        std::cout << "Blocked strafe right movement across elevation boundary" << std::endl;
    } else {
        // Normal collision detection
        bool validX = map.isValidPosition(newX, m_position.y);
        bool validY = map.isValidPosition(m_position.x, newY);
        
        // Check if the new position maintains the same elevation (unless on stairs)
        if (validX) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newXElevation = map.getCellElevation(static_cast<int>(newX), static_cast<int>(m_position.y));
            if (newXElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(newX), static_cast<int>(m_position.y))) {
                m_position.x = newX;
            }
        }
        
        if (validY) {
            // Only move if we're not crossing elevation boundaries without stairs
            int newYElevation = map.getCellElevation(static_cast<int>(m_position.x), static_cast<int>(newY));
            if (newYElevation == currentElevation || onStairs || map.isStairs(static_cast<int>(m_position.x), static_cast<int>(newY))) {
                m_position.y = newY;
            }
        }
    }
    
    // Check if we're on stairs and update elevation accordingly
    currentX = static_cast<int>(m_position.x);
    currentY = static_cast<int>(m_position.y);
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
    // For mouse movement, we don't want to multiply by deltaTime
    // as mouse input already gives us a delta
    m_lookAngle += m_verticalLookSpeed;
    
    // Clamp to maximum look angle
    if (m_lookAngle > m_maxVerticalAngle) {
        m_lookAngle = m_maxVerticalAngle;
    }
}

void Player::lookDown(double deltaTime) {
    // For mouse movement, we don't want to multiply by deltaTime
    // as mouse input already gives us a delta
    m_lookAngle -= m_verticalLookSpeed;
    
    // Clamp to maximum look angle
    if (m_lookAngle < -m_maxVerticalAngle) {
        m_lookAngle = -m_maxVerticalAngle;
    }
}

void Player::setCurrentWeapon(WeaponType weapon) {
    m_currentWeapon = weapon;
    
    // Set weapon properties based on type
    switch (weapon) {
        case WeaponType::Pistol:
            m_weaponDamage = 30.0;
            m_weaponCooldown = 0.4;
            break;
            
        case WeaponType::MachineGun:
            m_weaponDamage = 20.0;
            m_weaponCooldown = 0.1;  // Rapid fire
            break;
            
        case WeaponType::Shotgun:
            m_weaponDamage = 45.0;
            m_weaponCooldown = 0.8;
            break;
            
        case WeaponType::RocketLauncher:
            m_weaponDamage = 100.0;
            m_weaponCooldown = 1.0;
            break;
            
        case WeaponType::PlasmaGun:
            m_weaponDamage = 25.0;
            m_weaponCooldown = 0.1;  // Rapid fire
            break;
            
        case WeaponType::GrenadeLauncher:
            m_weaponDamage = 75.0;
            m_weaponCooldown = 1.2;
            break;
            
        case WeaponType::Chainsaw:
            m_weaponDamage = 40.0;
            m_weaponCooldown = 0.1;  // Continuous damage
            break;
            
        case WeaponType::SuperShotgun:
            m_weaponDamage = 90.0;   // Double the damage of regular shotgun
            m_weaponCooldown = 1.2;  // Slower reload
            break;
            
        case WeaponType::BFG9000:
            m_weaponDamage = 200.0;  // Massive damage
            m_weaponCooldown = 3.0;  // Long cooldown
            break;
            
        default:
            m_weaponDamage = 25.0;
            m_weaponCooldown = 0.5;
            break;
    }
}

bool Player::fire() {
    if (!m_projectileManager) {
        std::cerr << "No projectile manager in Player::fire!" << std::endl;
        return false;
    }
    
    // Hard limit to 10 shots total regardless of ammo value
    static int& totalShotsFired = getTotalShotsFired();
    if (totalShotsFired >= 10) {
        std::cout << "HARD LIMIT: Maximum 10 shots allowed. Total shots fired: " << totalShotsFired << std::endl;
        return false;
    }
    
    if (m_timeSinceLastShot < m_weaponCooldown) {
        std::cout << "COOLDOWN: Can't fire yet, cooldown still active." << std::endl;
        return false;
    }
    
    std::cout << "AMMO CHECK: Current ammo before firing: " << m_ammo << std::endl;
    
    if (m_ammo <= 0) {
        std::cout << "Out of ammo!" << std::endl;
        return false;
    }
    
    std::cout << "=======================================" << std::endl;
    std::cout << "[FIRE] Player firing weapon!" << std::endl;
    std::cout << "  Position: (" << m_position.x << ", " << m_position.y << ")" << std::endl;
    std::cout << "  Direction: (" << m_direction.x << ", " << m_direction.y << ")" << std::endl;
    
    // Convert weapon enum to string for better debugging
    std::string weaponName;
    switch (m_currentWeapon) {
        case WeaponType::Pistol: weaponName = "Pistol"; break;
        case WeaponType::MachineGun: weaponName = "Machine Gun"; break;
        case WeaponType::Shotgun: weaponName = "Shotgun"; break;
        case WeaponType::RocketLauncher: weaponName = "Rocket Launcher"; break;
        case WeaponType::PlasmaGun: weaponName = "Plasma Gun"; break;
        case WeaponType::GrenadeLauncher: weaponName = "Grenade Launcher"; break;
        case WeaponType::Chainsaw: weaponName = "Chainsaw"; break;
        case WeaponType::SuperShotgun: weaponName = "Super Shotgun"; break;
        case WeaponType::BFG9000: weaponName = "BFG9000"; break;
        default: weaponName = "Unknown"; break;
    }
    
    std::cout << "  Current weapon: " << weaponName << " (ID: " << static_cast<int>(m_currentWeapon) << ")" << std::endl;
    std::cout << "  Weapon damage: " << m_weaponDamage << std::endl;
    std::cout << "  Ammo remaining: " << m_ammo << std::endl;
    
    // Always decrement ammo and reset cooldown, even if projectile creation fails
    m_ammo--;
    totalShotsFired++;
    std::cout << "AMMO CONSUMED: Decremented ammo to: " << m_ammo << ". Total shots fired: " << totalShotsFired << std::endl;
    m_timeSinceLastShot = 0.0;  // Reset cooldown
    
    // Get nearby sprites for close-range hit detection
    std::vector<Sprite*> sprites;
    if (m_spriteManager) {
        sprites = m_spriteManager->getActiveSprites();
    }
    
    // Check for close-range hits first (hitscan for very close enemies)
    bool hitCloseEnemy = false;
    const double CLOSE_RANGE = 2.5; // Increased from 1.5 to 2.5 for better close-range detection
    
    // Sort sprites by distance to player (closest first)
    std::sort(sprites.begin(), sprites.end(), [this](Sprite* a, Sprite* b) {
        double distA = (a->getPosition() - m_position).length();
        double distB = (b->getPosition() - m_position).length();
        return distA < distB;
    });
    
    // Check for close enemies in front of the player
    for (Sprite* sprite : sprites) {
        if (!sprite || sprite->isDying() || !sprite->isActive()) continue;
        
        // Calculate vector from player to sprite
        Vec2 toSprite = sprite->getPosition() - m_position;
        double distToSprite = toSprite.length();
        
        // Skip if too far away
        if (distToSprite > CLOSE_RANGE) continue;
        
        // Calculate dot product to check if sprite is in front of player
        double dotProduct = m_direction.x * toSprite.x + m_direction.y * toSprite.y;
        
        // Normalize by the length of toSprite to get the actual cosine
        double cosAngle = dotProduct / distToSprite;
        
        // Check if sprite is within a 90-degree cone in front of player (cos(45°) ≈ 0.707)
        if (cosAngle > 0.707) {
            // Hit the close enemy directly!
            double damage = m_weaponDamage;
            
            // Apply weapon-specific damage
            if (m_currentWeapon == WeaponType::Shotgun) {
                // Shotguns do more damage at close range
                damage *= 1.5;
            }
            
            sprite->takeDamage(damage);
            std::cout << "Direct hit on close enemy! Damage: " << damage << std::endl;
            hitCloseEnemy = true;
            
            // For shotgun, we might hit multiple enemies, so don't break
            if (m_currentWeapon != WeaponType::Shotgun) {
                break;
            }
        }
    }
    
    // Starting position for projectiles
    Vec2 bulletPos = m_position;
    
    // Adjust bullet start position based on closest enemy
    double offsetDistance = 0.5; // Default offset
    
    // If there's a close enemy, reduce the offset to avoid shooting through them
    if (!sprites.empty()) {
        Sprite* closestSprite = sprites[0];
        double closestDist = (closestSprite->getPosition() - m_position).length();
        
        if (closestDist < 1.0) {
            // Reduce offset for very close enemies
            offsetDistance = std::min(offsetDistance, closestDist * 0.5);
        }
    }
    
    // Apply the offset
    bulletPos.x += m_direction.x * offsetDistance;
    bulletPos.y += m_direction.y * offsetDistance;
    
    
    bool success = false;
    
    // For close-range hits, we might not need to create projectiles
    // But we'll still create them for visual effect unless it's a shotgun
    if (hitCloseEnemy && m_currentWeapon == WeaponType::Shotgun) {
        // For shotgun, we'll skip creating projectiles if we hit something at close range
        return true;
    }
    
    // Handle different weapon types
    switch (m_currentWeapon) {
        case WeaponType::Pistol:
        {
            // Single bullet, no spread
            success = m_projectileManager->createProjectile(
                bulletPos,
                m_direction,
                ProjectileType::Bullet,
                20.0,
                m_weaponDamage
            ) >= 0;
            break;
        }
        
        case WeaponType::MachineGun:
        {
            // Rapid fire with slight spread
            double spreadAmount = 0.03; // Small spread
            
            // Add a small random spread
            Vec2 spreadDir = m_direction;
            double randomAngle = (rand() % 100 - 50) / 1000.0; // -0.05 to 0.05
            spreadDir.rotate(randomAngle);
            
            success = m_projectileManager->createProjectile(
                bulletPos,
                spreadDir,
                ProjectileType::Bullet,
                25.0,
                m_weaponDamage
            ) >= 0;
            break;
        }
        
        case WeaponType::Shotgun:
        {
            // Multiple bullets with spread
            double spreadAmount = 0.15;
            
            // Center bullet
            bool centerBulletSuccess = m_projectileManager->createProjectile(
                bulletPos,
                m_direction,
                ProjectileType::Bullet,
                15.0,
                m_weaponDamage
            ) >= 0;
            success |= centerBulletSuccess;
            
            // Create 4 spread bullets (2 left, 2 right)
            for (int i = 1; i <= 2; i++) {
                // Left spread
                Vec2 leftDir = m_direction;
                leftDir.rotate(-spreadAmount * i);
                bool leftSuccess = m_projectileManager->createProjectile(
                    bulletPos,
                    leftDir,
                    ProjectileType::Bullet,
                    15.0,
                    m_weaponDamage * 0.7  // Reduced damage for spread bullets
                ) >= 0;
                success |= leftSuccess;
                
                // Right spread
                Vec2 rightDir = m_direction;
                rightDir.rotate(spreadAmount * i);
                bool rightSuccess = m_projectileManager->createProjectile(
                    bulletPos,
                    rightDir,
                    ProjectileType::Bullet,
                    15.0,
                    m_weaponDamage * 0.7
                ) >= 0;
                success |= rightSuccess;
            }
            break;
        }
        
        case WeaponType::SuperShotgun:
        {
            // Super shotgun has wider spread and more pellets
            double spreadAmount = 0.2;
            
            // Center bullet
            bool centerBulletSuccess = m_projectileManager->createProjectile(
                bulletPos,
                m_direction,
                ProjectileType::Bullet,
                15.0,
                m_weaponDamage * 0.5
            ) >= 0;
            success |= centerBulletSuccess;
            
            // Create 8 spread bullets (4 left, 4 right)
            for (int i = 1; i <= 4; i++) {
                // Left spread
                Vec2 leftDir = m_direction;
                leftDir.rotate(-spreadAmount * i);
                bool leftSuccess = m_projectileManager->createProjectile(
                    bulletPos,
                    leftDir,
                    ProjectileType::Bullet,
                    15.0,
                    m_weaponDamage * 0.3  // Reduced damage for spread bullets
                ) >= 0;
                success |= leftSuccess;
                
                // Right spread
                Vec2 rightDir = m_direction;
                rightDir.rotate(spreadAmount * i);
                bool rightSuccess = m_projectileManager->createProjectile(
                    bulletPos,
                    rightDir,
                    ProjectileType::Bullet,
                    15.0,
                    m_weaponDamage * 0.3
                ) >= 0;
                success |= rightSuccess;
            }
            break;
        }
        
        case WeaponType::RocketLauncher:
        {
            if (m_projectileManager) {
                // Create a rocket projectile
                int rocketId = m_projectileManager->createProjectile(
                    m_position + m_direction * 0.5, // Start position slightly in front of player
                    m_direction,
                    ProjectileType::Rocket,
                    8.0,  // Reduced speed for better visibility before collision
                    m_weaponDamage
                );
                
                // Debug output for rocket creation
                std::cout << "Created rocket projectile (ID: " << rocketId << ")" << std::endl;
                std::cout << "  Position: (" << (m_position.x + m_direction.x * 0.5) << ", " 
                          << (m_position.y + m_direction.y * 0.5) << ")" << std::endl;
                std::cout << "  Direction: (" << m_direction.x << ", " << m_direction.y << ")" << std::endl;
                std::cout << "  Speed: 8.0" << std::endl;
                std::cout << "  Damage: " << m_weaponDamage << std::endl;
                
                if (rocketId >= 0) {
                    std::cout << "Rocket projectile created successfully!" << std::endl;
                    return true;
                } else {
                    std::cerr << "ERROR: Failed to create rocket projectile!" << std::endl;
                    return false;
                }
            }
            return false;
        }
        
        case WeaponType::PlasmaGun:
        {
            // Rapid fire plasma bolts
            double spreadAmount = 0.05; // Slight spread
            
            // Add a small random spread
            Vec2 spreadDir = m_direction;
            double randomAngle = (rand() % 100 - 50) / 500.0; // -0.1 to 0.1
            spreadDir.rotate(randomAngle);
            
            success = m_projectileManager->createProjectile(
                bulletPos,
                spreadDir,
                ProjectileType::Plasma,
                25.0,
                m_weaponDamage
            ) >= 0;
            break;
        }
        
        case WeaponType::Chainsaw:
        {
            // Chainsaw is a melee weapon - check for close enemies
            const double CHAINSAW_RANGE = 1.0;
            bool hitEnemy = false;
            
            // Ensure even melee weapons consume ammo
            std::cout << "CHAINSAW: Consuming ammo for chainsaw attack." << std::endl;
            
            if (m_spriteManager) {
                std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
                
                for (Sprite* sprite : sprites) {
                    if (!sprite || sprite->isDying() || !sprite->isActive()) continue;
                    
                    // Calculate vector from player to sprite
                    Vec2 toSprite = sprite->getPosition() - m_position;
                    double distToSprite = toSprite.length();
                    
                    // Skip if too far away
                    if (distToSprite > CHAINSAW_RANGE) continue;
                    
                    // Calculate dot product to check if sprite is in front of player
                    double dotProduct = m_direction.x * toSprite.x + m_direction.y * toSprite.y;
                    double cosAngle = dotProduct / distToSprite;
                    
                    // Check if sprite is within a 90-degree cone in front of player (cos(45°) ≈ 0.7071)
                    if (cosAngle > 0.7071) {
                        // Hit the enemy with chainsaw!
                        sprite->takeDamage(m_weaponDamage * 0.2); // Apply a portion of damage per tick
                        hitEnemy = true;
                    }
                }
            }
            
            success = hitEnemy;
            break;
        }
        
        case WeaponType::BFG9000:
        {
            // Create a large BFG projectile
            int bfgId = m_projectileManager->createProjectile(
                bulletPos,
                m_direction,
                ProjectileType::BFG,
                8.0,
                m_weaponDamage
            );
            
            success = (bfgId >= 0);
            break;
        }
        
        case WeaponType::GrenadeLauncher:
        {
            // This should be handled by throwGrenade(), but just in case
            success = throwGrenade();
            break;
        }
        
        default:
            std::cerr << "Unknown weapon type: " << static_cast<int>(m_currentWeapon) << std::endl;
            break;
    }
    
    std::cout << "  Firing result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "  Ammo now: " << m_ammo << std::endl;
    std::cout << "FINAL AMMO CHECK: Ammo after all processing: " << m_ammo << std::endl;
    std::cout << "=======================================" << std::endl;
    
    return success;
}

void Player::reload() {
    // A very simplified reload mechanism
    m_ammo = 10;  // Reset to max ammo
    
    // Reset the total shots fired counter
    static int& totalShotsFired = Player::getTotalShotsFired();
    totalShotsFired = 0;
    std::cout << "RELOAD: Ammo reset to 10. Total shots fired reset to 0." << std::endl;
}

void Player::takeDamage(double amount) {
    // Check for invulnerability
    if (hasPowerUp(PowerUpType::Invulnerability)) {
        return;  // No damage when invulnerable
    }
    
    // Apply armor reduction if available
    if (m_armor > 0) {
        // Armor absorbs 2/3 of damage in Doom
        double armorAbsorption = amount * 2.0 / 3.0;
        
        // Ensure we don't use more armor than available
        armorAbsorption = std::min(armorAbsorption, m_armor);
        
        // Reduce armor
        m_armor -= armorAbsorption;
        
        // Remaining damage goes to health
        amount -= armorAbsorption;
    }
    
    // Apply damage to health
    m_health -= amount;
    
    // Ensure health doesn't go below 0
    if (m_health < 0) {
        m_health = 0;
    }
    
    std::cout << "Player took " << amount << " damage. Health: " << m_health << std::endl;
}

void Player::teleport(double x, double y) {
    m_position.x = x;
    m_position.y = y;
}

// New method for throwing grenades
bool Player::throwGrenade() {
    // Check if we have grenades
    if (m_grenades <= 0) {
        std::cout << "No grenades left!" << std::endl;
        return false;
    }
    
    // Check if we have a projectile manager
    if (!m_projectileManager) {
        std::cout << "No projectile manager!" << std::endl;
        return false;
    }
    
    // Check cooldown
    if (m_timeSinceLastShot < m_weaponCooldown) {
        std::cout << "Weapon on cooldown!" << std::endl;
        return false;
    }
    
    // Reset cooldown
    m_timeSinceLastShot = 0.0;
    
    // Decrease grenade count
    m_grenades--;
    
    std::cout << "=======================================" << std::endl;
    std::cout << "[GRENADE] Throwing grenade!" << std::endl;
    std::cout << "  Grenades left: " << m_grenades << std::endl;
    
    // Calculate starting position (slightly in front of player)
    Vec2 grenadePos = m_position + m_direction * 0.5;
    
    // Calculate initial velocity based on throw power and direction
    // Include vertical angle in the calculation
    Vec2 throwDir = m_direction;
    
    // Apply vertical angle to throw direction (positive angle = throwing upward)
    double verticalFactor = sin(m_verticalAngle);
    double horizontalFactor = cos(m_verticalAngle);
    
    // Adjust throw power based on vertical angle
    double effectiveThrowPower = m_throwPower * horizontalFactor;
    
    std::cout << "  Throw power: " << m_throwPower << std::endl;
    std::cout << "  Vertical factor: " << verticalFactor << std::endl;
    std::cout << "  Horizontal factor: " << horizontalFactor << std::endl;
    std::cout << "  Effective throw power: " << effectiveThrowPower << std::endl;
    
    // Create the grenade projectile
    int grenadeId = m_projectileManager->createProjectile(
        grenadePos,
        throwDir,
        ProjectileType::Grenade,
        effectiveThrowPower,
        50.0  // Grenade damage
    );
    
    if (grenadeId >= 0) {
        // Get the created projectile to adjust its properties
        Projectile* grenade = nullptr;
        const std::vector<Projectile*>& projectiles = m_projectileManager->getActiveProjectiles();
        for (auto proj : projectiles) {
            if (proj->getId() == grenadeId) {
                grenade = proj;
                break;
            }
        }
        
        if (grenade) {
            // Apply vertical velocity component based on look angle
            Vec2 velocity = grenade->getVelocity();
            
            // Adjust velocity based on vertical angle
            // In a 2D raycaster, we're simulating 3D with a 2D engine
            // We'll use the Y component to simulate vertical movement
            velocity.y -= verticalFactor * m_throwPower; // Negative Y is up in most 2D raycasters
            
            // Set the updated velocity
            grenade->setVelocity(velocity);
            
            // Make sure physics is enabled
            grenade->setUsePhysics(true);
            
            // Set appropriate physics properties
            grenade->setGravity(9.8);       // Standard gravity
            grenade->setAirResistance(0.02); // Light air resistance
            grenade->setMass(1.0);          // Standard mass
            grenade->setBounciness(0.6);    // Fairly bouncy
            grenade->setMaxBounces(3);      // Can bounce up to 3 times
            
            std::cout << "  Grenade thrown with velocity: (" << velocity.x << ", " << velocity.y << ")" << std::endl;
            std::cout << "=======================================" << std::endl;
            return true;
        }
    }
    
    std::cout << "  Failed to create grenade projectile!" << std::endl;
    std::cout << "=======================================" << std::endl;
    return false;
}

void Player::activatePowerUp(PowerUpType type, double duration) {
    m_activePowerUp = type;
    m_powerUpTimer = duration;
    m_powerUpDuration = duration;
    
    // Apply immediate effects
    switch (type) {
        case PowerUpType::Berserk:
            // Increase melee damage and heal player
            m_health = std::min(m_health + 50.0, 100.0);
            break;
            
        case PowerUpType::MegaSphere:
            // Full health and armor
            m_health = 200.0;  // Boost health beyond normal max
            m_armor = 200.0;   // Boost armor beyond normal max
            m_maxArmor = 200.0; // Temporarily increase max armor
            break;
            
        default:
            break;
    }
}

void Player::updatePowerUps(double deltaTime) {
    if (m_activePowerUp != PowerUpType::None && m_powerUpTimer > 0) {
        // Decrease timer
        m_powerUpTimer -= deltaTime;
        
        // Apply continuous effects
        switch (m_activePowerUp) {
            case PowerUpType::Berserk:
                // Berserk increases melee damage
                if (m_currentWeapon == WeaponType::Chainsaw) {
                    m_weaponDamage = 100.0;  // Increased damage with chainsaw
                }
                break;
                
            case PowerUpType::Invulnerability:
                // Invulnerability is handled in takeDamage method
                break;
                
            case PowerUpType::LightAmp:
                // Light amplification is handled in the renderer
                break;
                
            default:
                break;
        }
        
        // Check if power-up has expired
        if (m_powerUpTimer <= 0) {
            // Reset effects when power-up expires
            switch (m_activePowerUp) {
                case PowerUpType::Berserk:
                    // Reset weapon damage
                    setCurrentWeapon(m_currentWeapon);  // Reset damage based on current weapon
                    break;
                    
                case PowerUpType::MegaSphere:
                    // Reset max armor to normal
                    m_maxArmor = 100.0;
                    break;
                    
                default:
                    break;
            }
            
            m_activePowerUp = PowerUpType::None;
        }
    }
}

void Player::jump() {
    // Only allow jump if we're on the ground
    if (isOnGround()) {
        m_isJumping = true;
        // DOOM-style jumping has a strong initial upward force
        m_verticalVelocity = m_jumpForce;
        m_jumpHeight = 0.001; // Tiny initial offset to get off ground immediately
        
        // In DOOM, there's a distinctive jump sound
        std::cout << "Player jumped! Initial velocity: " << m_verticalVelocity << std::endl;
    }
}

void Player::updateJump(double deltaTime) {
    // Cap deltaTime to prevent physics glitches
    deltaTime = std::min(deltaTime, 0.033); // Cap at ~30 FPS equivalent
    
    // Apply gravity and update position
    m_verticalVelocity -= m_gravity * deltaTime;
    
    // Update jump height based on velocity
    double oldHeight = m_jumpHeight;
    m_jumpHeight += m_verticalVelocity * deltaTime;
    
    // DOOM-style: Less air control, more predictable arc
    // Add a slight constant downward acceleration for more DOOM-like feel
    if (m_isJumping && m_jumpHeight > 0.1) {
        m_verticalVelocity -= 5.0 * deltaTime; // Extra downward acceleration for faster drop
    }
    
    // Add slight air resistance - less than before for more DOOM-like physics
    m_verticalVelocity *= (1.0 - 0.05 * deltaTime);
    
    // Check for ground collision
    if (m_jumpHeight <= m_groundLevel) {
        m_jumpHeight = m_groundLevel;
        m_verticalVelocity = 0.0;
        m_isJumping = false;
        
        // DOOM-style: Add a slight landing sound/effect
        if (oldHeight > m_groundLevel + 0.1) { // Only if falling from a significant height
            std::cout << "Player landed from jump" << std::endl;
        }
    }
    
    // Debug output only when values change significantly
    if (m_isJumping || fabs(m_verticalVelocity) > 0.1 || fabs(m_jumpHeight - oldHeight) > 0.001) {
        std::cout << "Jump Height: " << m_jumpHeight << ", Velocity: " << m_verticalVelocity << std::endl;
    }
}

bool Player::isOnGround() const {
    return m_jumpHeight <= m_groundLevel && m_verticalVelocity <= 0;
}

int& Player::getTotalShotsFired() {
    static int totalShotsFired = 0;
    return totalShotsFired;
}

// New method to check for and pick up nearby items
void Player::checkNearbyItems() {
    if (!m_spriteManager) return;
    
    // Get all active sprites
    std::vector<Sprite*> sprites = m_spriteManager->getActiveSprites();
    
    // Check each sprite
    for (Sprite* sprite : sprites) {
        if (!sprite || !sprite->isActive() || sprite->getType() != SpriteType::Item) {
            continue;
        }
        
        // Calculate distance to sprite
        double dist = (sprite->getPosition() - m_position).length();
        
        // Pickup range - can be adjusted
        const double PICKUP_RANGE = 1.0;
        
        // If in range, apply item effect
        if (dist <= PICKUP_RANGE) {
            std::cout << "Player picked up item at distance: " << dist << std::endl;
            sprite->applyItemEffect(this);
        }
    }
} 