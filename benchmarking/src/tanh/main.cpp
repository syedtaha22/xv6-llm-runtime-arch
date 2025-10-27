#include <cmath>                  // For std::tanh and M_PI
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"        // Include the general benchmark header
#include "tanh_exp.hpp"                     // Include the custom Taylor series tanh implementation
#include "asymptotic_tanh.hpp"              // Include the range-reduced tanh implementation
#include "asymptotic_tanh_horner.hpp"       // Include the range-reduced tanh using Horner's method

// Define M_PI if it's not available (common on some systems)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * @breif Lambda function for std::tanh function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_tanh = [](float x) -> float { return std::tanh(x); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float max_input_value = 10.0f; // Maximum input value for tanh function

    // Generate random input data points for the tanh function.
    // The `generate_inputs` function is templated to return a `std::vector<float>`.
    std::vector<float> inputs = Benchmark::function_test_bench<Benchmark::func>::generate_inputs<float>(
        -max_input_value, max_input_value, num_data_points
    );

    // Create a benchmark instance for tanh functions.
    // `std::tanh` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func> tanh_bench("std::tanh", std_tanh, inputs);

    // Add std::tanh to the functions to be benchmarked.
    tanh_bench.add_function("tanh_exp", [](float x) { return tanh_exp(x); });
    tanh_bench.add_function("asymptotic_tanh", [](float x) { return asymptotic_tanh(x); });
    tanh_bench.add_function("asymptotic_tanh_horner", [](float x) { return asymptotic_tanh_horner(x);});

    tanh_bench.run();                                        // Run the benchmark for all added functions.
    tanh_bench.print_results();                              // Print the benchmark results to the console.
    tanh_bench.log_results("tanh_benchmark.log");            // Log the benchmark results.
    tanh_bench.save_raw_results_to_csv("raw_tanh_data.csv"); // Export the raw benchmark results to a CSV file.

    return 0;
}