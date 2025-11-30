#include <cmath>                  // For std::log
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"    // Include the general benchmark header
#include "ln_fast_bit_tricks.hpp"      // Include the fast bit tricks ln implementation
#include "ln_poly.hpp"                 // Include the polynomial ln implementation
#include "taylor_series_ln.hpp"        // Include the Taylor series ln implementation
#include "ln_horners_method.hpp"     // Include the Horner's method ln implementation


/**
 * @breif Lambda function for std::ln function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_ln = [](float x) -> float { return std::log(x); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float min_input_value = 0.1f; // Minimum input value for ln function
    const float max_input_value = 10.0f; // Maximum input value for ln function

    // Generate random input data points for the ln function.
    // The `generate_inputs` function is templated to return a `std::vector<float>`.
    std::vector<float> inputs = Benchmark::function_test_bench<Benchmark::func>::generate_inputs<float>(
        min_input_value, max_input_value, num_data_points
    );

    // Create a benchmark instance for ln functions.
    // `std::log` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func> ln_bench("std::log", std_ln, inputs);

    // Add std::log to the functions to be benchmarked.
    ln_bench.add_function("ln_fast_bit_tricks", ln_fast_bit_tricks);
    ln_bench.add_function("ln_poly", ln_polyf);
    ln_bench.add_function("taylor_series_ln_100", [](float x) { return ln_taylorf(x, 100); });
    ln_bench.add_function("taylor_series_ln_10", [](float x) { return ln_taylorf(x, 10); });
    ln_bench.add_function("ln_horners_method", xlnf);

    ln_bench.run();                                        // Run the benchmark for all added functions.
    ln_bench.print_results();                              // Print the benchmark results to the console.
    ln_bench.log_results("ln_benchmark.log");              // Log the benchmark results.
    ln_bench.save_raw_results_to_csv("raw_ln_data.csv");   // Export the raw benchmark results to a CSV file.

    return 0;
}