#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

// Include SDL headers for SDL_Surface declarations
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// Platform-specific includes
#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#if defined(__SSE2__) || defined(_MSC_VER)
#include <emmintrin.h>
#include <xmmintrin.h>

// Forward declarations for bit vector operations
namespace optimized {
    // Bit vector operations
    void bitvector_copy(uint64_t* dst, const uint64_t* src, size_t n_words);
    void bitvector_clear(uint64_t* bits, size_t n_words);
    void bitvector_set_all(uint64_t* bits, size_t n_words);
    void bitvector_and(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words);
    void bitvector_or(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words);
    void bitvector_xor(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words);
    
    // SSE optimized vector operations
    // Fill an array of floats with a constant value (replaces std::__fill_a1<float*, float>)
    inline void fill_floats(float* begin, float* end, float value) {
        const size_t totalSize = end - begin;
        // Nothing to do if array is empty
        if (totalSize <= 0) return;
        
        // Create an SSE register with our value repeated 4 times
        __m128 val_sse = _mm_set1_ps(value);
        
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = totalSize / 4;
        
        // Fill the array 4 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_ps(begin + i * 4, val_sse);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 4; i < totalSize; ++i) {
            begin[i] = value;
        }
    }
    
    // Fill an array of unsigned ints with a constant value
    inline void fill_uints(uint32_t* begin, uint32_t* end, uint32_t value) {
        const size_t totalSize = end - begin;
        if (totalSize <= 0) return;
        
        // Create an SSE register with our value repeated 4 times
        __m128i val_sse = _mm_set1_epi32(value);
        
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = totalSize / 4;
        
        // Fill the array 4 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_si128((__m128i*)(begin + i * 4), val_sse);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 4; i < totalSize; ++i) {
            begin[i] = value;
        }
    }

    // Fill an array of ints with a constant value
    inline void fill_ints(int* begin, int* end, int value) {
        const size_t totalSize = end - begin;
        if (totalSize <= 0) return;
        
        // Create an SSE register with our value repeated 4 times
        __m128i val_sse = _mm_set1_epi32(value);
        
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = totalSize / 4;
        
        // Fill the array 4 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_si128((__m128i*)(begin + i * 4), val_sse);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 4; i < totalSize; ++i) {
            begin[i] = value;
        }
    }

    // Fill an array of doubles with a constant value
    inline void fill_doubles(double* begin, double* end, double value) {
        const size_t totalSize = end - begin;
        if (totalSize <= 0) return;
        
        // For doubles, we use SSE2 instructions
        __m128d val_sse = _mm_set1_pd(value);
        
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = totalSize / 2;  // SSE processes 2 doubles at a time
        
        // Fill the array 2 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            _mm_storeu_pd(begin + i * 2, val_sse);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 2; i < totalSize; ++i) {
            begin[i] = value;
        }
    }
    
    // Fast copy for float arrays
    inline void copy_floats(float* dest, const float* src, size_t count) {
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = count / 4;
        
        // Copy 4 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            __m128 src_data = _mm_loadu_ps(src + i * 4);
            _mm_storeu_ps(dest + i * 4, src_data);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 4; i < count; ++i) {
            dest[i] = src[i];
        }
    }
    
    // Fast copy for uint32_t arrays
    inline void copy_uints(uint32_t* dest, const uint32_t* src, size_t count) {
        // Calculate how many complete vector operations we can do
        const size_t vectorizedSize = count / 4;
        
        // Copy 4 elements at a time using SSE
        for (size_t i = 0; i < vectorizedSize; ++i) {
            __m128i src_data = _mm_loadu_si128((__m128i*)(src + i * 4));
            _mm_storeu_si128((__m128i*)(dest + i * 4), src_data);
        }
        
        // Handle the remaining elements (if any)
        for (size_t i = vectorizedSize * 4; i < count; ++i) {
            dest[i] = src[i];
        }
    }
}
#else
// Fallback implementations for systems without SSE
namespace optimized {
    // Bit vector operations without SSE
    inline void bitvector_copy(uint64_t* dst, const uint64_t* src, size_t n_words) {
        std::copy(src, src + n_words, dst);
    }
    
    inline void bitvector_clear(uint64_t* bits, size_t n_words) {
        std::fill(bits, bits + n_words, 0ULL);
    }
    
