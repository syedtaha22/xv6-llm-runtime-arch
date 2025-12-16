/**
 * @file inference.c
 * @brief LLM inference program with persistent shared memory weight caching.
 *
 * @author Syed Taha
 * @date 28th November 2025
 *
 * @details
 * This program demonstrates LLM inference with persistent shared memory caching.
 * On first run, it fetches model weights from the UDP server and stores them in
 * persistent shared memory. On subsequent runs, it reuses the cached weights,
 * eliminating redundant network fetches.
 *
 * The program uses the shared memory subsystem to maintain weights across
 * process boundaries, enabling efficient LLM runtime in resource-constrained
 * environments.
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include "testutil.h"
#include "ftpclient.h"

#define WEIGHTS_SEGMENT "llm_weights"
#define TOKENIZER_SIZE 433869  // Example expected size
#define WEIGHTS_SIZE 60816028

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


int main(int argc, char* argv[]) {
    set_tag("LLM_INFERENCE");
    info("LLM Inference Program Starting...");

    // Fetch model weights
    void* weights_ptr = fetch_if_not_cached("llm_weights", WEIGHTS_SIZE, fetch_model_weights);
    if (!weights_ptr) fail("Unable to obtain model weights");

    // Fetch tokenizer
    void* tokenizer_ptr = fetch_if_not_cached("llm_tokenizer", TOKENIZER_SIZE, fetch_tokenizer);
    if (!tokenizer_ptr) {
        failnoex("Unable to obtain tokenizer");
        shmdt(weights_ptr); // Release weights if allocated
        exit(1);
    }

    // Here you can use weights_ptr and tokenizer_ptr for inference

    // Detach after use
    shmdt(weights_ptr);
    shmdt(tokenizer_ptr);

    pass("LLM Inference Program Completed Successfully.");
    exit(0);
}
