#ifndef RANGE_REDUCED_EXP_HPP
#define RANGE_REDUCED_EXP_HPP

#include "taylor_series_exp.hpp" // Include taylor series exp implementation

/**
 * @brief Calculates the exponential of a value using range reduction with base 2 and a Taylor series approximation.
 *
 * This implements the formula: exp(x) = 2^n * exp(r), where n = floor(x / ln(2))
 * and r = x - n * ln(2). The 2^n term is computed efficiently using integer shifts
 * for positive n and reciprocal shifts for negative n. The remainder r is small,
 * so the Taylor series converges quickly.
 *
 * @param x The input value.
 * @return The exponential of x.
 */
float range_reduced_exp(float x) {
    // ln(2) constant
    constexpr float LN2 = 0.6931471805599453f;

    // Determine n and r
    int n = static_cast<int>(x / LN2);
    float r = x - n * LN2;

    // Compute e^r using Taylor series
    const int NUM_TAYLOR_TERMS = 10;
    float exp_r = taylor_series_exp(r, NUM_TAYLOR_TERMS);

    // Compute 2^n using integer shifts
    float pow2_n;
    if (n >= 0) pow2_n = static_cast<float>(1 << n);         // positive powers
    else        pow2_n = 1.0f / static_cast<float>(1 << -n); // negative powers

    return exp_r * pow2_n;
}

#endif // RANGE_REDUCED_EXP_HPP