    inline void bitvector_set_all(uint64_t* bits, size_t n_words) {
        std::fill(bits, bits + n_words, ~0ULL);
    }
    
    inline void bitvector_and(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] & b[i];
        }
    }
    
    inline void bitvector_or(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] | b[i];
        }
    }
    
    inline void bitvector_xor(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] ^ b[i];
        }
    }
    
    // Fill operations without SSE
    inline void fill_floats(float* begin, float* end, float value) {
        std::fill(begin, end, value);
    }
    
    inline void fill_uints(uint32_t* begin, uint32_t* end, uint32_t value) {
        std::fill(begin, end, value);
    }
    
    inline void fill_ints(int* begin, int* end, int value) {
        std::fill(begin, end, value);
    }
    
    inline void fill_doubles(double* begin, double* end, double value) {
        std::fill(begin, end, value);
    }
    
    inline void copy_floats(float* dest, const float* src, size_t count) {
        std::copy(src, src + count, dest);
    }
    
    inline void copy_uints(uint32_t* dest, const uint32_t* src, size_t count) {
        std::copy(src, src + count, dest);
    }
}
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
    Vec2 operator-() const { return Vec2(-x, -y); }
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

// Helper function to create a DOOM-style flat texture (for floors/ceilings)
SDL_Surface* createDoomFlatTexture(int width, int height, bool isFloor);

// Helper function to load an animated WEBP file and extract its frames
std::vector<SDL_Surface*> loadAnimatedWebp(const std::string& filename);

// Helper function to check if a WEBP file is animated
bool isWebpAnimated(const std::string& filename);

// Helper function to get the number of frames in an animated WEBP file
int getWebpFrameCount(const std::string& filename);

// Helper function to load a specific frame from an animated WEBP file
SDL_Surface* loadWebpFrame(const std::string& filename, int frameIndex);

// Fast BitVector class to replace std::vector<bool> and avoid the overhead of
// std::_Bvector_base<std::allocator<bool>>::_Bvector_impl::~_Bvector_impl()
class BitVector {
private:
    uint64_t* m_bits;
    size_t m_size;
    size_t m_wordCount;
    
    // Calculate number of 64-bit words needed to store n bits
    static size_t calcWordCount(size_t n) {
        return (n + 63) / 64;
    }
    
public:
    BitVector() : m_bits(nullptr), m_size(0), m_wordCount(0) {}
    
    explicit BitVector(size_t size, bool initialValue = false) : m_size(size), m_wordCount(calcWordCount(size)) {
        if (m_wordCount == 0) {
            m_bits = nullptr;
            return;
        }
        
        m_bits = new uint64_t[m_wordCount];
        
        #if defined(__SSE2__) || defined(_MSC_VER)
        if (initialValue) {
            optimized::bitvector_set_all(m_bits, m_wordCount);
        } else {
            optimized::bitvector_clear(m_bits, m_wordCount);
        }
        #else
        if (initialValue) {
            std::fill(m_bits, m_bits + m_wordCount, ~0ULL);
        } else {
            std::fill(m_bits, m_bits + m_wordCount, 0ULL);
        }
        #endif
    }
    
    // Copy constructor
    BitVector(const BitVector& other) : m_size(other.m_size), m_wordCount(other.m_wordCount) {
        if (m_wordCount == 0) {
            m_bits = nullptr;
            return;
        }
        
        m_bits = new uint64_t[m_wordCount];
        
        #if defined(__SSE2__) || defined(_MSC_VER)
        optimized::bitvector_copy(m_bits, other.m_bits, m_wordCount);
        #else
        std::copy(other.m_bits, other.m_bits + m_wordCount, m_bits);
        #endif
    }
    
    // Move constructor
    BitVector(BitVector&& other) noexcept : m_bits(other.m_bits), m_size(other.m_size), m_wordCount(other.m_wordCount) {
        other.m_bits = nullptr;
        other.m_size = 0;
        other.m_wordCount = 0;
    }
    
    // Copy assignment
    BitVector& operator=(const BitVector& other) {
        if (this != &other) {
            delete[] m_bits;
            
            m_size = other.m_size;
            m_wordCount = other.m_wordCount;
            
            if (m_wordCount == 0) {
                m_bits = nullptr;
                return *this;
            }
            
            m_bits = new uint64_t[m_wordCount];
            
            #if defined(__SSE2__) || defined(_MSC_VER)
            optimized::bitvector_copy(m_bits, other.m_bits, m_wordCount);
            #else
            std::copy(other.m_bits, other.m_bits + m_wordCount, m_bits);
            #endif
        }
        return *this;
    }
    
