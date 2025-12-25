/**
 * @file llama.c
 * @brief Pure C llama inference program integrated with xv6 userland and persistent shared memory caching.
 *
 * @author Hadiya Muneeb
 *
 * @date 29th November 2025
 *
 * @details
 * This program provides a fully self-contained llama inference engine designed for
 * execution inside xv6 user space. All model weights and tokenizer data are loaded
 * exclusively from persistent shared memory, eliminating any dependency on the
 * filesystem or mmap-based checkpoint loading.
 *
 * The program integrates:
 * - A memory-mapped weight parser for the Transformer architecture (without file I/O)
 * - A tokenizer implementation that constructs vocab, scores, and lookup tables
 *   directly from a raw binary buffer present in shared memory
 * - A sampler (top-p, temperature scaling, greedy sampling)
 * - End-to-end generation and chat loops consistent with the llama 2 Chat schema
 *
 * Changes and enhancements introduced in this implementation:
 * - Shared Memory Model Loading
 * - Shared Memory Tokenizer Loading
 * - Filesystem-Free Execution
 * - xv6 Compatibility Adjustments
 * - Retention of Original llama Inference Logic
 *
 * This file serves as a fully operational llama runtime inside xv6, relying
 * solely on shared memory, UDP-based model distribution, and xv6-compatible
 * minimal C library abstractions.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include "user/xstdlib.h"
#include "user/xstrlib.h"
#include "user/xmath.h"
#include "user/ftpclient.h"
#include "user/perf.h"

#include "testutil.h"
#include "ftpclient.h"

 // stdlib functions
#define calloc   xcalloc
#define bsearch  xbsearch
#define qsort    xqsort
#define atoi     xatoi
#define atof     xatof

// strlib functions
#define sprintf  xsprintf
#define sscanf   xsscanf
#define isprint  xisprint
#define isspace  xisspace

// Math function macros
#define sqrtf    xsqrtf
#define expf     xexpf
#define powf     xpowf
#define cosf     xcosf
#define sinf     xsinf
#define abs      xfabsf
#define floorf   xfloorf

#define stdout 1 // fd for standard output
#define stderr 2 // fd for standard error

// Exit codes
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define WEIGHTS_SEGMENT "llm_weights" // name of shared memory region holding model weights
#define TOKENIZER_SIZE 433869  // expected tokenizer binary size (bytes)
#define WEIGHTS_SIZE 60816028 // expected model weight buffer size (bytes)

typedef uint32 size_t;
#define NULL ((void*)0)
#define eprintf(fmt, ...) printf(fmt, ##__VA_ARGS__) // xv6 replacement for fprintf(stderr, ...)

void* GLOBAL_WEIGHTS_PTR = NULL; // global pointer to the model weights in shared memory
void* GLOBAL_TOKENIZER_PTR = NULL; // global pointer to the tokenizer data in shared memory

void shutdown_thread_pool(void);

/**
 * @brief Print performance metrics.
 */
void release_and_exit(int code) {
  shutdown_thread_pool();
  // clean up any global state here if needed
  shmdt(GLOBAL_WEIGHTS_PTR);
  shmdt(GLOBAL_TOKENIZER_PTR);
  exit(code);
}

/**
 * @brief Print current RAM usage.
 */
void print_ram_usage(const char* msg) {
  uint64 used = getramused();
  printf("%s: %lu bytes (%lu KB, %lu MB)\n", msg, used, used / 1024, used / (1024 * 1024));
}

/**
 * @brief Fetch or attach to cached model data in shared memory.
 *
 * @param segment_name Name of the shared memory segment.
 * @param expected_size Expected size of the file in bytes.
 * @param fetch_fn Function pointer to fetch the file from the server.
 *
 * @return void* Pointer to the shared memory containing the data, or 0 on failure.
 */
void* fetch_if_not_cached(const char* segment_name, int expected_size, char* (*fetch_fn)(int* size_out)) {
  int shmid;
  void* shmaddr;
  char* data_buffer;
  int size;

  // Try to get existing shared memory segment with correct size
  shmid = shmget(segment_name, expected_size, 0);
  if (shmid >= 0) {
    shmaddr = shmat(shmid, 0, SHM_RDONLY);
    if (shmaddr == (void*)-1) {
      failnoex(" Failed to attach to existing shared memory segment");
      return 0;
    }
    pass(" Cached data found (segment: %s, ID: %d). Ready for use.", segment_name, shmid);
    return shmaddr;
  }

  // Segment not found → fetch from server
  info(" No cached data found for %s. Fetching from server...", segment_name);
  data_buffer = fetch_fn(&size);
  if (!data_buffer || size != expected_size) {
    failnoex(" Failed to fetch %s or size mismatch (got %d, expected %d)", segment_name, size, expected_size);
    return 0;
  }

  // Create persistent shared memory segment
  shmid = shmget(segment_name, size, IPC_CREAT | SHM_PERSIST);
  if (shmid < 0) {
    failnoex(" Failed to create shared memory segment for %s", segment_name);
    free(data_buffer);
    return 0;
  }

  // Attach and copy data
  shmaddr = shmat(shmid, 0, SHM_RDWR);
  if (shmaddr == (void*)-1) {
    failnoex(" Failed to attach to newly created shared memory segment");
    free(data_buffer);
    return 0;
  }

  memcpy(shmaddr, data_buffer, size);
  free(data_buffer);

  pass(" %s fetched and cached in shared memory (ID: %d).\n", segment_name, shmid);
  return shmaddr;
}


// ------------------------------------------------------------------------------
// Data Structures for Model (Pre-declared for threading)
// ------------------------------------------------------------------------------

typedef struct {
  int dim;
  int hidden_dim;
  int n_layers;
  int n_heads;
  int n_kv_heads;
  int vocab_size;
  int seq_len;
} Config;

typedef struct {
  float* token_embedding_table;
  float* rms_att_weight;
  float* rms_ffn_weight;
  float* wq;
  float* wk;
  float* wv;
  float* wo;
  float* w1;
  float* w2;
  float* w3;
  float* rms_final_weight;
  float* wcls;
} TransformerWeights;

typedef struct {
  float* x;
  float* xb;
  float* xb2;
  float* hb;
  float* hb2;
  float* q;
  float* k;
  float* v;
  float* att;
  float* logits;
  float* key_cache;
  float* value_cache;
} RunState;

// ------------------------------------------------------------------------------
// Thread Pool for Parallel Execution (MatMul + Attention)
// ------------------------------------------------------------------------------

// Task Types
typedef enum {
    TASK_NONE = 0,
    TASK_MATMUL = 1,
    TASK_ATTENTION = 2
} TaskType;

/**
 * @brief Matmul Work Item
 */
typedef struct {
    float* xout;
    float* x;
    float* w;
    int n;
    int d;
    int start_row;
    int end_row;
} MatmulWork; 

/**
 * @brief Attention Work Item
 */
typedef struct {
    RunState* s;
    Config* p;
    TransformerWeights* w;
    unsigned long long layer;
    int pos;
    int head_start;
    int head_end;
} AttentionWork;


