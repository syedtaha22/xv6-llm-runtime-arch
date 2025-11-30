#ifndef POW_FAST_BIT_HPP
#define POW_FAST_BIT_HPP

#include <cmath>
#include <stdint.h>
#include <cstring> // For memcpy

/**
 * @brief Fast approximate pow(a,b) using float bit manipulation.
 *
 * Very fast (~1-2 cycles), low precision (~1-5%). Only works for a>0.
 */
float pow_fast_bit(float a, float b) {
    if (a <= 0.0f) return NAN;

    uint32_t ix;
    memcpy(&ix, &a, sizeof(ix)); // reinterpret float as int
    float log2a = ((float)ix - 0x3f800000) * 1.1920928955078125e-7f; // scale exponent
    float result_log2 = b * log2a;

    // Compute 2^(result_log2) using bit hack
    uint32_t res_bits = (uint32_t)(result_log2 * (1 << 23) + 0x3f800000);
    float res;
    memcpy(&res, &res_bits, sizeof(res));
    return res;
}

#endif // POW_FAST_BIT_HPP
