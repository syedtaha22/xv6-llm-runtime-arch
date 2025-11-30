#ifndef LN_FAST_BIT_TRICKS_HPP
#define LN_FAST_BIT_TRICKS_HPP

#include <cstring> // For memcpy

/**
 * @brief Fast approximate natural logarithm using IEEE-754 bit trick.
 *
 * Very fast but low-precision (~1-2% error). Suitable for graphics or games.
 */
float ln_fast_bit_tricks(float x) {
    if (x <= 0.0f) return NAN; // Return NaN for non-positive inputs

    unsigned int ix;
    memcpy(&ix, &x, sizeof(ix));      // reinterpret float bits as uint32_t

    float y = (float)ix;
    y = y * 1.1920928955078125e-7f - 127.0f; // scale exponent
    return y * 0.69314718f;
}


#endif // LN_FAST_BIT_TRICKS_HPP
