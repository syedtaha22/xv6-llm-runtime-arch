#ifndef QUAKE3_NR_SQRT_HPP
#define QUAKE3_NR_SQRT_HPP

/**
 * @brief Approximates the square root of a non-negative float
 *        using a bit-level exponent trick inspired by Quake III’s
 *        fast inverse square root, followed by Newton–Raphson refinement.
 *
 * This method derives an initial approximation by halving the exponent bits
 * of the floating-point representation of `x`, producing a close estimate
 * of sqrt(x). The approximation is then refined with a few iterations of:
 *
 *     yₙ₊₁ = 0.5 * (yₙ + x / yₙ)
 *
 * @param x The input value (must be non-negative).
 * @param num_iters The number of Newton–Raphson iterations to perform (default: 3).
 *                  More iterations yield higher accuracy.
 * @return Approximate square root of x.
 */
inline float quake3_nr_sqrt(float x, int num_iters = 3) {
    if (x <= 0.0f) return 0.0f;

    // --------------------------------------------
    // Initial guess via bit-level exponent halving
    // --------------------------------------------
    float guess = x;
    int i = *(int*)&guess;
    i = (i >> 1) + 0x1FC00000;  // heuristic exponent adjustment
    guess = *(float*)&i;

    // --------------------------------------------
    // Perform a few Newton–Raphson iterations
    // --------------------------------------------
    // y_{n+1} = 0.5 * (y_n + x / y_n)
    for (int iter = 0; iter < num_iters; ++iter)
        guess = 0.5f * (guess + x / guess);

    return guess;
}

#endif // QUAKE3_NR_SQRT_HPP
