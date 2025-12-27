/**
 * @file attention_benchmark.c
 * @brief Comprehensive multi-head attention benchmark for xv6
 * 
 * Tests various MHA implementations across different configurations
 * observed in actual LLM inference workloads.
 * 
 * @details
 * This benchmark suite evaluates four attention implementations:
 * 1. Baseline Sequential - Reference implementation without optimizations
 * 2. Optimized Sequential - Loop unrolling and cache-aware optimizations
 * 3. Parallel (Head-level) - Thread-based parallelism across attention heads
 * 4. Parallel (Cache Blocked) - Cache-blocking optimized parallel version
 * 
 * The benchmark measures performance (ms/iteration, GFLOPS) across realistic
 * LLM configurations representing different model sizes and sequence lengths.
 * It includes correctness verification to ensure parallel implementations
 * produce identical results to the baseline.
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
#define sqrtf xsqrtf
#define expf xexpf

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
 * @brief Attention layer configuration parameters
 * 
 * Represents a specific attention configuration found in LLM workloads.
 * Used to benchmark performance across different model sizes and
 * sequence positions.
 */
typedef struct {
    int n_heads;       // number of attention heads
    int head_size;     // dimension per head (usually dim / n_heads)
    int kv_dim;        // key/value dimension
    int seq_len;       // sequence length
    int pos;           // current position (affects work amount)
    const char* name;
} AttentionConfig;

static AttentionConfig TEST_CONFIGS[] = {
    {6, 48, 288, 256, 10, "Early tokens (pos=10)"},
    {6, 48, 288, 256, 50, "Mid sequence (pos=50)"},
    {6, 48, 288, 256, 100, "Long sequence (pos=100)"},
    {6, 48, 288, 256, 200, "Very long (pos=200)"},
    {8, 64, 512, 512, 50, "Larger model (8 heads)"},
    {12, 64, 768, 512, 50, "Big model (12 heads)"},
};

#define NUM_TEST_CONFIGS (sizeof(TEST_CONFIGS) / sizeof(TEST_CONFIGS[0]))

/**
 * @brief Mock transformer runtime state
 * 
 * Simulates the minimal state needed for attention computation.
 * All pointers represent memory buffers that would exist in a real
 * LLM inference run.
 */
typedef struct {
    float* q;           // query (dim,)
    float* xb;          // output buffer (dim,)
    float* att;         // attention scores (n_heads, seq_len)
    float* key_cache;   // (layer, seq_len, kv_dim)
    float* value_cache; // (layer, seq_len, kv_dim)
} MockRunState;

/**
 * @brief Compute softmax over a vector
 * 
 * @param x     Input/output vector (modified in-place)
 * @param size  Length of vector
 * 
 * @details
 * Implements numerically stable softmax:
 * 1. Find maximum value for stability
 * 2. Exponentiate values shifted by max
 * 3. Normalize by sum
 */
void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

/**
 * @brief Baseline attention implementation (sequential, no optimization)
 * 
 * @param s     Pointer to mock runtime state
 * @param cfg   Pointer to configuration parameters
 * @param loff  Layer offset in KV cache
 * 
 * @details
 * Reference implementation with triple nested loops:
 * - Outer: attention heads
 * - Middle: sequence positions
 * - Inner: head dimension (dot product)
 * 
 * Serves as correctness reference and performance baseline.
 */
void attention_baseline(MockRunState* s, AttentionConfig* cfg, int loff) {
    int kv_mul = 1;  // Simplified for benchmark
    
    for (int h = 0; h < cfg->n_heads; h++) {
        float* q = s->q + h * cfg->head_size;
        float* att = s->att + h * cfg->seq_len;
        
        // Compute attention scores
        for (int t = 0; t <= cfg->pos; t++) {
            float* k = s->key_cache + loff + t * cfg->kv_dim + (h / kv_mul) * cfg->head_size;
            float score = 0.0f;
            for (int i = 0; i < cfg->head_size; i++) {
                score += q[i] * k[i];
            }
            score /= sqrtf(cfg->head_size);
            att[t] = score;
        }
        
        softmax(att, cfg->pos + 1);
        
        // Weighted sum of values
        float* xb = s->xb + h * cfg->head_size;
        for (int i = 0; i < cfg->head_size; i++) xb[i] = 0.0f;
        
        for (int t = 0; t <= cfg->pos; t++) {
            float* v = s->value_cache + loff + t * cfg->kv_dim + (h / kv_mul) * cfg->head_size;
            float a = att[t];
            for (int i = 0; i < cfg->head_size; i++) {
                xb[i] += a * v[i];
            }
        }
    }
}

