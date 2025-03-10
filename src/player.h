#ifndef PLAYER_H
#define PLAYER_H

#include "utils.h"
#include <memory>
#include <vector>

// Forward declarations
class Map;
class Sector;
class Engine;

// Structure to define player movement settings
struct PlayerSettings {
    double moveSpeed;              // Movement speed (units per second)
    double rotateSpeed;            // Rotation speed (radians per second)
    double jumpHeight;             // Jump height
    double crouchHeight;           // Height when crouching
    double eyeHeight;              // Eye height above floor
    double maxStepHeight;          // Maximum step height (for stairs)
    double radius;                 // Player collision radius
    double gravity;                // Gravity strength
    double maxFallSpeed;           // Maximum falling speed
    double maxHeadroom;            // Minimum ceiling clearance
    double viewBobAmount;          // Amount of view bobbing
    double viewBobSpeed;           // Speed of view bobbing
    double maxMouseSensitivity;    // Maximum mouse sensitivity
    
    PlayerSettings() :
        moveSpeed(7.0),
        rotateSpeed(PI),
        jumpHeight(1.0),
        crouchHeight(0.5),
        eyeHeight(1.75),
        maxStepHeight(0.5),
        radius(0.5),
        gravity(20.0),
        maxFallSpeed(20.0),
        maxHeadroom(0.2),
        viewBobAmount(0.05),
        viewBobSpeed(10.0),
        maxMouseSensitivity(0.008)
    {}
};

// Structure to store player input state
struct InputState {
    bool moveForward;
    bool moveBackward;
    bool moveLeft;
    bool moveRight;
    bool rotateLeft;
    bool rotateRight;
    double mouseX;
    double mouseY;
    
    InputState() :
        moveForward(false),
        moveBackward(false),
        moveLeft(false),
        moveRight(false),
        rotateLeft(false),
        rotateRight(false),
        mouseX(0.0),
        mouseY(0.0)
    {}
};

// Player class representing the game player
class Player {
public:
    Player();
    ~Player();
    
    // Initialize the player
    void init(const Vec2& position, double angle, Map* map);
    
    // Update player state
    void update(double deltaTime);
    
    // Process input
    void processInput(bool moveForward, bool moveBackward, bool moveLeft, bool moveRight,
                     bool rotateLeft, bool rotateRight, bool jump, bool crouch,
                     double mouseX, double mouseY, bool mouseLook);
    
    // Getters
    Vec2 getPosition() const { return m_position; }
    double getAngle() const { return m_angle; }
    double getEyeHeight() const { return m_eyeHeight; }
    const Vec2& getDirection() const { return m_direction; }
    const Vec2& getRight() const { return m_right; }
    double getVerticalAngle() const { return m_verticalAngle; }
    bool isOnGround() const { return m_isOnGround; }
    bool isCrouching() const { return m_isCrouching; }
    double getVerticalVelocity() const { return m_verticalVelocity; }
    std::shared_ptr<Sector> getCurrentSector() const { return m_currentSector; }
    const PlayerSettings& getSettings() const { return m_settings; }
    int getHealth() const { return m_health; }
    int getArmor() const { return m_armor; }
    Map* getMap() const { return m_map; }
    
    // Setters
    void setPosition(const Vec2& position);
    void setAngle(double angle);
    void setDirection(const Vec2& direction);
    void setVerticalAngle(double angle);
    void setSettings(const PlayerSettings& settings) { m_settings = settings; }
    void setMap(Map* map) { m_map = map; }
    void setEngine(Engine* engine) { m_engine = engine; }
    
    // Try to move player to a new position, handling collisions
    bool tryMove(const Vec2& newPosition);
    
    // Apply damage to player
    void takeDamage(int amount);
    
    // Check if player is dead
    bool isDead() const { return m_health <= 0; }
    
    // Get player field of view
    double getFOV() const { return m_fov; }
    
