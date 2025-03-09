#include "player.h"
#include "map.h"
#include <cmath>

Player::Player() :
    m_position(0.0, 0.0),
    m_angle(0.0),
    m_verticalAngle(0.0),
    m_direction(1.0, 0.0),
    m_right(0.0, 1.0),
    m_eyeHeight(1.75),
    m_walkTimer(0.0),
    m_verticalVelocity(0.0),
    m_isOnGround(true),
    m_isCrouching(false),
    m_isJumping(false),
    m_health(100),
    m_armor(0),
    m_fov(90.0 * DEG_TO_RAD),
    m_map(nullptr),
    m_engine(nullptr)
{
    // Initialize player settings with defaults
    m_settings = PlayerSettings();
    updateDirectionVectors();
}

Player::~Player()
{
    // No resources to free
}

void Player::init(const Vec2& position, double angle, Map* map)
{
    m_position = position;
    m_angle = angle;
    m_map = map;
    m_eyeHeight = m_settings.eyeHeight;
    m_verticalAngle = 0.0;
    m_isOnGround = true;
    m_verticalVelocity = 0.0;
    m_isCrouching = false;
    m_isJumping = false;
    m_health = 100;
    m_armor = 0;
    
    updateDirectionVectors();
    
    // Find the sector containing the player
    m_currentSector = findContainingSector();
}

void Player::update(double deltaTime)
{
    // Handle vertical movement (jumping, falling)
    updateVerticalMovement(deltaTime);
    
    // Apply view bobbing while moving
    if (m_walkTimer > 0.0) {
        applyViewBob(deltaTime);
    }
    
    // Check if player is in a valid sector
    auto sector = findContainingSector();
    if (sector) {
        // Update current sector
        if (sector != m_currentSector) {
            enterSector(sector);
        }
    }
}

void Player::processInput(bool moveForward, bool moveBackward, bool moveLeft, bool moveRight,
                         bool rotateLeft, bool rotateRight, bool jump, bool crouch,
                         double mouseX, double mouseY, bool mouseLook)
{
    // Handle jumping
    if (jump && m_isOnGround && !m_isJumping) {
        m_isJumping = true;
        m_isOnGround = false;
        m_verticalVelocity = m_settings.jumpHeight * 5.0; // Initial velocity for jumping
    }
    
    // Handle crouching
    if (crouch != m_isCrouching) {
        if (crouch) {
            // Start crouching
            m_isCrouching = true;
            m_eyeHeight = m_settings.eyeHeight - m_settings.crouchHeight;
        } else if (canStandUp()) {
            // Stop crouching
            m_isCrouching = false;
            m_eyeHeight = m_settings.eyeHeight;
        }
    }
    
    // Calculate movement speed based on state
    double speed = m_settings.moveSpeed;
    if (m_isCrouching) {
        speed *= 0.5; // Slower when crouching
    }
    
    // Movement vectors
    Vec2 moveDir(0.0, 0.0);
    
    // Forward/backward movement along direction vector
    if (moveForward) {
        moveDir += m_direction;
    }
    if (moveBackward) {
        moveDir -= m_direction;
    }
    
    // Strafe left/right along right vector
    if (moveLeft) {
        moveDir -= m_right;
    }
    if (moveRight) {
        moveDir += m_right;
    }
    
    // Normalize the movement vector if necessary
    if (moveDir.length() > EPSILON) {
        moveDir.normalize();
        
        // Update player position
        Vec2 newPosition = m_position + moveDir * speed * 0.016; // Assuming ~60 FPS
        
        // Try to move to new position, handling collisions
        tryMove(newPosition);
        
        // Update walking animation state
        m_walkTimer = 0.5; // Reset walk timer when moving
    } else {
        // Gradually reduce walk timer when not moving
        m_walkTimer = std::max(0.0, m_walkTimer - 0.016); // Assuming ~60 FPS
    }
    
    // Rotation from keyboard
    if (rotateLeft) {
        m_angle -= m_settings.rotateSpeed * 0.016; // Assuming ~60 FPS
    }
    if (rotateRight) {
        m_angle += m_settings.rotateSpeed * 0.016; // Assuming ~60 FPS
    }
    
    // Mouse look
    if (mouseLook) {
        // Horizontal mouse movement rotates the player
        m_angle += mouseX * m_settings.maxMouseSensitivity;
        
        // Vertical mouse movement changes the view angle
        m_verticalAngle -= mouseY * m_settings.maxMouseSensitivity;
        
        // Clamp vertical angle to prevent flipping
        m_verticalAngle = clamp(m_verticalAngle, -HALF_PI * 0.9, HALF_PI * 0.9);
    }
    
    // Update direction vectors based on new angle
    updateDirectionVectors();
}

void Player::setPosition(const Vec2& position)
{
    m_position = position;
}

void Player::setAngle(double angle)
{
    m_angle = angle;
    updateDirectionVectors();
}

