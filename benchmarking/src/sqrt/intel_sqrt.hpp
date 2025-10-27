#ifndef INTEL_SQRT_HPP
#define INTEL_SQRT_HPP

#include <immintrin.h> // for _mm_sqrt_ss and _mm_store_ss

// ============================================================
//  intel_sqrt: Hardware-accelerated square root using x86 SSE
// ============================================================

/**
 * @brief Computes the square root using the Intel SSE hardware instruction.
 *
 * This implementation leverages the `sqrtss` instruction (via intrinsic)
 * to compute the single-precision floating-point square root directly in hardware.
 * It is fully supported on all modern x86 CPUs and avoids iterative software methods.
 *
 * @param x The input value (must be non-negative).
 * @return The square root of the input value computed via hardware instruction.
 */
inline float intel_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;

    // Use SSE intrinsic (emits sqrtss)
    __m128 val = _mm_set_ss(x);
    __m128 res = _mm_sqrt_ss(val);

    // Extract the scalar result
    float result;
    _mm_store_ss(&result, res);
    return result;
}

#endif // INTEL_SQRT_HPP
