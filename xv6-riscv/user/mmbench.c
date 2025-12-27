/**
 * @file matmul_benchmark.c
 * @brief Comprehensive matrix multiplication benchmark for xv6
 * 
 * Tests various matmul implementations across different matrix sizes
 * observed in actual LLM inference workloads.
 * 
 * @details
 * This benchmark suite evaluates four matrix multiplication implementations:
 * 1. Baseline - Simple nested loops without optimizations
 * 2. Original Threading - Row-level parallelization with naive scheduling
 * 3. Unrolled 8x + Threading - 8-way loop unrolling with thread pool
 * 4. Tiled/Blocked - Cache-aware blocking algorithm for large matrices
 * 
 * The benchmark measures performance across realistic LLM matrix sizes
 * including QKV projections, FFN layers, and embedding matrices.
 * Includes correctness verification and detailed performance analysis.
 * 
 * @author Hadiya Muneeb
 * @date December 2025
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/xstdlib.h"
#include "user/xmath.h"

#define NULL ((void*)0)

/**
 * @brief Get current time in milliseconds
 * 
 * @return long Time in milliseconds since arbitrary epoch
 * 
 * @details
 * Uses RISC-V rdtime() instruction which returns hardware timebase ticks.
 * xv6 on QEMU uses a 10 MHz timer (10,000,000 ticks per second).
 * Therefore, dividing by 10,000 converts raw ticks to milliseconds.
 */
long time_in_ms(void) {
  return (long long)(rdtime() / 10000);
}

/**
 * @brief Matrix multiplication configuration parameters
 * 
 * Represents specific matrix dimensions found in LLM workloads.
 * Includes both square and rectangular matrices that appear in
 * attention, feed-forward, and embedding layers.
 */
typedef struct {
    int d;  // output dimension
    int n;  // input dimension
    const char* name;
} MatmulSize;

static MatmulSize TEST_SIZES[] = {
    {288, 288, "Small Square (QKV projections)"},
    {768, 288, "Wide Rectangle (FFN w1/w3)"},
    {288, 768, "Tall Rectangle (FFN w2)"},
    {32000, 288, "Huge (Token embedding)"},
    {512, 512, "Medium Square"},
    {1024, 288, "Very Wide"},
    {128, 128, "Tiny Square"},
};

#define NUM_TEST_SIZES (sizeof(TEST_SIZES) / sizeof(TEST_SIZES[0]))

/**
 * @brief Work unit for parallel matrix multiplication
 * 
 * Contains all data needed by a worker thread to compute
 * a subset of output rows.
 */
typedef struct {
    float* xout;
    float* x;
    float* w;
    int n;
    int d;
    int start_row;
    int end_row;
    volatile int* work_ready;
    volatile int* work_done;
} MatmulWork;

typedef struct {
    int thread_id;
    MatmulWork* work;
    volatile int* should_exit;
    void (*worker_fn)(MatmulWork*);  // Function pointer for different implementations
} ThreadPoolWorker;

static int g_num_threads = 3;
static int thread_pool_initialized = 0;
static int* thread_ids = NULL;
static MatmulWork* work_items = NULL;
static volatile int* work_ready = NULL;
static volatile int* work_done = NULL;
static ThreadPoolWorker** worker_ptrs = NULL;
static volatile int thread_pool_exit = 0;

/**
 * @brief Generic worker thread entry point
 * 
 * @param arg Pointer to ThreadPoolWorker structure
 * 
 * @details
 * Worker thread that waits for work assignments and executes
 * the assigned worker function. Runs in infinite loop until
 * exit flag is set. Uses yield() to avoid busy-waiting.
 */
void generic_worker_thread(void* arg) {
    ThreadPoolWorker* worker = (ThreadPoolWorker*)arg;
    MatmulWork* work = worker->work;
    
    while (1) {
        while (!(*work->work_ready) && !(*worker->should_exit)) {
            yield();
        }
        
        if (*worker->should_exit) {
            thread_exit();
        }
        
        // Call the specific worker function
        worker->worker_fn(work);
        
        *work->work_done = 1;
        *work->work_ready = 0;
    }
}

/**
 * @brief Initialize thread pool with specified worker function
 * 
 * @param worker_fn Function pointer to worker implementation
 * 
 * @details
 * Creates worker threads and allocates all necessary data structures.
 * Each thread is configured to use the provided worker function.
 * Threads are created but idle until work is assigned.
 */
