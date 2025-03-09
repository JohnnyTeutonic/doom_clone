#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <functional>
#include <random>
#include <chrono>

// For SDL integration
#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

// Constants
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = PI * 2.0;
constexpr double HALF_PI = PI / 2.0;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;
constexpr double EPSILON = 1e-6;

// =========================================
// Vector and math utilities
// =========================================

struct Vec2 {
    double x, y;

    Vec2() : x(0.0), y(0.0) {}
    Vec2(double x, double y) : x(x), y(y) {}

    // Vector operations
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(double scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(double scalar) const { return Vec2(x / scalar, y / scalar); }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(double scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(double scalar) { x /= scalar; y /= scalar; return *this; }

    // Vector math
    double length() const { return std::sqrt(x * x + y * y); }
    double lengthSquared() const { return x * x + y * y; }
    
    Vec2 normalized() const {
        double len = length();
        if (len < EPSILON) return Vec2(0, 0);
        return Vec2(x / len, y / len);
    }
    
    void normalize() {
        double len = length();
        if (len < EPSILON) return;
        x /= len;
        y /= len;
    }
    
    double dot(const Vec2& other) const { return x * other.x + y * other.y; }
    
    // 2D cross product (returns the z component)
    double cross(const Vec2& other) const { return x * other.y - y * other.x; }
    
    // Rotate vector by angle (in radians)
    Vec2 rotated(double angle) const {
        double c = std::cos(angle);
        double s = std::sin(angle);
        return Vec2(x * c - y * s, x * s + y * c);
    }
    
    void rotate(double angle) {
        double c = std::cos(angle);
        double s = std::sin(angle);
        double newX = x * c - y * s;
        double newY = x * s + y * c;
        x = newX;
        y = newY;
    }
    
    // Perpendicular vector (90 degrees counterclockwise)
    Vec2 perpendicular() const { return Vec2(-y, x); }
    
    // Angle between vectors
    double angle(const Vec2& other) const {
        return std::atan2(cross(other), dot(other));
    }
    
    // Angle from positive x-axis
    double angle() const { return std::atan2(y, x); }
};

// Line segment
struct LineSegment {
    Vec2 start, end;
    
    LineSegment() = default;
    LineSegment(const Vec2& start, const Vec2& end) : start(start), end(end) {}
    
    // Get direction vector
    Vec2 direction() const { return end - start; }
    
    // Get length
    double length() const { return direction().length(); }
    
    // Get normal vector (perpendicular to the line, pointing outward)
    Vec2 normal() const {
        Vec2 dir = direction().normalized();
        return Vec2(-dir.y, dir.x);
    }
    
    // Check if point is on line segment
    bool containsPoint(const Vec2& point, double tolerance = EPSILON) const {
        Vec2 p1 = start;
        Vec2 p2 = end;
        Vec2 p = point;
        
        double d1 = (p - p1).length();
        double d2 = (p - p2).length();
        double lineLen = (p2 - p1).length();
        
        return std::abs(d1 + d2 - lineLen) <= tolerance;
    }
};

// Rectangle
struct Rect {
    double x, y, width, height;
    
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(double x, double y, double width, double height) 
        : x(x), y(y), width(width), height(height) {}
    
    // Check if point is inside rectangle
    bool contains(const Vec2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }
    
    // Check if rectangle intersects with another
    bool intersects(const Rect& other) const {
        return x < other.x + other.width && x + width > other.x &&
               y < other.y + other.height && y + height > other.y;
    }
    
    // Get center of rectangle
    Vec2 center() const { return Vec2(x + width / 2, y + height / 2); }
    
    // Convert to SDL_Rect
    SDL_Rect toSDLRect() const {
        SDL_Rect rect;
        rect.x = static_cast<int>(x);
        rect.y = static_cast<int>(y);
        rect.w = static_cast<int>(width);
        rect.h = static_cast<int>(height);
        return rect;
    }
};

// Circle
struct Circle {
    Vec2 center;
    double radius;
    
    Circle() : center(), radius(0) {}
    Circle(const Vec2& center, double radius) : center(center), radius(radius) {}
    
    // Check if point is inside circle
    bool contains(const Vec2& point) const {
        return (point - center).lengthSquared() <= radius * radius;
    }
    
    // Check if circle intersects with another
    bool intersects(const Circle& other) const {
        double distSq = (center - other.center).lengthSquared();
        double sumRadii = radius + other.radius;
        return distSq <= sumRadii * sumRadii;
    }
};

// =========================================
// Math utilities
// =========================================

// Clamp value between min and max
template<typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Linear interpolation
template<typename T>
T lerp(const T& a, const T& b, double t) {
    return a + static_cast<T>((b - a) * t);
}

// Normalize angle to [0, 2π)
inline double normalizeAngle(double angle) {
    angle = std::fmod(angle, TWO_PI);
    if (angle < 0.0) angle += TWO_PI;
    return angle;
}

// Normalize angle to [-π, π)
inline double normalizeAngleSigned(double angle) {
    angle = std::fmod(angle, TWO_PI);
    if (angle > PI) angle -= TWO_PI;
    if (angle < -PI) angle += TWO_PI;
    return angle;
}

// Get shortest angle between two angles
inline double angleDifference(double a, double b) {
    return normalizeAngleSigned(a - b);
}

// Check if two values are approximately equal
template<typename T>
bool approxEqual(T a, T b, T epsilon = EPSILON) {
    return std::abs(a - b) <= epsilon;
}

// Check if a value is approximately zero
template<typename T>
bool approxZero(T value, T epsilon = EPSILON) {
    return std::abs(value) <= epsilon;
}

// Check if two line segments intersect and calculate intersection point
inline bool lineSegmentIntersection(const LineSegment& a, const LineSegment& b, Vec2& intersection) {
    Vec2 p1 = a.start;
    Vec2 p2 = a.end;
    Vec2 p3 = b.start;
    Vec2 p4 = b.end;
    
    double denominator = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);
    
    if (approxZero(denominator)) {
        // Lines are parallel or coincident
        return false;
    }
    
    double ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denominator;
    double ub = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denominator;
    
    if (ua >= 0 && ua <= 1 && ub >= 0 && ub <= 1) {
        intersection.x = p1.x + ua * (p2.x - p1.x);
        intersection.y = p1.y + ua * (p2.y - p1.y);
        return true;
    }
    
    return false;
}

// Random number generator
class Random {
private:
    std::mt19937 generator;

public:
    // Initialize with random seed
    Random() {
        auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        generator = std::mt19937(static_cast<unsigned int>(seed));
    }
    
