#ifndef FUNCTION_BENCHMARK_HPP
#define FUNCTION_BENCHMARK_HPP

#include <iostream>     // For std::cerr, std::cout
#include <string>       // For std::string
#include <vector>       // For std::vector
#include <chrono>       // For std::chrono high_resolution_clock
#include <cmath>        // For std::fabs and mathematical functions (sqrt for RMS)
#include <fstream>      // For std::ofstream
#include <functional>   // For std::function
#include <utility>      // For std::pair
#include <filesystem>   // For std::filesystem::create_directory, std::filesystem::exists, std::filesystem::path
#include <random>       // For random number generation
#include <type_traits>  // For std::is_floating_point, std::is_integral, std::is_arithmetic
#include <algorithm>    // For std::generate_n, std::back_inserter
#include <numeric>      // For std::inner_product, std::accumulate (for RMS calculations)
#include <map>          // For storing function outputs by name
#include <iomanip>      // For std::fixed and std::setprecision
#include <stdexcept>    // For std::invalid_argument

/**
 * @brief Namespace for all benchmarking related classes and structs.
 * This helps to prevent naming conflicts with other parts of a larger codebase.
 */
namespace Benchmark {

    /**
     * @brief Define a type alias for a common function signature (float input, float output).
     * This is placed inside the namespace as it's directly related to the benchmark utilities.
     */
    using func = std::function<float(float)>;

    /**
     * @brief Enum to specify the type of error metric to calculate.
     */
    enum class ErrorMetric {
        RELATIVE_ERROR, ///< |predicted - actual| / |actual| (fallbacks to absolute if actual is near zero)
        ABSOLUTE_ERROR, ///< |predicted - actual|
        RMS_ERROR       ///< sqrt(sum((predicted - actual)^2) / N)
    };

    /**
     * @brief Structure to hold the results of a single benchmark run for a function.
     */
    struct result {
        std::string name;              ///< name of the benchmarked function
        float time_sec;                ///< execution time in seconds
        float calculated_error_value;  ///< the calculated error value (meaning depends on error_metric_type)
        std::vector<float> outputs;    ///< outputs from the function for all input values
    };

    /**
     * @brief A general-purpose test bench for mathematical functions.
     * @tparam FunctionType The type of the function to be benchmarked (e.g., std::function<float(float)>).
     * This allows flexibility for lambda functions, function pointers, etc.
     */
    template<typename FunctionType>
    class function_test_bench {
        // Removed: FunctionType reference_function;
        // Removed: std::string reference_function_name;
        // The reference function is now the first entry in test_results.

        std::vector<std::pair<std::string, FunctionType>> functions_to_benchmark; ///< List of functions to add for benchmarking (excluding reference)
        std::vector<float> input_values;                                  ///< Input values for function evaluation
        std::vector<float> reference_results;                             ///< Pre-calculated results from the reference function (for error calculations)
        // Removed: std::map<std::string, std::vector<float>> benchmarked_outputs;
        // All outputs are now directly stored within the 'result' struct in 'test_results'.

        std::vector<result> test_results;                                 ///< Stores the benchmark results for all functions (including reference)
        ErrorMetric chosen_error_metric;                                  ///< The error metric chosen for this benchmark instance

        /**
         * @brief Returns a string representation of the chosen error metric.
         * @return The name of the error metric.
         */
        std::string get_error_metric_name() const {
            switch (chosen_error_metric) {
            case ErrorMetric::RELATIVE_ERROR: return "Avg Relative Error";
            case ErrorMetric::ABSOLUTE_ERROR: return "Avg Absolute Error";
            case ErrorMetric::RMS_ERROR:      return "RMS Error";
            default:                          return "Unknown Error Metric"; // Should not happen
            }
        }

        /**
         * @brief Calculates the relative error between a predicted and actual value.
         * Handles cases where the actual value is very close to zero to avoid division by zero.
         * @param predicted The value obtained from the benchmarked function.
         * @param actual The reference value.
         * @return The relative error.
         */
        float calculate_relative_error(float predicted, float actual) const {
            // If the actual value is very close to zero, use absolute error
            // as relative error becomes unstable/meaningless.
            if (std::fabs(actual) < 1e-5f) return std::fabs(predicted - actual);
            return std::fabs(predicted - actual) / std::fabs(actual);
        }

        /**
         * @brief Calculates the absolute error between a predicted and actual value.
         * @param predicted The value obtained from the benchmarked function.
         * @param actual The reference value.
         * @return The absolute error.
         */
        float calculate_absolute_error(float predicted, float actual) const {
            return std::fabs(predicted - actual);
        }

