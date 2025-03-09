#include "sprite.h"

Sprite::Sprite(const Vec2& position, SpriteType type) :
    m_position(position),
    m_angle(0.0),
    m_height(1.0),
    m_width(1.0),
    m_bottomZ(0.0),
    m_textureId(-1),
    m_currentFrame(0),
    m_frameCount(1),
    m_animationTimer(0.0),
    m_animationSpeed(0.0),
    m_isAnimated(false),
    m_type(type),
    m_animState(AnimationState::IDLE),
    m_active(true),
    m_health(100),
    m_deathTimer(0.0),
    m_currentSector(nullptr),
    m_engine(nullptr)
{
}

Sprite::~Sprite()
{
    // Cleanup if needed
}

void Sprite::update(double deltaTime)
{
    if (!m_active) {
        return;
    }

    // Update animation if animated
    if (m_isAnimated) {
        updateAnimation(deltaTime);
    }
    
    // If sprite is dying, update death animation
    if (m_animState == AnimationState::DYING) {
        updateDeathAnimation(deltaTime);
    }
}

void Sprite::setAnimated(bool animated, int frameCount, double speed)
{
    m_isAnimated = animated;
    m_frameCount = frameCount;
    m_animationSpeed = speed;
    
    // Reset current frame if necessary
    if (m_currentFrame >= m_frameCount) {
                m_currentFrame = 0;
    }
}

void Sprite::setAnimationState(AnimationState state)
{
    // Don't change state if already dead
    if (m_animState == AnimationState::DEAD) {
            return;
    }
    
    // Set new animation state
    m_animState = state;
    
    // Reset animation frame and timer
    m_currentFrame = 0;
    m_animationTimer = 0.0;
}

void Sprite::takeDamage(int damage, const Vec2& source)
{
    // Ignore damage if already dead
    if (m_health <= 0) {
        return;
    }
    
    // Apply damage
    m_health -= damage;
    
    // Set to dying state if health depleted
    if (m_health <= 0) {
        setAnimationState(AnimationState::DYING);
        m_deathTimer = 1.0; // 1 second death animation
    } else {
        // Set to pain state
        setAnimationState(AnimationState::PAIN);
    }
}

void Sprite::updateAnimation(double deltaTime)
{
    // Skip animation if not animated or animation speed is zero
    if (!m_isAnimated || m_animationSpeed <= 0.0 || m_frameCount <= 1) {
        return;
    }
    
    // Update animation timer
    m_animationTimer += deltaTime * m_animationSpeed;
    
    // Update frame when timer passes 1.0
    while (m_animationTimer >= 1.0) {
        m_animationTimer -= 1.0;
        m_currentFrame = (m_currentFrame + 1) % m_frameCount;
    }
}

void Sprite::updateDeathAnimation(double deltaTime)
{
    // Update death timer
    m_deathTimer -= deltaTime;
    
    // When death animation completes, set to DEAD state
    if (m_deathTimer <= 0.0) {
        m_deathTimer = 0.0;
        m_animState = AnimationState::DEAD;
        m_active = false; // Deactivate the sprite
    }
} 