    // Initialize with specific seed
    Random(unsigned int seed) : generator(seed) {}
    
    // Random integer in range [min, max]
    int getInt(int min, int max) {
        std::uniform_int_distribution<int> distribution(min, max);
        return distribution(generator);
    }
    
    // Random float in range [min, max]
    float getFloat(float min, float max) {
        std::uniform_real_distribution<float> distribution(min, max);
        return distribution(generator);
    }
    
    // Random double in range [min, max]
    double getDouble(double min, double max) {
        std::uniform_real_distribution<double> distribution(min, max);
        return distribution(generator);
    }
    
    // Random boolean with probability
    bool getBool(double probability = 0.5) {
        std::bernoulli_distribution distribution(probability);
        return distribution(generator);
    }
    
    // Get normalized random direction vector
    Vec2 getDirection() {
        double angle = getDouble(0, TWO_PI);
        return Vec2(std::cos(angle), std::sin(angle));
    }
};

// =========================================
// Color utilities
// =========================================

struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    
    // Create color from packed RGBA value
    static Color fromRGBA(uint32_t rgba) {
        return Color(
            (rgba >> 24) & 0xFF,
            (rgba >> 16) & 0xFF,
            (rgba >> 8) & 0xFF,
            rgba & 0xFF
        );
    }
    
