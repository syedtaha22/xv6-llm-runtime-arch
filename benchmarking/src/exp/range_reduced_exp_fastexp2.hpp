#ifndef RANGE_REDUCED_EXP_FASTEXP2_HPP
#define RANGE_REDUCED_EXP_FASTEXP2_HPP

#include "taylor_series_exp.hpp" // Include your Taylor series exp implementation
#include "../utils.hpp"          // Include utility functions

/**
 * @brief Calculates the exponential of a value using range reduction with base 2
 *        and a Taylor series approximation for the remainder.
 *
 * This implements the formula: exp(x) = 2^n * exp(r), where:
 * - n = floor(x / ln(2))
 * - r = x - n * ln(2)
 *
 * The 2^n term is computed using `Utils::pow2`, which efficiently produces
 * the floating-point value of 2^n by directly manipulating the exponent bits
 * of an IEEE-754 single-precision float. This avoids loops or floating-point
 * multiplications for the power-of-two factor.
 *
 * The remainder r is small, so the Taylor series approximation converges quickly.
 *
 * @param x The input value.
 * @return Approximation of e^x computed via range reduction and fast 2^n multiplication.
 */
float range_reduced_exp_fastexp2(float x) {
    // ln(2) constant
    constexpr float LN2 = 0.6931471805599453f;

    // Determine n and r
    int n = static_cast<int>(x / LN2);
    float r = x - n * LN2;

    // Compute e^r using Taylor series
    const int NUM_TAYLOR_TERMS = 10;
    float exp_r = taylor_series_exp(r, NUM_TAYLOR_TERMS);

    // Multiply by 2^n using fast IEEE-754 exponent manipulation
    return exp_r * Utils::pow2(n);
}

#endif // RANGE_REDUCED_EXP_FASTEXP2_HPP
