#ifndef LN_HORNERS_METHOD_HPP
#define LN_HORNERS_METHOD_HPP

#include <stdint.h>
#include <cmath>  // For INFINITY and NAN

/****************************************************************************************
 *  ********* Constants for Horner's Method (Taylor Series for ln(1+y) around 0) ******
 * **************************************************************************************
 *
 * The Taylor series for ln(1+y) is y - y^2/2 + y^3/3 - y^4/4 + ...
 * Factor as polynomial in y = m-1: P(y) = 1 - y/2 + y^2/3 - y^3/4 + ...
 * For 10 terms, we need coefficients up to y^9:
 * P(y) = c0 + c1*y + c2*y^2 + ... + c9*y^9
 * where c_k = (-1)^k / (k+1) for k=1..9
 *
 * Coefficients for P(y) = 1 + c1*y + c2*y^2 + ... + c9*y^9
 * Note: c0 = 1, handled in Horner's evaluation.
 *
 * *************************************************************************************/

#define LN2      0.6931471805599453f
#define LN_C1   (-0.5f)          // -1/2
#define LN_C2    0.33333333f     // 1/3
#define LN_C3   (-0.25f)         // -1/4
#define LN_C4    0.2f            // 1/5
#define LN_C5   (-0.16666667f)   // -1/6
#define LN_C6    0.14285714f     // 1/7
#define LN_C7   (-0.125f)        // -1/8
#define LN_C8    0.11111111f     // 1/9
#define LN_C9   (-0.1f)          // -1/10

/**
 * @brief Calculates the natural logarithm of a positive number using range reduction
 * to [0.5, 1.0) and a Horner-polynomial approximation for ln(1+y).
 *
 * This function first reduces the input `x` to the form x = m * 2^k with m in [0.5,1),
 * which improves the convergence of the polynomial approximation. After reduction,
 * Horner's method is used to evaluate the Taylor series for ln(1+y) efficiently.
 *
 * @param x The input value (x > 0)
 * @return The natural logarithm of x
 */
float xlnf(float x) {
    if (x <= 0.0f) {
        if (x == 0.0f) return -INFINITY;
        return NAN;
    }
    if (x == 1.0f) return 0.0f;

    // Range reduction: x = m * 2^k, m in [0.5, 1)
    int32_t exponent = 0;
    float m = x;
    while (m > 1.0f) { m *= 0.5f; exponent++; }
    while (m < 0.5f) { m *= 2.0f; exponent--; }

    float y = m - 1.0f;

    // Horner's method evaluation for P(y) = c0 + c1*y + c2*y^2 + ... + c9*y^9
    float result_poly = LN_C9;
    result_poly = result_poly * y + LN_C8;
    result_poly = result_poly * y + LN_C7;
    result_poly = result_poly * y + LN_C6;
    result_poly = result_poly * y + LN_C5;
    result_poly = result_poly * y + LN_C4;
    result_poly = result_poly * y + LN_C3;
    result_poly = result_poly * y + LN_C2;
    result_poly = result_poly * y + LN_C1;
    result_poly = result_poly * y + 1.0f; // Add implicit c0 = 1

    return exponent * LN2 + y * result_poly;
}



#endif // LN_HORNERS_METHOD_HPP
