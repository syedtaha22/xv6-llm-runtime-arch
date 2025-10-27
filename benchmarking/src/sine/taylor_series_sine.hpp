#ifndef TAYLOR_SERIES_SINE_HPP
#define TAYLOR_SERIES_SINE_HPP

/**
 * @brief Taylor series approximation for sine.
 * This implementation avoids using built-in math functions like std::pow or std::factorial.
 * It calculates terms iteratively for efficiency and numerical stability.
 * The series used is:
 *
 *  sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ...
 *
 * Each term (Tn) is related to the previous term (Tn-1) by:
 *
 * Tn = Tn-1 * (-x^2) / ((2n)*(2n+1))
 *
 * where n is the term index (starting from 0 for x, 1 for -x^3/3!, etc.)
 *
 * @param x The input value for which to calculate sine.
 * @param num_terms The number of terms to use in the Taylor series (default: 1000).
 * More terms generally lead to higher accuracy but increased computation.
 * @return The approximated sine value.
 */
float taylor_series_sine(float x, int num_terms = 1000) {
    float sum = x;           // First term (n=0) is x
    float term = x;          // Initialize the current term with the first term
    float x_squared = x * x; // Pre-calculate x^2 for efficiency

    // Iterate to calculate subsequent terms
    for (int n = 1; n < num_terms; ++n) {
        term *= -x_squared / ((2.0f * n) * (2.0f * n + 1.0f));
        sum += term; // Add the calculated term to the sum
    }
    return sum;
}

#endif // TAYLOR_SERIES_SINE_HPP