#include <cmath>                  // For std::exp and M_E
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"       // Include the general benchmark header
#include "taylor_series_exp.hpp"           // Include the custom Taylor series exp implementation
#include "range_reduced_exp.hpp"           // Include the range-reduced exp implementation
#include "range_reduced_exp_fastexp2.hpp"  // Include the range-reduced exp using fast exp2
#include "range_reduced_exp_horner.hpp"    // Include the range-reduced exp using Horner's method

/**
 * @breif Lambda function for std::exp function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_exponential = [](float x) -> float { return std::exp(x); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float max_input_value = 10.0f; // Maximum input value for exponential function

    // Generate random input data points for the exponential function.
    // The `generate_inputs` function is templated to return a `std::vector<float>`.
    std::vector<float> inputs = Benchmark::function_test_bench<Benchmark::func>::generate_inputs<float>(
        -max_input_value, max_input_value, num_data_points
    );

    // Create a benchmark instance for exponential functions.
    // `std::exp` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func> exp_bench("std::exp", std_exponential, inputs);

    // Add std::exp to the functions to be benchmarked.
    exp_bench.add_function("taylor_series_exp_1000", [](float x) { return taylor_series_exp(x, 1000); });
    exp_bench.add_function("taylor_series_exp_10", [](float x) { return taylor_series_exp(x, 10); });
    exp_bench.add_function("range_reduced_exp", [](float x) { return range_reduced_exp(x); });
    exp_bench.add_function("rr_exp_fastexp2", [](float x) { return range_reduced_exp_fastexp2(x); });
    exp_bench.add_function("rr_exp_horner", [](float x) { return range_reduced_exp_horner(x);});

    exp_bench.run();                                        // Run the benchmark for all added functions.
    exp_bench.print_results();                              // Print the benchmark results to the console.
    exp_bench.log_results("exp_benchmark.log");            // Log the benchmark results.
    exp_bench.save_raw_results_to_csv("raw_exp_data.csv"); // Export the raw benchmark results to a CSV file.

    return 0;
}