void init_thread_pool(void (*worker_fn)(MatmulWork*)) {
    if (thread_pool_initialized) return;
    thread_pool_exit = 0;

    int n = g_num_threads;
    thread_ids = malloc(sizeof(int) * n);
    work_items = malloc(sizeof(MatmulWork) * n);
    work_ready = malloc(sizeof(volatile int) * n);
    work_done = malloc(sizeof(volatile int) * n);
    worker_ptrs = malloc(sizeof(ThreadPoolWorker*) * n);

    for (int t = 0; t < n; t++) {
        work_ready[t] = 0;
        work_done[t] = 0;

        ThreadPoolWorker* worker = malloc(sizeof(ThreadPoolWorker));
        worker->thread_id = t;
        worker->work = &work_items[t];
        worker->should_exit = &thread_pool_exit;
        worker->worker_fn = worker_fn;
        worker_ptrs[t] = worker;

        work_items[t].work_ready = &work_ready[t];
        work_items[t].work_done = &work_done[t];

        thread_ids[t] = thread_create(generic_worker_thread, worker);
    }

    thread_pool_initialized = 1;
}

/**
 * @brief Shutdown thread pool and cleanup resources
 * 
 * @details
 * Signals all worker threads to exit, waits for completion,
 * and frees all allocated memory. Must be called before
 * program exit or when switching implementations.
 */
void shutdown_thread_pool(void) {
    if (!thread_pool_initialized) return;

    thread_pool_exit = 1;

    for (int t = 0; t < g_num_threads; t++) {
        if (thread_ids && thread_ids[t] > 0) thread_join(thread_ids[t]);
    }

    for (int t = 0; t < g_num_threads; t++) {
        if (worker_ptrs && worker_ptrs[t]) free(worker_ptrs[t]);
    }
    free(worker_ptrs);
    free(thread_ids);
    free(work_items);
    free((void*)work_ready);
    free((void*)work_done);

    worker_ptrs = NULL;
    thread_ids = NULL;
    work_items = NULL;
    work_ready = NULL;
    work_done = NULL;
    thread_pool_initialized = 0;
}

/**
 * @brief Baseline matrix multiplication (no optimizations)
 * 
 * @param xout Output vector (d,)
 * @param x    Input vector (n,)
 * @param w    Weight matrix (d, n)
 * @param n    Input dimension
 * @param d    Output dimension
 * 
 * @details
 * Simple reference implementation with double nested loops:
 * - Outer loop: output dimension (d)
 * - Inner loop: input dimension (n)
 * 
 * Serves as correctness reference and performance baseline.
 */
