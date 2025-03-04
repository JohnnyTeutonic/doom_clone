#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>

// Platform-specific includes
#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// Constants
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = PI * 2.0;

// Utility functions
inline double deg2rad(double deg) { return deg * PI / 180.0; }
inline double rad2deg(double rad) { return rad * 180.0 / PI; }
inline double normalizeAngle(double angle) {
    angle = fmod(angle, TWO_PI);
    if (angle < 0) angle += TWO_PI;
    return angle;
}

// Simple 2D vector class
struct Vec2 {
    double x, y;
    
    Vec2() : x(0), y(0) {}
    Vec2(double x, double y) : x(x), y(y) {}
    
    double length() const { return sqrt(x*x + y*y); }
    double lengthSquared() const { return x*x + y*y; }
    
    Vec2 normalized() const {
        double len = length();
        if (len < 0.0001) return Vec2(0, 0);
        return Vec2(x / len, y / len);
    }
    
    void rotate(double angle) {
        double cosA = cos(angle);
        double sinA = sin(angle);
        double newX = x * cosA - y * sinA;
        double newY = x * sinA + y * cosA;
        x = newX;
        y = newY;
    }
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(double scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(double scalar) const { return Vec2(x / scalar, y / scalar); }
};

// Simple color class
struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }
    static Color White() { return Color(255, 255, 255); }
    static Color Black() { return Color(0, 0, 0); }
    static Color Gray() { return Color(128, 128, 128); }
    
    // Apply darkness factor (0.0 = black, 1.0 = original color)
    Color withLighting(double factor) const {
        factor = std::max(0.0, std::min(1.0, factor));
        return Color(
            static_cast<uint8_t>(r * factor),
            static_cast<uint8_t>(g * factor),
            static_cast<uint8_t>(b * factor),
            a
        );
    }

    // Apply ambient, diffuse, and specular lighting
    Color withAdvancedLighting(const Color& ambient, const Color& diffuse, const Color& specular, double intensity) const {
        intensity = std::max(0.0, std::min(1.0, intensity));
        return Color(
            static_cast<uint8_t>(std::min(255.0, 
                (r * ambient.r / 255.0) + 
                (r * diffuse.r / 255.0 * intensity) +
                (specular.r * intensity))),
            static_cast<uint8_t>(std::min(255.0, 
                (g * ambient.g / 255.0) + 
                (g * diffuse.g / 255.0 * intensity) +
                (specular.g * intensity))),
            static_cast<uint8_t>(std::min(255.0, 
                (b * ambient.b / 255.0) + 
                (b * diffuse.b / 255.0 * intensity) +
                (specular.b * intensity))),
            a
        );
    }

    // Add colors together (for multiple light sources)
    Color operator+(const Color& other) const {
        return Color(
            static_cast<uint8_t>(std::min(255, int(r) + int(other.r))),
            static_cast<uint8_t>(std::min(255, int(g) + int(other.g))),
            static_cast<uint8_t>(std::min(255, int(b) + int(other.b))),
            static_cast<uint8_t>(std::min(255, int(a) + int(other.a)))
        );
    }

    // Multiply colors (for light filtering)
    Color operator*(const Color& other) const {
        return Color(
            static_cast<uint8_t>((r * other.r) / 255),
            static_cast<uint8_t>((g * other.g) / 255),
            static_cast<uint8_t>((b * other.b) / 255),
            static_cast<uint8_t>((a * other.a) / 255)
        );
    }

    // Scale color by a factor
    Color operator*(double factor) const {
        factor = std::max(0.0, std::min(1.0, factor));
        return Color(
            static_cast<uint8_t>(r * factor),
            static_cast<uint8_t>(g * factor),
            static_cast<uint8_t>(b * factor),
            a
        );
    }
};

// Timer class for measuring elapsed time
class Timer {
private:
    std::chrono::high_resolution_clock::time_point m_start;
    std::chrono::high_resolution_clock::time_point m_end;
    
public:
    void start() {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    void stop() {
        m_end = std::chrono::high_resolution_clock::now();
    }
    
    double elapsedSeconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_end - m_start);
        return duration.count() / 1000000.0;
    }
    
    double elapsedMilliseconds() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_end - m_start);
        return duration.count() / 1000.0;
    }
};

// Sleep function (platform-independent)
inline void sleep_ms(unsigned int ms) {
#ifdef PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

#endif // UTILS_H 