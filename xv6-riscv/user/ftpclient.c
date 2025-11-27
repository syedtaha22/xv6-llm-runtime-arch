/**
 * @file ftpclient.c
 * @brief User-space UDP client for fetching files from the LLM UDP server.
 * @author Hamna Sajid
 * @date 22nd November 2025
 *
 * @details
 * Implements a simple application-level protocol over UDP used to fetch
 * large files (model weights, tokenizer) from a remote server. The client
 * performs three main operations:
 *  - META_REQ / META_RESP to learn file size, chunk count and expected SHA-256
 *  - DATA_RANGE_REQ to request contiguous ranges of chunks
 *  - RETRANS_REQ to explicitly request retransmission of missing chunks
 *
 * The client reassembles the file from fixed-size chunks, verifies integrity
 * with SHA-256, and returns an allocated buffer containing the file on
 * success. This module is intended to run inside xv6 userland for testing
 * the kernel UDP facilities.
 */

#include "kernel/types.h"
#include "user.h"
#include "ftpclient.h"
#include "sha256.h"

 /**
  * @struct transfer_ctx_t
  * @brief Context tracking an in-progress file transfer.
  *
  * @details
  * Holds the per-transfer mutable state:
  * - file_buf: malloc'd reassembly buffer for the whole file
  * - received: a per-chunk bitmap (one byte per chunk, 1 = received)
  * - file_size: total file size in bytes
  * - total_chunks: number of chunks expected
  * - file_sha256: expected SHA-256 digest (32 bytes)
  *
  * Memory lifecycle:
  * - Allocated by llm_fetch_file()
  * - Freed by caller after successful completion
  */
typedef struct {
  char* file_buf;           // malloc'd buffer for file data
  char* received;           // bitmap: 1 = chunk received, 0 = missing
  int file_size;
  int total_chunks;
  unsigned char file_sha256[32];  // Expected SHA-256
} transfer_ctx_t;

/**
 * @brief Check whether all chunks in a transfer context have been received.
 * @param ctx Pointer to transfer_ctx_t describing the transfer state.
 * @return 1 if all chunks are received, 0 otherwise.
 *
 * @note Simple linear scan of the received bitmap. Used to determine
 *       whether transfer is complete.
 */
static int all_chunks_received(transfer_ctx_t* ctx) {
  for (int i = 0; i < ctx->total_chunks; i++) {
    if (!ctx->received[i]) {
      return 0;
    }
  }
  return 1;
}

/**
 * @brief Count how many chunks are missing in [start, end).
 * @param ctx Pointer to transfer_ctx_t.
 * @param start Inclusive start index.
 * @param end Exclusive end index.
 * @return Number of missing chunks inside the given range.
 *
 * @note Range is clipped against total_chunks.
 */
static int count_missing_in_range(transfer_ctx_t* ctx, uint32_t start, uint32_t end) {
  int count = 0;
  for (uint32_t i = start; i < end && i < ctx->total_chunks; i++) {
    if (!ctx->received[i]) {
      count++;
    }
  }
  return count;
}

/**
 * @brief Populate an indices array with up to MAX_RETRANS missing chunk indices.
 * @param ctx Pointer to transfer_ctx_t.
 * @param start Inclusive start index.
 * @param end Exclusive end index.
 * @param indices Preallocated array to receive missing indices (caller-supplied).
 * @param count Out parameter set to the number of indices written.
 *
 * @details Writes at most MAX_RETRANS indices. Clips the search to total_chunks.
 */
static void get_missing_in_range(transfer_ctx_t* ctx, uint32_t start, uint32_t end, uint32_t* indices, int* count) {
  *count = 0;
  for (uint32_t i = start; i < end && i < ctx->total_chunks && *count < MAX_RETRANS; i++) {
    if (!ctx->received[i]) {
      indices[(*count)++] = i;
    }
  }
}