/**
 * @brief Structure representing a thread pool worker.
 * Each worker maintains its thread ID, associated work item,
 * exit flag, and function pointer for the specific worker implementation.
 * 
 * @param thread_id Unique identifier for the thread.
 * @param task_type Type of task assigned to the worker (TaskType enum).
 * @param mm_work Pointer to the Matmul work item.
 * @param att_work Pointer to the Attention work item.
 * @param work_ready Pointer to the flag indicating if work is ready.
 * @param work_done Pointer to the flag indicating if work is done.
 * @param should_exit Pointer to the flag indicating if the worker should exit.
 * 
 * @author Hadiya Muneeb
 */
typedef struct {
    int thread_id;
    
    // Task definition
    volatile int task_type; // TaskType enum
    MatmulWork* mm_work;
    AttentionWork* att_work;

    // Synchronization flags
    volatile int* work_ready;
    volatile int* work_done;
    volatile int* should_exit;
} ThreadPoolWorker;

// Global thread pool state. @todo encapsulate in a struct if needed.
static int thread_pool_initialized = 0;       /// @brief Flag indicating if the thread pool is initialized
static int* thread_ids = NULL;                /// @brief Array of thread IDs
static volatile int* work_ready = NULL;       /// @brief Array of work ready flags
static volatile int* work_done = NULL;        /// @brief Array of work done flags
static ThreadPoolWorker** worker_ptrs = NULL; /// @brief Array of pointers to ThreadPoolWorker structs
static volatile int thread_pool_exit = 0;     /// @brief Flag to signal thread pool shutdown
static volatile int g_num_threads = 3;        /// @brief Number of threads in the pool


// Work Item Storage
static MatmulWork* mm_work_items = NULL;
static AttentionWork* att_work_items = NULL;

// Forward decls
void softmax(float* x, int size);

/**
 * @brief Unrolled dot-product computation for better performance.
 * Uses a loop unrolling factor of 8.
 * 
 * @author Hadiya Muneeb
 * 
 * @param w_row Pointer to the weight row.
 * @param x Pointer to the input vector.
 * @param n Length of the vectors.
 * @return float Result of the dot product.
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
 * @brief Dedicated worker for processing attention heads.
 */
static void worker_do_attention(AttentionWork* work) {
    // Extract context
    RunState* s = work->s;
    Config* p = work->p;
    // TransformerWeights* w = work->w; // Unused in this specific kernel part
    int layer = work->layer;
    int pos = work->pos;

    int head_size = p->dim / p->n_heads;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads;
    int loff = layer * p->seq_len * kv_dim;

    // Iterate assigned heads
    for (int h = work->head_start; h < work->head_end; h++) {
        // --- 1. Score Calculation (Q * K) ---
        float* q = s->q + h * head_size;
        float* att = s->att + h * p->seq_len;
        
        for (int t = 0; t <= pos; t++) {
            float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
            // Use unrolled dot product optimization
            float score = dot_product_unrolled(q, k, head_size);
            score /= sqrtf(head_size);
            att[t] = score;
        }

        // --- 2. Softmax ---
        // Operates on the time dimension, safe to do per-head
        softmax(att, pos + 1);

        // --- 3. Weighted Sum (Att * V) ---
        float* xb = s->xb + h * head_size;
        // Zero output buffer
        for(int i=0; i<head_size; i++) xb[i] = 0.0f;
        
        for (int t = 0; t <= pos; t++) {
            float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
            float a = att[t];
            for (int i = 0; i < head_size; i++) {
                xb[i] += a * v[i];
            }
        }
    }
}

/**
 * @brief Universal Worker Thread
 * Waits for signal, checks task type, executes, signals done.
 */
void universal_worker_thread(void* arg) {
    ThreadPoolWorker* worker = (ThreadPoolWorker*)arg;
    
    while (!(*worker->should_exit)) {
        // Spin-wait with yield
        while (!(*worker->work_ready) && !(*worker->should_exit)) {
            yield(); 
        }

        if (*worker->should_exit) break;

        if (worker->task_type == TASK_MATMUL) {
            MatmulWork* work = worker->mm_work;
            for (int i = work->start_row; i < work->end_row; i++) {
                float* w_row = &work->w[i * work->n];
                work->xout[i] = dot_product_unrolled(w_row, work->x, work->n);
            }
        } 
        else if (worker->task_type == TASK_ATTENTION) {
            worker_do_attention(worker->att_work);
        }

        // Signal completion
        *worker->work_done = 1;
        *worker->work_ready = 0;
    }
    thread_exit();
}

/**
 * @brief Initialize the thread pool (called once at startup)
 */
void init_thread_pool(void) {
    // If already initialized, do nothing
    if (thread_pool_initialized) return;

    // Reset exit flag
    thread_pool_exit = 0;

    // Allocate arrays
    int n = g_num_threads;

    thread_ids = malloc(sizeof(int) * n);
    mm_work_items = malloc(sizeof(MatmulWork) * n);
    att_work_items = malloc(sizeof(AttentionWork) * n); // Alloc attn work
    work_ready = malloc(sizeof(volatile int) * n);
    work_done = malloc(sizeof(volatile int) * n);
    worker_ptrs = malloc(sizeof(ThreadPoolWorker*) * n);

    if (!thread_ids || !mm_work_items || !att_work_items || !worker_ptrs) {
        eprintf("thread pool malloc failed\n");
        release_and_exit(EXIT_FAILURE);
    }

    for (int t = 0; t < n; t++) {
        work_ready[t] = 0;
        work_done[t] = 0;

        // Allocate and initialize worker struct
        ThreadPoolWorker* worker = malloc(sizeof(ThreadPoolWorker));
        if (!worker) {
            eprintf("worker malloc failed\n");
            release_and_exit(EXIT_FAILURE);
        }

        // Initialize worker fields
        // User references so that modifying the global flags
        // Directly affects the worker threads
        worker->thread_id = t;
        worker->mm_work = &mm_work_items[t];
        worker->att_work = &att_work_items[t];
        worker->should_exit = &thread_pool_exit;
        worker->work_ready = &work_ready[t];
        worker->work_done = &work_done[t];
        
        worker_ptrs[t] = worker;

        thread_ids[t] = thread_create(universal_worker_thread, worker);
    }
    thread_pool_initialized = 1;
} 

/**
 * @brief Shutdown the thread pool (called at program exit)
 * @author Hadiya Muneeb
 */
void shutdown_thread_pool(void) {
    if (!thread_pool_initialized) return;

    // Signal threads to exit
    thread_pool_exit = 1;

    for (int t = 0; t < g_num_threads; t++) {
        // Waiting for threads to exit...
        if (thread_ids && thread_ids[t] > 0) thread_join(thread_ids[t]);
    }

    for (int t = 0; t < g_num_threads; t++) free(worker_ptrs[t]);
    free(worker_ptrs);
    free(thread_ids);
    free(mm_work_items);
    free(att_work_items);
    free((void*)work_ready);
    free((void*)work_done);
    thread_pool_initialized = 0;
} 

