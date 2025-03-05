#ifndef LIGHTING_H
#define LIGHTING_H

#include <vector>
#include <cmath>
#include <algorithm>
#include "utils.h"

enum class LightType {
    Point,          // Light emanating from a point (like explosions, items)
    Directional,    // Light coming from a direction (like sun/moon)
    Flickering,     // Light that flickers (like torches)
    Pulsing,        // Light that pulses (like machinery)
    Strobe,         // Light that strobes on and off (like alarms)
    Glow            // Ambient glow (like lava or radioactive materials)
};

struct DoomColors {
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }
    static Color Yellow() { return Color(255, 255, 0); }
    static Color Orange() { return Color(255, 165, 0); }
    static Color Purple() { return Color(128, 0, 128); }
    static Color Cyan() { return Color(0, 255, 255); }
    static Color LavaRed() { return Color(255, 50, 0); }
    static Color AcidGreen() { return Color(150, 255, 50); }
    static Color TeleportBlue() { return Color(50, 50, 255); }
};

struct Light {
    LightType type;
    Vec2 position;      // For point lights
    Vec2 direction;     // For directional lights
    Color color;
    double intensity;
    double radius;      // For point lights
    bool enabled;
    bool needsUpdate;   // Flag for batch updates
    
    // Special effect properties
    double effectSpeed;     // Speed of flickering/pulsing/strobing
    double effectIntensity; // Intensity of the effect
    double effectTimer;     // Internal timer for effects
    
    // Constructor for point lights
    static Light createPointLight(const Vec2& pos, const Color& col, double intens, double rad) {
        Light light;
        light.type = LightType::Point;
        light.position = pos;
        light.color = col;
        light.intensity = intens;
        light.radius = rad;
        light.enabled = true;
        light.needsUpdate = true;
        light.effectSpeed = 1.0;
        light.effectIntensity = 0.0;
        light.effectTimer = 0.0;
        return light;
    }

    // Constructor for directional lights
    static Light createDirectionalLight(const Vec2& dir, const Color& col, double intens) {
        Light light;
        light.type = LightType::Directional;
        light.direction = dir.normalized();
        light.color = col;
        light.intensity = intens;
        light.enabled = true;
        light.needsUpdate = true;
        light.effectSpeed = 1.0;
        light.effectIntensity = 0.0;
        light.effectTimer = 0.0;
        return light;
    }
    
    // Constructor for flickering lights (like torches)
    static Light createFlickeringLight(const Vec2& pos, const Color& col, double intens, double rad, double speed, double flickerAmount) {
        Light light = createPointLight(pos, col, intens, rad);
        light.type = LightType::Flickering;
        light.effectSpeed = speed;
        light.effectIntensity = flickerAmount;
        return light;
    }
    
    // Constructor for pulsing lights (like machinery)
    static Light createPulsingLight(const Vec2& pos, const Color& col, double intens, double rad, double speed) {
        Light light = createPointLight(pos, col, intens, rad);
        light.type = LightType::Pulsing;
        light.effectSpeed = speed;
        light.effectIntensity = 0.5; // Pulse by 50%
        return light;
    }
    
    // Constructor for strobe lights (like alarms)
    static Light createStrobeLight(const Vec2& pos, const Color& col, double intens, double rad, double speed) {
        Light light = createPointLight(pos, col, intens, rad);
        light.type = LightType::Strobe;
        light.effectSpeed = speed;
        light.effectIntensity = 1.0; // Full on/off
        return light;
    }
    
    // Constructor for glow lights (like lava)
    static Light createGlowLight(const Vec2& pos, const Color& col, double intens, double rad) {
        Light light = createPointLight(pos, col, intens, rad);
        light.type = LightType::Glow;
        light.effectSpeed = 0.5;
        light.effectIntensity = 0.2; // Subtle glow
        return light;
    }
};

class LightingSystem {
private:
    std::vector<Light> m_lights;
    Color m_ambientColor;
    double m_ambientIntensity;
    bool m_enabled;
    
    // Optimization: Pre-calculated ambient lighting
    Color m_cachedAmbientLight;
    
    // Culling parameters
    double m_cullDistance;     // Maximum distance to consider lights
    bool m_useCulling;         // Whether culling is enabled
    
    // Batch update parameters
    int m_updateFrequency;     // How many frames between light updates
    int m_updateCounter;       // Current update counter

    // Pre-calculated values for flicker effect to avoid computing them per-light
    double m_fastFlickerBase;
    double m_mediumFlickerBase;
    double m_slowFlickerBase;
    double m_flickerTimer;

public:
    LightingSystem() 
        : m_ambientColor(Color(64, 64, 96))  // Slight bluish ambient for doom-like atmosphere
        , m_ambientIntensity(0.3)           // Increased from 0.23 to 0.3 for better visibility
        , m_enabled(true)
        , m_cullDistance(15.0)               // Default culling distance
        , m_useCulling(true)                 // Enable culling by default
        , m_updateFrequency(3)               // Update lights every 3 frames
        , m_updateCounter(0)
        , m_fastFlickerBase(0.0)
        , m_mediumFlickerBase(0.0)
        , m_slowFlickerBase(0.0)
        , m_flickerTimer(0.0)
    {
        // Pre-calculate ambient light
        updateCachedAmbientLight();
    }

