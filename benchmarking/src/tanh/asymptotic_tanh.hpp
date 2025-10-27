#ifndef ASYMPTOTIC_TANH_HPP
#define ASYMPTOTIC_TANH_HPP

#include "tanh_exp.hpp" // Include the Taylor series tanh implementation
#include "../utils.hpp"           // Include utility functions

/**
 * @brief Calculates the hyperbolic tangent (tanh) using asymptotic range clamping and definition.
 *
 * This function clamps the input `x` to the asymptotic bounds of tanh:
 * - For `x < -5`, returns -1.0f
 * - For `x > 5`, returns 1.0f
 * - For `-5 <= x <= 5`, applies a Taylor series approximation around 0.
 *
 * @param x The input value.
 * @return The hyperbolic tangent of the input, approximated using range clamping and Taylor series.
 */
inline float asymptotic_tanh(float x) {

    // absolute value of x
    float abs_x = (x < 0.0f) ? -x : x;

    // sign as ±1.0f using ternary
    float sign = (x < 0.0f) ? -1.0f : 1.0f;

    // Only call Taylor when abs_x <= 5.0f. Ternary guarantees only selected branch is evaluated.
    return (abs_x <= 5.0f) ? tanh_exp(x) : sign * 1.0f;
}

#endif // ASYMPTOTIC_TANH_HPP
