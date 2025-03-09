#include <SDL2/SDL.h>
#include "utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Fast boolean vector operations with SSE
#if defined(__SSE2__) || defined(_MSC_VER)
#include <emmintrin.h>
#include <xmmintrin.h>

namespace optimized {
    // Fast implementation of a bit vector to replace std::vector<bool> where possible
    // This reduces the overhead of std::_Bvector_base<std::allocator<bool>>::_Bvector_impl::~_Bvector_impl()
    void bitvector_copy(uint64_t* dst, const uint64_t* src, size_t n_words) {
        // Process 2 words (128 bits) at a time using SSE
        size_t sse_blocks = n_words / 2;
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            __m128i data = _mm_loadu_si128((__m128i*)(src + i * 2));
            _mm_storeu_si128((__m128i*)(dst + i * 2), data);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            dst[i] = src[i];
        }
    }
    
    // Clear multiple words of a bit vector at once
    void bitvector_clear(uint64_t* bits, size_t n_words) {
        // Clear 2 words (128 bits) at a time using SSE
        size_t sse_blocks = n_words / 2;
        __m128i zero = _mm_setzero_si128();
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            _mm_storeu_si128((__m128i*)(bits + i * 2), zero);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            bits[i] = 0;
        }
    }
    
    // Set multiple words of a bit vector at once
    void bitvector_set_all(uint64_t* bits, size_t n_words) {
        // Set 2 words (128 bits) at a time using SSE
        size_t sse_blocks = n_words / 2;
        __m128i all_ones = _mm_set1_epi32(0xFFFFFFFF);
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            _mm_storeu_si128((__m128i*)(bits + i * 2), all_ones);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            bits[i] = ~0ULL;
        }
    }
    
    // Perform bitwise AND of two bit vectors
    void bitvector_and(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        size_t sse_blocks = n_words / 2;
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            __m128i a_data = _mm_loadu_si128((__m128i*)(a + i * 2));
            __m128i b_data = _mm_loadu_si128((__m128i*)(b + i * 2));
            __m128i result_data = _mm_and_si128(a_data, b_data);
            _mm_storeu_si128((__m128i*)(result + i * 2), result_data);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            result[i] = a[i] & b[i];
        }
    }
    
    // Perform bitwise OR of two bit vectors
    void bitvector_or(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        size_t sse_blocks = n_words / 2;
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            __m128i a_data = _mm_loadu_si128((__m128i*)(a + i * 2));
            __m128i b_data = _mm_loadu_si128((__m128i*)(b + i * 2));
            __m128i result_data = _mm_or_si128(a_data, b_data);
            _mm_storeu_si128((__m128i*)(result + i * 2), result_data);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            result[i] = a[i] | b[i];
        }
    }
    
    // Perform bitwise XOR of two bit vectors
    void bitvector_xor(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        size_t sse_blocks = n_words / 2;
        
        for (size_t i = 0; i < sse_blocks; ++i) {
            __m128i a_data = _mm_loadu_si128((__m128i*)(a + i * 2));
            __m128i b_data = _mm_loadu_si128((__m128i*)(b + i * 2));
            __m128i result_data = _mm_xor_si128(a_data, b_data);
            _mm_storeu_si128((__m128i*)(result + i * 2), result_data);
        }
        
        // Handle any remaining words
        for (size_t i = sse_blocks * 2; i < n_words; ++i) {
            result[i] = a[i] ^ b[i];
        }
    }
}
#else
// Non-SSE implementations
namespace optimized {
    void bitvector_copy(uint64_t* dst, const uint64_t* src, size_t n_words) {
        std::copy(src, src + n_words, dst);
    }
    
    void bitvector_clear(uint64_t* bits, size_t n_words) {
        std::fill(bits, bits + n_words, 0ULL);
    }
    
    void bitvector_set_all(uint64_t* bits, size_t n_words) {
        std::fill(bits, bits + n_words, ~0ULL);
    }
    
    void bitvector_and(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] & b[i];
        }
    }
    
    void bitvector_or(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] | b[i];
        }
    }
    
    void bitvector_xor(uint64_t* result, const uint64_t* a, const uint64_t* b, size_t n_words) {
        for (size_t i = 0; i < n_words; ++i) {
            result[i] = a[i] ^ b[i];
        }
    }
}
#endif // defined(__SSE2__) || defined(_MSC_VER)
