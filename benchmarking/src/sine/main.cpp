#include <cmath>                  // For std::sin and M_PI
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"        // Include the general benchmark header
#include "taylor_series_sine.hpp"           // Include the custom Taylor series sine implementation
#include "range_reduced_sine.hpp"           // Include the range-reduced sine implementation
#include "range_reduced_sine_horner.hpp"    // Include the range-reduced sine using Horner's method

// Define M_PI if it's not available (common on some systems)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * @breif Lambda function for std::sine function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_sine = [](float x) -> float { return std::sin(x); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float max_input_value = 3 * M_PI; // Maximum input value for sine function

    // Generate random input data points for the sine function.
    // The `generate_inputs` function is templated to return a `std::vector<float>`.
    std::vector<float> inputs = Benchmark::function_test_bench<Benchmark::func>::generate_inputs<float>(
        -max_input_value, max_input_value, num_data_points
    );

    // Create a benchmark instance for sine functions.
    // `std::sin` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func> sine_bench("std::sin", std_sine, inputs);

    // Add std::sin to the functions to be benchmarked.
    sine_bench.add_function("taylor_series_sine_1000", [](float x) { return taylor_series_sine(x, 1000); });
    sine_bench.add_function("taylor_series_sine_7", [](float x) { return taylor_series_sine(x, 7); });
    sine_bench.add_function("range_reduced_sine", [](float x) { return range_reduced_sine(x); });
    sine_bench.add_function("rr_sine_horner", [](float x) { return range_reduced_sine_horner(x);});

    sine_bench.run();                                        // Run the benchmark for all added functions.
    sine_bench.print_results();                              // Print the benchmark results to the console.
    sine_bench.log_results("sine_benchmark.log");            // Log the benchmark results.
    sine_bench.save_raw_results_to_csv("raw_sine_data.csv"); // Export the raw benchmark results to a CSV file.

    return 0;
}
