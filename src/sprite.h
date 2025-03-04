#ifndef SPRITE_H
#define SPRITE_H

#include "utils.h"
#include "map.h"
#include <string>
#include <vector>

// Forward declarations
class TextureManager;

// Different types of sprites
enum class SpriteType {
    Enemy,
    Item,
    Decoration,
    Projectile
};

class Sprite {
private:
    Vec2 m_position;       // Position in the world
    Vec2 m_direction;      // Movement direction
    double m_size;         // Size of the sprite
    int m_textureId;       // Texture ID
    SpriteType m_type;     // Type of sprite
    bool m_isVisible;      // Is the sprite visible
    bool m_isActive;       // Is the sprite active/alive
    
    // Health and damage
    double m_health;       // Current health
    double m_maxHealth;    // Maximum health
    bool m_isDying;        // Is the sprite in death animation
    double m_deathTimer;   // Timer for death animation
    
    // Movement properties
    double m_moveSpeed;    // Movement speed
    double m_turnSpeed;    // Turning speed
    double m_moveTimer;    // Timer for movement changes
    double m_moveDuration; // How long to move in current direction
    
    // For animated sprites
    bool m_isAnimated;
    int m_frameCount;
    int m_currentFrame;
    double m_animationSpeed;
    double m_animationTimer;
    
public:
    Sprite(double x, double y, double size, int textureId, SpriteType type);
    
    // Update sprite state
    void update(double deltaTime, const Map& map, const Vec2& playerPos);
    
    // Damage handling
    void takeDamage(double damage);
    bool isDead() const { return m_health <= 0 && !m_isDying; }
    bool isDying() const { return m_isDying; }
    double getHealth() const { return m_health; }
    double getMaxHealth() const { return m_maxHealth; }


    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    const Vec2& getDirection() const { return m_direction; }
    double getSize() const { return m_size; }
    int getTextureId() const { return m_textureId; }
    SpriteType getType() const { return m_type; }
    bool isVisible() const { return m_isVisible; }
    bool isActive() const { return m_isActive; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setPosition(double x, double y) { m_position = Vec2(x, y); }
    void setDirection(const Vec2& direction) { m_direction = direction; }
    void setSize(double size) { m_size = size; }
    void setTextureId(int textureId) { m_textureId = textureId; }
    void setVisible(bool visible) { m_isVisible = visible; }
    void setActive(bool active) { m_isActive = active; }
    void setMoveSpeed(double speed) { m_moveSpeed = speed; }
    void setTurnSpeed(double speed) { m_turnSpeed = speed; }
    void setHealth(double health) { m_health = health; }
    
    // Animation methods
    void setAnimated(bool animated, int frameCount = 1, double animationSpeed = 1.0);
    int getCurrentFrame() const { return m_currentFrame; }
    
private:
    // AI methods
    void updateEnemyBehavior(double deltaTime, const Map& map, const Vec2& playerPos);
    void changeDirection(const Map& map);
    bool canMoveTo(const Vec2& newPos, const Map& map) const;
    void updateDeathAnimation(double deltaTime);
};

class SpriteManager {
private:
    std::vector<Sprite> m_sprites;
    const TextureManager* m_textureManager;
    
public:
    SpriteManager(const TextureManager* textureManager);
    
    // Add a new sprite and return its ID
    int addSprite(double x, double y, double size, int textureId, SpriteType type);
    
    // Remove a sprite by ID
    void removeSprite(int id);
    
    // Clear all sprites
    void clearSprites() { m_sprites.clear(); }
    
    // Update all sprites
    void update(double deltaTime, const Map& map, const Vec2& playerPos);
    
    // Get a sprite by ID
    Sprite* getSprite(int id);
    
    // Get all sprites
    const std::vector<Sprite>& getSprites() const { return m_sprites; }
    
    // Get active sprites (for rendering optimization)
    std::vector<Sprite*> getActiveSprites();
    
    // Sort sprites by distance to player (for rendering)
    void sortSpritesByDistance(const Vec2& playerPos);
};

#endif // SPRITE_H 