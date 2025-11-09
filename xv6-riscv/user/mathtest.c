/**
 * @file test_math.c
 * @brief Comprehensive test suite for custom math library functions in xv6
 * 
 * @details
 * This file implements a comprehensive test suite for custom mathematical
 * functions implemented for the xv6 operating system. It tests functions
 * including sqrtf_new, expf_new, powf_new, sinf_new, cosf_new, and fabsf_new
 * against expected values with configurable error tolerances.
 * 
 * The test suite features:
 * - Color-coded output for easy result interpretation
 * - Detailed error reporting with floating-point value visualization
 * - Support for special values (NaN, Infinity)
 * - Multiple error metrics (absolute, relative)
 * - Comprehensive test case coverage
 * 
 * @author Hamna Sajid
 * @date 11/09/2025
 */

#include "kernel/types.h"
#include "user.h"
#include "math.h"
#include "testmath.h"
#include "math_test_cases.h"

/** @brief ANSI Color Codes for terminal output */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

/** @brief Global test counters */
int tests_passed = 0;  /**< Count of passed test cases */
int tests_failed = 0;  /**< Count of failed test cases */

/**
 * @brief Build detailed failure message for test cases
 * 
 * @param buffer     Output buffer for the failure message
 * @param size       Size of the output buffer
 * @param func_name  Name of the function being tested
 * @param case_num   Test case number
 * @param input1     First input value to the function
 * @param input2     Second input value (for two-parameter functions)
 * @param result     Actual result from the function
 * @param expected   Expected result
 * @param error      Calculated error between result and expected
 * @param extra      Additional context information
 * 
 * @details
 * Constructs a human-readable failure message showing the test case details,
 * inputs, expected output, actual output, and error. Handles floating-point
 * to string conversion manually for xv6 compatibility.
 */
void build_fail_message(char* buffer, int size, const char* func_name, int case_num, 
                       float input1, float input2, float result, float expected, 
                       float error, const char* extra) {
    int pos = 0;
    
    // Start with case number and function name
    pos += strlen(strcpy(buffer + pos, "Case "));
    
    // Convert case number to string
    int n = case_num;
    char num_str[16];
    char* num_ptr = num_str;
    if (n == 0) {
        *num_ptr++ = '0';
    } else {
        char rev[16];
        char* rev_ptr = rev;
        while (n > 0) {
            *rev_ptr++ = '0' + (n % 10);
            n /= 10;
        }
        while (rev_ptr > rev) {
            *num_ptr++ = *(--rev_ptr);
        }
    }
    *num_ptr = '\0';
    pos += strlen(strcpy(buffer + pos, num_str));
    
    pos += strlen(strcpy(buffer + pos, ": "));
    pos += strlen(strcpy(buffer + pos, func_name));
    pos += strlen(strcpy(buffer + pos, "("));
    
    // Add first input
    if (pos < size - 50) {
        char float_str[32];
        char* fptr = float_str;
        
        // Simple float to string conversion
        if (input1 < 0) {
            *fptr++ = '-';
            input1 = -input1;
        }
        int ipart = (int)input1;
        
        // Convert integer part
        if (ipart == 0) {
            *fptr++ = '0';
        } else {
            char rev[16];
            char* rev_ptr = rev;
            int n2 = ipart;
            while (n2 > 0) {
                *rev_ptr++ = '0' + (n2 % 10);
                n2 /= 10;
            }
            while (rev_ptr > rev) {
                *fptr++ = *(--rev_ptr);
            }
        }
        
        *fptr++ = '.';
        
        // Fractional part (2 digits)
        float frac = input1 - ipart;
        for (int i = 0; i < 2 && pos < size - 10; i++) {
            frac *= 10;
            int digit = (int)frac;
            *fptr++ = '0' + digit;
            frac -= digit;
        }
        *fptr = '\0';
        
        pos += strlen(strcpy(buffer + pos, float_str));
    }
    
    // Add second input if provided (for powf)
    if (input2 != 0.0f || strcmp(func_name, "powf_new") == 0) {
        pos += strlen(strcpy(buffer + pos, ", "));
        
        if (pos < size - 50) {
            char float_str[32];
            char* fptr = float_str;
            
            if (input2 < 0) {
                *fptr++ = '-';
                input2 = -input2;
            }
            int ipart = (int)input2;
            
            if (ipart == 0) {
                *fptr++ = '0';
            } else {
                char rev[16];
                char* rev_ptr = rev;
                int n2 = ipart;
                while (n2 > 0) {
                    *rev_ptr++ = '0' + (n2 % 10);
                    n2 /= 10;
                }
                while (rev_ptr > rev) {
                    *fptr++ = *(--rev_ptr);
                }
            }
            
            *fptr++ = '.';
            
            float frac = input2 - ipart;
            for (int i = 0; i < 2 && pos < size - 10; i++) {
                frac *= 10;
                int digit = (int)frac;
                *fptr++ = '0' + digit;
                frac -= digit;
            }
            *fptr = '\0';
            
            pos += strlen(strcpy(buffer + pos, float_str));
        }
    }
    
    pos += strlen(strcpy(buffer + pos, ")"));
    
    // Add extra info if provided
    if (extra && pos < size - strlen(extra) - 1) {
        pos += strlen(strcpy(buffer + pos, extra));
    }
}

