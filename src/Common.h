#ifndef COMMON_H
#define COMMON_H

#include <cmath>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>

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
    
    // Cross product (technically a scalar in 2D)
    double cross(const Vec2& other) const { return x * other.y - y * other.x; }
    
    // Length/magnitude
    double length() const { return std::sqrt(x * x + y * y); }
    
    // Normalized vector
    Vec2 normalized() const {
        double len = length();
        return len > 0 ? Vec2(x / len, y / len) : Vec2(0, 0);
    }
    
    // Distance to another point
    double distanceTo(const Vec2& other) const {
        return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
    }
    
    // Rotate vector by angle (in radians)
    Vec2 rotated(double angle) const {
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);
        return Vec2(x * cosA - y * sinA, x * sinA + y * cosA);
    }
};

// Line segment between two points
struct Line {
    Vec2 start;
    Vec2 end;
    
    Line() = default;
    Line(const Vec2& _start, const Vec2& _end) : start(_start), end(_end) {}
    
    // Line direction vector
    Vec2 direction() const { return end - start; }
    
    // Line length
    double length() const { return start.distanceTo(end); }
    
    // Check if point is on line
    bool containsPoint(const Vec2& point, double epsilon = 0.001) const {
        // Check if point is collinear and within segment bounds
        double crossProduct = (point.y - start.y) * (end.x - start.x) - 
                              (point.x - start.x) * (end.y - start.y);
                              
        if (std::abs(crossProduct) > epsilon)
            return false;
            
        double dotProduct = (point.x - start.x) * (end.x - start.x) + 
                           (point.y - start.y) * (end.y - start.y);
                           
        if (dotProduct < 0)
            return false;
            
        double squaredLength = (end.x - start.x) * (end.x - start.x) + 
                              (end.y - start.y) * (end.y - start.y);
                              
        return dotProduct <= squaredLength;
    }
    
    // Get closest point on line to a given point
    Vec2 closestPoint(const Vec2& point) const {
        Vec2 dir = direction();
        double len2 = dir.dot(dir);
        
        // If line is just a point, return that
        if (len2 < 0.0000001)
            return start;
            
        // Project point onto line
        double t = std::max(0.0, std::min(1.0, (point - start).dot(dir) / len2));
        return start + dir * t;
    }
    
    // Distance from a point to the line
    double distanceToPoint(const Vec2& point) const {
        return point.distanceTo(closestPoint(point));
    }
    
    // Check if line intersects with another line
    bool intersects(const Line& other, Vec2* intersection = nullptr) const {
        // Calculate denominators
        double den = (other.end.y - other.start.y) * (end.x - start.x) - 
                    (other.end.x - other.start.x) * (end.y - start.y);
                    
        if (den == 0)
            return false;  // Lines are parallel
            
        double ua = ((other.end.x - other.start.x) * (start.y - other.start.y) - 
                    (other.end.y - other.start.y) * (start.x - other.start.x)) / den;
        double ub = ((end.x - start.x) * (start.y - other.start.y) - 
                    (end.y - start.y) * (start.x - other.start.x)) / den;
                    
        // Check if intersection is within both line segments
        if (ua < 0 || ua > 1 || ub < 0 || ub > 1)
            return false;
            
        // Calculate intersection point if needed
        if (intersection) {
            intersection->x = start.x + ua * (end.x - start.x);
            intersection->y = start.y + ua * (end.y - start.y);
        }
        
        return true;
    }
};

// RGBA Color 
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
        // Simple alpha blending
        float alpha = other.a / 255.0f;
        return Color(
            static_cast<uint8_t>(r * (1 - alpha) + other.r * alpha),
            static_cast<uint8_t>(g * (1 - alpha) + other.g * alpha),
            static_cast<uint8_t>(b * (1 - alpha) + other.b * alpha),
            a
        );
    }
    
    // Create color with modified brightness
    Color withBrightness(float factor) const {
        return Color(
            static_cast<uint8_t>(std::min(255.0f, r * factor)),
            static_cast<uint8_t>(std::min(255.0f, g * factor)),
            static_cast<uint8_t>(std::min(255.0f, b * factor)),
            a
        );
    }
};

// Rectangle
struct Rect {
    double x, y;
    double width, height;
    
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(double _x, double _y, double _w, double _h) 
        : x(_x), y(_y), width(_w), height(_h) {}
        
    bool contains(const Vec2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }
    
    bool intersects(const Rect& other) const {
        return !(x + width < other.x || other.x + other.width < x ||
                 y + height < other.y || other.y + other.height < y);
    }
};

// Side enum for BSP
enum class Side {
    FRONT,
    BACK,
    ON,
    SPANNING
};

// Utility functions
template <typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Linear interpolation
template <typename T>
T lerp(T a, T b, float t) {
    return a + (b - a) * t;
}

// Angle normalization
inline double normalizeAngle(double angle) {
    while (angle >= TWO_PI) angle -= TWO_PI;
    while (angle < 0) angle += TWO_PI;
    return angle;
}

// Distance between angles
inline double angleDifference(double a, double b) {
    double diff = std::abs(a - b);
    return std::min(diff, TWO_PI - diff);
}

#endif // COMMON_H 