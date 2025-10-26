#ifndef TAYLOR_SERIES_EXP_HPP
#define TAYLOR_SERIES_EXP_HPP

/**
 * @brief Taylor series approximation for exponential function.
 * This implementation avoids using built-in math functions like std::pow or std::factorial.
 * It calculates terms iteratively for efficiency and numerical stability.
 * The series used is:
 *
 * exp(x) = 1 + x/1! + x^2/2! + x^3/3! + ...
 *
 * Each term (Tn) is related to the previous term (Tn-1) by:
 *
 * Tn = Tn-1 * (x / n)
 *
 * where n is the term index (starting from 0 for 1, 1 for x/1!, etc.)
 *
 * @param x The input value for which to calculate the exponential.
 * @param num_terms The number of terms to use in the Taylor series (default:
 * 1000). More terms generally lead to higher accuracy but increased computation.
 * @return The approximated exponential value.
 */
float taylor_series_exp(float x, int num_terms = 1000) {
    float sum = 1.0f; // First term (n=0) is 1
    float term = 1.0f; // Initialize the current term with the first term

    // Iterate to calculate subsequent terms
    for (int n = 1; n < num_terms; ++n) {
        term *= x / static_cast<float>(n); // Calculate the next term
        sum += term; // Add the calculated term to the sum
    }
    return sum;
}

#endif // TAYLOR_SERIES_EXP_HPP