/**
 * @brief Print a passing test message in green
 * 
 * @param message Description of the passed test
 */
void pass(const char* message) {
    printf(COLOR_GREEN "✓ PASS: " COLOR_RESET "%s\n", message);
}

/**
 * @brief Print a failing test message in red
 * 
 * @param message Description of the failed test
 */
void fail(const char* message) {
    printf(COLOR_RED "✗ FAIL: " COLOR_RESET "%s\n", message);
}

/**
 * @brief Print an informational message in blue
 * 
 * @param message Information to display
 */
void info(const char* message) {
    printf(COLOR_BLUE " INFO: " COLOR_RESET "%s\n", message);
}

/**
 * @brief Print a warning message in yellow
 * 
 * @param message Warning to display
 */
void warning(const char* message) {
    printf(COLOR_YELLOW " WARN: " COLOR_RESET "%s\n", message);
}

/**
 * @brief Check if a float value is NaN (Not a Number)
 * 
 * @param x Float value to check
 * @return int Nonzero if x is NaN, zero otherwise
 * 
 * @note Uses the IEEE-754 property that NaN != NaN
 */
int is_nan(float x)
{
    return (x != x);
}

/**
 * @brief Check if a float value is Infinity
 * 
 * @param x Float value to check
 * @return int Nonzero if x is Infinity, zero otherwise
 * 
 * @note Uses the property that Infinity * 2 == Infinity
 */
int is_inf(float x)
{
    return (x != 0.0f && x * 2.0f == x);
}

/**
 * @brief Check if a float value is finite (not NaN or Infinity)
 * 
 * @param x Float value to check
 * @return int Nonzero if x is finite, zero otherwise
 */
int is_finite(float x)
{
    return !is_nan(x) && !is_inf(x);
}

/**
 * @brief Print a float value with special handling for NaN and Infinity
 * 
 * @param f Float value to print
 * 
 * @details
 * Handles special cases (NaN, Infinity) with colored output and provides
 * formatted output for regular numbers with up to 4 decimal places.
 */
void print_float(float f)
{
    // Handle special cases first
    if (f != f)
    { // NaN
        printf(COLOR_YELLOW "NaN" COLOR_RESET);
        return;
    }

    // Check for infinity
    if (f * 2.0f == f && f != 0.0f)
    {
        if (f > 0)
        {
            printf(COLOR_MAGENTA "INF" COLOR_RESET);
        }
        else
        {
            printf(COLOR_MAGENTA "-INF" COLOR_RESET);
        }
        return;
    }

    // Regular number
    if (f < 0)
    {
        printf("-");
        f = -f;
    }

    int intpart = (int)f;
    printf("%d.", intpart);

    float frac = f - intpart;
    for (int i = 0; i < 4; i++)
    {
        frac *= 10;
        int digit = (int)frac;
        printf("%d", digit);
        frac -= digit;
        if (frac < 1e-6f)
            break; // Stop if remainder is negligible
    }
}