int llm_meta_request(uint8_t file_id, uint32_t* file_size, uint32_t* total_chunks, unsigned char* file_hash) {
  // Bind to a random port
  uint16_t port = 10000 + (getpid() % 1000); // Use PID to get somewhat unique port
  if (bind(port) < 0) {
    printf("Failed to bind to port %d\n", port);
    return -1;
  }

  // Prepare META_REQ message
  unsigned char req[4];
  req[0] = MSG_META_REQ;
  req[1] = file_id;
  req[2] = 0; // reserved
  req[3] = 0; // reserved

  // Send request
  if (send(port, SERVER_IP, SERVER_PORT, (char*)req, 4) < 0) {
    printf("Failed to send META_REQ\n");
    unbind(port);
    return -1;
  }
  printf("META_REQ sent, waiting for response...\n");

  // Receive response
  unsigned char resp[48];
  uint32 src_ip;
  uint16 src_port;
  int len = recv(port, &src_ip, &src_port, (char*)resp, sizeof(resp));

  if (len < 48 || resp[0] != MSG_META_RESP) {
    printf("Invalid META_RESP: len=%d, type=%d\n", len, resp[0]);
    unbind(port);
    return -1;
  }

  // Parse META_RESP (big-endian)
  *file_size = (resp[4] << 24) | (resp[5] << 16) | (resp[6] << 8) | resp[7];
  // chunk_size = (resp[8] << 24) | (resp[9] << 16) | (resp[10] << 8) | resp[11]; // Should be 512
  *total_chunks = (resp[12] << 24) | (resp[13] << 16) | (resp[14] << 8) | resp[15];

  // Copy SHA-256 hash
  for (int i = 0; i < 32; i++) {
    file_hash[i] = resp[16 + i];
  }
  unbind(port);
  return 0;
}

int llm_data_range_request(uint8_t file_id, uint32_t start_idx, uint16_t count) {
  uint16_t port = 10000 + (getpid() % 1000);

  // Prepare DATA_RANGE_REQ message
  unsigned char req[12];
  req[0] = MSG_DATA_RANGE_REQ;
  req[1] = file_id;
  req[2] = (count >> 8) & 0xFF;  // big-endian
  req[3] = count & 0xFF;
  req[4] = 0; // reserved
  req[5] = 0;
  req[6] = 0;
  req[7] = 0;
  req[8] = (start_idx >> 24) & 0xFF;
  req[9] = (start_idx >> 16) & 0xFF;
  req[10] = (start_idx >> 8) & 0xFF;
  req[11] = start_idx & 0xFF;

  if (send(port, SERVER_IP, SERVER_PORT, (char*)req, 12) < 0) {
    printf("Failed to send DATA_RANGE_REQ\n");
    return -1;
  }

  return 0;
}


int llm_retrans_request(uint8_t file_id, uint32_t* indices, uint16_t count) {
  uint16_t port = 10000 + (getpid() % 1000);

  // Prepare RETRANS_REQ message
  int msg_size = 4 + 4 * count;
  unsigned char* req = malloc(msg_size);
  if (!req) {
    printf("Memory allocation failed for RETRANS_REQ\n");
    return -1;
  }

  req[0] = MSG_RETRANS_REQ;
  req[1] = file_id;
  req[2] = (count >> 8) & 0xFF;
  req[3] = count & 0xFF;

  // Add indices (big-endian)
  for (int i = 0; i < count; i++) {
    int offset = 4 + i * 4;
    req[offset] = (indices[i] >> 24) & 0xFF;
    req[offset + 1] = (indices[i] >> 16) & 0xFF;
    req[offset + 2] = (indices[i] >> 8) & 0xFF;
    req[offset + 3] = indices[i] & 0xFF;
  }

  int result = send(port, SERVER_IP, SERVER_PORT, (char*)req, msg_size);
  free(req);

  if (result < 0) {
    printf("Failed to send RETRANS_REQ\n");
    return -1;
  }

  return 0;
}

/**
 * @brief Count how many chunks have been received so far.
 * @param ctx Pointer to transfer_ctx_t.
 * @return Number of chunks that have been successfully received.
 */
static int count_received_chunks(transfer_ctx_t* ctx) {
  int count = 0;
  for (int i = 0; i < ctx->total_chunks; i++) {
    if (ctx->received[i]) count++;
  }
  return count;
}

/**
 * @brief Parse and process a received DATA_PACKET into transfer context buffer.
 * @param ctx Pointer to the transfer context.
 * @param packet Raw packet bytes received from recv().
 * @param len Length of the received packet in bytes.
 * @return -1 on malformed packet or error,
 *          0 if duplicate packet / already received,
 *          1 if a new chunk was processed successfully.
 *
 * @details
 * Expects packet[0] == MSG_DATA_PACKET, header fields are big-endian.
 * Validates bounds to avoid writing beyond the allocated file buffer.
 */