/**
 * @brief Optimized parallel matrix multiplication
 * 
 * Key optimizations:
 * - Reuses pre-created thread pool
 * - Distributes work in cache-friendly chunks
 * - Minimizes synchronization overhead
 * - Better load balancing
 * 
 * @param xout Output vector (d,)
 * @param x Input vector (n,)
 * @param w Weight matrix (d, n)
 * @param n Number of columns in w
 * @param d Number of rows in w
 */
void matmul(float* xout, float* x, float* w, int n, int d) {
    perf_start_function("matmul");
    
    // Fallback for small matrices or uninitialized pool
    if (d < 128 || !thread_pool_initialized) {
        for (int i = 0; i < d; i++) {
            float val = 0.0f;
            float* w_row = &w[i * n];
            for (int j = 0; j < n; j++) {
                val += w_row[j] * x[j];
            }
            xout[i] = val;
        }
        perf_end_function("matmul");
        return;
    }
  
    // Thread distribution logic
    int rows_per_thread = (d + g_num_threads - 1) / g_num_threads;
    // Align to 16 floats (cache line friendly)
    rows_per_thread = ((rows_per_thread + 15) / 16) * 16; 

    int num_active_threads = (d + rows_per_thread - 1) / rows_per_thread;
    if (num_active_threads > g_num_threads) num_active_threads = g_num_threads;

    for (int t = 0; t < num_active_threads; t++) {
        int start_row = t * rows_per_thread;
        int end_row = start_row + rows_per_thread;
        if (end_row > d) end_row = d;
        if (start_row >= d) break;

        mm_work_items[t].xout = xout;
        mm_work_items[t].x = x;
        mm_work_items[t].w = w;
        mm_work_items[t].n = n;
        mm_work_items[t].d = d;
        mm_work_items[t].start_row = start_row;
        mm_work_items[t].end_row = end_row;
        
        worker_ptrs[t]->task_type = TASK_MATMUL; // Set Task Type
        
        work_done[t] = 0;
        work_ready[t] = 1;
    }

    for (int t = 0; t < num_active_threads; t++) {
        while (!work_done[t]) { yield(); }
    }
    
    perf_end_function("matmul");
}

// ----------------------------------------------------------------------------
// Model Structure & Logic
// ----------------------------------------------------------------------------

typedef struct {
  Config config; 
  TransformerWeights weights; 
  RunState state; 
} Transformer;

void malloc_run_state(RunState* s, Config* p) {
  // we calloc instead of malloc to keep valgrind happy
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  s->x = calloc(p->dim, sizeof(float));
  s->xb = calloc(p->dim, sizeof(float));
  s->xb2 = calloc(p->dim, sizeof(float));
  s->hb = calloc(p->hidden_dim, sizeof(float));
  s->hb2 = calloc(p->hidden_dim, sizeof(float));
  s->q = calloc(p->dim, sizeof(float));
  s->key_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
  s->value_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
  s->att = calloc(p->n_heads * p->seq_len, sizeof(float));
  s->logits = calloc(p->vocab_size, sizeof(float));
  // ensure all mallocs went fine
  if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
    || !s->key_cache || !s->value_cache || !s->att || !s->logits) {
    eprintf("malloc failed!\n");
    release_and_exit(EXIT_FAILURE);
  }
}

void free_run_state(RunState* s) {
  free(s->x);
  free(s->xb);
  free(s->xb2);
  free(s->hb);
  free(s->hb2);
  free(s->q);
  free(s->att);
  free(s->logits);
  free(s->key_cache);
  free(s->value_cache);
}