    // Set player field of view
    void setFOV(double fov) { m_fov = clamp(fov, 60.0 * DEG_TO_RAD, 120.0 * DEG_TO_RAD); }
    
    // Handle player entering a new sector
    void enterSector(std::shared_ptr<Sector> sector);
    
    // Reset player state
    void reset();
    
private:
    Vec2 m_position;                        // Player position
    double m_angle;                         // Player angle (horizontal)
    double m_verticalAngle;                 // Player angle (vertical)
    Vec2 m_direction;                       // Direction vector (normalized)
    Vec2 m_right;                           // Right vector (perpendicular to direction)
    double m_eyeHeight;                     // Current eye height
    double m_walkTimer;                     // Timer for walk cycle
    double m_verticalVelocity;              // Vertical velocity
    bool m_isOnGround;                      // Whether player is on the ground
    bool m_isCrouching;                     // Whether player is crouching
    bool m_isJumping;                       // Whether player is jumping
    int m_health;                           // Player health
    int m_armor;                            // Player armor
    double m_fov;                           // Field of view (in radians)
    PlayerSettings m_settings;              // Player settings
    InputState m_inputs;                    // Current input state
    std::shared_ptr<Sector> m_currentSector; // Current sector
    Map* m_map;                             // Reference to the map
    Engine* m_engine;                       // Reference to the engine
    
    // Update player direction vectors
    void updateDirectionVectors();
    
    // Handle vertical movement (jumping, falling, etc.)
    void updateVerticalMovement(double deltaTime);
    
    // Check if player can stand up from crouch
    bool canStandUp() const;
    
    // Apply view bobbing
    void applyViewBob(double deltaTime);
    
    // Check for collisions with walls
    bool checkWallCollisions(const Vec2& newPosition, Vec2& adjustedPosition);
    
    // Try to climb a step
    bool tryClimbStep(const Vec2& newPosition, double stepHeight);
    
    // Find the sector containing the player
    std::shared_ptr<Sector> findContainingSector() const;
};

// Camera class representing the player's view
class Camera {
public:
    Camera();
    ~Camera();
    
    // Initialize the camera
    void init(Player* player);
    
    // Update camera state
    void update(double deltaTime);
    
    // Getters
    Vec2 getPosition() const { return m_position; }
    double getAngle() const { return m_angle; }
    double getVerticalAngle() const { return m_verticalAngle; }
    const Vec2& getDirection() const { return m_direction; }
    const Vec2& getRight() const { return m_right; }
    double getFOV() const { return m_fov; }
    
    // Setters
    void setPosition(const Vec2& position) { m_position = position; }
    void setAngle(double angle);
    void setVerticalAngle(double angle);
    void setFOV(double fov) { m_fov = clamp(fov, 60.0 * DEG_TO_RAD, 120.0 * DEG_TO_RAD); }
    
    // Get view matrix
    void getViewMatrix(double& eyeX, double& eyeY, double& eyeZ,
                     double& dirX, double& dirY, double& dirZ) const;
    
    // Apply camera effects (shaking, tilting, etc.)
    void applyEffect(const std::string& effect, double amount, double duration);
    
private:
    Vec2 m_position;                // Camera position
    double m_height;                // Camera height
    double m_angle;                 // Camera angle (horizontal)
    double m_verticalAngle;         // Camera angle (vertical)
    Vec2 m_direction;               // Direction vector (normalized)
    Vec2 m_right;                   // Right vector (perpendicular to direction)
    double m_fov;                   // Field of view (in radians)
    
    // Camera effects
    double m_shakeMagnitude;        // Current shake magnitude
    double m_shakeTimer;            // Shake timer
    double m_tiltAngle;             // Current tilt angle
    double m_tiltTimer;             // Tilt timer
    
    Player* m_player;               // Reference to the player
    
    // Update camera direction vectors
    void updateDirectionVectors();
    
    // Update camera effects
    void updateEffects(double deltaTime);
};

#endif // PLAYER_H 