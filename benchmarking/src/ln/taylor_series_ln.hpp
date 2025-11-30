#ifndef TAYLOR_SERIES_LN_HPP
#define TAYLOR_SERIES_LN_HPP

/**
 * @brief Computes natural logarithm using the pure Taylor series expansion.
 *
 * ln(1 + y) = y - y^2/2 + y^3/3 - y^4/4 + ... up to n terms.
 *
 * @param x Input value (>0)
 * @param n Number of terms in the Taylor series
 * @return Approximated ln(x)
 */
float ln_taylorf(float x, int n) {
    if (x <= 0.0f) return NAN;

    float y = x - 1.0f;        // Center series at 1
    float term = y;             // Current term y^k / k
    float result = term;

    for (int k = 2; k <= n; ++k) {
        term *= -y * (k - 1) / k;  // Efficiently compute (-1)^(k+1) * y^k / k
        result += term;
    }

    return result;
}


#endif // TAYLOR_SERIES_LN_HPP