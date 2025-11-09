/**
 * @file math.c
 * @brief Custom mathematical function implementations for xv6 operating system
 * 
 * @author Hamna Sajid
 * @date 11/09/2025
 * 
 * @details
 * This file implements a comprehensive set of mathematical functions including
 * square root, exponential, logarithm, power, trigonometric functions, and
 * absolute value. The implementations are designed for xv6 userland and use
 * iterative methods and series approximations to provide reasonable accuracy
 * while maintaining computational efficiency.
 * 
 * Features include:
 * - Proper handling of special cases (NaN, Infinity, edge conditions)
 * - Range reduction for improved numerical stability
 * - Iterative refinement for convergence
 * - Series approximations with early termination
 * - Overflow and underflow protection
 * 
  */

#include "xmath.h"

/**
 * @brief Compute absolute value of a float
 * 
 * @param x Input value
 * @return float Absolute value of x
 * 
 * @details
 * Simple implementation that returns x if positive, -x if negative.
 * Handles all finite values correctxfabsfly including zero.
 */
float xfabsf(float x) {
    if (x < 0) return -x;
    return x;
}

/**
 * @brief Compute square root using Babylonian method
 * 
 * @param x Input value (must be non-negative)
 * @return float Square root of x, NaN if x is negative
 * 
 * @details
 * Implements the Babylonian method (Heron's method) for square root calculation:
 * - Returns NaN for negative inputs
 * - Returns immediately for 0 and 1
 * - Iteratively refines estimate: z = (y + x/y) * 0.5f
 * - Terminates when convergence achieved or maximum iterations reached
 * 
 * @note Maximum of 20 iterations with tolerance 1e-7f
 */
float xsqrtf(float x) {
    if (x < 0.0f) return 0.0f / 0.0f; // NaN
    if (x == 0.0f || x == 1.0f) return x;
    
    // Babylonian method
    float y = x;
    float z = (y + x / y) * 0.5f;
    
    int iterations = 0;
    while (xfabsf(y - z) > 1e-7f && iterations < 20) {
        y = z;
        z = (y + x / y) * 0.5f;
        iterations++;
    }
    return z;
}

/**
 * @brief Compute exponential function e^x
 * 
 * @param x Exponent value
 * @return float e raised to the power x
 * 
 * @details
 * Computes exponential function using multiple strategies:
 * - Special case handling for 0, large positive (overflow), large negative (underflow)
 * - Range reduction for |x| > 1 using identity: exp(x) = exp(x/2)^2
 * - Taylor series expansion for |x| <= 1
 * - Early termination when term magnitude below threshold
 * 
 * @note Overflow threshold at x > 88.0f, underflow at x < -88.0f
 * @note Uses up to 20 terms in Taylor series with 1e-8f termination threshold
 */
float xexpf(float x) {
    // Handle special cases
    if (x == 0.0f) return 1.0f;
    if (x > 88.0f) return 1.0f / 0.0f; // INFINITY
    if (x < -88.0f) return 0.0f;
    
    // Use range reduction for better accuracy
    if (xfabsf(x) > 1.0f) {
        // exp(x) = exp(x/2)^2
        float half_exp = xexpf(x * 0.5f);
        return half_exp * half_exp;
    }
    
    // Taylor series for |x| <= 1
    float result = 1.0f;
    float term = 1.0f;
    
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        result += term;
        if (xfabsf(term) < 1e-8f) break;
    }
    return result;
}

/**
 * @brief Compute natural logarithm ln(x)
 * 
 * @param x Input value (must be positive)
 * @return float Natural logarithm of x
 * 
 * @details
 * Computes natural logarithm using argument reduction and series expansion:
 * - Returns -INF for x = 0, NaN for x < 0
 * - Argument reduction: brings input into range [0.5, 1.0]
 * - Uses series expansion: ln(1+z) = z - z^2/2 + z^3/3 - ...
 * - Combines result with exponent * ln(2) for final value
 * 
 * @note Uses precomputed ln(2) = 0.6931471805599453f
 * @note Early termination when term magnitude below 1e-8f
 */
float xlogf(float x) {
    if (x <= 0.0f) {
        if (x == 0.0f) return -1.0f / 0.0f; // -INF
        return 0.0f / 0.0f; // NaN
    }
    if (x == 1.0f) return 0.0f;
    
    // Argument reduction: ln(x) = ln(m * 2^e) = ln(m) + e*ln(2)
    // Bring x into range [0.5, 1.0]
    int exponent = 0;
    float y = x;
    
    while (y > 1.0f) {
        y *= 0.5f;
        exponent++;
    }
    while (y < 0.5f) {
        y *= 2.0f;
        exponent--;
    }
    
    // Now compute ln(y) where y in [0.5, 1.0]
    // Use series: ln(1+z) = z - z^2/2 + z^3/3 - ... where z = y-1
    float z = y - 1.0f;
    float result = z;
    float term = z;
    
    
    for (int i = 2; i < 20; i++) {
        term *= -z;
        result += term / i;
        if (xfabsf(term) < 1e-8f) break;
    }
    
    // Add exponent part: result + exponent * ln(2)
    return result + exponent * 0.6931471805599453f;
}

