/**
 * @file xmath.c
 * @brief Optimized mathematical function implementations for xv6 operating system
 * 
 * @author Hamna Sajid
 * @date 11/09/2025
 * 
 * @details
 * This file implements optimized mathematical functions using methods recommended
 * in the xv4 research paper. Implementations use range reduction, polynomial
 * approximations with Horner's method, and hardware-efficient algorithms.
 * 
 * Features include:
 * - Range reduction for numerical stability
 * - Horner's method for polynomial evaluation
 * - Hardware-efficient algorithms (Quake III sqrt)
 * - Proper special case handling
 * - High performance suitable for LLM inference
 */

#include "xmath.h"

// Mathematical constants
#define PI 3.14159265358979323846f
#define PI_2 1.57079632679489661923f
#define PI_4 0.78539816339744830962f
#define LN2 0.69314718055994530942f
#define INV_LN2 1.44269504088896340736f
#define INFINITY (1.0f / 0.0f)
#define NAN (0.0f / 0.0f)

/**
 * @brief Hardware-accelerated square root using RISC-V fsqrt.s instruction
 * 
 * @param x Input value (must be non-negative)
 * @return float Square root of x, NaN if x is negative
 * 
 * @details
 * Uses the RISC-V F extension fsqrt.s instruction for maximum performance.
 * This is much faster and more accurate than any software implementation.
 */
float xsqrtf(float x) {
    // Handle special cases in software
    if (x < 0.0f) return NAN;
    if (x == 0.0f) return 0.0f;
    if (x == 1.0f) return 1.0f;
    
    // Use hardware instruction for the actual computation
    float result;
    asm volatile ("fsqrt.s %0, %1" : "=f"(result) : "f"(x));
    return result;
}

/**
 * @brief Absolute value for floats
 */
float xfabsf(float x)
{
    return (x < 0.0f) ? -x : x;
}

/**
 * @brief Exponential function using range reduction and polynomial approximation
 */
float xexpf(float x) {
    // Handle special cases
    if (x == 0.0f) return 1.0f;
    if (x > 88.0f) return INFINITY;
    if (x < -88.0f) return 0.0f;
    
    // Range reduction: x = k*ln(2) + r
    float k_float = x * INV_LN2;
    int k;
    if (k_float >= 0.0f) {
        k = (int)(k_float + 0.5f);
    } else {
        k = (int)(k_float - 0.5f);
    }
    float r = x - k * LN2;
    
    // Polynomial approximation using Horner's method
    float result = 1.0f + r * (1.0f + r * (
        0.5f + r * (0.1666666667f + r * (
        0.0416666667f + r * (0.008333333333f + r * (
        0.001388888889f + r * 0.0001984126984f
    ))))));
    
    // Scale by 2^k
    if (k > 0) {
        while (k-- > 0) result *= 2.0f;
    } else {
        while (k++ < 0) result *= 0.5f;
    }
    
    return result;
}

/**
 * @brief Natural logarithm using argument reduction
 */