    // Add a new light
    int addLight(const Light& light) {
        m_lights.push_back(light);
        return m_lights.size() - 1;
    }

    // Remove a light
    void removeLight(int index) {
        if (index >= 0 && index < m_lights.size()) {
            m_lights.erase(m_lights.begin() + index);
        }
    }
    
    // Pre-calculate ambient lighting
    void updateCachedAmbientLight() {
        m_cachedAmbientLight = m_ambientColor * m_ambientIntensity;
        
        // Ensure minimum ambient light (prevent pitch black)
        m_cachedAmbientLight.r = std::max(15, static_cast<int>(m_cachedAmbientLight.r));
        m_cachedAmbientLight.g = std::max(15, static_cast<int>(m_cachedAmbientLight.g));
        m_cachedAmbientLight.b = std::max(15, static_cast<int>(m_cachedAmbientLight.b));
    }

    // Update flicker bases (called once per frame)
    void updateFlickerBases(double deltaTime) {
        m_flickerTimer += deltaTime;
        m_fastFlickerBase = sin(m_flickerTimer * 15.0) * 0.2;
        m_mediumFlickerBase = sin(m_flickerTimer * 7.0) * 0.15;
        m_slowFlickerBase = sin(m_flickerTimer * 3.0) * 0.1;
    }

    // Batch update lights (called once per frame)
    void updateLights(double deltaTime) {
        // Only update lights according to frequency
        if (++m_updateCounter >= m_updateFrequency) {
            m_updateCounter = 0;
            
            // Update flicker bases
            updateFlickerBases(deltaTime);
            
            // Update lights that need updating
            for (Light& light : m_lights) {
                if (!light.enabled) continue;
                
                // Update effect timer for special light types
                light.effectTimer += deltaTime * light.effectSpeed;
                
                // Handle different light types
                switch (light.type) {
                    case LightType::Point:
                        // Basic point light - just apply small flicker for realism
                        if (light.needsUpdate) {
                            // Use light's position to create variation
                            float positionOffset = (light.position.x + light.position.y) * 0.1f;
                            
                            // Combine pre-calculated flicker values with position offset
                            float flickerAmount = m_fastFlickerBase * 0.5f + 
                                               m_mediumFlickerBase * cos(positionOffset) * 0.3f + 
                                               m_slowFlickerBase * sin(positionOffset) * 0.2f;
                            
                            // Apply the flicker effect (very subtle for regular lights)
                            float baseIntensity = static_cast<float>(light.intensity);
                            light.intensity = std::max(0.5f, std::min(1.5f, baseIntensity + flickerAmount * 0.1f));
                        }
                        break;
                        
                    case LightType::Flickering:
                        // Flickering light (like torches)
                        {
                            // Calculate flicker based on noise functions
                            float noise1 = sin(light.effectTimer * 15.0f) * 0.5f;
                            float noise2 = sin(light.effectTimer * 7.3f + 1.5f) * 0.3f;
                            float noise3 = sin(light.effectTimer * 3.7f + 0.7f) * 0.2f;
                            
                            // Combine noise for natural flickering
                            float flicker = (noise1 + noise2 + noise3) * static_cast<float>(light.effectIntensity);
                            
                            // Apply flicker to base intensity
                            float baseIntensity = static_cast<float>(light.intensity);
                            light.intensity = std::max(0.2f, baseIntensity * (1.0f + flicker));
                        }
                        break;
                        
                    case LightType::Pulsing:
                        // Pulsing light (like machinery)
                        {
                            // Smooth sine wave pulsing
                            float pulse = sin(light.effectTimer * 2.0f) * static_cast<float>(light.effectIntensity);
                            
                            // Apply pulse to base intensity
                            float baseIntensity = static_cast<float>(light.intensity);
                            light.intensity = baseIntensity * (1.0f + pulse);
                        }
                        break;
                        
                    case LightType::Strobe:
                        // Strobe light (like alarms)
                        {
                            // Sharp on/off pattern
                            float strobe = (fmod(light.effectTimer, 1.0) < 0.5) ? 
                                static_cast<float>(light.effectIntensity) : 
                                -static_cast<float>(light.effectIntensity);
                            
                            // Apply strobe effect
                            float baseIntensity = static_cast<float>(light.intensity);
                            light.intensity = baseIntensity * (1.0f + strobe);
                        }
                        break;
                        
                    case LightType::Glow:
                        // Glow light (like lava)
                        {
                            // Slow, gentle pulsing
                            float glow = sin(light.effectTimer * 0.5f) * static_cast<float>(light.effectIntensity);
                            
                            // Apply glow effect
                            float baseIntensity = static_cast<float>(light.intensity);
                            light.intensity = baseIntensity * (1.0f + glow);
                            
                            // Also vary the radius slightly
                            float baseRadius = static_cast<float>(light.radius);
                            light.radius = baseRadius * (1.0f + glow * 0.2f);
                        }
                        break;
                        
                    case LightType::Directional:
                        // Directional lights don't need special updates
                        break;
                }
                
                // Ensure intensity stays in reasonable range
                light.intensity = std::max(0.1f, std::min(2.0f, static_cast<float>(light.intensity)));
            }
        }
    }