static int process_data_packet(transfer_ctx_t* ctx, unsigned char* packet, int len) {
  if (len < 12) return -1; // Too short

  if (packet[0] != MSG_DATA_PACKET) return -1; // Wrong message type

  // Parse header (big-endian)
  uint32_t chunk_idx = (packet[4] << 24) | (packet[5] << 16) | (packet[6] << 8) | packet[7];
  uint16_t payload_len = (packet[8] << 8) | packet[9];

  if (chunk_idx >= ctx->total_chunks) return -1; // Invalid chunk index
  if (len < 12 + payload_len) return -1; // Packet too short for claimed payload
  if (ctx->received[chunk_idx]) return 0; // Duplicate, but not an error

  // Calculate destination offset
  uint32_t offset = chunk_idx * CHUNK_SIZE;
  if (offset + payload_len > ctx->file_size) return -1; // Would write beyond file buffer

  // Copy payload to file buffer
  for (int i = 0; i < payload_len; i++) ctx->file_buf[offset + i] = packet[12 + i];

  // Mark as received
  ctx->received[chunk_idx] = 1;

  return 1; // Successfully processed new chunk
}


/**
 * @brief Request a range of chunks and receive responses.
 * @param ctx Pointer to transfer_ctx_t.
 * @param file_id File identifier for the request.
 * @param start_idx Starting chunk index.
 * @param count Number of chunks to request.
 * @param client_port Port to receive on.
 * @param attempt Current retry attempt number (for display).
 * @return Number of new chunks received in this batch.
 */
static int request_chunk_range(transfer_ctx_t* ctx, uint8_t file_id, uint32_t start_idx, uint16_t count, uint16_t client_port, int attempt) {
  // Skip if all chunks in this range are already received
  if (count_missing_in_range(ctx, start_idx, start_idx + count) == 0) {
    return 0;
  }

  if (llm_data_range_request(file_id, start_idx, count) < 0) {
    printf("Failed to request chunks %d-%d\n", start_idx, start_idx + count - 1);
    return 0;
  }

  int new_chunks = 0;

  // Receive and process packets
  for (int spins = 0; spins < MAX_RECEIVE_SPINS / 10; spins++) {
    unsigned char buffer[12 + CHUNK_SIZE];
    uint32 src_ip;
    uint16 src_port;

    int len = recv(client_port, &src_ip, &src_port, (char*)buffer, sizeof(buffer));
    if (len > 0) process_data_packet(ctx, buffer, len);

    // Early exit if we've received all chunks in this range
    if (count_missing_in_range(ctx, start_idx, start_idx + count) == 0) {
      break;
    }
  }

  return new_chunks;
}

/**
 * @brief Request retransmission of specific missing chunks.
 * @param ctx Pointer to transfer_ctx_t.
 * @param file_id File identifier.
 * @param client_port Port to receive on.
 * @return Number of chunks successfully retransmitted.
 */
static int handle_missing_chunks(transfer_ctx_t* ctx, uint8_t file_id, uint16_t client_port) {
  int total_recovered = 0;
  uint32_t missing_indices[MAX_RETRANS];
  int missing_count;

  for (uint32_t start = 0; start < ctx->total_chunks; start += MAX_RETRANS) {
    get_missing_in_range(ctx, start, start + MAX_RETRANS, missing_indices, &missing_count);

    if (missing_count > 0) {
      printf("\nRequesting retransmission of %d missing chunks...", missing_count);

      if (llm_retrans_request(file_id, missing_indices, missing_count) == 0) {
        // Receive retransmitted packets
        for (int spins = 0; spins < MAX_RECEIVE_SPINS / 10; spins++) {
          unsigned char buffer[12 + CHUNK_SIZE];
          uint32 src_ip;
          uint16 src_port;

          int len = recv(client_port, &src_ip, &src_port, (char*)buffer, sizeof(buffer));
          if (len > 0) {
            int result = process_data_packet(ctx, buffer, len);
            if (result == 1) total_recovered++;
          }

          // Check if we've received all requested retransmissions
          int still_missing = 0;
          for (int i = 0; i < missing_count; i++) {
            if (!ctx->received[missing_indices[i]]) {
              still_missing = 1;
              break;
            }
          }
          if (!still_missing) break;
        }
      }
    }
  }

  return total_recovered;
}

/**
 * @brief Verify file integrity using SHA-256.
 * @param file_buf Buffer containing the complete file data.
 * @param file_size Size of the file in bytes.
 * @param expected_hash Expected SHA-256 hash (32 bytes).
 * @return 1 if hash matches, 0 otherwise.
 */
