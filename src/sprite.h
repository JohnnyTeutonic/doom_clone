#ifndef SPRITE_H
#define SPRITE_H

#include "utils.h"
#include <memory>
#include <string>

// Forward declarations
class Sector;
class Engine;

// Sprite types
enum class SpriteType {
    STATIC,         // Static decoration
    ENEMY,          // Enemy character
    PICKUP,         // Item pickup
    PROJECTILE,     // Projectile (bullet, rocket, etc.)
    PARTICLE,       // Visual effect particle
    EXPLOSION,      // Explosion effect
    PLAYER_WEAPON   // Player's visible weapon
};

// Animation states
enum class AnimationState {
    IDLE,           // Standing still
    MOVING,         // Moving
    ATTACKING,      // Attacking
    PAIN,           // Taking damage
    DYING,          // Death animation
    DEAD            // Dead (finished death animation)
};

// Sprite class for entities in the game world
class Sprite {
public:
    Sprite(const Vec2& position, SpriteType type = SpriteType::STATIC);
    virtual ~Sprite();
    
    // Update sprite state
    virtual void update(double deltaTime);
    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    double getAngle() const { return m_angle; }
    double getHeight() const { return m_height; }
    double getWidth() const { return m_width; }
    double getBottomZ() const { return m_bottomZ; }
    double getTopZ() const { return m_bottomZ + m_height; }
    int getTextureId() const { return m_textureId; }
    int getCurrentFrame() const { return m_currentFrame; }
    SpriteType getType() const { return m_type; }
    bool isActive() const { return m_active; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setAngle(double angle) { m_angle = angle; }
    void setHeight(double height) { m_height = height; }
    void setWidth(double width) { m_width = width; }
    void setBottomZ(double z) { m_bottomZ = z; }
    void setTextureId(int id) { m_textureId = id; }
    void setCurrentFrame(int frame) { m_currentFrame = frame; }
    void setActive(bool active) { m_active = active; }
    
    // Animation control
    void setAnimated(bool animated, int frameCount = 1, double speed = 1.0);
    void setAnimationState(AnimationState state);
    AnimationState getAnimationState() const { return m_animState; }
    
    // Set current sector - sprites need to know what sector they're in for collision and rendering
    void setCurrentSector(Sector* sector) { m_currentSector = sector; }
    Sector* getCurrentSector() const { return m_currentSector; }
    
    // Damage handling
    virtual void takeDamage(int damage, const Vec2& source);
    bool isDead() const { return m_health <= 0; }
    int getHealth() const { return m_health; }
    void setHealth(int health) { m_health = health; }
    
    // Set engine reference
    void setEngine(Engine* engine) { m_engine = engine; }
    
protected:
    Vec2 m_position;                // Position in world
    double m_angle;                 // Facing angle
    double m_height;                // Height of sprite
    double m_width;                 // Width of sprite
    double m_bottomZ;               // Z-coordinate of bottom of sprite
    int m_textureId;                // Base texture ID
    int m_currentFrame;             // Current animation frame
    int m_frameCount;               // Total frames in animation
    double m_animationTimer;        // Timer for animation
    double m_animationSpeed;        // Speed of animation (frames per second)
    bool m_isAnimated;              // Whether this sprite has animation
    SpriteType m_type;              // Type of sprite
    AnimationState m_animState;     // Current animation state
    bool m_active;                  // Whether sprite is active
    int m_health;                   // Health points
    double m_deathTimer;            // Timer for death animation
    Sector* m_currentSector;        // Current sector the sprite is in
    Engine* m_engine;               // Reference to the engine
    
    // Update animation frames
    virtual void updateAnimation(double deltaTime);
    
    // Update death animation
    virtual void updateDeathAnimation(double deltaTime);
};

#endif // SPRITE_H 