/**
 * @brief Compare two floats for equality using absolute error
 * 
 * @param a First float value
 * @param b Second float value
 * @param epsilon Maximum allowed absolute difference
 * @return int Nonzero if |a-b| < epsilon, zero otherwise
 * 
 * @note Suitable for comparing numbers near zero
 */
int float_equals_abs(float a, float b, float epsilon)
{
    float diff = a - b;
    if (diff < 0)
        diff = -diff;
    return diff < epsilon;
}

/**
 * @brief Compare two floats for equality using relative error
 * 
 * @param a First float value
 * @param b Second float value
 * @param epsilon Maximum allowed relative difference
 * @return int Nonzero if relative error < epsilon, zero otherwise
 * 
 * @details
 * Uses relative error for larger numbers and absolute error for small numbers
 * to provide robust comparison across different magnitude ranges.
 */
int float_equals_rel(float a, float b, float epsilon)
{
    if (a == b)
        return 1;

    float diff = a - b;
    if (diff < 0)
        diff = -diff;

    // Use relative error for larger numbers, absolute for small
    float magnitude = (fabsf_new(a) + fabsf_new(b)) * 0.5f;
    if (magnitude < 1.0f)
    {
        return diff < epsilon;
    }
    else
    {
        return diff / magnitude < epsilon;
    }
}

/**
 * @brief Calculate and display error metrics for a test function
 * 
 * @param func_name    Name of the function being tested
 * @param total_cases  Total number of test cases
 * @param passed_cases Number of passed test cases
 * @param max_error    Maximum observed error
 * @param avg_error    Average error across all cases
 * 
 * @details
 * Displays comprehensive test results with color-coded pass rates and
 * error statistics. Provides success rate as a percentage.
 */
void calculate_error_metrics(const char *func_name, int total_cases, int passed_cases,
                             float max_error, float avg_error)
{
    printf(COLOR_CYAN "  %s Results: " COLOR_RESET, func_name);
    
    // Color code based on pass rate
    if (passed_cases == total_cases) {
        printf(COLOR_GREEN "Passed: %d/%d" COLOR_RESET, passed_cases, total_cases);
    } else if (passed_cases >= total_cases * 0.8) {
        printf(COLOR_YELLOW "Passed: %d/%d" COLOR_RESET, passed_cases, total_cases);
    } else {
        printf(COLOR_RED "Passed: %d/%d" COLOR_RESET, passed_cases, total_cases);
    }
    
    printf(", Max error: ");
    print_float(max_error);
    printf(", Avg error: ");
    print_float(avg_error);
    printf("\n");

    // Calculate and display percentage with color
    float percentage = (float)passed_cases / total_cases * 100.0f;
    printf("  Success rate: ");
    
    if (percentage == 100.0f) {
        printf(COLOR_GREEN);
    } else if (percentage >= 80.0f) {
        printf(COLOR_YELLOW);
    } else {
        printf(COLOR_RED);
    }
    
    print_float(percentage);
    printf("%%" COLOR_RESET "\n");
}

/**
 * @brief Test suite for sqrtf_new function
 * 
 * @details
 * Tests the custom square root implementation against a comprehensive set
 * of test cases. Handles special values (NaN, Infinity) and calculates
 * both absolute error and success metrics.
 */