/**
 * @brief Compute power function x^y
 * 
 * @param x Base value
 * @param y Exponent value
 * @return float x raised to the power y
 * 
 * @details
 * Computes power function using identity: x^y = exp(y * ln(|x|))
 * Special case handling:
 * - x^0 = 1 for all x
 * - 1^y = 1 for all y
 * - 0^y = 0 for y > 0, INF for y <= 0
 * - Negative base with non-integer exponent returns NaN
 * - Negative base with odd integer exponent returns negative result
 * 
 * @note Uses absolute value of base for logarithm to handle negative bases properly
 */
float xpowf(float x, float y) {
    // Handle special cases
    if (y == 0.0f) return 1.0f;
    if (x == 1.0f) return 1.0f;
    if (x == 0.0f) {
        if (y > 0.0f) return 0.0f;
        return 1.0f / 0.0f; // INFINITY
    }
    
    // pow(x,y) = exp(y * ln(|x|))
    float result = xexpf(y * xlogf(xfabsf(x)));
    
    // Handle negative base with integer exponent
    if (x < 0.0f) {
        // Check if exponent is integer
        int is_integer = (y == (float)(int)y);
        if (is_integer) {
            int int_y = (int)y;
            if (int_y % 2 != 0) { // Odd exponent
                result = -result;
            }
        } else {
            // Non-integer exponent with negative base -> NaN
            return 0.0f / 0.0f;
        }
    }
    
    return result;
}

/**
 * @brief Reduce angle to principal value in [-π, π]
 * 
 * @param x Input angle in radians
 * @return float Angle reduced to range [-π, π]
 * 
 * @details
 * Performs angle reduction for trigonometric functions:
 * - First reduces to [-2π, 2π] range using modulus operation
 * - Further reduces to [-π, π] range by adjusting boundaries
 * - Improves numerical stability for large input angles
 * 
 * @note Uses precomputed π and 2π constants
 * @note Essential for accurate trigonometric calculations with large inputs
 */
float xreduce_angle(float x) {
    const float two_pi = 6.28318530717958647692f;
    const float pi = 3.14159265358979323846f;
    
    // Reduce to [-2π, 2π] first
    x = x - (int)(x / two_pi) * two_pi;
    
    // Further reduce to [-π, π]
    if (x > pi) x -= two_pi;
    if (x < -pi) x += two_pi;
    
    return x;
}

/**
 * @brief Compute sine function with improved argument reduction
 * 
 * @param x Angle in radians
 * @return float Sine of x
 * 
 * @details
 * Computes sine using Taylor series expansion with argument reduction:
 * - First reduces input angle to [-π, π] range
 * - Uses direct value approximation for very small angles (|x| < 1e-4f)
 * - Taylor series: sin(x) = x - x^3/3! + x^5/5! - ...
 * - Early termination when term magnitude below threshold
 * 
 * @note Uses up to 12 terms in Taylor series with 1e-8f termination threshold
 */
float xsinf(float x) {
    // Reduce angle first
    x = xreduce_angle(x);
    
    // For very small angles, use the angle directly
    if (xfabsf(x) < 1e-4f) return x;
    
    // Taylor series
    float result = x;
    float term = x;
    float x2 = x * x;
    
    for (int i = 1; i < 12; i++) {
        term *= -x2 / ((2*i) * (2*i + 1));
        result += term;
        if (xfabsf(term) < 1e-8f) break;
    }
    return result;
}

/**
 * @brief Compute cosine function with improved argument reduction
 * 
 * @param x Angle in radians
 * @return float Cosine of x
 * 
 * @details
 * Computes cosine using Taylor series expansion with argument reduction:
 * - First reduces input angle to [-π, π] range
 * - Uses quadratic approximation for very small angles (|x| < 1e-4f)
 * - Taylor series: cos(x) = 1 - x^2/2! + x^4/4! - ...
 * - Early termination when term magnitude below threshold
 * 
 * @note Uses up to 12 terms in Taylor series with 1e-8f termination threshold
 */
float xcosf(float x) {
    // Reduce angle first
    x = xreduce_angle(x);
    
    // For very small angles, use approximation
    if (xfabsf(x) < 1e-4f) return 1.0f - x*x*0.5f;
    
    // Taylor series
    float result = 1.0f;
    float term = 1.0f;
    float x2 = x * x;
    
    for (int i = 1; i < 12; i++) {
        term *= -x2 / ((2*i - 1) * (2*i));
        result += term;
        if (xfabsf(term) < 1e-8f) break;
    }
    return result;
}