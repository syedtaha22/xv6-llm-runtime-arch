#ifndef TANH_EXP_HPP
#define TANH_EXP_HPP

#include "../exp/range_reduced_exp.hpp"


/**
 * @brief Calculates the hyperbolic tangent (tanh) using its definition in terms of
 * the exponential function (e^x).
 *
 * tanh(x) = (e^x - e^(-x)) / (e^x + e^(-x))
 *
 * This function uses the numerically stable equivalent form, tanh(x) = (e^(2x) - 1) / (e^(2x) + 1),
 * to compute the result. This approach is highly accurate across the entire domain.
 *
 * @param x The input value for which to calculate tanh.
 * @return The calculated tanh value.
 */
float tanh_exp(float x) {
    // 1. Handle large magnitudes for asymptotic clamping (prevents overflow/underflow)
    // 9.0f is a practical threshold for float precision (e^(2*9) ~ 8.1e7)
    if (x > 9.0f) {
        return 1.0f;
    }
    if (x < -9.0f) {
        return -1.0f;
    }

    // 2. Calculate e^(2x)
    // NOTE: Replace std::exp with your custom/optimized exp function if available
    float exp_2x = range_reduced_exp(2.0f * x);

    // 3. Compute tanh(x) = (e^(2x) - 1) / (e^(2x) + 1)
    return (exp_2x - 1.0f) / (exp_2x + 1.0f);
}

#endif // TANH_EXP_HPP