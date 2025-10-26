#ifndef RANGE_REDUCED_EXP_HORNER_HPP
#define RANGE_REDUCED_EXP_HORNER_HPP

#include "../utils.hpp" // Include our custom utility functions

/****************************************************************************************
 *  ********* Constants for Horner's Method (Taylor Series for e^r around 0) *********
 * **************************************************************************************
 *
 * The Taylor series for e^r is 1 + r + r^2/2! + r^3/3! + ... + r^10/10!
 * We evaluate it using Horner's method:
 * e^r ≈ 1 + r*(1 + r/2*(1 + r/3*(1 + ...)))
 *
 * Fully unrolled for 10 terms. r is the reduced value after x = n*ln2 + r
 * where n is an integer and r is small.
 *
 ***************************************************************************************/

 // Precomputed factorial reciprocals
constexpr float C_1 = 1.0f / 1.0f;       // 1/1!
constexpr float C_2 = 1.0f / 2.0f;       // 1/2!
constexpr float C_3 = 1.0f / 6.0f;       // 1/3!
constexpr float C_4 = 1.0f / 24.0f;      // 1/4!
constexpr float C_5 = 1.0f / 120.0f;     // 1/5!
constexpr float C_6 = 1.0f / 720.0f;     // 1/6!
constexpr float C_7 = 1.0f / 5040.0f;    // 1/7!
constexpr float C_8 = 1.0f / 40320.0f;   // 1/8!
constexpr float C_9 = 1.0f / 362880.0f;  // 1/9!
constexpr float C_10 = 1.0f / 3628800.0f;// 1/10!

/**
 * @brief Computes e^r for small r using Horner's method (fully unrolled).
 *
 * @param r Small input value (|r| < ln2/2)
 * @return Approximation of e^r
 */
float horner_exp(float r) {
    float result = C_10;
    result = result * r + C_9;
    result = result * r + C_8;
    result = result * r + C_7;
    result = result * r + C_6;
    result = result * r + C_5;
    result = result * r + C_4;
    result = result * r + C_3;
    result = result * r + C_2;
    result = result * r + C_1;
    result = result * r + 1.0f;
    return result;
}


/**
 * @brief Computes e^x using range reduction: e^x = 2^n * e^r
 *
 * @param x Input value
 * @return Approximation of e^x
 */
float range_reduced_exp_horner(float x) {
    constexpr float LN2 = 0.6931471805599453f;

    // Range reduction: x = n*ln2 + r
    int n = static_cast<int>(x / LN2);
    float r = x - n * LN2;

    // Compute e^r using Horner's method
    float exp_r = horner_exp(r);

    // Compute 2^n inline using shifts
    return exp_r * Utils::pow2(n);
}

#endif // RANGE_REDUCED_EXP_HORNER_HPP