void Player::setDirection(const Vec2& direction)
{
    m_direction = direction.normalized();
    m_angle = atan2(m_direction.y, m_direction.x);
    updateDirectionVectors();
}

void Player::setVerticalAngle(double angle)
{
    m_verticalAngle = clamp(angle, -HALF_PI * 0.9, HALF_PI * 0.9);
}

bool Player::tryMove(const Vec2& newPosition)
{
    // Check for wall collisions
    if (checkWallCollisions(newPosition)) {
        return false;
    }
    
    // No collisions, move to new position
    m_position = newPosition;
    return true;
}

void Player::takeDamage(int amount)
{
    // Apply armor protection if available
    int damageToHealth = amount;
    if (m_armor > 0) {
        int armorAbsorption = static_cast<int>(amount * 0.5); // 50% damage reduction from armor
        m_armor -= armorAbsorption;
        damageToHealth -= armorAbsorption;
        
        // Ensure armor doesn't go negative
        if (m_armor < 0) {
            damageToHealth -= m_armor; // Add overflow damage back to health damage
            m_armor = 0;
        }
    }
    
    // Apply damage to health
    m_health -= damageToHealth;
    
    // Ensure health doesn't go below 0
    if (m_health < 0) {
        m_health = 0;
    }
}

void Player::enterSector(std::shared_ptr<Sector> sector)
{
    // Update current sector
    m_currentSector = sector;
    
    // Update eye height based on floor height
    if (sector) {
        // Keep player's eyes at constant height above floor
        double targetEyeHeight = m_settings.eyeHeight;
        if (m_isCrouching) {
            targetEyeHeight -= m_settings.crouchHeight;
        }
        
        m_eyeHeight = targetEyeHeight;
    }
}

void Player::reset()
{
    m_health = 100;
    m_armor = 0;
    m_isOnGround = true;
    m_verticalVelocity = 0.0;
    m_isCrouching = false;
    m_isJumping = false;
    m_eyeHeight = m_settings.eyeHeight;
    m_verticalAngle = 0.0;
}

void Player::updateDirectionVectors()
{
    // Normalize angle to [0, 2π)
    m_angle = normalizeAngle(m_angle);
    
    // Update direction vector
    m_direction.x = cos(m_angle);
    m_direction.y = sin(m_angle);
    
    // Update right vector (perpendicular to direction)
    m_right.x = -m_direction.y;
    m_right.y = m_direction.x;
}

void Player::updateVerticalMovement(double deltaTime)
{
    if (!m_currentSector) return;
    
    // Apply gravity
    if (!m_isOnGround) {
        m_verticalVelocity -= m_settings.gravity * deltaTime;
        
        // Clamp to max fall speed
        if (m_verticalVelocity < -m_settings.maxFallSpeed) {
            m_verticalVelocity = -m_settings.maxFallSpeed;
        }
    }
    
    // Calculate new vertical position
    double floorHeight = m_currentSector->getFloorHeight();
    double newEyePos = m_eyeHeight + m_verticalVelocity * deltaTime;
    
    // Check for floor collision
    if (newEyePos < m_settings.eyeHeight) {
        // We've hit the floor
        newEyePos = m_settings.eyeHeight;
        m_verticalVelocity = 0.0;
        m_isOnGround = true;
        m_isJumping = false;
    }
    
    // Check for ceiling collision
    double ceilingHeight = m_currentSector->getCeilingHeight();
    double headroom = ceilingHeight - floorHeight;
    if (newEyePos > headroom - m_settings.maxHeadroom) {
        // We've hit the ceiling
        newEyePos = headroom - m_settings.maxHeadroom;
        m_verticalVelocity = 0.0;
    }
    
    // Update eye height
    m_eyeHeight = newEyePos;
}

bool Player::canStandUp() const
{
    if (!m_currentSector) return true;
    
    // Check if there's enough room to stand up
    double floorHeight = m_currentSector->getFloorHeight();
    double ceilingHeight = m_currentSector->getCeilingHeight();
    double roomHeight = ceilingHeight - floorHeight;
    
    return roomHeight >= m_settings.eyeHeight + m_settings.maxHeadroom;
}

void Player::applyViewBob(double deltaTime)
{
    // Simple sine wave bobbing effect
    double bobAmount = m_settings.viewBobAmount;
    if (m_isCrouching) {
        bobAmount *= 0.5; // Less bobbing when crouching
    }
    
    // Reduce bob timer
    m_walkTimer = std::max(0.0, m_walkTimer - deltaTime);
    
    // Apply bobbing to eye height
    double bobOffset = sin(m_walkTimer * m_settings.viewBobSpeed * TWO_PI) * bobAmount;
    m_eyeHeight += bobOffset;
}