void memory_map_weights(TransformerWeights* w, Config* p, float* ptr, int shared_weights) {
  int head_size = p->dim / p->n_heads;
  // make sure the multiplications below are done in 64bit to fit the parameter counts of 13B+ models
  unsigned long long n_layers = p->n_layers;
  w->token_embedding_table = ptr;
  ptr += p->vocab_size * p->dim;
  w->rms_att_weight = ptr;
  ptr += n_layers * p->dim;
  w->wq = ptr;
  ptr += n_layers * p->dim * (p->n_heads * head_size);
  w->wk = ptr;
  ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
  w->wv = ptr;
  ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
  w->wo = ptr;
  ptr += n_layers * (p->n_heads * head_size) * p->dim;
  w->rms_ffn_weight = ptr;
  ptr += n_layers * p->dim;
  w->w1 = ptr;
  ptr += n_layers * p->dim * p->hidden_dim;
  w->w2 = ptr;
  ptr += n_layers * p->hidden_dim * p->dim;
  w->w3 = ptr;
  ptr += n_layers * p->dim * p->hidden_dim;
  w->rms_final_weight = ptr;
  ptr += p->dim;
  ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_real (for RoPE)
  ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_imag (for RoPE)
  w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

/**
 * @brief Initialize Transformer model using weights stored in shared memory.
 *
 * @param t Pointer to a Transformer structure to initialize.
 * @param weights_ptr Pointer to the raw memory buffer containing model weights.
 *
 * @author Hadiya Muneeb
 * @date 29th November 2025
 *
 * @details
 * This version removes all filesystem-dependent checkpoint loading and instead
 * constructs the Transformer directly from a memory block retrieved through
 * persistent shared memory. The function:
 * - Reads the Config header from the first bytes of the weight buffer
 * - Maps all weight matrices using pointer arithmetic
 * - Allocates run-state buffers with xv6-compatible xcalloc()
 *
 * No file descriptors, mmap regions, or disk reads are used.
 */
void build_transformer(Transformer* t, void* weights_ptr) {
  perf_start_function("build_transformer");

  // First bytes of weights contain Config
  memcpy(&t->config, weights_ptr, sizeof(Config));
  int shared_weights = t->config.vocab_size > 0 ? 1 : 0;
  t->config.vocab_size = abs(t->config.vocab_size);
  // Map weights that follow the config section
  float* weights_f32 = (float*)((char*)weights_ptr + sizeof(Config));
  memory_map_weights(&t->weights, &t->config, weights_f32, shared_weights);
  // Allocate run-state buffers
  malloc_run_state(&t->state, &t->config);

  perf_end_function("build_transformer");
}

void free_transformer(Transformer* t) { free_run_state(&t->state); }

// ----------------------------------------------------------------------------
// neural net blocks; the dynamics of the Transformer

void rmsnorm(float* o, float* x, float* weight, int size) {
  perf_start_function("rmsnorm");
  // calculate sum of squares
  float ss = 0.0f;
  for (int j = 0; j < size; j++) {
    ss += x[j] * x[j];
  }
  ss /= size;
  ss += 1e-5f;
  ss = 1.0f / sqrtf(ss);
  // normalize and scale
  for (int j = 0; j < size; j++) {
    o[j] = weight[j] * (ss * x[j]);
  }
  perf_end_function("rmsnorm");
}

void softmax(float* x, int size) {
  // find max value (for numerical stability)
  float max_val = x[0];
  for (int i = 1; i < size; i++) {
    if (x[i] > max_val) {
      max_val = x[i];
    }
  }
  // exp and sum
  float sum = 0.0f;
  for (int i = 0; i < size; i++) {
    x[i] = expf(x[i] - max_val);
    sum += x[i];
  }
  // normalize
  for (int i = 0; i < size; i++) {
    x[i] /= sum;
  }
}

/**
 * @brief Hybrid Parallel Multi-Head Attention
 * Uses static head partitioning. If pos < 32, runs sequentially.
 * If pos >= 32, dispatches to worker pool.
 * 
 * @param s Pointer to the current run state.
 * @param p Pointer to the model configuration.
 * @param w Pointer to the model weights.
 * @param l Current layer index.
 * @param pos Current position in the sequence.
 */
static void multihead_attention(RunState* s, Config* p, TransformerWeights* w, unsigned long long l, int pos) {
  perf_start_function("multihead_attention");

  // HYBRID CHECK: If sequence is short, overhead > gain. Run sequentially.
  if (pos < 32 || !thread_pool_initialized) {
      // Create a temporary "work" item on stack and run it directly
      // This reuses the exact same logic as the workers
      AttentionWork seq_work = {
          .s = s, .p = p, .w = w, 
          .layer = l, .pos = pos, 
          .head_start = 0, .head_end = p->n_heads
      };
      worker_do_attention(&seq_work);
      perf_end_function("multihead_attention");
      return;
  }

  // PARALLEL DISPATCH
  
  int heads_per_thread = (p->n_heads + g_num_threads - 1) / g_num_threads;
  int num_active_threads = (p->n_heads + heads_per_thread - 1) / heads_per_thread;
  if (num_active_threads > g_num_threads) num_active_threads = g_num_threads;

  for (int t = 0; t < num_active_threads; t++) {
      int start = t * heads_per_thread;
      int end = start + heads_per_thread;
      if (end > p->n_heads) end = p->n_heads;
      if (start >= p->n_heads) break;

      att_work_items[t].s = s;
      att_work_items[t].p = p;
      att_work_items[t].w = w;
      att_work_items[t].layer = l;
      att_work_items[t].pos = pos;
      att_work_items[t].head_start = start;
      att_work_items[t].head_end = end;

      worker_ptrs[t]->task_type = TASK_ATTENTION;
      
      work_done[t] = 0;
      work_ready[t] = 1; // Signal
  }

  // Synchronization
  for (int t = 0; t < num_active_threads; t++) 
      while (!work_done[t]) {
        // yeild to avoid busy-waiting  
        yield(); 
      }
  
  perf_end_function("multihead_attention");
}


float* forward(Transformer* transformer, int token, int pos) {
  perf_start_function("forward");

  // a few convenience variables
  Config* p = &transformer->config;
  TransformerWeights* w = &transformer->weights;
  RunState* s = &transformer->state;
  float* x = s->x;
  int dim = p->dim;
  int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
  int hidden_dim = p->hidden_dim;
  int head_size = dim / p->n_heads;

  // copy the token embedding into x
  float* content_row = w->token_embedding_table + token * dim;
  memcpy(x, content_row, dim * sizeof(*x));

  // forward all the layers
  for (unsigned long long l = 0; l < p->n_layers; l++) {

    // attention rmsnorm
    rmsnorm(s->xb, x, w->rms_att_weight + l * dim, dim);

    // key and value point to the kv cache
    int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
    s->k = s->key_cache + loff + pos * kv_dim;
    s->v = s->value_cache + loff + pos * kv_dim;

    // qkv matmuls for this position
    matmul(s->q, s->xb, w->wq + l * dim * dim, dim, dim);
    matmul(s->k, s->xb, w->wk + l * dim * kv_dim, dim, kv_dim);
    matmul(s->v, s->xb, w->wv + l * dim * kv_dim, dim, kv_dim);

    // RoPE relative positional encoding: complex-valued rotate q and k in each head
    for (int i = 0; i < dim; i += 2) {
      int head_dim = i % head_size;
      float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
      float val = pos * freq;
      float fcr = cosf(val);
      float fci = sinf(val);
      int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
      for (int v = 0; v < rotn; v++) {
        float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
        float v0 = vec[i];
        float v1 = vec[i + 1];
        vec[i] = v0 * fcr - v1 * fci;
        vec[i + 1] = v0 * fci + v1 * fcr;
      }
    }

    // Call Hybrid Parallel Attention
    multihead_attention(s, p, w, l, pos);

    // final matmul to get the output of the attention
    matmul(s->xb2, s->xb, w->wo + l * dim * dim, dim, dim);

    // residual connection back into x
    for (int i = 0; i < dim; i++) {
      x[i] += s->xb2[i];
    }

    // ffn rmsnorm
    rmsnorm(s->xb, x, w->rms_ffn_weight + l * dim, dim);

    // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
    // first calculate self.w1(x) and self.w3(x)
    matmul(s->hb, s->xb, w->w1 + l * dim * hidden_dim, dim, hidden_dim);
    matmul(s->hb2, s->xb, w->w3 + l * dim * hidden_dim, dim, hidden_dim);

    // SwiGLU non-linearity
    for (int i = 0; i < hidden_dim; i++) {
      float val = s->hb[i];
      // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
      val *= (1.0f / (1.0f + expf(-val)));
      // elementwise multiply with w3(x)
      val *= s->hb2[i];
      s->hb[i] = val;
    }

    // final matmul to get the output of the ffn
    matmul(s->xb, s->hb, w->w2 + l * dim * hidden_dim, hidden_dim, dim);

    // residual connection
    for (int i = 0; i < dim; i++) {
      x[i] += s->xb[i];
    }
  }

  // final rmsnorm
  rmsnorm(x, x, w->rms_final_weight, dim);

  // classifier into logits
  matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
  perf_end_function("forward");
  return s->logits;
}

// ----------------------------------------------------------------------------
// The Byte Pair Encoding (BPE) Tokenizer that translates strings <-> tokens

typedef struct {
  char* str;
  int id;
} TokenIndex;

typedef struct {
  char** vocab;
  float* vocab_scores;
  TokenIndex* sorted_vocab;
  int vocab_size;
  unsigned int max_token_length;
  unsigned char byte_pieces[512]; // stores all single-byte strings
} Tokenizer;

int compare_tokens(const void* a, const void* b) {
  return strcmp(((TokenIndex*)a)->str, ((TokenIndex*)b)->str);
}

/**
 * @brief Construct tokenizer vocabulary and score tables from shared memory.
 *
 * @param t Pointer to Tokenizer structure to initialize.
 * @param tokenizer_data Raw pointer to tokenizer binary buffer in shared memory.
 * @param vocab_size Number of tokenizer entries defined by the model config.
 *
 * @author Hadiya Muneeb
 *
 * @date 29th November 2025
 *
 * @details
 * This implementation eliminates all filesystem interaction and parses the
 * tokenizer from a memory region delivered by the shared-memory caching layer.
 * The binary layout is interpreted as:
 *
 *   int max_token_length
 *   repeat vocab_size times:
 *       float score
 *       int token_length
 *       byte[token_length]
 *
 * All vocab strings, scores, and byte-pieces are reconstructed in xv6 userland
 * without reading tokenizer.bin from disk.
 */
void build_tokenizer(Tokenizer* t, void* tokenizer_data, int vocab_size) {
  perf_start_function("build_tokenizer");

  // i should have written the vocab_size into the tokenizer file... sigh
  t->vocab_size = vocab_size;
  // malloc space to hold the scores and the strings
  t->vocab = (char**)malloc(vocab_size * sizeof(char*));
  t->vocab_scores = (float*)malloc(vocab_size * sizeof(float));
  t->sorted_vocab = NULL; // initialized lazily
  for (int i = 0; i < 256; i++) {
    t->byte_pieces[i * 2] = (unsigned char)i;
    t->byte_pieces[i * 2 + 1] = '\0';
  }
  // read from memory
  char* ptr = (char*)tokenizer_data;

  t->max_token_length = *(int*)ptr;
  ptr += sizeof(int);

  int len;
  for (int i = 0; i < vocab_size; i++) {
    t->vocab_scores[i] = *(float*)ptr;
    ptr += sizeof(float);

    len = *(int*)ptr;
    ptr += sizeof(int);

    t->vocab[i] = (char*)malloc(len + 1);
    memcpy(t->vocab[i], ptr, len);
    ptr += len;

    t->vocab[i][len] = '\0';
  }

  perf_end_function("build_tokenizer");
}

void free_tokenizer(Tokenizer* t) {
  for (int i = 0; i < t->vocab_size; i++) { free(t->vocab[i]); }
  free(t->vocab);
  free(t->vocab_scores);
  free(t->sorted_vocab);
}

char* decode(Tokenizer* t, int prev_token, int token) {
  perf_start_function("decode");
  char* piece = t->vocab[token];
  // following BOS (1) token, sentencepiece decoder strips any leading whitespace (see PR #89)
  if (prev_token == 1 && piece[0] == ' ') { piece++; }
  // careful, some tokens designate raw bytes, and look like e.g. '<0x01>'
  // parse this and convert and return the actual byte
  unsigned char byte_val;
  if (sscanf(piece, "<0x%02hhX>", &byte_val) == 1) {
    piece = (char*)t->byte_pieces + byte_val * 2;
  }
  perf_end_function("decode");
  return piece;
}

void safe_printf(char* piece) {
  // piece might be a raw byte token, and we only want to print printable chars or whitespace
  // because some of the other bytes can be various control codes, backspace, etc.
  if (piece == NULL) { return; }
  if (piece[0] == '\0') { return; }

  // Handle byte tokens like <0x0A>
  if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x' && piece[5] == '>') {
    // Parse hex byte token like <0x0A>
    unsigned char byte_val = 0;
    for (int i = 3; i < 5; i++) {
      byte_val <<= 4;
      if (piece[i] >= '0' && piece[i] <= '9') {
        byte_val += piece[i] - '0';
      }
      else if (piece[i] >= 'A' && piece[i] <= 'F') {
        byte_val += piece[i] - 'A' + 10;
      }
      else if (piece[i] >= 'a' && piece[i] <= 'f') {
        byte_val += piece[i] - 'a' + 10;
      }
    }

    // Only print if it's printable or whitespace
    if (isprint(byte_val) || isspace(byte_val)) {
      printf("%c", byte_val);
    }
    return;
  }

  // Handle single byte tokens
  if (piece[1] == '\0') {
    unsigned char byte_val = piece[0];
    if (!(isprint(byte_val) || isspace(byte_val))) {
      return; // bad byte, don't print it
    }
  }

  printf("%s", piece);
}