    // Move assignment
    BitVector& operator=(BitVector&& other) noexcept {
        if (this != &other) {
            delete[] m_bits;
            
            m_bits = other.m_bits;
            m_size = other.m_size;
            m_wordCount = other.m_wordCount;
            
            other.m_bits = nullptr;
            other.m_size = 0;
            other.m_wordCount = 0;
        }
        return *this;
    }
    
    ~BitVector() {
        delete[] m_bits;
    }
    
    // Proxy class for bit reference
    class BitReference {
    private:
        uint64_t* m_word;
        uint64_t m_mask;
        
    public:
        BitReference(uint64_t* word, size_t bit) : m_word(word), m_mask(1ULL << bit) {}
        
        // Conversion to bool
        operator bool() const {
            return (*m_word & m_mask) != 0;
        }
        
        // Assignment
        BitReference& operator=(bool value) {
            if (value) {
                *m_word |= m_mask;
            } else {
                *m_word &= ~m_mask;
            }
            return *this;
        }
    };
    
    // Access a bit
    BitReference operator[](size_t index) {
        if (index >= m_size) {
            throw std::out_of_range("BitVector: index out of range");
        }
        size_t wordIndex = index / 64;
        size_t bitIndex = index % 64;
        return BitReference(&m_bits[wordIndex], bitIndex);
    }
    
    // Const access
    bool operator[](size_t index) const {
        if (index >= m_size) {
            throw std::out_of_range("BitVector: index out of range");
        }
        size_t wordIndex = index / 64;
        size_t bitIndex = index % 64;
        return (m_bits[wordIndex] & (1ULL << bitIndex)) != 0;
    }
    
    // Get size
    size_t size() const {
        return m_size;
    }
    
    // Resize
    void resize(size_t newSize, bool initialValue = false) {
        size_t newWordCount = calcWordCount(newSize);
        
        if (newSize == 0) {
            delete[] m_bits;
            m_bits = nullptr;
            m_size = 0;
            m_wordCount = 0;
            return;
        }
        
        if (newWordCount != m_wordCount) {
            uint64_t* newBits = new uint64_t[newWordCount];
            
            // Initialize new memory
            #if defined(__SSE2__) || defined(_MSC_VER)
            if (initialValue) {
                optimized::bitvector_set_all(newBits, newWordCount);
            } else {
                optimized::bitvector_clear(newBits, newWordCount);
            }
            #else
            if (initialValue) {
                std::fill(newBits, newBits + newWordCount, ~0ULL);
            } else {
                std::fill(newBits, newBits + newWordCount, 0ULL);
            }
            #endif
            
            // Copy existing bits if we had any
            if (m_bits != nullptr && m_wordCount > 0) {
                size_t commonWords = std::min(m_wordCount, newWordCount);
                if (commonWords > 0) {
                    #if defined(__SSE2__) || defined(_MSC_VER)
                    optimized::bitvector_copy(newBits, m_bits, commonWords);
                    #else
                    std::copy(m_bits, m_bits + commonWords, newBits);
                    #endif
                }
            }
            
            delete[] m_bits;
            m_bits = newBits;
            m_wordCount = newWordCount;
        }
        
        m_size = newSize;
    }
    
    // Clear all bits
    void clear() {
        if (m_bits == nullptr || m_wordCount == 0) {
            return;
        }
        
        #if defined(__SSE2__) || defined(_MSC_VER)
        optimized::bitvector_clear(m_bits, m_wordCount);
        #else
        std::fill(m_bits, m_bits + m_wordCount, 0ULL);
        #endif
    }
    
    // Set all bits
    void setAll() {
        if (m_bits == nullptr || m_wordCount == 0) {
            return;
        }
        
        #if defined(__SSE2__) || defined(_MSC_VER)
        optimized::bitvector_set_all(m_bits, m_wordCount);
        #else
        std::fill(m_bits, m_bits + m_wordCount, ~0ULL);
        #endif
    }
};

#endif // UTILS_H 