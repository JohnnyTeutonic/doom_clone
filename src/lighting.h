#ifndef LIGHTING_H
#define LIGHTING_H

#include <vector>
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

    // Constructor for point lights
    static Light createPointLight(const Vec2& pos, const Color& col, double intens, double rad) {
        Light light;
        light.type = LightType::Point;
        light.position = pos;
        light.color = col;
        light.intensity = intens;
        light.radius = rad;
        light.enabled = true;
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
        return light;
    }
};

class LightingSystem {
private:
    std::vector<Light> m_lights;
    Color m_ambientColor;
    double m_ambientIntensity;
    bool m_enabled;

public:
    LightingSystem() 
        : m_ambientColor(Color(64, 64, 96))  // Slight bluish ambient for doom-like atmosphere
        , m_ambientIntensity(0.2)            // Low ambient light by default
        , m_enabled(true) {}

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

    // Calculate lighting at a point
    Color calculateLighting(const Vec2& position, const Vec2& normal) const {
        if (!m_enabled) return Color::White();  // Return full brightness if lighting is disabled

        // Start with ambient lighting
        Color totalLight = m_ambientColor * m_ambientIntensity;

        // Add contribution from each light
        for (const Light& light : m_lights) {
            if (!light.enabled) continue;

            double intensity = 0.0;
            Vec2 lightDir;

            if (light.type == LightType::Point) {
                // Calculate direction and distance to light
                Vec2 toLight = light.position - position;
                double distance = toLight.length();
                
                // Skip if beyond light radius
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

    // Getters and setters
    void setAmbientColor(const Color& color) { m_ambientColor = color; }
    void setAmbientIntensity(double intensity) { m_ambientIntensity = intensity; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    const std::vector<Light>& getLights() const { return m_lights; }
    Light* getLight(int index) {
        if (index >= 0 && index < m_lights.size()) {
            return &m_lights[index];
        }
        return nullptr;
    }
};

#endif // LIGHTING_H 