int str_lookup(char* str, TokenIndex* sorted_vocab, int vocab_size) {
  // efficiently find the perfect match for str in vocab, return its index or -1 if not found
  TokenIndex tok = { .str = str }; // acts as the key to search for
  TokenIndex* res = bsearch(&tok, sorted_vocab, vocab_size, sizeof(TokenIndex), compare_tokens);
  return res != NULL ? res->id : -1;
}

void encode(Tokenizer* t, char* text, int8_t bos, int8_t eos, int* tokens, int* n_tokens) {
  perf_start_function("encode");

  // encode the string text (input) into an upper-bound preallocated tokens[] array
  // bos != 0 means prepend the BOS token (=1), eos != 0 means append the EOS token (=2)
  if (text == NULL) { eprintf("cannot encode NULL text\n"); release_and_exit(EXIT_FAILURE); }

  if (t->sorted_vocab == NULL) {
    // lazily malloc and sort the vocabulary
    t->sorted_vocab = malloc(t->vocab_size * sizeof(TokenIndex));
    for (int i = 0; i < t->vocab_size; i++) {
      t->sorted_vocab[i].str = t->vocab[i];
      t->sorted_vocab[i].id = i;
    }
    qsort(t->sorted_vocab, t->vocab_size, sizeof(TokenIndex), compare_tokens);
  }

  // create a temporary buffer that will store merge candidates of always two consecutive tokens
  // *2 for concat, +1 for null terminator +2 for UTF8 (in case max_token_length is 1)
  char* str_buffer = malloc((t->max_token_length * 2 + 1 + 2) * sizeof(char));
  size_t str_len = 0;

  // start at 0 tokens
  *n_tokens = 0;

  // add optional BOS (=1) token, if desired
  if (bos) tokens[(*n_tokens)++] = 1;

  // add_dummy_prefix is true by default
  // so prepend a dummy prefix token to the input string, but only if text != ""
  // TODO: pretty sure this isn't correct in the general case but I don't have the
  // energy to read more of the sentencepiece code to figure out what it's doing
  if (text[0] != '\0') {
    int dummy_prefix = str_lookup(" ", t->sorted_vocab, t->vocab_size);
    tokens[(*n_tokens)++] = dummy_prefix;
  }

  // Okay UTF-8 time. This will get messy. Here is the reference from Wikipedia:
  // Code point ↔ UTF-8 conversion
  // First code point	Last code point	Byte 1	Byte 2	Byte 3	Byte 4
  // U+0000	U+007F	    0xxxxxxx
  // U+0080	U+07FF	    110xxxxx	10xxxxxx
  // U+0800	U+FFFF	    1110xxxx	10xxxxxx	10xxxxxx
  // U+10000	U+10FFFF    11110xxx	10xxxxxx	10xxxxxx	10xxxxxx

  // process the raw (UTF-8) byte sequence of the input string
  for (char* c = text; *c != '\0'; c++) {

    // reset buffer if the current byte is ASCII or a leading byte
    // 0xC0 is 11000000, so (*c & 0xC0) keeps the first 2 bits and zeros the rest
    // 0x80 is 10000000
    // in UTF-8, all continuation bytes start with "10" in first two bits
    // so in English this is: "if this byte is not a continuation byte"
    if ((*c & 0xC0) != 0x80) {
      // this byte must be either a leading byte (11...) or an ASCII char (0x...)
      // => reset our location, as we're starting a new UTF-8 codepoint
      str_len = 0;
    }

    // append the current byte to the buffer
    str_buffer[str_len++] = *c; // ++ is post-increment, incremented after this line
    str_buffer[str_len] = '\0';

    // while the next character is a continuation byte, continue appending
    // but if there are too many of them, just stop to avoid overruning str_buffer size.
    if ((*(c + 1) & 0xC0) == 0x80 && str_len < 4) {
      continue;
    }

    // ok c+1 is not a continuation byte, so we've read in a full codepoint
    int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);

    if (id != -1) {
      // we found this codepoint in vocab, add it as a token
      tokens[(*n_tokens)++] = id;
    }
    else {
      // byte_fallback encoding: just encode each byte as a token
      // +3 is here because the first 3 vocab elements are <unk>, <s>, </s>
      // so the individual bytes only start at index 3
      for (int i = 0; i < str_len; i++) {
        tokens[(*n_tokens)++] = (unsigned char)str_buffer[i] + 3;
      }
    }
    str_len = 0; // protect against a sequence of stray UTF8 continuation bytes
  }

  // merge the best consecutive pair each iteration, according the scores in vocab_scores
  while (1) {
    float best_score = -1e10;
    int best_id = -1;
    int best_idx = -1;

    for (int i = 0; i < (*n_tokens - 1); i++) {
      // check if we can merge the pair (tokens[i], tokens[i+1])
      sprintf(str_buffer, "%s%s", t->vocab[tokens[i]], t->vocab[tokens[i + 1]]);
      int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);
      if (id != -1 && t->vocab_scores[id] > best_score) {
        // this merge pair exists in vocab! record its score and position
        best_score = t->vocab_scores[id];
        best_id = id;
        best_idx = i;
      }
    }

    if (best_idx == -1) {
      break; // we couldn't find any more pairs to merge, so we're done
    }

    // merge the consecutive pair (best_idx, best_idx+1) into new token best_id
    tokens[best_idx] = best_id;
    // delete token at position best_idx+1, shift the entire sequence back 1
    for (int i = best_idx + 1; i < (*n_tokens - 1); i++) {
      tokens[i] = tokens[i + 1];
    }
    (*n_tokens)--; // token length decreased
  }

  // add optional EOS (=2) token, if desired
  if (eos) tokens[(*n_tokens)++] = 2;

  free(str_buffer);

  perf_end_function("encode");
}

