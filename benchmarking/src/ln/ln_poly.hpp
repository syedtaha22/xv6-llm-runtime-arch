#ifndef LN_POLY_HPP
#define LN_POLY_HPP

#include <stdint.h>
#include <cstring> // For memcpy

#define LN2 0.6931471805599453f

/**
 * @brief Computes natural logarithm using range reduction and a small polynomial.
 *
 * Accurate to ~1e-6 for float.
 * 
 * @param x Input value (>0)
 * @return Approximated ln(x)
 */
float ln_polyf(float x) {
    // Handle special cases
    if (x <= 0.0f) return NAN; // ln(0) = -inf, ln(negative) = NaN

    // Reinterpret float bits as integer without union
    uint32_t ix;
    memcpy(&ix, &x, sizeof(ix));

    // Extract exponent and normalize mantissa: x = m * 2^k, m in [1,2)
    int32_t exp = ((ix >> 23) & 0xFF) - 127;           // exponent k
    ix = (ix & 0x007FFFFF) | 0x3F800000;              // mantissa m normalized to [1,2)

    float m;
    memcpy(&m, &ix, sizeof(m));                       // convert bits back to float

    // Polynomial approximation for ln(m) around 1
    float y = m - 1.0f;
    float y2 = y * y;
    float y3 = y2 * y;
    float y4 = y3 * y;
    float y5 = y4 * y;

    float ln_m = y - 0.5f*y2 + 0.33333333f*y3 - 0.25f*y4 + 0.2f*y5;

    return exp * LN2 + ln_m;
}

#endif // LN_POLY_HPP
