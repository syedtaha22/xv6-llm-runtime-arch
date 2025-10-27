#ifndef NR_SQRT_HPP
#define NR_SQRT_HPP

/**
 * @brief Computes the square root of a non-negative number using the Newton–Raphson method.
 *
 * This function provides a simple software implementation of square root.
 * It iteratively refines an initial guess using the update rule:
 *     Y(n+1) = 0.5 * { Y(n) + x / Y(n) }
 *
 * @param x The input value (must be non-negative).
 * @param num_iters The number of iterations to perform (default: 1000).
 * More iterations generally lead to higher accuracy.
 * @return The approximate square root of the input value.
 */
inline float nr_sqrt(float x, int num_iters = 1000) {
    if (x <= 0.0f) return 0.0f;

    // Initial guess (simple heuristic)
    float guess = x * 0.5f;

    // Perform a few Newton–Raphson iterations
    // Y(n+1) = 0.5 * { Y(n) + x / Y(n) }
    for (int i = 0; i < num_iters; ++i)
        guess = 0.5f * (guess + x / guess);

    return guess;
}

#endif // NR_SQRT_HPP
