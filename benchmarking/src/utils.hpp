#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstring> // For std::memcpy

namespace Utils {

    /**
     * @brief Custom implementation of the floor function for floating-point numbers.
     * This function calculates the largest integer less than or equal to `val`.
     * It avoids using built-in functions like std::floor.
     *
     * @param val The floating-point value.
     * @return The floor of `val` as a float.
     */
    float floor(float val) {
        long long int_part = static_cast<long long>(val);
        // If val is positive or exactly an integer, static_cast truncates towards zero (which is floor).
        // If val is negative and not an exact integer (e.g., -2.5), static_cast truncates towards zero (-2),
        // so we need to subtract 1 to get the floor (-3).
        if (val < 0.0f && static_cast<float>(int_part) != val) {
            return static_cast<float>(int_part - 1);
        }
        return static_cast<float>(int_part);
    }

    /**
     * @brief Custom implementation of the floating-point mathematical modulo function.
     * This function calculates the remainder of x divided by y, such that the result
     * has the same sign as the divisor (y).
     *
     * When y is positive, the result will always be non-negative and in the range [0, y).
     * For example, Utils::mod(-100.0f, 9.0f) will return 8.0f.
     * This behavior is crucial for consistent range reduction in periodic functions.
     *
     * This implementation avoids using built-in functions like std::fmod, std::floor, std::ceil, std::trunc.
     * It also does not handle NaN or Inf values gracefully as it avoids cmath.
     *
     * @param x The dividend.
     * @param y The divisor (period). For range reduction, y is typically positive.
     * @return The floating-point mathematical modulo (remainder with same sign as y).
     */
    float mod(float x, float y) { // Renamed from fmod to mod
        // If y is zero, the modulo operation is undefined.
        // Returning 0.0f is a pragmatic choice for basic arithmetic,
        // but in a robust system, an error handling mechanism would be needed.
        if (y == 0.0f) return 0.0f;

        // Calculate n = floor(x / y) using the custom floor implementation.
        float n = Utils::floor(x / y); // Use Utils::floor to call our custom implementation

        // Calculate the remainder: x - n * y
        return x - n * y;
    }

    /**
     * @brief Computes 2 raised to the power of n as a float by directly manipulating
     *        the IEEE-754 single-precision exponent bits.
     *
     * This function takes an integer exponent `n` and returns the floating-point value
     * of 2^n. It works by constructing the float representation manually:
     * the exponent field of the IEEE-754 float is set to `n + 127` (bias),
     * and the mantissa is zero. This avoids runtime loops or multiplications.
     *
     * Values of `n` outside the valid float exponent range [-126, 127] are clamped
     * to prevent overflow or underflow.
     *
     * @param n The integer exponent.
     *          - Positive values correspond to 2^n.
     *          - Negative values correspond to fractional powers 2^n (e.g., n = -3 -> 1/8).
     * @return The float value of 2^n.
     *
     * @note This is a low-level bit manipulation implementation. It assumes a
     *       32-bit IEEE-754 single-precision float representation.
     * @note For very large positive or negative exponents, the value will be clamped
     *       to the closest representable float.
     */
    inline float pow2(int n) {
        // Clamp n to valid exponent range for float
        if (n < -126) n = -126;
        if (n > 127) n = 127;

        uint32_t bits = (uint32_t)(n + 127) << 23; // shift n into exponent field
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }


} // namespace Utils

#endif // UTILS_HPP