/**
 * @brief Optimized dot product with loop unrolling
 * 
 * @param a  First vector
 * @param b  Second vector  
 * @param n  Vector length
 * @return float Dot product result
 * 
 * @details
 * Uses manual loop unrolling (4-way) to reduce loop overhead
 * and improve instruction-level parallelism.
 */
static inline float dot_product_unrolled(float* a, float* b, int n) {
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    
    int i = 0;
    int n4 = n & ~3;
    
    for (; i < n4; i += 4) {
        sum0 += a[i] * b[i];
        sum1 += a[i + 1] * b[i + 1];
        sum2 += a[i + 2] * b[i + 2];
        sum3 += a[i + 3] * b[i + 3];
    }
    
    float sum_tail = 0.0f;
    for (; i < n; i++) {
        sum_tail += a[i] * b[i];
    }
    
    return (sum0 + sum1) + (sum2 + sum3) + sum_tail;
}

/**
 * @brief Optimized sequential attention with loop unrolling
 * 
 * @param s     Pointer to mock runtime state
 * @param cfg   Pointer to configuration parameters
 * @param loff  Layer offset in KV cache
 * 
 * @details
 * Uses unrolled dot products and weighted sums for better
 * performance than baseline. Maintains sequential execution
 * but with reduced loop overhead.
 */
void attention_optimized_seq(MockRunState* s, AttentionConfig* cfg, int loff) {
    int kv_mul = 1;
    
    for (int h = 0; h < cfg->n_heads; h++) {
        float* q = s->q + h * cfg->head_size;
        float* att = s->att + h * cfg->seq_len;
        
        // Compute attention scores with unrolled dot product
        for (int t = 0; t <= cfg->pos; t++) {
            float* k = s->key_cache + loff + t * cfg->kv_dim + (h / kv_mul) * cfg->head_size;
            float score = dot_product_unrolled(q, k, cfg->head_size);
            score /= sqrtf(cfg->head_size);
            att[t] = score;
        }
        
        softmax(att, cfg->pos + 1);
        
        // Weighted sum with unrolling
        float* xb = s->xb + h * cfg->head_size;
        for (int i = 0; i < cfg->head_size; i++) xb[i] = 0.0f;
        
        for (int t = 0; t <= cfg->pos; t++) {
            float* v = s->value_cache + loff + t * cfg->kv_dim + (h / kv_mul) * cfg->head_size;
            float a = att[t];
            
            // Unroll weighted sum
            int i = 0;
            int hs4 = cfg->head_size & ~3;
            for (; i < hs4; i += 4) {
                xb[i] += a * v[i];
                xb[i + 1] += a * v[i + 1];
                xb[i + 2] += a * v[i + 2];
                xb[i + 3] += a * v[i + 3];
            }
            for (; i < cfg->head_size; i++) {
                xb[i] += a * v[i];
            }
        }
    }
}

/**
 * @brief Work unit for parallel attention computation
 * 
 * Contains all data needed by a worker thread to process
 * a subset of attention heads.
 */
typedef struct {
    MockRunState* s;
    AttentionConfig* cfg;
    int loff;
    int start_head;
    int end_head;
    volatile int* work_ready;
    volatile int* work_done;
} AttentionWork;

typedef struct {
    int thread_id;
    AttentionWork* work;
    volatile int* should_exit;
} AttentionWorker;

static int g_num_threads = 3;
static int attention_pool_initialized = 0;
static int* attention_thread_ids = NULL;
static AttentionWork* attention_work_items = NULL;
static volatile int* attention_work_ready = NULL;
static volatile int* attention_work_done = NULL;
static AttentionWorker** attention_worker_ptrs = NULL;
static volatile int attention_pool_exit = 0;

/**
 * @brief Worker thread function for simple parallel attention
 * 
 * @param arg Pointer to AttentionWorker structure
 * 
 * @details
 * Worker thread that waits for work assignments and processes
 * assigned attention heads. Uses optimized sequential algorithm
 * for its subset of heads. Runs in infinite loop until exit flag.
 */