        /**
         * @brief Calculates the Root Mean Square (RMS) error between predicted and actual values.
         * @param outputs The vector of predicted values.
         * @param references The vector of reference values.
         * @return The RMS error.
         * @throws std::invalid_argument if outputs or references are empty or have different sizes.
         */
        float calculate_rms_error(const std::vector<float>& outputs, const std::vector<float>& references) const {
            if (outputs.empty() || outputs.size() != references.size()) {
                throw std::invalid_argument("Outputs and references must be non-empty and of the same size for RMS error calculation.");
            }

            double sum_sq_diff = 0.0;
            for (size_t i = 0; i < outputs.size(); ++i) {
                double diff = static_cast<double>(outputs[i]) - static_cast<double>(references[i]);
                sum_sq_diff += diff * diff;
            }
            return static_cast<float>(std::sqrt(sum_sq_diff / outputs.size()));
        }

        /**
         * @brief Calculates the average error based on the chosen error metric for a set of outputs.
         * @param outputs The outputs from the benchmarked function.
         * @return The calculated error value.
         */
        float get_error(const std::vector<float>& outputs) const {
            if (chosen_error_metric == ErrorMetric::RMS_ERROR)
                return calculate_rms_error(outputs, reference_results);

            else {
                double total_err = 0.0;
                for (size_t i = 0; i < input_values.size(); ++i) {
                    if (chosen_error_metric == ErrorMetric::RELATIVE_ERROR)
                        total_err += calculate_relative_error(outputs[i], reference_results[i]);

                    else total_err += calculate_absolute_error(outputs[i], reference_results[i]);
                }
                return static_cast<float>(total_err / input_values.size());
            }
        }

        /**
         * @brief Benchmarks a single function, calculating its execution time, outputs, and error.
         * This is an internal helper method.
         * @param name The name of the function to benchmark.
         * @param func_to_benchmark The function callable itself.
         * @param is_reference_function True if this is the reference function (error will be 0.0).
         * @return A result struct containing the benchmark data for this function.
         */
        result benchmark(const std::string& name, FunctionType func_to_benchmark, bool is_reference_function = false) {
            std::vector<float> current_outputs(input_values.size());

            auto start = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < input_values.size(); ++i) current_outputs[i] = func_to_benchmark(input_values[i]);
            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<float> elapsed = end - start;

            float calculated_error = 0.0f;
            // If this is the reference function, we don't calculate error, just return 0.0
            if (!is_reference_function) calculated_error = get_error(current_outputs);

            return { name, elapsed.count(), calculated_error, current_outputs };
        }

    public:
        /**
         * @brief Constructs a function_test_bench instance.
         * This constructor also benchmarks the reference function, which will be the first entry in the results.
         * @param ref_func_name The name of the reference function (for logging purposes).
         * @param ref_func The reference function to compare others against.
         * @param inputs A vector of input values to evaluate functions with.
         * @param metric The error metric to use for reporting (default: RELATIVE_ERROR).
         */
        function_test_bench(std::string ref_func_name, FunctionType ref_func, const std::vector<float>& inputs,
            ErrorMetric metric = ErrorMetric::RELATIVE_ERROR)
            : input_values(inputs),
            chosen_error_metric(metric)
        {
            // Benchmark the reference function itself
            result ref_benchmark_result = benchmark(ref_func_name, ref_func, true);

            // Store reference_results from the benchmarked outputs for other functions' error calculations
            reference_results = ref_benchmark_result.outputs;

            // Store the reference function's benchmark result. It will always be the first in test_results.
            test_results.push_back(ref_benchmark_result);
        }

        /**
         * @brief Adds a function to the benchmark list.
         * @param name A descriptive name for the function.
         * @param f The function to add.
         */
        void add_function(const std::string& name, FunctionType f) {
            functions_to_benchmark.emplace_back(name, f);
        }

        /**
         * @brief Runs the benchmark for all added functions.
         * Measures execution time and calculates the chosen average error metric.
         */
        void run() {
            for (const auto& [name, func_to_run] : functions_to_benchmark) {
                result current_benchmark_result = benchmark(name, func_to_run);
                test_results.push_back(current_benchmark_result);
            }
        }

        /**
         * @brief Logs the benchmark results (time and aggregated error) to a specified file within the 'results' directory.
         * Creates the 'results' directory if it doesn't exist.
         * @param filename The name of the log file (e.g., "exp_results.log").
         */
        void log_results(const std::string& filename = "results.log") const {
            // Create the 'results' directory if it doesn't exist
            std::filesystem::path log_dir = "results";
            if (!std::filesystem::exists(log_dir)) {
                if (!std::filesystem::create_directory(log_dir)) {
                    std::cerr << "Error: Failed to create 'results' directory.\n";
                    return;
                }
            }

            std::filesystem::path full_path = log_dir / filename;
            std::ofstream log_file(full_path);
            if (!log_file) {
                std::cerr << "Error: Failed to open log file: " << full_path << "\n";
                return;
            }

            // Set output precision and fixed-point notation for time and error values
            log_file << std::fixed << std::setprecision(7);

            log_file << "Function Benchmark Results ( " << input_values.size() << " evaluations per function )\n\n";
            // Iterate through test_results, first one is always the reference
            for (const auto& r : test_results) {
                log_file << "Function: " << r.name << "\n";
                log_file << "  Time: " << r.time_sec << " seconds\n";
                log_file << "  " << get_error_metric_name() << ": " << r.calculated_error_value << "\n";
                log_file << "\n";
            }
            log_file.close();
            std::cout << "Benchmark results written to: " << full_path << "\n";
        }

