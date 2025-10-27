#include <cmath>                  // For std::sqrt
#include <vector>                 // For std::vector
#include <iostream>               // For console output

#include "../function_benchmark.hpp"    // Include the general benchmark header
#include "nr_sqrt.hpp"                  // Include the Newton-Raphson sqrt implementation
#include "quake3_nr_sqrt.hpp"           // Include the Quake III inspired sqrt implementation
#include "intel_sqrt.hpp"               // Include the Intel SSE sqrt implementation

/**
 * @breif Lambda function for std::sqrt function.
 * This is used to provide a reference implementation for benchmarking.
 * It is defined here to match the expected function signature in the benchmark.
 */
auto std_sqrt = [](float x) -> float { return std::sqrt(x); };

int main() {
    // Number of data points for benchmarking
    const int num_data_points = 1000;

    const float max_input_value = 1e6f; // Maximum input value for square root function

    // Generate random input data points for the square root function.
    // The `generate_inputs` function is templated to return a `std::vector<float>`.
    std::vector<float> inputs = Benchmark::function_test_bench<Benchmark::func>::generate_inputs<float>(
        0.0f, max_input_value, num_data_points
    );

    // Create a benchmark instance for square root functions.
    // `std::sqrt` is used as the reference function for accuracy comparison.
    Benchmark::function_test_bench<Benchmark::func> sqrt_bench("std::sqrt", std_sqrt, inputs);

    // Add custom square root implementations to the functions to be benchmarked.
    sqrt_bench.add_function("nr_sqrt_1000", [](float x) { return nr_sqrt(x, 1000); });
    sqrt_bench.add_function("nr_sqrt_10", [](float x) { return nr_sqrt(x, 10); });
    sqrt_bench.add_function("nr_sqrt_5", [](float x) { return nr_sqrt(x, 5); });
    sqrt_bench.add_function("quake3_nr_sqrt_3", [](float x) { return quake3_nr_sqrt(x, 3); });
    sqrt_bench.add_function("quake3_nr_sqrt_5", [](float x) { return quake3_nr_sqrt(x, 5); });
    sqrt_bench.add_function("intel_sqrt", [](float x) { return intel_sqrt(x); });

    sqrt_bench.run();                                        // Run the benchmark for all added functions.
    sqrt_bench.print_results();                              // Print the benchmark results to the console.
    sqrt_bench.log_results("sqrt_benchmark.log");            // Log the benchmark results.
    sqrt_bench.save_raw_results_to_csv("raw_sqrt_data.csv"); // Export the raw benchmark results to a CSV file.

    return 0;
}