// ----------------------------------------------------------------------------
// The Sampler, which takes logits and returns a sampled token
// sampling can be done in a few ways: greedy argmax, sampling, top-p sampling

typedef struct {
  float prob;
  int index;
} ProbIndex; // struct used when sorting probabilities during top-p sampling

typedef struct {
  int vocab_size;
  ProbIndex* probindex; // buffer used in top-p sampling
  float temperature;
  float topp;
  unsigned long long rng_state;
} Sampler;

int sample_argmax(float* probabilities, int n) {
  // return the index that has the highest probability
  int max_i = 0;
  float max_p = probabilities[0];
  for (int i = 1; i < n; i++) {
    if (probabilities[i] > max_p) {
      max_i = i;
      max_p = probabilities[i];
    }
  }
  return max_i;
}

int sample_mult(float* probabilities, int n, float coin) {
  // sample index from probabilities (they must sum to 1!)
  // coin is a random number in [0, 1), usually from random_f32()
  float cdf = 0.0f;
  for (int i = 0; i < n; i++) {
    cdf += probabilities[i];
    if (coin < cdf) {
      return i;
    }
  }
  return n - 1; // in case of rounding errors
}

int compare(const void* a, const void* b) {
  ProbIndex* a_ = (ProbIndex*)a;
  ProbIndex* b_ = (ProbIndex*)b;
  if (a_->prob > b_->prob) return -1;
  if (a_->prob < b_->prob) return 1;
  return 0;
}

int sample_topp(float* probabilities, int n, float topp, ProbIndex* probindex, float coin) {
  // top-p sampling (or "nucleus sampling") samples from the smallest set of
  // tokens that exceed probability topp. This way we never sample tokens that
  // have very low probabilities and are less likely to go "off the rails".
  // coin is a random number in [0, 1), usually from random_f32()

  int n0 = 0;
  // quicksort indices in descending order of probabilities
  // values smaller than (1 - topp) / (n - 1) cannot be part of the result
  // so for efficiency we crop these out as candidates before sorting
  const float cutoff = (1.0f - topp) / (n - 1);
  for (int i = 0; i < n; i++) {
    if (probabilities[i] >= cutoff) {
      probindex[n0].index = i;
      probindex[n0].prob = probabilities[i];
      n0++;
    }
  }
  qsort(probindex, n0, sizeof(ProbIndex), compare);

  // truncate the list where cumulative probability exceeds topp
  float cumulative_prob = 0.0f;
  int last_idx = n0 - 1; // in case of rounding errors consider all elements
  for (int i = 0; i < n0; i++) {
    cumulative_prob += probindex[i].prob;
    if (cumulative_prob > topp) {
      last_idx = i;
      break; // we've exceeded topp by including last_idx
    }
  }

  // sample from the truncated list
  float r = coin * cumulative_prob;
  float cdf = 0.0f;
  for (int i = 0; i <= last_idx; i++) {
    cdf += probindex[i].prob;
    if (r < cdf) {
      return probindex[i].index;
    }
  }
  return probindex[last_idx].index; // in case of rounding errors
}

void build_sampler(Sampler* sampler, int vocab_size, float temperature, float topp, unsigned long long rng_seed) {
  perf_start_function("build_sampler");

  sampler->vocab_size = vocab_size;
  sampler->temperature = temperature;
  sampler->topp = topp;
  sampler->rng_state = rng_seed;
  // buffer only used with nucleus sampling; may not need but it's ~small
  sampler->probindex = malloc(sampler->vocab_size * sizeof(ProbIndex));

  perf_end_function("build_sampler");
}

void free_sampler(Sampler* sampler) {
  free(sampler->probindex);
}

unsigned int random_u32(unsigned long long* state) {
  // xorshift rng: https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
  *state ^= *state >> 12;
  *state ^= *state << 25;
  *state ^= *state >> 27;
  return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}
float random_f32(unsigned long long* state) { // random float32 in [0,1)
  return (random_u32(state) >> 8) / 16777216.0f;
}

int sample(Sampler* sampler, float* logits) {
  perf_start_function("sample");
  int next;
  if (sampler->temperature == 0.0f) {
    // greedy argmax sampling: take the token with the highest probability
    next = sample_argmax(logits, sampler->vocab_size);
  }
  else {
    // apply the temperature to the logits
    for (int q = 0; q < sampler->vocab_size; q++) { logits[q] /= sampler->temperature; }
    // apply softmax to the logits to get the probabilities for next token
    softmax(logits, sampler->vocab_size);
    // flip a (float) coin (this is our source of entropy for sampling)
    float coin = random_f32(&sampler->rng_state);
    // we sample from this distribution to get the next token
    if (sampler->topp <= 0 || sampler->topp >= 1) {
      // simply sample from the predicted probability distribution
      next = sample_mult(logits, sampler->vocab_size, coin);
    }
    else {
      // top-p (nucleus) sampling, clamping the least likely tokens to zero
      next = sample_topp(logits, sampler->vocab_size, sampler->topp, sampler->probindex, coin);
    }
  }
  perf_end_function("sample");
  return next;
}


