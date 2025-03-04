#include "input.h"
#include <iostream>

InputHandler::InputHandler()
    : m_mouseX(0)
    , m_mouseY(0)
    , m_mouseRelX(0)
    , m_mouseRelY(0)
    , m_leftMouseButton(false)
    , m_rightMouseButton(false)
    , m_middleMouseButton(false)
{
}

InputHandler::~InputHandler() {
    // Nothing to clean up
}

void InputHandler::init() {
    // Clear any existing bindings
    m_keyBindings.clear();
    
    // We'll let the Engine class set up the key bindings
}

void InputHandler::update() {
    // Store current key states as previous states
    m_prevKeyStates = m_keyStates;
    
    // Get current keyboard state directly from SDL
    int numKeys;
    const Uint8* keyboardState = SDL_GetKeyboardState(&numKeys);
    
    // Update our key state map
    for (auto& binding : m_keyBindings) {
        SDL_Scancode scancode = binding.first;
        if (scancode < numKeys) {
            bool wasDown = m_keyStates[scancode];
            bool isDown = keyboardState[scancode] ? true : false;
            m_keyStates[scancode] = isDown;
            
            // Debug output for key state changes
            if (isDown && !wasDown) {
                std::cout << "Key pressed (direct): " << SDL_GetScancodeName(scancode) << " (scancode: " << scancode << ")" << std::endl;
            } else if (!isDown && wasDown) {
                std::cout << "Key released (direct): " << SDL_GetScancodeName(scancode) << " (scancode: " << scancode << ")" << std::endl;
            }
        }
    }
    
    // Reset mouse motion
    m_mouseRelX = 0;
    m_mouseRelY = 0;
}

bool InputHandler::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_KEYDOWN:
            std::cout << "Key pressed: " << SDL_GetScancodeName(event.key.keysym.scancode) << " (scancode: " << event.key.keysym.scancode << ")" << std::endl;
            m_keyStates[event.key.keysym.scancode] = true;
            return true;
            
        case SDL_KEYUP:
            std::cout << "Key released: " << SDL_GetScancodeName(event.key.keysym.scancode) << " (scancode: " << event.key.keysym.scancode << ")" << std::endl;
            m_keyStates[event.key.keysym.scancode] = false;
            return true;
            
        case SDL_MOUSEMOTION:
            m_mouseX = event.motion.x;
            m_mouseY = event.motion.y;
            m_mouseRelX += event.motion.xrel;
            m_mouseRelY += event.motion.yrel;
            return true;
            
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
                m_leftMouseButton = true;
            else if (event.button.button == SDL_BUTTON_RIGHT)
                m_rightMouseButton = true;
            else if (event.button.button == SDL_BUTTON_MIDDLE)
                m_middleMouseButton = true;
            return true;
            
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT)
                m_leftMouseButton = false;
            else if (event.button.button == SDL_BUTTON_RIGHT)
                m_rightMouseButton = false;
            else if (event.button.button == SDL_BUTTON_MIDDLE)
                m_middleMouseButton = false;
            return true;
            
        default:
            return false; // Event not handled
    }
}

bool InputHandler::isKeyDown(SDL_Scancode key) const {
    auto it = m_keyStates.find(key);
    return (it != m_keyStates.end() && it->second);
}

bool InputHandler::isKeyPressed(SDL_Scancode key) const {
    // Key is down now, but wasn't down last frame
    auto currIt = m_keyStates.find(key);
    auto prevIt = m_prevKeyStates.find(key);
    
    bool isDown = (currIt != m_keyStates.end() && currIt->second);
    bool wasDown = (prevIt != m_prevKeyStates.end() && prevIt->second);
    
    return isDown && !wasDown;
}

bool InputHandler::isKeyReleased(SDL_Scancode key) const {
    // Key is up now, but was down last frame
    auto currIt = m_keyStates.find(key);
    auto prevIt = m_prevKeyStates.find(key);
    
    bool isDown = (currIt != m_keyStates.end() && currIt->second);
    bool wasDown = (prevIt != m_prevKeyStates.end() && prevIt->second);
    
    return !isDown && wasDown;
}

void InputHandler::getMousePosition(int& x, int& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

void InputHandler::getMouseMotion(int& x, int& y) const {
    x = m_mouseRelX;
    y = m_mouseRelY;
}

bool InputHandler::isActionActive(InputAction action) const {
    // Find all keys bound to this action
    for (const auto& binding : m_keyBindings) {
        if (binding.second == action && isKeyDown(binding.first)) {
            std::cout << "Action active: " << static_cast<int>(action) << " (key: " << SDL_GetScancodeName(binding.first) << ")" << std::endl;
            return true;
        }
    }
    
    return false;
}

bool InputHandler::isActionJustPressed(InputAction action) const {
    // Find all keys bound to this action
    for (const auto& binding : m_keyBindings) {
        if (binding.second == action && isKeyPressed(binding.first)) {
            std::cout << "Action just pressed: " << static_cast<int>(action) << " (key: " << SDL_GetScancodeName(binding.first) << ")" << std::endl;
            return true;
        }
    }
    
    return false;
}

bool InputHandler::isActionJustReleased(InputAction action) const {
    // Find all keys bound to this action
    for (const auto& binding : m_keyBindings) {
        if (binding.second == action && isKeyReleased(binding.first)) {
            return true;
        }
    }
    
    return false;
}

void InputHandler::bindKey(SDL_Scancode key, InputAction action) {
    m_keyBindings[key] = action;
    std::cout << "Bound key: " << SDL_GetScancodeName(key) << " (scancode: " << key << ") to action: " << static_cast<int>(action) << std::endl;
}

void InputHandler::unbindKey(SDL_Scancode key) {
    m_keyBindings.erase(key);
}

SDL_Scancode InputHandler::getKeyForAction(InputAction action) const {
    for (const auto& binding : m_keyBindings) {
        if (binding.second == action) {
            return binding.first;
        }
    }
    
    return SDL_SCANCODE_UNKNOWN;
}

std::string InputHandler::getKeyNameForAction(InputAction action) const {
    SDL_Scancode key = getKeyForAction(action);
    if (key == SDL_SCANCODE_UNKNOWN) {
        return "None";
    }
    
    return SDL_GetScancodeName(key);
}

void InputHandler::setMouseCapture(bool capture) {
    SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
}

bool InputHandler::isMouseCaptured() const {
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
} 