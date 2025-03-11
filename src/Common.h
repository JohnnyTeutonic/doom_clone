#ifndef COMMON_H
#define COMMON_H

#include <cmath>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>

// Only define these if utils.h hasn't been included already
#ifndef UTILS_H

// Math constants
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double HALF_PI = PI / 2.0;

// Vector2D structure for 2D coordinates and vectors
struct Vec2 {
    double x;
    double y;
    
    Vec2() : x(0), y(0) {}
    Vec2(double _x, double _y) : x(_x), y(_y) {}
    
    // Vector operations
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(double scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(double scalar) const { return Vec2(x / scalar, y / scalar); }
    
    // Dot product
    double dot(const Vec2& other) const { return x * other.x + y * other.y; }
    
    // Cross product (z-component only for 2D vectors)
    double cross(const Vec2& other) const { return x * other.y - y * other.x; }
    
    // Vector length
    double length() const { return std::sqrt(x * x + y * y); }
    
    // Normalized vector
    Vec2 normalized() const {
        double len = length();
        if (len < 1e-6) return Vec2(0, 0);
        return Vec2(x / len, y / len);
    }
    
    // Distance to another point
    double distanceTo(const Vec2& other) const {
        return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
    }
    
    // Rotate vector by angle (in radians)
    Vec2 rotated(double angle) const {
        double cs = std::cos(angle);
        double sn = std::sin(angle);
        return Vec2(x * cs - y * sn, x * sn + y * cs);
    }
};

// Line structure for 2D line segments
struct Line {
    Vec2 start;
    Vec2 end;
    
    Line() = default;
    Line(const Vec2& _start, const Vec2& _end) : start(_start), end(_end) {}
    
    // Line direction vector
    Vec2 direction() const { return end - start; }
    
    // Line length
    double length() const { return start.distanceTo(end); }
    
    // Check if a point is on the line segment (within epsilon)
    bool containsPoint(const Vec2& point, double epsilon = 0.001) const {
        double d1 = point.distanceTo(start);
        double d2 = point.distanceTo(end);
        double lineLen = length();
        return std::abs(d1 + d2 - lineLen) < epsilon;
    }
    
    // Get the closest point on the line to a given point
    Vec2 closestPoint(const Vec2& point) const {
        Vec2 dir = direction();
        double len = dir.length();
        if (len < 1e-6) return start; // degenerate line
        
        dir = dir / len; // normalize
        double t = dir.dot(point - start);
        
        if (t < 0) return start;
        if (t > len) return end;
        
        return start + dir * t;
    }
    
    // Distance from a point to the line
    double distanceToPoint(const Vec2& point) const {
        return point.distanceTo(closestPoint(point));
    }
    
    // Check if this line intersects with another line
    bool intersects(const Line& other, Vec2* intersection = nullptr) const {
        // Line 1 represented as a1x + b1y = c1
        double a1 = end.y - start.y;
        double b1 = start.x - end.x;
        double c1 = a1 * start.x + b1 * start.y;
        
        // Line 2 represented as a2x + b2y = c2
        double a2 = other.end.y - other.start.y;
        double b2 = other.start.x - other.end.x;
        double c2 = a2 * other.start.x + b2 * other.start.y;
        
        double determinant = a1 * b2 - a2 * b1;
        
        if (std::abs(determinant) < 1e-6) {
            // Lines are parallel
            return false;
        }
        
        // Find intersection point
        Vec2 intersectionPoint(
            (b2 * c1 - b1 * c2) / determinant,
            (a1 * c2 - a2 * c1) / determinant
        );
        
        // Check if the intersection point is on both line segments
        if (containsPoint(intersectionPoint) && other.containsPoint(intersectionPoint)) {
            if (intersection) *intersection = intersectionPoint;
            return true;
        }
        
        return false;
    }
};

// Color structure for RGBA color representation
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255) 
        : r(_r), g(_g), b(_b), a(_a) {}
        
    // Convert to uint32_t RGBA
    uint32_t toRGBA() const {
        return (static_cast<uint32_t>(r) << 24) | 
               (static_cast<uint32_t>(g) << 16) | 
               (static_cast<uint32_t>(b) << 8) | 
               static_cast<uint32_t>(a);
    }
    
    // Blend with another color
    Color blend(const Color& other) const {
        float srcAlpha = other.a / 255.0f;
        float destAlpha = a / 255.0f;
        float outAlpha = srcAlpha + destAlpha * (1.0f - srcAlpha);
        
        if (outAlpha < 0.001f) {
            return Color(0, 0, 0, 0);
        }
        
        float srcFactor = srcAlpha / outAlpha;
        float destFactor = destAlpha * (1.0f - srcAlpha) / outAlpha;
        
        return Color(
            static_cast<uint8_t>(other.r * srcFactor + r * destFactor),
            static_cast<uint8_t>(other.g * srcFactor + g * destFactor),
            static_cast<uint8_t>(other.b * srcFactor + b * destFactor),
            static_cast<uint8_t>(outAlpha * 255.0f)
        );
    }
    
    // Apply brightness factor
    Color withBrightness(float factor) const {
        return Color(
            static_cast<uint8_t>(std::min(255.0f, r * factor)),
            static_cast<uint8_t>(std::min(255.0f, g * factor)),
            static_cast<uint8_t>(std::min(255.0f, b * factor)),
            a
        );
    }
};

// Rectangle structure
struct Rect {
    double x, y;
    double width, height;
    
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(double _x, double _y, double _w, double _h) 
        : x(_x), y(_y), width(_w), height(_h) {}
        
    // Check if a point is inside the rectangle
    bool contains(const Vec2& point) const {
        return point.x >= x && point.x < x + width &&
               point.y >= y && point.y < y + height;
    }
    
    // Check if this rectangle intersects with another
    bool intersects(const Rect& other) const {
        return x < other.x + other.width && x + width > other.x &&
               y < other.y + other.height && y + height > other.y;
    }
};

// Direction enumeration
enum class Side {
    NONE,
    LEFT,
    RIGHT,
    TOP,
    BOTTOM
};

// Utility function to clamp a value between min and max
template<typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Normalize an angle to the range [0, 2π)
inline double normalizeAngle(double angle) {
    angle = std::fmod(angle, TWO_PI);
    if (angle < 0.0) angle += TWO_PI;
    return angle;
}

// Calculate the smallest difference between two angles
inline double angleDifference(double a, double b) {
    double diff = std::fmod(std::abs(a - b), TWO_PI);
    if (diff > PI) diff = TWO_PI - diff;
    return diff;
}

// Define some common colors
namespace Colors {
    const Color BLACK(0, 0, 0);
    const Color WHITE(255, 255, 255);
    const Color RED(255, 0, 0);
    const Color GREEN(0, 255, 0);
    const Color BLUE(0, 0, 255);
    const Color YELLOW(255, 255, 0);
    const Color CYAN(0, 255, 255);
    const Color MAGENTA(255, 0, 255);
    const Color GRAY(128, 128, 128);
    const Color DARK_GRAY(64, 64, 64);
    const Color LIGHT_GRAY(192, 192, 192);
}

#endif // !UTILS_H

#endif // COMMON_H 