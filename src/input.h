#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

// Action types for different inputs
enum class InputAction {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    RotateLeft,
    RotateRight,
    Fire,
    Reload,
    Use,
    Jump,
    Crouch,
    Menu,
    Quit,
    ToggleFPS,
    ToggleMinimap,
    ToggleWeapon,
    ToggleCeilings,
    ToggleMusic,
    IncreaseMusicVolume,
    DecreaseMusicVolume,
    IncreaseSfxVolume,
    DecreaseSfxVolume,
    Weapon1,
    Weapon2,
    Weapon3,
    Weapon4,
    Weapon5,
    Weapon6,
    Weapon7,
    TestSound  // New action for testing sounds
};

// Input handler class to manage keyboard and mouse input
class InputHandler {
private:
    // Key state map (current frame)
    std::unordered_map<SDL_Scancode, bool> m_keyStates;
    
    // Key state map (previous frame)
    std::unordered_map<SDL_Scancode, bool> m_prevKeyStates;
    
    // Mouse position
    int m_mouseX;
    int m_mouseY;
    
    // Mouse motion
    int m_mouseRelX;
    int m_mouseRelY;
    
    // Mouse button states
    bool m_leftMouseButton;
    bool m_rightMouseButton;
    bool m_middleMouseButton;
    
    // Key bindings (key to action mapping)
    std::unordered_map<SDL_Scancode, InputAction> m_keyBindings;
    
public:
    InputHandler();
    ~InputHandler();
    
    // Initialize input handler
    void init();
    
    // Update input state
    void update();
    
    // Process SDL events
    bool processEvent(const SDL_Event& event);
    
    // Handle SDL events
    bool handleEvent(const SDL_Event& event);
    
    // Key state methods
    bool isKeyDown(SDL_Scancode key) const;
    bool isKeyPressed(SDL_Scancode key) const;  // Key was just pressed this frame
    bool isKeyReleased(SDL_Scancode key) const; // Key was just released this frame
    
    // Mouse state methods
    bool isLeftMouseDown() const { return m_leftMouseButton; }
    bool isRightMouseDown() const { return m_rightMouseButton; }
    bool isMiddleMouseDown() const { return m_middleMouseButton; }
    
    void getMousePosition(int& x, int& y) const;
    void getMouseMotion(int& x, int& y) const;
    int getMouseRelX() const { return m_mouseRelX; }
    int getMouseRelY() const { return m_mouseRelY; }
    void resetMouseRel() { m_mouseRelX = 0; m_mouseRelY = 0; }
    
    // Action state methods
    bool isActionActive(InputAction action) const;
    bool isActionJustPressed(InputAction action) const;
    bool isActionJustReleased(InputAction action) const;
    
    // Action methods
    bool isActionTriggered(InputAction action) const;
    bool isAnyKeyPressed() const;
    
    // Key binding methods
    void bindKey(SDL_Scancode key, InputAction action);
    void unbindKey(SDL_Scancode key);
    SDL_Scancode getKeyForAction(InputAction action) const;
    std::string getKeyNameForAction(InputAction action) const;
    
    // Enable/disable mouse capture
    void setMouseCapture(bool capture);
    bool isMouseCaptured() const;
};

#endif // INPUT_H 