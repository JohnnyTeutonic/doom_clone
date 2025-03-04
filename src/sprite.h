#ifndef SPRITE_H
#define SPRITE_H

#include "utils.h"
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
    double m_size;         // Size of the sprite
    int m_textureId;       // Texture ID
    SpriteType m_type;     // Type of sprite
    bool m_isVisible;      // Is the sprite visible
    bool m_isActive;       // Is the sprite active/alive
    
    // For animated sprites
    bool m_isAnimated;
    int m_frameCount;
    int m_currentFrame;
    double m_animationSpeed;
    double m_animationTimer;
    
public:
    Sprite(double x, double y, double size, int textureId, SpriteType type);
    
    // Update sprite state
    void update(double deltaTime);
    
    // Getters
    const Vec2& getPosition() const { return m_position; }
    double getSize() const { return m_size; }
    int getTextureId() const { return m_textureId; }
    SpriteType getType() const { return m_type; }
    bool isVisible() const { return m_isVisible; }
    bool isActive() const { return m_isActive; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setPosition(double x, double y) { m_position = Vec2(x, y); }
    void setSize(double size) { m_size = size; }
    void setTextureId(int textureId) { m_textureId = textureId; }
    void setVisible(bool visible) { m_isVisible = visible; }
    void setActive(bool active) { m_isActive = active; }
    
    // Animation methods
    void setAnimated(bool animated, int frameCount = 1, double animationSpeed = 1.0);
    int getCurrentFrame() const { return m_currentFrame; }
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
    
    // Update all sprites
    void update(double deltaTime);
    
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