float xlogf(float x) {
    if (x <= 0.0f) {
        if (x == 0.0f) return -INFINITY;
        return NAN;
    }
    if (x == 1.0f) return 0.0f;
    
    // Argument reduction
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
    
    // Compute ln(y) where y in [0.5, 1.0]
    float z = y - 1.0f;
    float result = z - z*z*0.5f;
    float term = z*z*z;
    
    // Series expansion with early termination
    for (int i = 3; i < 12; i++) {
        float new_term = term * z / i;
        result += (i % 2 == 1) ? new_term : -new_term;
        if (xfabsf(new_term) < 1e-8f) break;
        term = new_term * i;
    }
    
    return result + exponent * LN2;
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
 * @brief Reduce angle to principal value in [-π/2, π/2] for better accuracy
 * 
 * @details
 * More accurate range reduction than previous [-π, π] version
 * Uses periodicity and symmetry properties of trig functions
 */
float xreduce_angle(float x) {
    // Reduce to [-2π, 2π] first
    const float two_pi = 6.28318530717958647692f;
    x = x - (int)(x / two_pi) * two_pi;
    
    // Reduce to [-π, π]
    if (x > PI) x -= two_pi;
    if (x < -PI) x += two_pi;
    
    // Further reduce to [-π/2, π/2] using sin/cos properties
    if (x > PI_2) return PI - x;
    if (x < -PI_2) return -PI - x;
    
    return x;
}

/**
 * @brief Compute sine using Range-Reduced Taylor Series with Horner's Method
 * 
 * Implementation follows the mathematical specification:
 * 1. Range reduction to [-π/2, π/2] using periodicity and symmetry
 * 2. Taylor series (Maclaurin expansion) with Horner's method
 * 3. Target accuracy: ~10^-6 relative error
 */
/**
 * @brief Compute sine using Range-Reduced Taylor Series with Horner's Method
 * 
 * FIXED VERSION: Proper sign handling for all quadrants
 */
float xsinf(float x)
{
    const float pi = 3.14159265358979323846f;
    const float two_pi = 6.28318530717958647692f;
    const float half_pi = 1.57079632679489661923f;
    
    /* === STEP 1: RANGE REDUCTION TO [-π/2, π/2] === */
    
    // Store original sign for negative inputs
    float original_sign = (x < 0.0f) ? -1.0f : 1.0f;
    
    // Work with absolute value for range reduction
    float abs_x = (x < 0.0f) ? -x : x;
    
    // Reduce to [0, 2π) using periodicity
    float n = xfloorf(abs_x / two_pi);
    float reduced = abs_x - n * two_pi;
    
    // Now reduced is in [0, 2π)
    // Determine quadrant and final sign
    
    float sign = original_sign; // Start with original sign
    
    if (reduced > 1.5f * pi) {
        // Quadrant IV: [3π/2, 2π) - sin is negative
        reduced = two_pi - reduced;
        sign = -sign;
    } else if (reduced > pi) {
        // Quadrant III: [π, 3π/2) - sin is negative  
        reduced = reduced - pi;
        sign = -sign;
    } else if (reduced > half_pi) {
        // Quadrant II: [π/2, π) - sin is positive
        reduced = pi - reduced;
        // sign remains unchanged
    }
    // Quadrant I: [0, π/2] - sin is positive, no changes needed
    
    // Now 'reduced' is in [0, π/2]
    
    /* === STEP 2: TAYLOR SERIES WITH HORNER'S METHOD === */
    
    float x2 = reduced * reduced;
    
    // High-precision coefficients
    const float c1 = -1.66666666666666657415e-1f;  // -1/3!
    const float c2 =  8.33333333333333231206e-3f;  //  1/5!
    const float c3 = -1.98412698412698400577e-4f;  // -1/7!
    const float c4 =  2.75573192239858826783e-6f;  //  1/9!
    const float c5 = -2.50521083854417133756e-8f;  // -1/11!
    
    // Horner's method
    float result = reduced * (1.0f + x2 * (
        c1 + x2 * (
        c2 + x2 * (
        c3 + x2 * (
        c4 + x2 * c5
    )))));
    
    return sign * result;
}

/**
 * @brief Floor function implementation for floats
 */
float xfloorf(float x)
{
    if (x >= 0.0f) {
        return (float)(int)x;
    } else {
        float y = (float)(int)x;
        return (y == x) ? y : y - 1.0f;
    }
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
float xcosf(float x)
{
    const float pi = 3.14159265358979323846f;
    const float two_pi = 6.28318530717958647692f;
    const float half_pi = 1.57079632679489661923f;
    
    /* === STEP 1: RANGE REDUCTION TO [0, π/2] === */
    
    // Handle sign: cos(-x) = cos(x)
    float sign = 1.0f;
    if (x < 0.0f) {
        x = -x;
    }
    
    // Reduce to [0, 2π) using periodicity
    float n = xfloorf(x / two_pi);
    float reduced = x - n * two_pi;
    
    // Now reduced is in [0, 2π)
    // Determine quadrant and adjust sign for cosine
    
    if (reduced > 1.5f * pi) {
        // Quadrant IV: [3π/2, 2π) - cos is POSITIVE
        reduced = two_pi - reduced;
        // sign remains positive
    } else if (reduced > pi) {
        // Quadrant III: [π, 3π/2) - cos is NEGATIVE  <-- THIS WAS THE BUG!
        reduced = reduced - pi;
        sign = -sign;  // Flip sign for quadrant III
    } else if (reduced > half_pi) {
        // Quadrant II: [π/2, π) - cos is NEGATIVE
        reduced = pi - reduced;
        sign = -sign;
    }
    // Quadrant I: [0, π/2] - cos is positive, no changes needed
    
    // Now 'reduced' is in [0, π/2]
    
    /* === STEP 2: TAYLOR SERIES WITH HORNER'S METHOD === */
    
    // Maclaurin expansion: cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + x⁸/8! - ...
    float x2 = reduced * reduced;
    
    // High-precision coefficients
    const float c1 = -0.5f;                    // -1/2!
    const float c2 =  4.166666666666666e-2f;   //  1/4!
    const float c3 = -1.388888888888889e-3f;   // -1/6!
    const float c4 =  2.480158730158730e-5f;   //  1/8!
    const float c5 = -2.755731922398589e-7f;   // -1/10!
    
    // Horner's method: 1 + x²*(c1 + x²*(c2 + x²*(c3 + x²*(c4 + x²*c5))))
    float result = 1.0f + x2 * (
        c1 + x2 * (
        c2 + x2 * (
        c3 + x2 * (
        c4 + x2 * c5
    ))));
    
    return sign * result;
}

/**
 * @brief Hyperbolic tangent using Numerically Stable Exponential Form with Asymptotic Clamping
 * 
 * Follows the mathematical specification:
 * - Asymptotic clamping for |x| > 5
 * - Stable form: tanh(x) = (e^(2x) - 1) / (e^(2x) + 1)
 */
float xtanhf(float x)
{
    /* === ASYMPTOTIC CLAMPING === */
    if (x > 5.0f) return 1.0f;
    if (x < -5.0f) return -1.0f;
    
    /* === NUMERICALLY STABLE EXPONENTIAL FORM === */
    float exp_2x = xexpf(2.0f * x);
    return (exp_2x - 1.0f) / (exp_2x + 1.0f);
}