        /**
         * @brief Prints the benchmark results (time and aggregated error) to the console.
         */
        void print_results() const {
            std::cout << "\n--- Benchmark Results ---\n";
            std::cout << "Evaluated over " << input_values.size() << " input values.\n\n";

            // Set output precision and fixed-point notation for time and error values
            std::cout << std::fixed << std::setprecision(7);

            // Iterate through test_results, first one is always the reference
            for (const auto& r : test_results) {
                std::cout << "Function: " << r.name << "\n";
                std::cout << "  Time: " << r.time_sec << " seconds\n";
                std::cout << "  " << get_error_metric_name() << ": " << r.calculated_error_value << "\n";
                std::cout << "\n";
            }
        }

        /**
         * @brief Saves all raw benchmark results (inputs, reference outputs, and function outputs) to a CSV file.
         * The CSV will have columns: x, reference_function_output, func1_output, func2_output, ...
         *
         * @param filename The name of the CSV file (e.g., "raw_sine_data.csv").
         */
        void save_raw_results_to_csv(const std::string& filename = "raw_benchmark_data.csv") const {
            std::filesystem::path log_dir = "results";
            if (!std::filesystem::exists(log_dir)) {
                if (!std::filesystem::create_directory(log_dir)) {
                    std::cerr << "Error: Failed to create 'results' directory for CSV.\n";
                    return;
                }
            }

            std::filesystem::path full_path = log_dir / filename;
            std::ofstream csv_file(full_path);
            if (!csv_file) {
                std::cerr << "Error: Failed to open CSV file: " << full_path << "\n";
                return;
            }

            // Set output precision for CSV, but allow defaultfloat for x values for exactness
            // For general output precision in CSV, it's often better to let it be more verbose
            // or use a very high precision, then let analysis tools handle formatting.
            // However, to strictly avoid scientific for *all* floats, we'll apply fixed precision here too.
            // csv_file << std::fixed << std::setprecision(9); // More precision for CSV raw data

            // Write CSV header
            csv_file << "x";
            // Add headers for all benchmarked functions, in the order they appear in test_results
            for (const auto& r : test_results) {
                csv_file << "," << r.name << "_output";
            }
            csv_file << "\n";

            // Write data rows
            for (size_t i = 0; i < input_values.size(); ++i) {
                csv_file << input_values[i]; // x value
                // Write outputs for each function in the order they appear in test_results
                for (const auto& r : test_results) csv_file << "," << r.outputs[i];
                csv_file << "\n";
            }

            csv_file.close();
            std::cout << "Raw benchmark results saved to CSV: " << full_path << "\n";
        }


        /**
         * @brief Generates a vector of N random data points of a specified numeric type within a given range.
         * The type T determines the type of the elements in the generated vector and the type of the range values.
         * @tparam T The numeric type of the elements in the generated vector (e.g., int, float, double).
         * @param min_val The minimum value of the range (inclusive).
         * @param max_val The maximum value of the range (inclusive).
         * @param num_points The number of data points to generate. Must be at least 0.
         * @return A std::vector<T> containing the randomly generated data points.
         */
        template<typename T>
        static std::vector<T> generate_inputs(T min_val, T max_val, size_t num_points) {
            std::vector<T> inputs;
            if (num_points == 0) return inputs; // Return empty vector if no points requested

            inputs.reserve(num_points);

            // Seed the random number generator
            std::random_device rd;
            std::mt19937 gen(rd());

            // Use if constexpr to select the appropriate distribution based on T
            if constexpr (std::is_floating_point_v<T>) {
                std::uniform_real_distribution<T> distrib(min_val, max_val);
                std::generate_n(std::back_inserter(inputs), num_points, [&]() {
                    return distrib(gen);
                    });
            }
            else if constexpr (std::is_integral_v<T>) {
                std::uniform_int_distribution<T> distrib(min_val, max_val);
                std::generate_n(std::back_inserter(inputs), num_points, [&]() {
                    return distrib(gen);
                    });
            }
            else {
                // This static_assert will cause a compile-time error if an unsupported type is used.
                static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type (e.g., int, float, double).");
            }

            return inputs;
        }
    };

} // namespace Benchmark

#endif // FUNCTION_BENCHMARK_HPP