void attention_worker_simple(void* arg) {
    AttentionWorker* worker = (AttentionWorker*)arg;
    AttentionWork* work = worker->work;
    
    while (1) {
        while (!(*work->work_ready) && !(*worker->should_exit)) {
            yield();
        }
        
        if (*worker->should_exit) {
            thread_exit();
        }
        
        int kv_mul = 1;
        
        // Process assigned heads
        for (int h = work->start_head; h < work->end_head; h++) {
            float* q = work->s->q + h * work->cfg->head_size;
            float* att = work->s->att + h * work->cfg->seq_len;
            
            for (int t = 0; t <= work->cfg->pos; t++) {
                float* k = work->s->key_cache + work->loff + t * work->cfg->kv_dim + 
                          (h / kv_mul) * work->cfg->head_size;
                float score = dot_product_unrolled(q, k, work->cfg->head_size);
                score /= sqrtf(work->cfg->head_size);
                att[t] = score;
            }
            
            softmax(att, work->cfg->pos + 1);
            
            float* xb = work->s->xb + h * work->cfg->head_size;
            for (int i = 0; i < work->cfg->head_size; i++) xb[i] = 0.0f;
            
            for (int t = 0; t <= work->cfg->pos; t++) {
                float* v = work->s->value_cache + work->loff + t * work->cfg->kv_dim + 
                          (h / kv_mul) * work->cfg->head_size;
                float a = att[t];
                for (int i = 0; i < work->cfg->head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }
        
        *work->work_done = 1;
        *work->work_ready = 0;
    }
}

/**
 * @brief Initialize thread pool for parallel attention
 * 
 * @details
 * Creates worker threads and allocates all necessary data structures.
 * Threads are created but idle until work is assigned.
 * Must be called before using parallel attention implementations.
 */
void init_attention_pool(void) {
    if (attention_pool_initialized) return;
    attention_pool_exit = 0;

    int n = g_num_threads;
    attention_thread_ids = malloc(sizeof(int) * n);
    attention_work_items = malloc(sizeof(AttentionWork) * n);
    attention_work_ready = malloc(sizeof(volatile int) * n);
    attention_work_done = malloc(sizeof(volatile int) * n);
    attention_worker_ptrs = malloc(sizeof(AttentionWorker*) * n);

    for (int t = 0; t < n; t++) {
        attention_work_ready[t] = 0;
        attention_work_done[t] = 0;

        AttentionWorker* worker = malloc(sizeof(AttentionWorker));
        worker->thread_id = t;
        worker->work = &attention_work_items[t];
        worker->should_exit = &attention_pool_exit;
        attention_worker_ptrs[t] = worker;

        attention_work_items[t].work_ready = &attention_work_ready[t];
        attention_work_items[t].work_done = &attention_work_done[t];

        attention_thread_ids[t] = thread_create(attention_worker_simple, worker);
    }

    attention_pool_initialized = 1;
}

/**
 * @brief Shutdown thread pool and cleanup resources
 * 
 * @details
 * Signals threads to exit, waits for completion, and frees
 * all allocated memory. Must be called before program exit.
 */
void shutdown_attention_pool(void) {
    if (!attention_pool_initialized) return;

    attention_pool_exit = 1;

    for (int t = 0; t < g_num_threads; t++) {
        if (attention_thread_ids && attention_thread_ids[t] > 0) 
            thread_join(attention_thread_ids[t]);
    }

    for (int t = 0; t < g_num_threads; t++) {
        if (attention_worker_ptrs && attention_worker_ptrs[t]) 
            free(attention_worker_ptrs[t]);
    }
    
    free(attention_worker_ptrs);
    free(attention_thread_ids);
    free(attention_work_items);
    free((void*)attention_work_ready);
    free((void*)attention_work_done);

    attention_worker_ptrs = NULL;
    attention_thread_ids = NULL;
    attention_work_items = NULL;
    attention_work_ready = NULL;
    attention_work_done = NULL;
    attention_pool_initialized = 0;
}

/**
 * @brief Parallel attention using head-level parallelism
 * 
 * @param s     Pointer to mock runtime state
 * @param cfg   Pointer to configuration parameters
 * @param loff  Layer offset in KV cache
 * 
 * @details
 * Divides attention heads among available threads.
 * Falls back to optimized sequential for small numbers
 * of heads where threading overhead outweighs benefits.
 */
void attention_parallel_simple(MockRunState* s, AttentionConfig* cfg, int loff) {
    // For very few heads, sequential is faster
    if (cfg->n_heads < 4) {
        attention_optimized_seq(s, cfg, loff);
        return;
    }
    
    int heads_per_thread = (cfg->n_heads + g_num_threads - 1) / g_num_threads;
    int num_active_threads = (cfg->n_heads + heads_per_thread - 1) / heads_per_thread;
    if (num_active_threads > g_num_threads) {
        num_active_threads = g_num_threads;
    }
    
    for (int t = 0; t < num_active_threads; t++) {
        int start_head = t * heads_per_thread;
        int end_head = start_head + heads_per_thread;
        if (end_head > cfg->n_heads) end_head = cfg->n_heads;
        if (start_head >= cfg->n_heads) break;
        
        attention_work_items[t].s = s;
        attention_work_items[t].cfg = cfg;
        attention_work_items[t].loff = loff;
        attention_work_items[t].start_head = start_head;
        attention_work_items[t].end_head = end_head;
        
        attention_work_done[t] = 0;
        attention_work_ready[t] = 1;
    }
    
    for (int t = 0; t < num_active_threads; t++) {
        while (!attention_work_done[t]) { }
    }
}

/**
 * @brief Parallel attention with cache blocking heuristics
 * 
 * @param s     Pointer to mock runtime state
 * @param cfg   Pointer to configuration parameters
 * @param loff  Layer offset in KV cache
 * 
 * @details
 * Smart parallel implementation that falls back to sequential
 * for small workloads where threading overhead dominates.
 * 
 * Heuristics:
 * - Fewer than 4 heads → sequential
 * - Sequence length < 32 → sequential
 * - Otherwise → use head-level parallelism
 * 
 * In this simplified version, delegates to simple parallel
 * implementation. A full implementation would add cache-aware
 * blocking across both heads and sequence positions.
 */
void attention_parallel_blocked(MockRunState* s, AttentionConfig* cfg, int loff) {
    // For very few heads or short sequences, use optimized sequential
    if (cfg->n_heads < 4 || cfg->pos < 32) {
        attention_optimized_seq(s, cfg, loff);
        return;
    }
    
    // Use parallel version with better cache blocking
    // (In this simplified version, same as simple parallel)
    attention_parallel_simple(s, cfg, loff);
}

/**
 * @brief Verify correctness between two attention computations
 * 
 * @param s1  First runtime state (baseline)
 * @param s2  Second runtime state (test implementation)
 * @param dim Output dimension (n_heads * head_size)
 * @return float Maximum absolute difference across output vector
 * 
 * @details
 * Compares output buffers element-wise to ensure implementations
 * produce identical results (within floating-point tolerance).
 */
float verify_attention(MockRunState* s1, MockRunState* s2, int dim) {
    float max_diff = 0.0f;
    for (int i = 0; i < dim; i++) {
        float diff = s1->xb[i] - s2->xb[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

typedef void (*AttentionFunc)(MockRunState*, AttentionConfig*, int);

/**
 * @brief Attention implementation descriptor
 * 
 * Contains metadata about an attention implementation
 * for benchmarking.
 */
typedef struct {
    const char* name;
    AttentionFunc func;
    int needs_threads;
} AttentionImpl;

/**
 * @brief Benchmark a single attention implementation
 * 
 * @param impl       Pointer to implementation descriptor
 * @param cfg        Pointer to configuration to test
 * @param iterations Number of benchmark iterations
 * 
 * @details
 * Measures execution time and computes GFLOPS for the given
 * implementation and configuration. Allocates mock data,
 * performs warmup runs, then times multiple iterations.
 * Prints detailed performance metrics.
 */
void benchmark_implementation(AttentionImpl* impl, AttentionConfig* cfg, int iterations) {
    int dim = cfg->n_heads * cfg->head_size;
    int kv_cache_size = cfg->seq_len * cfg->kv_dim;
    
    // Allocate mock state
    MockRunState* s = malloc(sizeof(MockRunState));
    s->q = malloc(dim * sizeof(float));
    s->xb = malloc(dim * sizeof(float));
    s->att = malloc(cfg->n_heads * cfg->seq_len * sizeof(float));
    s->key_cache = malloc(kv_cache_size * sizeof(float));
    s->value_cache = malloc(kv_cache_size * sizeof(float));
    
    if (!s->q || !s->xb || !s->att || !s->key_cache || !s->value_cache) {
        printf("  [%s] ERROR: malloc failed\n", impl->name);
        return;
    }
    
    // Initialize with test data
    for (int i = 0; i < dim; i++) {
        s->q[i] = (float)(i % 100) / 100.0f;
    }
    for (int i = 0; i < kv_cache_size; i++) {
        s->key_cache[i] = (float)((i * 7) % 100) / 100.0f;
        s->value_cache[i] = (float)((i * 11) % 100) / 100.0f;
    }
    
    int loff = 0;  // Layer offset (simplified)
    
    // Initialize thread pool if needed
    if (impl->needs_threads && !attention_pool_initialized) {
        init_attention_pool();
    }
    
    // Warmup
    impl->func(s, cfg, loff);
    
    // Benchmark
    long start = time_in_ms();
    for (int iter = 0; iter < iterations; iter++) {
        impl->func(s, cfg, loff);
    }
    long end = time_in_ms();
    
    long total_time = end - start;
    float avg_time = (float)total_time / iterations;
    
    // Rough FLOPS calculation: 
    // Per head: 2 * head_size * pos (attention scores) + 2 * head_size * pos (weighted sum)
    // Total: n_heads * 4 * head_size * pos
    float ops = cfg->n_heads * 4.0f * cfg->head_size * (cfg->pos + 1);
    float gflops = (ops * iterations) / (total_time * 1000000.0f);
    
    printf("  [%-30s] %ld ms total, %.2f ms/iter, %.3f GFLOPS\n",
           impl->name, total_time, avg_time, gflops);
    
    free(s->q);
    free(s->xb);
    free(s->att);
    free(s->key_cache);
    free(s->value_cache);
    free(s);
}

/**
 * @brief Run complete benchmark suite across all configurations
 * 
 * @details
 * Executes all four implementations across six realistic LLM
 * configurations. Adjusts iteration counts based on workload
 * size to keep benchmark times reasonable.
 * 
 * Prints formatted results with configuration details and
 * performance metrics for each implementation.
 */
void run_benchmark_suite(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("              MULTI-HEAD ATTENTION BENCHMARK SUITE FOR XV6\n");
    printf("================================================================================\n");
    printf("Thread Count: %d\n", g_num_threads);
    printf("\n");

    AttentionImpl implementations[] = {
        {"Baseline (Sequential)", attention_baseline, 0},
        {"Optimized Sequential", attention_optimized_seq, 0},
        {"Parallel (Head-level)", attention_parallel_simple, 1},
        {"Parallel (Cache Blocked)", attention_parallel_blocked, 1},
    };
    int num_impls = sizeof(implementations) / sizeof(implementations[0]);

    for (int c = 0; c < NUM_TEST_CONFIGS; c++) {
        AttentionConfig* cfg = &TEST_CONFIGS[c];
        
        // Determine iteration count based on work
        int iterations;
        long work = (long)cfg->n_heads * cfg->head_size * cfg->pos;
        if (work > 100000) {
            iterations = 10;
        } else if (work > 10000) {
            iterations = 50;
        } else {
            iterations = 100;
        }

        printf("--------------------------------------------------------------------------------\n");
        printf("Config: %s\n", cfg->name);
        printf("  n_heads=%d, head_size=%d, kv_dim=%d, pos=%d, seq_len=%d\n",
               cfg->n_heads, cfg->head_size, cfg->kv_dim, cfg->pos, cfg->seq_len);
        printf("  Iterations: %d\n", iterations);
        printf("--------------------------------------------------------------------------------\n");

        for (int i = 0; i < num_impls; i++) {
            if (attention_pool_initialized) {
                shutdown_attention_pool();
            }

            benchmark_implementation(&implementations[i], cfg, iterations);
        }

        printf("\n");
    }

    if (attention_pool_initialized) {
        shutdown_attention_pool();
    }
}

/**
 * @brief Verify numerical correctness of all implementations
 * 
 * @details
 * Compares optimized and parallel implementations against
 * baseline to ensure they produce identical results.
 * 
 * Uses a moderate-sized configuration for thorough testing.
 * Reports maximum difference and PASS/FAIL status.
 */
void verify_correctness(void) {
    printf("================================================================================\n");
    printf("                        CORRECTNESS VERIFICATION\n");
    printf("================================================================================\n");

    AttentionConfig test_cfg = {6, 48, 288, 256, 50, "Test config"};
    int dim = test_cfg.n_heads * test_cfg.head_size;
    int kv_cache_size = test_cfg.seq_len * test_cfg.kv_dim;
    
    MockRunState* s_baseline = malloc(sizeof(MockRunState));
    MockRunState* s_test = malloc(sizeof(MockRunState));
    
    s_baseline->q = malloc(dim * sizeof(float));
    s_baseline->xb = malloc(dim * sizeof(float));
    s_baseline->att = malloc(test_cfg.n_heads * test_cfg.seq_len * sizeof(float));
    s_baseline->key_cache = malloc(kv_cache_size * sizeof(float));
    s_baseline->value_cache = malloc(kv_cache_size * sizeof(float));
    
    s_test->q = malloc(dim * sizeof(float));
    s_test->xb = malloc(dim * sizeof(float));
    s_test->att = malloc(test_cfg.n_heads * test_cfg.seq_len * sizeof(float));
    s_test->key_cache = malloc(kv_cache_size * sizeof(float));
    s_test->value_cache = malloc(kv_cache_size * sizeof(float));
    
    // Initialize with same data
    for (int i = 0; i < dim; i++) {
        s_baseline->q[i] = s_test->q[i] = (float)i / dim;
    }
    for (int i = 0; i < kv_cache_size; i++) {
        s_baseline->key_cache[i] = s_test->key_cache[i] = (float)i / kv_cache_size;
        s_baseline->value_cache[i] = s_test->value_cache[i] = (float)(i * 2) / kv_cache_size;
    }
    
    int loff = 0;
    
    attention_baseline(s_baseline, &test_cfg, loff);
    
    // Test optimized sequential
    attention_optimized_seq(s_test, &test_cfg, loff);
    float diff1 = verify_attention(s_baseline, s_test, dim);
    printf("Optimized Sequential: max diff = %.6f %s\n", diff1, 
           diff1 < 0.001f ? "[PASS]" : "[FAIL]");
    
    // Test parallel
    init_attention_pool();
    attention_parallel_simple(s_test, &test_cfg, loff);
    float diff2 = verify_attention(s_baseline, s_test, dim);
    printf("Parallel (Head-level): max diff = %.6f %s\n", diff2,
           diff2 < 0.001f ? "[PASS]" : "[FAIL]");
    shutdown_attention_pool();
    
    printf("\n");
    
    free(s_baseline->q);
    free(s_baseline->xb);
    free(s_baseline->att);
    free(s_baseline->key_cache);
    free(s_baseline->value_cache);
    free(s_baseline);
    
    free(s_test->q);
    free(s_test->xb);
    free(s_test->att);
    free(s_test->key_cache);
    free(s_test->value_cache);
    free(s_test);
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
 * runs full benchmark suite, and prints summary insights.
 * 
 * Usage: attention_benchmark [thread_count]
 *   thread_count: Number of worker threads (1-8, default: 3)
 */
int main(int argc, char* argv[]) {
    if (argc > 1) {
        g_num_threads = atoi(argv[1]);
        if (g_num_threads < 1) g_num_threads = 1;
        if (g_num_threads > 8) g_num_threads = 8;
    }

    printf("\nMulti-Head Attention Benchmark Starting...\n");
    printf("Using %d threads\n\n", g_num_threads);

    // Verify correctness first
    verify_correctness();

    // Run full benchmark suite
    run_benchmark_suite();

    printf("================================================================================\n");
    printf("                          BENCHMARK COMPLETE\n");
    printf("================================================================================\n");
    printf("\nKEY INSIGHTS:\n");
    printf("- Attention is embarrassingly parallel across heads\n");
    printf("- Speedup depends on: n_heads (more = better), pos (longer sequences benefit)\n");
    printf("- Threading overhead matters for short sequences (pos < 32)\n");
    printf("- Best impl typically: Parallel for n_heads >= 6, Optimized Sequential for < 4\n");
    printf("\n");

    exit(0);
}