    // Pack to RGBA value
    uint32_t toRGBA() const {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8) |
               static_cast<uint32_t>(a);
    }
    
    // Blend with another color (alpha blending)
    Color blend(const Color& other) const {
        float a1 = a / 255.0f;
        float a2 = other.a / 255.0f;
        float a3 = a1 + a2 * (1.0f - a1);
        
        if (a3 < EPSILON) return Color(0, 0, 0, 0);
        
        float r3 = (r * a1 + other.r * a2 * (1.0f - a1)) / a3;
        float g3 = (g * a1 + other.g * a2 * (1.0f - a1)) / a3;
        float b3 = (b * a1 + other.b * a2 * (1.0f - a1)) / a3;
        
        return Color(
            static_cast<uint8_t>(r3),
            static_cast<uint8_t>(g3),
            static_cast<uint8_t>(b3),
            static_cast<uint8_t>(a3 * 255)
        );
    }
    
    // Lerp between colors
    static Color lerp(const Color& a, const Color& b, float t) {
        t = clamp(t, 0.0f, 1.0f);
        return Color(
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<uint8_t>(a.a + (b.a - a.a) * t)
        );
    }
};

// Predefined colors as global constants for easy access
namespace Colors {
    const Color BLACK(0, 0, 0);
    const Color WHITE(255, 255, 255);
    const Color RED(255, 0, 0);
    const Color GREEN(0, 255, 0);
    const Color BLUE(0, 0, 255);
    const Color YELLOW(255, 255, 0);
    const Color CYAN(0, 255, 255);
    const Color MAGENTA(255, 0, 255);
    const Color TRANSPARENT(0, 0, 0, 0);
}

// =========================================
// Timer and profiling utilities
// =========================================

class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> pauseTime;
    bool paused;

public:
    Timer() : paused(false) {
        reset();
    }
    
    void reset() {
        startTime = std::chrono::high_resolution_clock::now();
        paused = false;
    }
    
    void pause() {
        if (!paused) {
            pauseTime = std::chrono::high_resolution_clock::now();
            paused = true;
        }
    }
    
    void resume() {
        if (paused) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            startTime += currentTime - pauseTime;
            paused = false;
        }
    }
    
    // Get elapsed time in seconds
    double getElapsedSeconds() const {
        if (paused) {
            return std::chrono::duration<double>(pauseTime - startTime).count();
        } else {
            return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - startTime).count();
        }
    }
    
    // Get elapsed time in milliseconds
    double getElapsedMilliseconds() const {
        return getElapsedSeconds() * 1000.0;
    }
};

// Simple profiler for measuring execution time of code sections
class Profiler {
private:
    struct ProfileSample {
        std::string name;
        double startTime;
        double elapsedTime;
        int calls;
    };
    
    std::unordered_map<std::string, ProfileSample> samples;
    Timer timer;
    
public:
    Profiler() {
        timer.reset();
    }
    
    // Start profiling a section
    void beginSample(const std::string& name) {
        if (samples.find(name) == samples.end()) {
            samples[name] = {name, timer.getElapsedSeconds(), 0.0, 0};
        } else {
            samples[name].startTime = timer.getElapsedSeconds();
        }
    }
    
    // End profiling a section
    void endSample(const std::string& name) {
        if (samples.find(name) != samples.end()) {
            double endTime = timer.getElapsedSeconds();
            samples[name].elapsedTime += endTime - samples[name].startTime;
            samples[name].calls++;
        }
    }
    
    // Get profile results
    std::vector<std::pair<std::string, double>> getResults() const {
        std::vector<std::pair<std::string, double>> results;
        for (const auto& pair : samples) {
            results.push_back({pair.first, pair.second.elapsedTime});
        }
        
        // Sort by elapsed time (descending)
        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        return results;
    }
    
    // Reset all samples
    void reset() {
        samples.clear();
        timer.reset();
    }
};

#endif // UTILS_H 