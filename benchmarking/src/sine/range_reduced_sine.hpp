#ifndef RANGE_REDUCED_SINE_HPP
#define RANGE_REDUCED_SINE_HPP

#include "taylor_series_sine.hpp" // Include the Taylor series sine implementation
#include "../utils.hpp"           // Include utility functions

// Define M_PI if it's not available (common on some systems)
// If taylor_series_sine.hpp already defines it, this will be redundant but harmless due to #ifndef.
// TODO: Consider moving this to a common header for mathematical constants.
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * @brief Calculates the sine of an angle using range reduction to (-PI, PI] and a Taylor series approximation.
 *
 * This function first reduces the input angle `x` using the formula `PI - mod(x, 2*PI)`,
 * which maps `x` to the range `(-PI, PI]`. This centering around zero is highly beneficial
 * for maintaining the accuracy of Taylor series expansions, as they are most accurate
 * near their expansion point (0). After range reduction, the Taylor series for sine
 * is applied to the reduced angle.
 *
 * @param x The input angle in radians.
 * @return The sine of the angle, approximated using range reduction and Taylor series.
 */
float range_reduced_sine(float x) {
    // Define the period for sine function (2*PI)
    // TODO: Consider moving this to a common header for mathematical constants.
    const float period = 2.0f * M_PI;

    // Apply the range reduction formula: PI - mod(x, 2*PI) to map x to (-PI, PI].
    // The Utils::mod function ensures mod(x, 2*PI) gives a result in [0, 2*PI).
    float reduced_x = M_PI - Utils::mod(x, period);

    // Apply the Taylor series approximation to the range-reduced angle.
    const int NUM_TAYLOR_TERMS = 10;
    return taylor_series_sine(reduced_x, NUM_TAYLOR_TERMS);
}

#endif // RANGE_REDUCED_SINE_HPP