static int verify_file_integrity(char* file_buf, int file_size, unsigned char* expected_hash) {
  unsigned char computed_hash[32];

  sha256_hash((unsigned char*)file_buf, file_size, computed_hash);

  if (memcmp(computed_hash, expected_hash, 32) != 0) {
    // print the hash
    char hex[65];
    sha256_to_hex(computed_hash, hex);
    printf("Computed SHA-256: %s\n", hex);
    sha256_to_hex(expected_hash, hex);
    printf("Expected SHA-256: %s\n", hex);
    return 0; // Hash mismatch
  }

  return 1; // Hash matches
}

/**
 * @brief Main file transfer function - fetches a file from the LLM server.
 * @param file_id File identifier (FILE_WEIGHTS or FILE_TOKENIZER).
 * @param size_out Output parameter for file size.
 * @return Pointer to allocated file buffer on success, NULL on failure.
 *
 * @details
 * Transfer process:
 * 1. Request metadata (file size, chunk count, expected hash)
 * 2. Request all chunks in batches
 * 3. If chunks missing, retry with retransmission requests (up to MAX_RETRY_ROUNDS)
 * 4. Verify integrity with SHA-256
 *
 * Caller must free() the returned buffer.
 */
char* llm_fetch_file(uint8_t file_id, int* size_out) {
  uint32_t file_size, total_chunks;
  unsigned char expected_hash[32];
  transfer_ctx_t ctx;
  uint16_t client_port = 10000 + (getpid() % 1000);

  printf("Starting file transfer for file_id=%d\n", file_id);

  // Step 1: Request metadata
  if (llm_meta_request(file_id, &file_size, &total_chunks, expected_hash) < 0) {
    printf("Failed to get file metadata\n");
    return 0;
  }

  printf("File metadata: size=%d, total_chunks=%d\n", file_size, total_chunks);

  // Allocate transfer context
  ctx.file_buf = malloc(file_size);
  ctx.received = malloc(total_chunks);
  ctx.file_size = file_size;
  ctx.total_chunks = total_chunks;
  memcpy(ctx.file_sha256, expected_hash, 32);

  if (!ctx.file_buf || !ctx.received) {
    printf("Memory allocation failed\n");
    if (ctx.file_buf) free(ctx.file_buf);
    if (ctx.received) free(ctx.received);
    return 0;
  }

  // Initialize received bitmap
  for (int i = 0; i < total_chunks; i++) ctx.received[i] = 0;

  // Bind to client port for receiving
  if (bind(client_port) < 0) {
    printf("Failed to bind to port %d\n", client_port);
    free(ctx.file_buf);
    free(ctx.received);
    return 0;
  }

  // Step 2: Request all chunks in batches
  printf("[Attempt 1] Requesting %d chunks...\n", total_chunks);
  for (uint32_t start_idx = 0; start_idx < total_chunks; start_idx += MAX_RANGE) {
    uint16_t count = (start_idx + MAX_RANGE > total_chunks) ?
      (total_chunks - start_idx) : MAX_RANGE;

    request_chunk_range(&ctx, file_id, start_idx, count, client_port, 1);
  }

  // Step 3: Retry with retransmission if chunks are missing
  int attempt = 2;
  while (!all_chunks_received(&ctx) && attempt <= MAX_RETRY_ROUNDS) {
    printf("\n[Attempt %d] Retrying missing chunks...", attempt);
    handle_missing_chunks(&ctx, file_id, client_port);
    attempt++;
  }

  if (!all_chunks_received(&ctx)) {
    int missing = count_received_chunks(&ctx);
    printf("Transfer failed: received only %d/%d chunks after %d attempts\n", missing, total_chunks, MAX_RETRY_ROUNDS);
    unbind(client_port);
    free(ctx.file_buf);
    free(ctx.received);
    return 0;
  }

  // Step 4: Verify integrity
  printf("All chunks received. Verifying integrity...\n");

  if (!verify_file_integrity(ctx.file_buf, file_size, expected_hash)) {
    printf("SHA-256 verification failed!\n");
    unbind(client_port);
    free(ctx.file_buf);
    free(ctx.received);
    return 0;
  }

  printf("File transfer completed successfully\n");
  unbind(client_port);
  free(ctx.received);
  *size_out = file_size;
  return ctx.file_buf;
}

// LLM-specific wrapper functions
char* fetch_model_weights(int* size_out) {
  printf("Fetching model weights (stories15M.bin)...\n");
  return llm_fetch_file(FILE_WEIGHTS, size_out);
}

char* fetch_tokenizer(int* size_out) {
  printf("Fetching tokenizer (tokenizer.bin)...\n");
  return llm_fetch_file(FILE_TOKENIZER, size_out);
}