void test_sqrtf()
{
    printf(COLOR_BOLD "Testing sqrtf_new - Comprehensive Test Suite" COLOR_RESET "\n");
    int passed = 0;
    float max_error = 0.0f;
    float total_error = 0.0f;
    int valid_cases = 0; // Cases where expected is finite

    for (int i = 0; i < sqrtf_count; i++)
    {
        float input = sqrtf_inputs[i];
        float expected = sqrtf_expected[i];
        float result = sqrtf_new(input);

        // Handle NaN and Infinity cases
        if (is_nan(expected) && is_nan(result))
        {
            passed++;
            continue;
        }
        if (is_inf(expected) && is_inf(result))
        {
            passed++;
            continue;
        }

        // Calculate error for finite cases
        if (is_finite(expected) && is_finite(result))
        {
            float error = fabsf_new(result - expected);
            if (error > max_error)
                max_error = error;
            total_error += error;
            valid_cases++;

            if (error < 1e-5f)
            {
                passed++;
            }
            else
            {
                printf("  ");
                fail("sqrtf case failed - see details above");
                printf("    Input: "); print_float(input); printf("\n");
                printf("    Expected: "); print_float(expected); printf("\n");
                printf("    Got: "); print_float(result); printf("\n");
                printf("    Error: "); print_float(error); printf("\n");
            }
        }
        else
        {
            // Handle cases where result doesn't match expected special value
            if (!float_equals_abs(result, expected, 1e-5f))
            {
                printf("  ");
                fail("sqrtf special value mismatch");
                printf("    Input: "); print_float(input); printf("\n");
                printf("    Expected: "); print_float(expected); printf("\n");
                printf("    Got: "); print_float(result); printf("\n");
            }
            else
            {
                passed++;
            }
        }
    }

    calculate_error_metrics("sqrtf", sqrtf_count, passed, max_error,
                            valid_cases > 0 ? total_error / valid_cases : 0.0f);

    if (passed == sqrtf_count)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Test suite for expf_new function
 * 
 * @details
 * Tests the custom exponential function implementation. Uses relative error
 * for larger values and handles edge cases including overflow to infinity
 * and NaN inputs.
 */
void test_expf()
{
    printf(COLOR_BOLD "Testing expf_new - Comprehensive Test Suite" COLOR_RESET "\n");
    int passed = 0;
    float max_error = 0.0f;
    float total_error = 0.0f;
    int valid_cases = 0;

    for (int i = 0; i < expf_count; i++)
    {
        float input = expf_inputs[i];
        float expected = expf_expected[i];
        float result = expf_new(input);

        // Handle infinity cases
        if (expected > 1e10f && result > 1e10f)
        { // Both are large (infinity)
            passed++;
            continue;
        }

        // Handle NaN cases
        if (expected != expected && result != result)
        { // Both are NaN
            passed++;
            continue;
        }

        // Calculate error for finite cases
        if (expected < 1e10f && result < 1e10f)
        {
            float error = fabsf_new(result - expected);
            if (error > max_error)
                max_error = error;
            total_error += error;
            valid_cases++;

            // Use relative error for larger values
            float rel_error = (expected > 1.0f) ? error / expected : error;

            if (rel_error < 0.01f)
            { // 1% relative error
                passed++;
            }
            else
            {
                printf("  ");
                fail("expf case failed - see details above");
                printf("    Input: "); print_float(input); printf("\n");
                printf("    Expected: "); print_float(expected); printf("\n");
                printf("    Got: "); print_float(result); printf("\n");
                printf("    Rel Error: "); print_float(rel_error); printf("\n");
            }
        }
        else
        {
            printf("  ");
            fail("expf infinity mismatch");
            printf("    Input: "); print_float(input); printf("\n");
            printf("    Expected: "); print_float(expected); printf("\n");
            printf("    Got: "); print_float(result); printf("\n");
        }
    }

    calculate_error_metrics("expf", expf_count, passed, max_error,
                            valid_cases > 0 ? total_error / valid_cases : 0.0f);

    if (passed == expf_count)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Test suite for powf_new function
 * 
 * @details
 * Tests the custom power function implementation. Uses relative error
 * due to the wide range of possible outputs. Handles special cases
 * including zero and negative exponents, and edge cases with large values.
 */
void test_powf()
{
    printf(COLOR_BOLD "Testing powf_new - Comprehensive Test Suite" COLOR_RESET "\n");
    int passed = 0;
    float max_error = 0.0f;
    float total_error = 0.0f;
    int valid_cases = 0;

    for (int i = 0; i < powf_count; i++)
    {
        float base = powf_inputs[i * 2];
        float exponent = powf_inputs[i * 2 + 1];
        float expected = powf_expected[i];
        float result = powf_new(base, exponent);

        // Handle NaN and Infinity cases
        if (is_nan(expected) && is_nan(result))
        {
            passed++;
            continue;
        }
        if (is_inf(expected) && is_inf(result))
        {
            passed++;
            continue;
        }

        // Calculate error for finite cases
        if (is_finite(expected) && is_finite(result))
        {
            // Use relative error for powf
            float error = fabsf_new(result - expected);
            float rel_error = (expected != 0) ? error / fabsf_new(expected) : error;

            if (rel_error > max_error)
                max_error = rel_error;
            total_error += rel_error;
            valid_cases++;

            if (rel_error < 0.05f)
            { // 5% relative error for powf
                passed++;
            }
            else
            {
                printf("  ");
                fail("powf case failed - see details above");
                printf("    Base: "); print_float(base); printf("\n");
                printf("    Exponent: "); print_float(exponent); printf("\n");
                printf("    Expected: "); print_float(expected); printf("\n");
                printf("    Got: "); print_float(result); printf("\n");
                printf("    Rel Error: "); print_float(rel_error); printf("\n");
            }
        }
        else
        {
            if (!float_equals_abs(result, expected, 1e-5f))
            {
                printf("  ");
                fail("powf special value mismatch");
                printf("    Base: "); print_float(base); printf("\n");
                printf("    Exponent: "); print_float(exponent); printf("\n");
                printf("    Expected: "); print_float(expected); printf("\n");
                printf("    Got: "); print_float(result); printf("\n");
            }
            else
            {
                passed++;
            }
        }
    }

    calculate_error_metrics("powf", powf_count, passed, max_error,
                            valid_cases > 0 ? total_error / valid_cases : 0.0f);

    if (passed == powf_count)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Test suite for sinf_new function
 * 
 * @details
 * Tests the custom sine function implementation. Uses absolute error
 * with adaptive thresholds - more lenient for large input angles where
 * precision loss is expected due to argument reduction.
 */
void test_sinf()
{
    printf(COLOR_BOLD "Testing sinf_new - Comprehensive Test Suite" COLOR_RESET "\n");
    int passed = 0;
    float max_error = 0.0f;
    float total_error = 0.0f;
    int valid_cases = 0;

    for (int i = 0; i < sinf_count; i++)
    {
        float input = sinf_inputs[i];
        float expected = sinf_expected[i];
        float result = sinf_new(input);

        // For sinf, we care about absolute error
        float error = fabsf_new(result - expected);
        if (error > max_error)
            max_error = error;
        total_error += error;
        valid_cases++;

        // Use more lenient threshold for large angles
        float threshold = (fabsf_new(input) > 100.0f) ? 1e-3f : 1e-5f;

        if (error < threshold)
        {
            passed++;
        }
        else
        {
            printf("  ");
            fail("sinf case failed - see details above");
            printf("    Input: "); print_float(input); printf("\n");
            printf("    Expected: "); print_float(expected); printf("\n");
            printf("    Got: "); print_float(result); printf("\n");
            printf("    Error: "); print_float(error); printf("\n");
        }
    }

    calculate_error_metrics("sinf", sinf_count, passed, max_error,
                            valid_cases > 0 ? total_error / valid_cases : 0.0f);

    if (passed == sinf_count)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Test suite for cosf_new function
 * 
 * @details
 * Tests the custom cosine function implementation. Similar to sinf_new
 * testing but with cosine-specific test cases. Uses adaptive error
 * thresholds based on input magnitude.
 */
void test_cosf()
{
    printf(COLOR_BOLD "Testing cosf_new - Comprehensive Test Suite" COLOR_RESET "\n");
    int passed = 0;
    float max_error = 0.0f;
    float total_error = 0.0f;
    int valid_cases = 0;

    for (int i = 0; i < cosf_count; i++)
    {
        float input = cosf_inputs[i];
        float expected = cosf_expected[i];
        float result = cosf_new(input);

        float error = fabsf_new(result - expected);
        if (error > max_error)
            max_error = error;
        total_error += error;
        valid_cases++;

        // Use more lenient threshold for large angles
        float threshold = (fabsf_new(input) > 100.0f) ? 1e-3f : 1e-5f;

        if (error < threshold)
        {
            passed++;
        }
        else
        {
            printf("  ");
            fail("cosf case failed - see details above");
            printf("    Input: "); print_float(input); printf("\n");
            printf("    Expected: "); print_float(expected); printf("\n");
            printf("    Got: "); print_float(result); printf("\n");
            printf("    Error: "); print_float(error); printf("\n");
        }
    }

    calculate_error_metrics("cosf", cosf_count, passed, max_error,
                            valid_cases > 0 ? total_error / valid_cases : 0.0f);

    if (passed == cosf_count)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Test suite for fabsf_new function
 * 
 * @details
 * Tests the custom absolute value function implementation. Uses simple
 * test cases since fabsf is a straightforward function. Tests positive,
 * negative, zero, and edge cases including negative zero.
 */
void test_fabsf()
{
    printf(COLOR_BOLD "Testing fabsf_new" COLOR_RESET "\n");
    int passed = 0;

    float test_cases[][2] = {
        {5.0f, 5.0f},
        {-5.0f, 5.0f},
        {0.0f, 0.0f},
        {-3.14159f, 3.14159f},
        {1e-6f, 1e-6f},
        {-1e-6f, 1e-6f},
        {-0.0f, 0.0f} // Test negative zero
    };

    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_cases; i++)
    {
        float input = test_cases[i][0];
        float expected = test_cases[i][1];
        float result = fabsf_new(input);

        if (result == expected || (result == 0.0f && expected == 0.0f))
        {
            passed++;
        }
        else
        {
            printf("  ");
            fail("fabsf case failed - see details above");
            printf("    Input: "); print_float(input); printf("\n");
            printf("    Expected: "); print_float(expected); printf("\n");
            printf("    Got: "); print_float(result); printf("\n");
        }
    }

    if (passed == num_cases) {
        pass("All fabsf tests passed");
    } else {
        printf("  ");
        fail("Some fabsf tests failed");
        printf("    Passed: %d/%d\n", passed, num_cases);
    }

    if (passed == num_cases)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
    }
}

/**
 * @brief Main test runner for all mathematical functions
 * 
 * @details
 * Orchestrates the execution of all individual function test suites
 * in sequence and provides a comprehensive summary at the end.
 */
void test_math_functions()
{
    printf(COLOR_BOLD COLOR_CYAN "\n=== Math Functions Tests ===" COLOR_RESET "\n");
    test_sqrtf();
    test_expf();
    test_powf();
    test_sinf();
    test_cosf();
    test_fabsf();
}

/**
 * @brief Program entry point for the math test suite
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 * @return int Exit status (0 for success)
 * 
 * @details
 * Initializes the test environment, runs all mathematical function tests,
 * and displays a comprehensive summary of results with color-coded output.
 */
int main(int argc, char *argv[])
{
    printf(COLOR_BOLD COLOR_MAGENTA "\n=== Starting Math Test Suite ===" COLOR_RESET "\n\n");

    // Run math tests
    test_math_functions();

    printf(COLOR_BOLD COLOR_CYAN "\n=== Math Test Summary ===" COLOR_RESET "\n");
    printf("Total Math Tests: %d\n", tests_passed + tests_failed);
    
    if (tests_passed > 0) {
        printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", tests_passed);
    } else {
        printf("Passed: %d\n", tests_passed);
    }
    
    if (tests_failed > 0) {
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", tests_failed);
    } else {
        printf("Failed: %d\n", tests_failed);
    }

    if (tests_failed == 0)
    {
        printf(COLOR_BOLD COLOR_GREEN "\n ALL MATH TESTS PASSED! ✓" COLOR_RESET "\n");
    }
    else
    {
        printf(COLOR_BOLD COLOR_RED "\n SOME MATH TESTS FAILED! ✗" COLOR_RESET "\n");
    }

    printf("\nExiting test suite...\n");
    exit(0);
}