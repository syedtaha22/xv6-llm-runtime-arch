#ifndef RANGE_REDUCED_SINE_HORNER_HPP
#define RANGE_REDUCED_SINE_HORNER_HPP

#include "../utils.hpp" // Include our custom utility functions

// Define M_PI if it's not available (common on some systems or for strict non-stdlib math)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/****************************************************************************************
 *  ********* Constants for Horner's Method (Taylor Series for Sine around 0) *********
 * **************************************************************************************
 *
 * The Taylor series for sin(x) is x - x^3/3! + x^5/5! - x^7/7! + ...
 * We can factor out x: x * (1 - x^2/3! + x^4/5! - x^6/7! + ...)
 * Let y = x^2. The inner polynomial P(y) = 1 - y/3! + y^2/5! - y^3/7! + ...
 * For 10 terms of the sine series, we need coefficients up to x^19/19!, which means y^9.
 * P(y) = c0 + c1*y + c2*y^2 + ... + c9*y^9
 * where c_k = (-1)^k / (2k + 1)!
 *
 * Coefficients for P(y) = 1 + c1*y + c2*y^2 + ... + c9*y^9
 * Note: c0 = 1, which will be handled as the first term in Horner's evaluation.
 * The constants below are c1 to c9.
 *
 * *************************************************************************************/

constexpr float C_1_DENOM = 3.0f * 2.0f * 1.0f; // 3!
constexpr float C_1 = -1.0f / C_1_DENOM;        // -1/6

constexpr float C_2_DENOM = C_1_DENOM * 5.0f * 4.0f; // 5!
constexpr float C_2 = 1.0f / C_2_DENOM;         // 1/120

constexpr float C_3_DENOM = C_2_DENOM * 7.0f * 6.0f; // 7!
constexpr float C_3 = -1.0f / C_3_DENOM;        // -1/5040

constexpr float C_4_DENOM = C_3_DENOM * 9.0f * 8.0f; // 9!
constexpr float C_4 = 1.0f / C_4_DENOM;         // 1/362880

constexpr float C_5_DENOM = C_4_DENOM * 11.0f * 10.0f; // 11!
constexpr float C_5 = -1.0f / C_5_DENOM;        // -1/39916800

constexpr float C_6_DENOM = C_5_DENOM * 13.0f * 12.0f; // 13!
constexpr float C_6 = 1.0f / C_6_DENOM;         // 1/6227020800

constexpr float C_7_DENOM = C_6_DENOM * 15.0f * 14.0f; // 15!
constexpr float C_7 = -1.0f / C_7_DENOM;        // -1/1307674368000

constexpr float C_8_DENOM = C_7_DENOM * 17.0f * 16.0f; // 17!
constexpr float C_8 = 1.0f / C_8_DENOM;         // 1/355687428096000

constexpr float C_9_DENOM = C_8_DENOM * 19.0f * 18.0f; // 19!
constexpr float C_9 = -1.0f / C_9_DENOM;        // -1/121645100408832000

/**
 * @brief Computes the sine of an angle using Horner's method for its Taylor series.
 * This function is designed to be called with an angle already reduced to a small range
 * (e.g., [-PI, PI]) for optimal accuracy with a fixed number of terms.
 *
 * It calculates the polynomial P(y) = 1 + c1*y + c2*y^2 + ... + c9*y^9 where y = x^2,
 * and then returns x * P(y).
 *
 * @param x The input angle in radians, assumed to be in a reduced range.
 * @return The approximated sine value.
 */
float horner_sine(float x) {
    float x_squared = x * x; // y = x^2

    // Horner's method evaluation for P(y) = c0 + c1*y + c2*y^2 + ... + c9*y^9
    // P(y) = c0 + y * (c1 + y * (c2 + ... + y * (c9)...))
    // Our c0 is effectively 1 for the x(1 - y/3! + ...) form
    // Start with the innermost coefficient (c9) and work outwards.
    // Basically a bunch of FMADD (Multiply-Add) operations.
    float result_poly = C_9;
    result_poly = result_poly * x_squared + C_8;
    result_poly = result_poly * x_squared + C_7;
    result_poly = result_poly * x_squared + C_6;
    result_poly = result_poly * x_squared + C_5;
    result_poly = result_poly * x_squared + C_4;
    result_poly = result_poly * x_squared + C_3;
    result_poly = result_poly * x_squared + C_2;
    result_poly = result_poly * x_squared + C_1;
    result_poly = result_poly * x_squared + 1.0f; // Add the implicit c0 = 1

    return x * result_poly;
}

/**
 * @brief Calculates the sine of an angle using range reduction to (-PI, PI]
 * and then computes the sine using Horner's method for its Taylor series.
 *
 * This function first reduces the input angle `x` using the formula `PI - mod(x, 2*PI)`,
 * which maps `x` to the range `(-PI, PI]`. This centering around zero is highly beneficial
 * for maintaining the accuracy of Taylor series expansions, as they are most accurate
 * near their expansion point (0). After range reduction, Horner's method is applied
 * to the reduced angle.
 *
 * @param x The input angle in radians.
 * @return The sine of the angle, approximated using range reduction and Horner's method.
 */
float range_reduced_sine_horner(float x) {
    // Define the period for sine function (2*PI)
    const float period = 2.0f * M_PI;

    // Apply the range reduction formula: PI - mod(x, 2*PI)
    // The Utils::mod function ensures mod(x, 2*PI) gives a result in [0, 2*PI).
    float reduced_x = M_PI - Utils::mod(x, period);

    // Apply Horner's method approximation to the range-reduced angle.
    return horner_sine(reduced_x);
}


#endif // RANGE_REDUCED_SINE_HORNER_HPP
