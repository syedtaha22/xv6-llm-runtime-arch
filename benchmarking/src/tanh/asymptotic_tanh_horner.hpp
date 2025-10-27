#ifndef ASYMPTOTIC_TANH_HORNER_HPP
#define ASYMPTOTIC_TANH_HORNER_HPP

#include "../utils.hpp" // Include our custom utility functions

// Define M_PI if it's not available (common on some systems or for strict non-stdlib math)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/****************************************************************************************
 *  ******* Constants for Horner's Method (Taylor Series for Tanh around 0) ********
 * *************************************************************************************
 *
 * The Taylor series for tanh(x) is x - x^3/3 + 2x^5/15 - 17x^7/315 + ...
 * We can factor out x: x * (1 - x^2/3 + 2x^4/15 - 17x^6/315 + ...)
 * Let y = x^2. The inner polynomial P(y) = 1 - y/3 + 2y^2/15 - 17y^3/315 + ...
 * For 10 terms of the tanh series, we need coefficients up to x^19, which means y^9.
 * P(y) = c0 + c1*y + c2*y^2 + ... + c9*y^9
 * where c_k are the coefficients from the tanh Taylor series.
 *
 * Coefficients for P(y) = 1 + c1*y + c2*y^2 + ... + c9*y^9
 * Note: c0 = 1, which will be handled as the first term in Horner's evaluation.
 * The constants below are c1 to c9.
 *
 * *************************************************************************************/

constexpr float C_1 = -1.0f / 3.0f;          // -1/3
constexpr float C_2 = 2.0f / 15.0f;          // 2/15
constexpr float C_3 = -17.0f / 315.0f;      // -17/315
constexpr float C_4 = 62.0f / 2835.0f;      // 62/2835
constexpr float C_5 = -1382.0f / 155925.0f; // -1382/155925
constexpr float C_6 = 21844.0f / 6081075.0f; // 21844/6081075
constexpr float C_7 = -929569.0f / 638512875.0f; // -929569/638512875
constexpr float C_8 = 6404582.0f / 10854718875.0f; // 6404582/10854718875
constexpr float C_9 = -443861162.0f / 1856156927625.0f; // -443861162/1856156927625

/**
 * @brief Computes the hyperbolic tangent (tanh) of an angle using Horner's method for its Taylor series.
 * This function is designed to be called with an angle already reduced to a small range
 * (e.g., [-5, 5]) for better accuracy.
 *
 * It calculates the polynomial P(y) = 1 + c1*y + c2*y^2 + ... + c9*y^9 where y = x^2,
 * and then multiplies the result by x to get tanh(x).
 *
 * @param x The input value for which to calculate tanh.
 * @return The approximated tanh value.
 */
float horner_tanh(float x) {
    float x_squared = x * x; // Compute x^2

    // Evaluate the polynomial P(y) using Horner's method
    float poly = C_9;
    poly = poly * x_squared + C_8;
    poly = poly * x_squared + C_7;
    poly = poly * x_squared + C_6;
    poly = poly * x_squared + C_5;
    poly = poly * x_squared + C_4;
    poly = poly * x_squared + C_3;
    poly = poly * x_squared + C_2;
    poly = poly * x_squared + C_1;
    poly = poly * x_squared + 1.0f; // Add the constant term c0

    return x * poly; // Multiply by x to get tanh(x)
}

/**
 * @brief Calculates the hyperbolic tangent (tanh) using asymptotic range clamping and Horner's method.
 *
 * This function clamps the input `x` to the asymptotic bounds of tanh:
 * - For `x < -5`, returns -1.0f
 * - For `x > 5`, returns 1.0f
 * - For `-5 <= x <= 5`, applies Horner's method for the Taylor series approximation around 0.
 *
 * @param x The input value.
 * @return The hyperbolic tangent of the input, approximated using range clamping and
 * Horner's method.
 */
inline float asymptotic_tanh_horner(float x) {
    const int NUM_TAYLOR_TERMS = 10;

    // absolute value of x
    float abs_x = (x < 0.0f) ? -x : x;

    // sign as ±1.0f using ternary
    float sign = (x < 0.0f) ? -1.0f : 1.0f;

    // Only call Taylor when abs_x <= 5.0f. Ternary guarantees only selected branch is evaluated.
    return (abs_x <= 5.0f) ? horner_tanh(x) : sign * 1.0f;
}

#endif // ASYMPTOTIC_TANH_HORNER_HPP