#ifndef TEST_MILESTONE2_H
#define TEST_MILESTONE2_H

// Test counters
extern int tests_passed;
extern int tests_failed;

// Test function declarations
void test_math_functions(void);

// Math test functions
void test_sqrtf(void);
void test_expf(void);
void test_powf(void);
void test_sinf(void);
void test_cosf(void);
void test_tanhf(void);     
void test_fabsf(void);
void test_performance(void); 

// Helper functions
void print_float(float f);
int float_equals_abs(float a, float b, float epsilon);  
int float_equals_rel(float a, float b, float epsilon);  
void calculate_error_metrics(const char* func_name, int total_cases, int passed_cases, 
                            float max_error, float avg_error);

// NaN/Infinity checking
int is_nan(float x);
int is_inf(float x);
int is_finite(float x);


#endif