bool Player::checkWallCollisions(const Vec2& newPosition)
{
    if (!m_map) return true;  // No map, don't allow movement
    
    // Get the player's collision radius
    double radius = m_settings.radius;
    
    // Simple collision check against walls in the current sector
    if (m_currentSector) {
        for (const auto& wall : m_currentSector->getWalls()) {
            // Skip portal walls (can walk through them)
            if (wall->isPortal()) continue;
            
            // Calculate closest point on wall to the new position
            Vec2 wallStart = wall->getStart();
            Vec2 wallEnd = wall->getEnd();
            Vec2 wallDir = wallEnd - wallStart;
            double wallLength = wallDir.length();
            
            if (wallLength < EPSILON) continue;  // Skip zero-length walls
            
            wallDir = wallDir / wallLength;  // Normalize
            
            // Vector from wall start to new position
            Vec2 wallToPos = newPosition - wallStart;
            
            // Project wallToPos onto wallDir to find closest point
            double projection = wallToPos.dot(wallDir);
            projection = clamp(projection, 0.0, wallLength);
            
            // Closest point on wall
            Vec2 closestPoint = wallStart + wallDir * projection;
            
            // Check distance to closest point
            double distance = (newPosition - closestPoint).length();
            if (distance < radius) {
                // Collision detected
                return true;
            }
        }
    }
    
    return false;  // No collisions
}

bool Player::tryClimbStep(const Vec2& newPosition, double stepHeight)
{
    // Not implemented yet - would allow climbing stairs
    return false;
}

std::shared_ptr<Sector> Player::findContainingSector() const
{
    if (!m_map) return nullptr;
    
    return m_map->findSectorContainingPoint(m_position);
}

// Camera implementation
Camera::Camera() :
    m_position(0.0, 0.0),
    m_height(0.0),
    m_angle(0.0),
    m_verticalAngle(0.0),
    m_direction(1.0, 0.0),
    m_right(0.0, 1.0),
    m_fov(90.0 * DEG_TO_RAD),
    m_shakeMagnitude(0.0),
    m_shakeTimer(0.0),
    m_tiltAngle(0.0),
    m_tiltTimer(0.0),
    m_player(nullptr)
{
}

Camera::~Camera()
{
    // No resources to free
}

void Camera::init(Player* player)
{
    m_player = player;
    if (player) {
        m_position = player->getPosition();
        m_height = player->getEyeHeight();
        m_angle = player->getAngle();
        m_verticalAngle = player->getVerticalAngle();
        m_fov = player->getFOV();
        updateDirectionVectors();
    }
}

void Camera::update(double deltaTime)
{
    // Update camera effects
    updateEffects(deltaTime);
    
    // Update camera position and orientation from player
    if (m_player) {
        m_position = m_player->getPosition();
        m_height = m_player->getEyeHeight();
        m_angle = m_player->getAngle();
        m_verticalAngle = m_player->getVerticalAngle();
        m_fov = m_player->getFOV();
        updateDirectionVectors();
    }
}

void Camera::setAngle(double angle)
{
    m_angle = angle;
    updateDirectionVectors();
}

void Camera::setVerticalAngle(double angle)
{
    m_verticalAngle = clamp(angle, -HALF_PI * 0.9, HALF_PI * 0.9);
}

void Camera::getViewMatrix(double& eyeX, double& eyeY, double& eyeZ,
                          double& dirX, double& dirY, double& dirZ) const
{
    // Eye position
    eyeX = m_position.x;
    eyeY = m_position.y;
    eyeZ = m_height;
    
    // View direction
    dirX = m_direction.x;
    dirY = m_direction.y;
    dirZ = -tan(m_verticalAngle);  // Down is negative in 3D space
}

void Camera::applyEffect(const std::string& effect, double amount, double duration)
{
    if (effect == "shake") {
        m_shakeMagnitude = amount;
        m_shakeTimer = duration;
    } else if (effect == "tilt") {
        m_tiltAngle = amount;
        m_tiltTimer = duration;
    }
}

void Camera::updateDirectionVectors()
{
    // Normalize angle to [0, 2π)
    m_angle = normalizeAngle(m_angle);
    
    // Apply camera shake effect
    double shakeAngle = m_angle;
    if (m_shakeMagnitude > 0.0) {
        // Random angle offset for shaking
        shakeAngle += (rand() / (double)RAND_MAX - 0.5) * m_shakeMagnitude;
    }
    
    // Update direction vector
    m_direction.x = cos(shakeAngle);
    m_direction.y = sin(shakeAngle);
    
    // Update right vector (perpendicular to direction)
    m_right.x = -m_direction.y;
    m_right.y = m_direction.x;
}

void Camera::updateEffects(double deltaTime)
{
    // Update shake effect
    if (m_shakeTimer > 0.0) {
        m_shakeTimer -= deltaTime;
        if (m_shakeTimer <= 0.0) {
            m_shakeTimer = 0.0;
            m_shakeMagnitude = 0.0;
        }
    }
    
    // Update tilt effect
    if (m_tiltTimer > 0.0) {
        m_tiltTimer -= deltaTime;
        if (m_tiltTimer <= 0.0) {
            m_tiltTimer = 0.0;
            m_tiltAngle = 0.0;
        }
    }
} 