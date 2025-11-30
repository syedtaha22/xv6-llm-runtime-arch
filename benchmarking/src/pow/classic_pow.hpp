#ifndef CLASSIC_POW_HPP
#define CLASSIC_POW_HPP

#include <stdint.h>
#include <string.h>

#include "../ln/ln_horners_method.hpp" // Include our custom ln implementation
#include "../exp/range_reduced_exp_horner.hpp" // Include our custom exp implementation

/**
 * @brief Computes a^b using e^(b ln(a))
 */
float pow_classic(float a, float b) {
    if (a == 0.0f) return (b == 0.0f) ? 1.0f : 0.0f; // handle 0^0 and 0^b
    if (a < 0.0f) return NAN; // only positive a supported
    float log_a = xlnf(a);
    return range_reduced_exp_horner(b * log_a);
}

#endif // CLASSIC_POW_HPP