// ----------------------------------------------------------------------------
// generation loop

void generate(Transformer* transformer, Tokenizer* tokenizer, Sampler* sampler, char* prompt, int steps) {
  perf_start_function("generate");

  char* empty_prompt = "";
  if (prompt == NULL) { prompt = empty_prompt; }

  // encode the (string) prompt into tokens sequence
  int num_prompt_tokens = 0;
  int* prompt_tokens = (int*)malloc((strlen(prompt) + 3) * sizeof(int)); // +3 for '\0', ?BOS, ?EOS

  encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
  if (num_prompt_tokens < 1) {
    eprintf("something is wrong, expected at least 1 prompt token\n");
    release_and_exit(EXIT_FAILURE);
  }

  // start the main loop
  long start = 0;  // used to time our code, only initialized after first iteration
  int next;        // will store the next token in the sequence
  int token = prompt_tokens[0]; // kick off with the first token in the prompt
  int pos = 0;     // position in the sequence
  int first_token_generated = 0;  // flag to track when first token is generated

  while (pos < steps) {
    // forward the transformer to get logits for the next token
    float* logits = forward(transformer, token, pos);

    // advance the state machine
    if (pos < num_prompt_tokens - 1) {
      // if we are still processing the input prompt, force the next prompt token
      next = prompt_tokens[pos + 1];
    }
    else {
      // otherwise sample the next token from the logits
      next = sample(sampler, logits);
    }
    pos++;

    // data-dependent terminating condition: the BOS (=1) token delimits sequences
    if (next == 1) { break; }

    // print the token as string, decode it with the Tokenizer object
    char* piece = decode(tokenizer, token, next);

    safe_printf(piece); // same as printf("%s", piece), but skips "unsafe" bytes
    token = next;

    // Track Time to First Token (TTFT) - capture when first output token is generated
    if (!first_token_generated && pos > num_prompt_tokens) {
      perf_metrics.time_to_first_token_ms = perf_time_in_ms() - perf_metrics.start_time_ms;
      first_token_generated = 1;
    }

    // init the timer here because the first iteration can be slower
    if (start == 0) { start = perf_time_in_ms(); }
  }
  printf("\n");

  // report achieved tok/s (pos-1 because the timer starts after first iteration)
  if (pos > 1) {
    long end = perf_time_in_ms();
    eprintf("\nachieved tok/s: %f\n", (pos - 1) / (double)(end - start) * 1000);
    perf_metrics.total_tokens_generated = pos - 1;
    perf_metrics.total_inference_time_ms = end - start;
    perf_metrics.tokens_per_second = (pos - 1) / (double)(end - start) * 1000;
  }

  free(prompt_tokens);
  perf_end_function("generate");
}

void read_stdin(const char* guide, char* buffer, size_t bufsize)
{
  // read a line from stdin, up to but not including \n
  printf("%s", guide);
  // Use xv6's gets function
  if (gets(buffer, bufsize) == 0) {
    // Handle EOF or error
    buffer[0] = '\0';
  }

  // Remove trailing newline if present (gets might include it)
  size_t len = strlen(buffer);
  if (len > 0 && buffer[len - 1] == '\n') {
    buffer[len - 1] = '\0';
  }
}

// ----------------------------------------------------------------------------
// chat loop
// I manually inspected the tokens for a few chat conversations compared to
// python reference and that seemed ok, but this was not thoroughly tested and
// is not safely implemented, it's more a proof of concept atm.