    // Calculate lighting at a point with culling optimization
    Color calculateLighting(const Vec2& position, const Vec2& normal, const Vec2& playerPosition) const {
        // If lighting is disabled, return full brightness
        if (!m_enabled) return Color::White();

        // Start with ambient lighting (pre-calculated)
        Color totalLight = m_cachedAmbientLight;

        // Add contribution from each light
        for (const Light& light : m_lights) {
            if (!light.enabled) continue;

            // Culling check for all point-like lights (Point, Flickering, Pulsing, Strobe, Glow)
            bool isPointLike = (light.type == LightType::Point || 
                               light.type == LightType::Flickering ||
                               light.type == LightType::Pulsing ||
                               light.type == LightType::Strobe ||
                               light.type == LightType::Glow);
                               
            if (m_useCulling && isPointLike) {
                // Skip if light is too far from player (performance optimization)
                double distanceToPlayer = (light.position - playerPosition).length();
                if (distanceToPlayer > m_cullDistance + light.radius) {
                    continue;
                }
                
                // Skip if light is too far from the point being lit
                double distanceToPoint = (light.position - position).length();
                if (distanceToPoint > light.radius) {
                    continue;
                }
            }

            double intensity = 0.0;
            Vec2 lightDir;

            // Handle all point-like lights similarly
            if (isPointLike) {
                // Calculate direction and distance to light
                Vec2 toLight = light.position - position;
                double distance = toLight.length();
                
                // Skip if beyond light radius (redundant but kept for safety)
                if (distance > light.radius) {
                    continue;
                }

                // Calculate attenuation
                double attenuation = 1.0 - (distance / light.radius);
                attenuation = std::max(0.0, std::min(1.0, attenuation));
                
                lightDir = toLight.normalized();
                intensity = light.intensity * attenuation;
            }
            else {  // Directional light
                lightDir = light.direction * -1;  // Flip direction since we want vector towards light
                intensity = light.intensity;
            }

            // Calculate diffuse lighting (dot product of normal and light direction)
            double diffuse = std::max(0.0, normal.x * lightDir.x + normal.y * lightDir.y);
            
            // Add this light's contribution
            Color lightContribution = light.color * (intensity * diffuse);
            totalLight = totalLight + lightContribution;
        }

        // Ensure the total light doesn't exceed maximum values
        totalLight.r = std::min(255, static_cast<int>(totalLight.r));
        totalLight.g = std::min(255, static_cast<int>(totalLight.g));
        totalLight.b = std::min(255, static_cast<int>(totalLight.b));
        
        // Ensure the total light doesn't go below minimum values (prevent pitch black)
        totalLight.r = std::max(30, static_cast<int>(totalLight.r));
        totalLight.g = std::max(30, static_cast<int>(totalLight.g));
        totalLight.b = std::max(30, static_cast<int>(totalLight.b));

        return totalLight;
    }

    // Backward compatibility version of calculateLighting
    Color calculateLighting(const Vec2& position, const Vec2& normal) const {
        // For backward compatibility - assume player is at (0,0) if not provided
        static const Vec2 defaultPlayerPos(0, 0);
        return calculateLighting(position, normal, defaultPlayerPos);
    }

    // Getters and setters
    void setAmbientColor(const Color& color) { 
        m_ambientColor = color; 
        updateCachedAmbientLight();
    }
    
    void setAmbientIntensity(double intensity) { 
        m_ambientIntensity = intensity; 
        updateCachedAmbientLight();
    }
    
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setCullingEnabled(bool enabled) { m_useCulling = enabled; }
    void setCullDistance(double distance) { m_cullDistance = distance; }
    void setUpdateFrequency(int frequency) { m_updateFrequency = std::max(1, frequency); }
    
    const std::vector<Light>& getLights() const { return m_lights; }
    Light* getLight(int index) {
        if (index >= 0 && index < m_lights.size()) {
            return &m_lights[index];
        }
        return nullptr;
    }

    // Get number of lights
    size_t getLightCount() const { return m_lights.size(); }

    // Get modifiable reference to light at index
    Light& getLightAt(size_t index) { return m_lights[index]; }
};

#endif // LIGHTING_H 