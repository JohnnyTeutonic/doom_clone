#ifndef LIGHTING_H
#define LIGHTING_H

#include <vector>
#include <cmath>
#include <algorithm>
#include "utils.h"

enum class LightType {
    Point,      // Light emanating from a point (like explosions, items)
    Directional // Light coming from a direction (like sun/moon)
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
        , m_ambientIntensity(0.23)           // Increased from 0.2 to 0.23 (15% increase)
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
                if (!light.enabled || !light.needsUpdate) continue;
                
                if (light.type == LightType::Point) {
                    // Use light's position to create variation without expensive calculations
                    float positionOffset = (light.position.x + light.position.y) * 0.1f;
                    
                    // Combine pre-calculated flicker values with position offset
                    float flickerAmount = m_fastFlickerBase + 
                                       m_mediumFlickerBase * cos(positionOffset) + 
                                       m_slowFlickerBase * sin(positionOffset);
                    
                    // Add slight randomness (less frequently)
                    static int randomSkip = 0;
                    if (++randomSkip >= 10) {  // Only add randomness every 10 updates
                        flickerAmount += (rand() % 10) * 0.01f - 0.05f;
                        randomSkip = 0;
                        
                        // Vary radius less frequently and with less intensity
                        float radiusVariation = sin(m_flickerTimer + positionOffset) * 0.2f;
                        light.radius = std::max(1.0f, static_cast<float>(light.radius + radiusVariation));
                    }
                    
                    // Apply the flicker effect
                    float baseIntensity = light.intensity;
                    light.intensity = std::max(0.1f, std::min(1.0f, baseIntensity + flickerAmount));
                }
            }
        }
    }

    // Calculate lighting at a point with culling optimization
    Color calculateLighting(const Vec2& position, const Vec2& normal, const Vec2& playerPosition) const {
        if (!m_enabled) return Color::White();  // Return full brightness if lighting is disabled

        // Start with ambient lighting (pre-calculated)
        Color totalLight = m_cachedAmbientLight;

        // Add contribution from each light
        for (const Light& light : m_lights) {
            if (!light.enabled) continue;

            // Culling check for point lights
            if (m_useCulling && light.type == LightType::Point) {
                // Skip if light is too far from player (performance optimization)
                double distanceToPlayer = (light.position - playerPosition).length();
                if (distanceToPlayer > m_cullDistance + light.radius) continue;
                
                // Skip if light is too far from the point being lit
                double distanceToPoint = (light.position - position).length();
                if (distanceToPoint > light.radius) continue;
            }

            double intensity = 0.0;
            Vec2 lightDir;

            if (light.type == LightType::Point) {
                // Calculate direction and distance to light
                Vec2 toLight = light.position - position;
                double distance = toLight.length();
                
                // Skip if beyond light radius (redundant but kept for safety)
                if (distance > light.radius) continue;

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
            totalLight = totalLight + (light.color * (intensity * diffuse));
        }

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