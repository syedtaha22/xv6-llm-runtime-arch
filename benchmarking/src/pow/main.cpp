#include <cmath>                  // For std::pow
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"    // Include the general benchmark header
#include "pow_fast_bit.hpp"            // Include the fast bit tricks pow implementation
#include "classic_pow.hpp"             // Include the classic pow implementation

/**
 * @breif Lambda function for std::pow function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_pow = [](float a, float b) -> float { return std::pow(a, b); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float min_base_value = 0.0f;  // Minimum base value for pow function
    const float max_base_value = 10.0f; // Maximum base value for pow function
    const float min_exponent_value = -3.0f; // Minimum exponent value for pow function
    const float max_exponent_value = 3.0f;  // Maximum exponent value for pow function

    // Generate random input data points for the pow function.
    // The `generate_inputs_pair` function is templated to return a `std::vector<std::pair<float, float>>`.
    std::vector<std::pair<float, float>> inputs = Benchmark::function_test_bench<Benchmark::func_pair>::generate_inputs_pair<float>(
        min_base_value, max_base_value,
        min_exponent_value, max_exponent_value,
        num_data_points
    );

    // Create a benchmark instance for pow functions.
    // `std::pow` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func_pair> pow_bench("std::pow", std_pow, inputs);

    // Add std::pow to the functions to be benchmarked.
    pow_bench.add_function("pow_fast_bit", pow_fast_bit);
    pow_bench.add_function("pow_classic", pow_classic);

    pow_bench.run();                                        // Run the benchmark for all added functions.
    pow_bench.print_results();                              // Print the benchmark results to the console.
    pow_bench.log_results("pow_benchmark.log");            // Log the benchmark results.
    pow_bench.save_raw_results_to_csv("raw_pow_data.csv"); // Export the raw benchmark results to a CSV file.

    return 0;
}