void matmul_baseline(float* xout, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

/**
 * @brief Worker function for original threading implementation
 * 
 * @param work Pointer to work assignment
 * 
 * @details
 * Computes a subset of output rows using simple inner product.
 * Used by the original threading implementation for row-level
 * parallelization.
 */
void matmul_worker_original(MatmulWork* work) {
    for (int i = work->start_row; i < work->end_row; i++) {
        float val = 0.0f;
        float* w_row = &work->w[i * work->n];
        for (int j = 0; j < work->n; j++) {
            val += w_row[j] * work->x[j];
        }
        work->xout[i] = val;
    }
}

/**
 * @brief Original threading implementation of matmul
 * 
 * @param xout Output vector (d,)
 * @param x    Input vector (n,)
 * @param w    Weight matrix (d, n)
 * @param n    Input dimension
 * @param d    Output dimension
 * 
 * @details
 * Parallelizes matrix multiplication by dividing output rows
 * among available threads. Falls back to baseline for small
 * matrices where threading overhead exceeds benefits.
 */
void matmul_threaded_original(float* xout, float* x, float* w, int n, int d) {
    if (d < 128) {
        matmul_baseline(xout, x, w, n, d);
        return;
    }

    int chunk_size = (d + g_num_threads - 1) / g_num_threads;
    int num_active_threads = (d + chunk_size - 1) / chunk_size;
    if (num_active_threads > g_num_threads) {
        num_active_threads = g_num_threads;
    }

    for (int t = 0; t < num_active_threads; t++) {
        int start_row = t * chunk_size;
        int end_row = start_row + chunk_size;
        if (end_row > d) end_row = d;
        if (start_row >= d) break;

        work_items[t].xout = xout;
        work_items[t].x = x;
        work_items[t].w = w;
        work_items[t].n = n;
        work_items[t].d = d;
        work_items[t].start_row = start_row;
        work_items[t].end_row = end_row;
        work_done[t] = 0;
        work_ready[t] = 1;
    }

    for (int t = 0; t < num_active_threads; t++) {
        while (!work_done[t]) { }
    }
}

/**
 * @brief Optimized dot product with 8-way loop unrolling
 * 
 * @param w_row Weight matrix row
 * @param x     Input vector
 * @param n     Vector length
 * @return float Dot product result
 * 
 * @details
 * Uses manual 8-way loop unrolling to reduce loop overhead
 * and improve instruction-level parallelism. Handles tail
 * elements for non-multiples of 8.
 */
static inline float dot_product_unrolled(float* w_row, float* x, int n) {
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    float sum4 = 0.0f, sum5 = 0.0f, sum6 = 0.0f, sum7 = 0.0f;
    
    int j = 0;
    int n8 = n & ~7;
    
    for (; j < n8; j += 8) {
        sum0 += w_row[j] * x[j];
        sum1 += w_row[j + 1] * x[j + 1];
        sum2 += w_row[j + 2] * x[j + 2];
        sum3 += w_row[j + 3] * x[j + 3];
        sum4 += w_row[j + 4] * x[j + 4];
        sum5 += w_row[j + 5] * x[j + 5];
        sum6 += w_row[j + 6] * x[j + 6];
        sum7 += w_row[j + 7] * x[j + 7];
    }
    
    float sum_tail = 0.0f;
    for (; j < n; j++) {
        sum_tail += w_row[j] * x[j];
    }
    
    return (sum0 + sum1 + sum2 + sum3) + (sum4 + sum5 + sum6 + sum7) + sum_tail;
}

/**
 * @brief Worker function for unrolled implementation
 * 
 * @param work Pointer to work assignment
 * 
 * @details
 * Uses unrolled dot product for each assigned output row.
 * Provides better cache locality and instruction efficiency
 * compared to the original worker.
 */
void matmul_worker_unrolled(MatmulWork* work) {
    for (int i = work->start_row; i < work->end_row; i++) {
        float* w_row = &work->w[i * work->n];
        work->xout[i] = dot_product_unrolled(w_row, work->x, work->n);
    }
}

/**
 * @brief Unrolled 8x + threading matmul implementation
 * 
 * @param xout Output vector (d,)
 * @param x    Input vector (n,)
 * @param w    Weight matrix (d, n)
 * @param n    Input dimension
 * @param d    Output dimension
 * 
 * @details
 * Combines 8-way loop unrolling with thread-level parallelism.
 * Uses cache-aware scheduling with aligned work assignments.
 * Falls back to sequential unrolled version for small matrices.
 */
void matmul_unrolled(float* xout, float* x, float* w, int n, int d) {
    if (d < 256 || n < 256) {
        for (int i = 0; i < d; i++) {
            float* w_row = &w[i * n];
            xout[i] = dot_product_unrolled(w_row, x, n);
        }
        return;
    }

    const int CACHE_LINE_FLOATS = 16;
    int min_rows_per_thread = 64;
    int max_threads = d / min_rows_per_thread;
    if (max_threads > g_num_threads) max_threads = g_num_threads;
    if (max_threads < 1) max_threads = 1;

    int rows_per_thread = (d + max_threads - 1) / max_threads;
    rows_per_thread = ((rows_per_thread + CACHE_LINE_FLOATS - 1) / CACHE_LINE_FLOATS) * CACHE_LINE_FLOATS;

    int num_active_threads = (d + rows_per_thread - 1) / rows_per_thread;
    if (num_active_threads > g_num_threads) {
        num_active_threads = g_num_threads;
    }

    for (int t = 0; t < num_active_threads; t++) {
        int start_row = t * rows_per_thread;
        int end_row = start_row + rows_per_thread;
        if (end_row > d) end_row = d;
        if (start_row >= d) break;

        work_items[t].xout = xout;
        work_items[t].x = x;
        work_items[t].w = w;
        work_items[t].n = n;
        work_items[t].d = d;
        work_items[t].start_row = start_row;
        work_items[t].end_row = end_row;
        work_done[t] = 0;
        work_ready[t] = 1;
    }

    for (int t = 0; t < num_active_threads; t++) {
        while (!work_done[t]) { }
    }
}

/**
 * @brief Cache-blocked tiled matmul implementation
 * 
 * @param xout Output vector (d,)
 * @param x    Input vector (n,)
 * @param w    Weight matrix (d, n)
 * @param n    Input dimension
 * @param d    Output dimension
 * 
 * @details
 * Implements cache-aware tiling algorithm:
 * - Processes matrices in TILE_SIZE x TILE_SIZE blocks
 * - Improves cache locality for large matrices
 * - Uses unrolled dot product within tiles
 * - Sequential implementation (no threading)
 */
void matmul_tiled(float* xout, float* x, float* w, int n, int d) {
    if (d < 256 || n < 256) {
        for (int i = 0; i < d; i++) {
            float* w_row = &w[i * n];
            xout[i] = dot_product_unrolled(w_row, x, n);
        }
        return;
    }

    const int TILE_SIZE = 64;

    for (int i = 0; i < d; i++) {
        xout[i] = 0.0f;
    }

    for (int ii = 0; ii < d; ii += TILE_SIZE) {
        int i_end = ii + TILE_SIZE;
        if (i_end > d) i_end = d;

        for (int jj = 0; jj < n; jj += TILE_SIZE) {
            int j_end = jj + TILE_SIZE;
            if (j_end > n) j_end = n;

            for (int i = ii; i < i_end; i++) {
                float* w_row = &w[i * n + jj];
                float* x_tile = &x[jj];
                int tile_n = j_end - jj;
                xout[i] += dot_product_unrolled(w_row, x_tile, tile_n);
            }
        }
    }
}

/**
 * @brief Verify correctness between two matmul results
 * 
 * @param result1 First result vector
 * @param result2 Second result vector  
 * @param d       Vector dimension
 * @return float Maximum absolute difference across elements
 * 
 * @details
 * Compares output vectors element-wise to ensure implementations
 * produce identical results (within floating-point tolerance).
 */
float verify_matmul(float* result1, float* result2, int d) {
    float max_diff = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = result1[i] - result2[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

typedef void (*MatmulFunc)(float*, float*, float*, int, int);

typedef struct {
    const char* name;
    MatmulFunc func;
    void (*worker_fn)(MatmulWork*);  // NULL if no threading needed
} MatmulImpl;

/**
 * @brief Benchmark a single matmul implementation
 * 
 * @param impl       Pointer to implementation descriptor
 * @param size       Pointer to matrix size configuration
 * @param iterations Number of benchmark iterations
 * 
 * @details
 * Measures execution time and computes GFLOPS for the given
 * implementation and matrix size. Allocates test data,
 * performs warmup runs, times multiple iterations, and
 * prints detailed performance metrics.
 */
void benchmark_implementation(MatmulImpl* impl, MatmulSize* size, int iterations) {
    int d = size->d;
    int n = size->n;

    // Allocate matrices
    float* xout = malloc(d * sizeof(float));
    float* x = malloc(n * sizeof(float));
    float* w = malloc(d * n * sizeof(float));

    if (!xout || !x || !w) {
        printf("  [%s] ERROR: malloc failed\n", impl->name);
        free(xout);
        free(x);
        free(w);
        return;
    }

    // Initialize with test data
    for (int i = 0; i < n; i++) {
        x[i] = (float)(i % 100) / 100.0f;
    }
    for (int i = 0; i < d * n; i++) {
        w[i] = (float)((i * 7) % 100) / 100.0f;
    }

    // Initialize thread pool if needed
    if (impl->worker_fn && !thread_pool_initialized) {
        init_thread_pool(impl->worker_fn);
    }

    // Warmup
    impl->func(xout, x, w, n, d);

    // Benchmark
    long start = time_in_ms();
    for (int iter = 0; iter < iterations; iter++) {
        impl->func(xout, x, w, n, d);
    }
    long end = time_in_ms();

    long total_time = end - start;
    float avg_time = (float)total_time / iterations;
    float ops = 2.0f * d * n;  // multiply-add per output element
    float gflops = (ops * iterations) / (total_time * 1000000.0f);

    printf("  [%25s] %ld ms total, %.2f ms/iter, %.3f GFLOPS\n",
           impl->name, total_time, avg_time, gflops);

    free(xout);
    free(x);
    free(w);
}

/**
 * @brief Run complete benchmark suite across all matrix sizes
 * 
 * @details
 * Executes all four implementations across seven realistic LLM
 * matrix configurations. Adjusts iteration counts based on
 * operation count to keep benchmark times reasonable.
 * 
 * Prints formatted results with configuration details and
 * performance metrics for each implementation.
 */
void run_benchmark_suite(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("                    MATMUL BENCHMARK SUITE FOR XV6\n");
    printf("================================================================================\n");
    printf("Thread Count: %d\n", g_num_threads);
    printf("Test Iterations: varies by size\n");
    printf("\n");

    MatmulImpl implementations[] = {
        {"Baseline (No Threading)", matmul_baseline, NULL},
        {"Original Threading", matmul_threaded_original, matmul_worker_original},
        {"Unrolled 8x + Threading", matmul_unrolled, matmul_worker_unrolled},
        {"Tiled/Blocked", matmul_tiled, NULL},
    };
    int num_impls = sizeof(implementations) / sizeof(implementations[0]);

    for (int s = 0; s < NUM_TEST_SIZES; s++) {
        MatmulSize* size = &TEST_SIZES[s];
        
        // Determine iteration count based on size
        int iterations;
        long total_ops = 2L * size->d * size->n;
        if (total_ops > 50000000) {
            iterations = 5;  // Very large matrices
        } else if (total_ops > 1000000) {
            iterations = 50;  // Medium matrices
        } else {
            iterations = 200;  // Small matrices
        }

        printf("--------------------------------------------------------------------------------\n");
        printf("Test: %s (%d x %d)\n", size->name, size->d, size->n);
        printf("Iterations: %d\n", iterations);
        printf("--------------------------------------------------------------------------------\n");

        for (int i = 0; i < num_impls; i++) {
            // Shutdown and reinit thread pool for each implementation
            if (thread_pool_initialized) {
                shutdown_thread_pool();
            }

            benchmark_implementation(&implementations[i], size, iterations);
        }

        printf("\n");
    }

    // Final cleanup
    if (thread_pool_initialized) {
        shutdown_thread_pool();
    }
}

/**
 * @brief Verify numerical correctness of all implementations
 * 
 * @details
 * Compares optimized and parallel implementations against
 * baseline to ensure they produce identical results.
 * 
 * Uses a 288x288 matrix (common QKV projection size) for testing.
 * Reports maximum differences and PASS/FAIL status for each impl.
 */
void verify_correctness(void) {
    printf("================================================================================\n");
    printf("                        CORRECTNESS VERIFICATION\n");
    printf("================================================================================\n");

    int test_d = 288, test_n = 288;
    
    float* xout_baseline = malloc(test_d * sizeof(float));
    float* xout_test = malloc(test_d * sizeof(float));
    float* x = malloc(test_n * sizeof(float));
    float* w = malloc(test_d * test_n * sizeof(float));

    for (int i = 0; i < test_n; i++) x[i] = (float)i / test_n;
    for (int i = 0; i < test_d * test_n; i++) w[i] = (float)i / (test_d * test_n);

    matmul_baseline(xout_baseline, x, w, test_n, test_d);

    // Test original threading
    init_thread_pool(matmul_worker_original);
    matmul_threaded_original(xout_test, x, w, test_n, test_d);
    float diff1 = verify_matmul(xout_baseline, xout_test, test_d);
    printf("Original Threading: max diff = %.6f %s\n", diff1, 
           diff1 < 0.0001f ? "[PASS]" : "[FAIL]");
    shutdown_thread_pool();

    // Test unrolled
    init_thread_pool(matmul_worker_unrolled);
    matmul_unrolled(xout_test, x, w, test_n, test_d);
    float diff2 = verify_matmul(xout_baseline, xout_test, test_d);
    printf("Unrolled 8x:        max diff = %.6f %s\n", diff2,
           diff2 < 0.0001f ? "[PASS]" : "[FAIL]");
    shutdown_thread_pool();

    // Test tiled
    matmul_tiled(xout_test, x, w, test_n, test_d);
    float diff3 = verify_matmul(xout_baseline, xout_test, test_d);
    printf("Tiled/Blocked:      max diff = %.6f %s\n", diff3,
           diff3 < 0.0001f ? "[PASS]" : "[FAIL]");

    printf("\n");

    free(xout_baseline);
    free(xout_test);
    free(x);
    free(w);
}

/**
 * @brief Program entry point
 * 
 * @param argc Argument count
 * @param argv Argument vector (optional thread count)
 * @return int Exit status (0 on success)
 * 
 * @details
 * Parses optional thread count argument, verifies correctness,
 * runs full benchmark suite, and prints summary.
 * 
 * Usage: matmul_benchmark [thread_count]
 *   thread_count: Number of worker threads (1-8, default: 3)
 */
int main(int argc, char* argv[]) {
    if (argc > 1) {
        g_num_threads = atoi(argv[1]);
        if (g_num_threads < 1) g_num_threads = 1;
        if (g_num_threads > 8) g_num_threads = 8;
    }

    printf("\nMatmul Benchmark Starting...\n");
    printf("Using %d threads\n\n", g_num_threads);

    // First verify correctness
    verify_correctness();

    // Then run full benchmark suite
    run_benchmark_suite();

    printf("================================================================================\n");
    printf("                          BENCHMARK COMPLETE\n");
    printf("================================================================================\n");

    exit(0);
}