void chat(Transformer* transformer, Tokenizer* tokenizer, Sampler* sampler,
  char* cli_user_prompt, char* cli_system_prompt, int steps) {
  perf_start_function("chat");

  // buffers for reading the system prompt and user prompt from stdin
  // you'll notice they are soomewhat haphazardly and unsafely set atm
  char system_prompt[512];
  char user_prompt[512];
  char rendered_prompt[1152];
  int num_prompt_tokens = 0;
  int* prompt_tokens = (int*)malloc(1152 * sizeof(int));
  int user_idx;

  // start the main loop
  int8_t user_turn = 1; // user starts
  int next = 0;        // will store the next token in the sequence
  int token;       // stores the current token to feed into the transformer
  // int prev_token;
  int pos = 0;     // position in the sequence
  while (pos < steps) {

    // when it is the user's turn to contribute tokens to the dialog...
    if (user_turn) {
      // get the (optional) system prompt at position 0
      if (pos == 0) {
        // at position 0, the user can also contribute a system prompt
        if (cli_system_prompt == NULL) {
          // system prompt was not passed in, attempt to get it from stdin
          read_stdin("Enter system prompt (optional): ", system_prompt, sizeof(system_prompt));
        }
        else {
          // system prompt was passed in, use it
          strcpy(system_prompt, cli_system_prompt);
        }
      }
      // get the user prompt
      if (pos == 0 && cli_user_prompt != NULL) {
        // user prompt for position 0 was passed in, use it
        strcpy(user_prompt, cli_user_prompt);
      }
      else {
        // otherwise get user prompt from stdin
        read_stdin("User: ", user_prompt, sizeof(user_prompt));
      }
      // render user/system prompts into the Llama 2 Chat schema
      if (pos == 0 && system_prompt[0] != '\0') {
        char system_template[] = "[INST] <<SYS>>\n%s\n<</SYS>>\n\n%s [/INST]";
        sprintf(rendered_prompt, system_template, system_prompt, user_prompt);
      }
      else {
        char user_template[] = "[INST] %s [/INST]";
        sprintf(rendered_prompt, user_template, user_prompt);
      }
      // encode the rendered prompt into tokens
      encode(tokenizer, rendered_prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
      user_idx = 0; // reset the user index
      user_turn = 0;
      printf("Assistant: ");
    }

    // determine the token to pass into the transformer next
    if (user_idx < num_prompt_tokens) {
      // if we are still processing the input prompt, force the next prompt token
      token = prompt_tokens[user_idx++];
    }
    else {
      // otherwise use the next token sampled from previous turn
      token = next;
    }
    // EOS (=2) token ends the Assistant turn
    if (token == 2) { user_turn = 1; }

    // forward the transformer to get logits for the next token
    float* logits = forward(transformer, token, pos);
    next = sample(sampler, logits);
    pos++;

    if (user_idx >= num_prompt_tokens && next != 2) {
      // the Assistant is responding, so print its output
      char* piece = decode(tokenizer, token, next);
      safe_printf(piece); // same as printf("%s", piece), but skips "unsafe" bytes
    }
    if (next == 2) { printf("\n"); }
  }
  printf("\n");
  free(prompt_tokens);
  perf_end_function("chat");
}

// ----------------------------------------------------------------------------
// CLI, include only if not testing
#ifndef TESTING

void error_usage() {
  eprintf("Usage:   llama [options]\n");
  eprintf("Example: llama -n 256 -i \"Once upon a time\"\n");
  eprintf("Options:\n");
  eprintf("  -t <float>  temperature in [0,inf], default 1.0\n");
  eprintf("  -p <float>  p value in top-p (nucleus) sampling in [0,1] default 0.9\n");
  eprintf("  -s <int>    random seed, default time(NULL)\n");
  eprintf("  -n <int>    number of steps to run for, default 256. 0 = max_seq_len\n");
  eprintf("  -i <string> input prompt\n");
  eprintf("  -m <string> mode: generate|chat, default: generate\n");
  eprintf("  -y <string> (optional) system prompt in chat mode\n");
  eprintf("  -x <int>    number of matmul worker threads (default: 3)\n");
  exit(EXIT_FAILURE);
}

// ----------------------------------------------------------------------------
// Argument parsing

/**
 * @brief Structure to hold command-line arguments.
 *
 * @author Syed Taha
 * @date   1st December 2025
 *
 * @details
 * This structure encapsulates the various command-line arguments that can be
 * passed to the program. It includes parameters for temperature, top-p sampling,
 * number of steps, input prompt, random seed, mode of operation, and an optional
 * system prompt for chat mode.
 */
typedef struct {
  float temperature;
  float topp;
  int steps;
  char* prompt;
  unsigned long long rng_seed;
  char* mode;
  char* system_prompt;
  int num_threads; // -x flag: number of matmul worker threads
} Args; 

/**
 * @brief Parse command-line arguments and populate the Args structure.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @param args Pointer to Args structure to populate.
 * @return int Returns 0 on success, -1 on failure.
 *
 * @author Syed Taha
 * @date   1st December 2025
 *
 * @details
 * This function processes the command-line arguments provided to the program,
 * extracting values for temperature, top-p sampling, number of steps, input prompt,
 * random seed, mode of operation, and an optional system prompt for chat mode.
 * It performs basic validation on the arguments and sets default values where necessary.
 */
int argparse(int argc, char* argv[], Args* args) {
  // default values
  args->temperature = 1.0f;      // 0.0 = greedy deterministic. 1.0 = original. don't set higher
  args->topp = 0.9f;             // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
  args->steps = 256;             // number of steps to run for
  args->prompt = NULL;           // prompt string
  args->rng_seed = 0;            // seed rng with time by default
  args->mode = "generate";       // generate|chat
  args->system_prompt = NULL;    // the (optional) system prompt to use in chat mode
  args->num_threads = 3;         // default number of matmul worker threads


  for (int i = 1; i < argc; i += 2) {
    // do some basic validation
    if (i + 1 >= argc) { return -1; } // must have arg after flag
    if (argv[i][0] != '-') { return -1; } // must start with dash
    if (strlen(argv[i]) != 2) { return -1; } // must be -x (one dash, one letter)
    // read in the args
    if (argv[i][1] == 't') { args->temperature = atof(argv[i + 1]); }
    else if (argv[i][1] == 'p') { args->topp = atof(argv[i + 1]); }
    else if (argv[i][1] == 's') { args->rng_seed = atoi(argv[i + 1]); }
    else if (argv[i][1] == 'n') { args->steps = atoi(argv[i + 1]); }
    else if (argv[i][1] == 'i') { args->prompt = argv[i + 1]; }
    else if (argv[i][1] == 'm') { args->mode = argv[i + 1]; }
    else if (argv[i][1] == 'y') { args->system_prompt = argv[i + 1]; }
    else if (argv[i][1] == 'x') { args->num_threads = atoi(argv[i + 1]); }
    else { return -1; }
  }

  if (args->rng_seed <= 0) args->rng_seed = (unsigned long long)rdtime();
  if (args->temperature < 0.0f) args->temperature = 0.0f;
  if (args->topp < 0.0f || args->topp > 1.0f) args->topp = 0.9f;
  if (args->steps < 0) args->steps = 0;
  if (args->num_threads <= 0) args->num_threads = 1;
  // cap to reasonable upper bound to avoid excessive allocations in xv6
  if (args->num_threads > 16) args->num_threads = 16; 

  return 0;
}

int main(int argc, char* argv[]) {
  // Initialize profiling
  perf_init();
  perf_register_function("fetch_model_weights");
  perf_register_function("fetch_tokenizer");
  perf_register_function("build_transformer");
  perf_register_function("build_tokenizer");
  perf_register_function("build_sampler");
  perf_register_function("generate");
  perf_register_function("chat");
  perf_register_function("forward");
  perf_register_function("multihead_attention");
  perf_register_function("matmul");
  perf_register_function("rmsnorm");
  perf_register_function("sample");
  perf_register_function("encode");
  perf_register_function("decode");

  Args args;

  if (argparse(argc, argv, &args) != 0) error_usage();

  // apply user-configured thread count for matmul worker pool
  g_num_threads = args.num_threads; 

  // Initialize performance metrics
  perf_metrics.start_time_ms = perf_time_in_ms();
  perf_metrics.initial_ram_usage = getramused();
  perf_metrics.peak_ram_usage = perf_metrics.initial_ram_usage;

  Transformer transformer;
  Tokenizer tokenizer;

  perf_start_function("fetch_model_weights");
  GLOBAL_WEIGHTS_PTR = fetch_if_not_cached("llm_weights", WEIGHTS_SIZE, fetch_model_weights);
  if (!GLOBAL_WEIGHTS_PTR) { eprintf("Could not load weights\n"); exit(1); }
  perf_end_function("fetch_model_weights");

  perf_start_function("fetch_tokenizer");
  GLOBAL_TOKENIZER_PTR = fetch_if_not_cached("llm_tokenizer", TOKENIZER_SIZE, fetch_tokenizer);
  if (!GLOBAL_TOKENIZER_PTR) {
    eprintf("Could not load tokenizer\n");
    shmdt(GLOBAL_WEIGHTS_PTR);
    exit(1);
  }
  perf_end_function("fetch_tokenizer");

  build_transformer(&transformer, GLOBAL_WEIGHTS_PTR);
  build_tokenizer(&tokenizer, GLOBAL_TOKENIZER_PTR, transformer.config.vocab_size);

  perf_update_peak_ram();

  if (args.steps == 0 || args.steps > transformer.config.seq_len)
    args.steps = transformer.config.seq_len; // override to ~max length

  // build the Sampler
  Sampler sampler;
  build_sampler(&sampler, transformer.config.vocab_size, args.temperature, args.topp, args.rng_seed);


  perf_update_peak_ram();

  init_thread_pool();

  // run!
  if (strcmp(args.mode, "generate") == 0) {
    generate(&transformer, &tokenizer, &sampler, args.prompt, args.steps);
  }
  else if (strcmp(args.mode, "chat") == 0) {
    chat(&transformer, &tokenizer, &sampler, args.prompt, args.system_prompt, args.steps);
  }
  else {
    eprintf("unknown mode: %s\n", args.mode);
    error_usage();
  }

  // memory and file handles cleanup
  free_sampler(&sampler);
  free_tokenizer(&tokenizer);
  free_transformer(&transformer);

  perf_metrics.end_time_ms = perf_time_in_ms();
  perf_update_peak_ram();
  perf_print_report();

  release_and_exit(EXIT_SUCCESS);

  // Should not reach here. 
  printf("Time to panic!\n");
  return -1;
}
#endif