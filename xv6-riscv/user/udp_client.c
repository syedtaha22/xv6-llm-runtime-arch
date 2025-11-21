#include "types.h"
#include "user.h"
#include "udp_client.h"
#include "sha256.h"

// Transfer context structure
typedef struct {
    char *file_buf;           // malloc'd buffer for file data
    char *received;           // bitmap: 1 = chunk received, 0 = missing
    int file_size;
    int total_chunks;
    unsigned char file_sha256[32];  // Expected SHA-256
} transfer_ctx_t;

// Helper function to check if all chunks are received
static int all_chunks_received(transfer_ctx_t *ctx) {
    for (int i = 0; i < ctx->total_chunks; i++) {
        if (!ctx->received[i]) {
            return 0;
        }
    }
    return 1;
}

// Count missing chunks in a range
static int count_missing_in_range(transfer_ctx_t *ctx, uint32_t start, uint32_t end) {
    int count = 0;
    for (uint32_t i = start; i < end && i < ctx->total_chunks; i++) {
        if (!ctx->received[i]) {
            count++;
        }
    }
    return count;
}

// Get missing chunk indices in a range
static void get_missing_in_range(transfer_ctx_t *ctx, uint32_t start, uint32_t end, 
                                uint32_t *indices, int *count) {
    *count = 0;
    for (uint32_t i = start; i < end && i < ctx->total_chunks && *count < MAX_RETRANS; i++) {
        if (!ctx->received[i]) {
            indices[(*count)++] = i;
        }
    }
}

// Send META_REQ and parse META_RESP
int llm_meta_request(uint8_t file_id, uint32_t *file_size, uint32_t *total_chunks, unsigned char *file_hash) {
    // Bind to a random port
    short port = 10000 + (getpid() % 1000); // Use PID to get somewhat unique port
    if (bind(port) < 0) {
        printf( "Failed to bind to port %d\n", port);
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
        printf( "Failed to send META_REQ\n");
        return -1;
    }
    
    // Receive response
    unsigned char resp[48];
    int src_ip;
    short src_port;
    int len = recv(port, &src_ip, &src_port, (char*)resp, sizeof(resp));
    
    if (len < 48 || resp[0] != MSG_META_RESP) {
        printf( "Invalid META_RESP: len=%d, type=%d\n", len, resp[0]);
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
    
    return 0;
}

// Send DATA_RANGE_REQ
int llm_data_range_request(uint8_t file_id, uint32_t start_idx, uint16_t count) {
    short port = 10000 + (getpid() % 1000);
    
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
        printf( "Failed to send DATA_RANGE_REQ\n");
        return -1;
    }
    
    return 0;
}

// Send RETRANS_REQ for specific chunk indices
int llm_retrans_request(uint8_t file_id, uint32_t *indices, uint16_t count) {
    short port = 10000 + (getpid() % 1000);
    
    // Prepare RETRANS_REQ message
    int msg_size = 4 + 4 * count;
    unsigned char *req = malloc(msg_size);
    if (!req) {
        printf( "Memory allocation failed for RETRANS_REQ\n");
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
        printf( "Failed to send RETRANS_REQ\n");
        return -1;
    }
    
    return 0;
}

// Process incoming DATA_PACKET
static int process_data_packet(transfer_ctx_t *ctx, unsigned char *packet, int len) {
    if (len < 12) {
        return -1; // Too short
    }
    
    if (packet[0] != MSG_DATA_PACKET) {
        return -1; // Wrong message type
    }
    
    // Parse header (big-endian)
    uint32_t chunk_idx = (packet[4] << 24) | (packet[5] << 16) | (packet[6] << 8) | packet[7];
    uint16_t payload_len = (packet[8] << 8) | packet[9];
    
    if (chunk_idx >= ctx->total_chunks) {
        return -1; // Invalid chunk index
    }
    
    if (len < 12 + payload_len) {
        return -1; // Packet too short for claimed payload
    }
    
    // Check if we already have this chunk
    if (ctx->received[chunk_idx]) {
        return 0; // Duplicate, but not an error
    }
    
    // Calculate destination offset
    uint32_t offset = chunk_idx * CHUNK_SIZE;
    if (offset + payload_len > ctx->file_size) {
        return -1; // Would write beyond file buffer
    }
    
    // Copy payload to file buffer
    for (int i = 0; i < payload_len; i++) {
        ctx->file_buf[offset + i] = packet[12 + i];
    }
    
    // Mark as received
    ctx->received[chunk_idx] = 1;
    
    return 1; // Successfully processed new chunk
}

// Base protocol function to fetch a complete file
char* llm_fetch_file(uint8_t file_id, int *size_out) {
    uint32_t file_size, total_chunks;
    unsigned char expected_hash[32];
    transfer_ctx_t ctx;
    short client_port = 10000 + (getpid() % 1000);
    
    printf( "Starting file transfer for file_id=%d\n", file_id);
    
    // Step 1: Request metadata
    if (llm_meta_request(file_id, &file_size, &total_chunks, expected_hash) < 0) {
        printf( "Failed to get file metadata\n");
        return 0;
    }
    
    printf( "File metadata: size=%d, total_chunks=%d\n", file_size, total_chunks);
    
    // Allocate buffers
    ctx.file_buf = malloc(file_size);
    ctx.received = malloc(total_chunks);
    ctx.file_size = file_size;
    ctx.total_chunks = total_chunks;
    memcpy(ctx.file_sha256, expected_hash, 32);
    
    if (!ctx.file_buf || !ctx.received) {
        printf( "Memory allocation failed\n");
        if (ctx.file_buf) free(ctx.file_buf);
        if (ctx.received) free(ctx.received);
        return 0;
    }
    
    // Initialize received bitmap to 0
    for (int i = 0; i < total_chunks; i++) {
        ctx.received[i] = 0;
    }
    
    // Bind to our port for receiving
    if (bind(client_port) < 0) {
        printf( "Failed to bind to port %d\n", client_port);
        free(ctx.file_buf);
        free(ctx.received);
        return 0;
    }
    
    // Step 2: Batch request loop
    int retry_round = 0;
    while (!all_chunks_received(&ctx) && retry_round < MAX_RETRY_ROUNDS) {
        int received_count = 0;
        for (int i = 0; i < total_chunks; i++) {
            if (ctx.received[i]) received_count++;
        }
        printf( "Transfer round %d: %d/%d chunks received\n", 
               retry_round + 1, received_count, total_chunks);
        
        // Request chunks in batches
        for (uint32_t start_idx = 0; start_idx < total_chunks; start_idx += MAX_RANGE) {
            uint16_t count = (start_idx + MAX_RANGE > total_chunks) ? 
                            (total_chunks - start_idx) : MAX_RANGE;
            
            // Skip if all chunks in this range are already received
            if (count_missing_in_range(&ctx, start_idx, start_idx + count) == 0) {
                continue;
            }
            
            if (llm_data_range_request(file_id, start_idx, count) < 0) {
                printf( "Failed to request chunks %d-%d\n", start_idx, start_idx + count - 1);
                continue;
            }
            
            // Receive and process packets with timeout simulation
            int spins;
            for (spins = 0; spins < MAX_RECEIVE_SPINS / 10; spins++) {
                unsigned char buffer[12 + CHUNK_SIZE]; // Max DATA_PACKET size
                int src_ip;
                short src_port;
                
                int len = recv(client_port, &src_ip, &src_port, (char*)buffer, sizeof(buffer));
                if (len > 0) {
                    process_data_packet(&ctx, buffer, len);
                }
                
                // Early exit if we've received all chunks in this range
                if (count_missing_in_range(&ctx, start_idx, start_idx + count) == 0) {
                    break;
                }
            }
        }
        
        // Step 3: Handle any remaining missing chunks with retransmission
        uint32_t missing_indices[MAX_RETRANS];
        int missing_count;
        
        for (uint32_t start = 0; start < total_chunks; start += MAX_RETRANS) {
            get_missing_in_range(&ctx, start, start + MAX_RETRANS, missing_indices, &missing_count);
            
            if (missing_count > 0) {
                printf( "Requesting retransmission of %d missing chunks\n", missing_count);
                
                if (llm_retrans_request(file_id, missing_indices, missing_count) == 0) {
                    // Receive retransmitted packets
                    for (int spins = 0; spins < MAX_RECEIVE_SPINS / 10; spins++) {
                        unsigned char buffer[12 + CHUNK_SIZE];
                        int src_ip;
                        short src_port;
                        
                        int len = recv(client_port, &src_ip, &src_port, (char*)buffer, sizeof(buffer));
                        if (len > 0) {
                            process_data_packet(&ctx, buffer, len);
                        }
                        
                        // Check if we've received all requested retransmissions
                        int still_missing = 0;
                        for (int i = 0; i < missing_count; i++) {
                            if (!ctx.received[missing_indices[i]]) {
                                still_missing = 1;
                                break;
                            }
                        }
                        if (!still_missing) break;
                    }
                }
            }
        }
        
        retry_round++;
    }
    
    if (!all_chunks_received(&ctx)) {
        int missing = 0;
        for (int i = 0; i < total_chunks; i++) {
            if (!ctx.received[i]) missing++;
        }
        printf( "Failed to receive all chunks after %d rounds (%d missing)\n", 
               MAX_RETRY_ROUNDS, missing);
        free(ctx.file_buf);
        free(ctx.received);
        return 0;
    }
    
    // Step 4: Integrity verification
    printf( "All chunks received. Verifying integrity...\n");
    
    unsigned char computed_hash[32];
    SHA256_CTX sha_ctx;
    sha256_init(&sha_ctx);
    sha256_update(&sha_ctx, (unsigned char*)ctx.file_buf, file_size);
    sha256_final(&sha_ctx, computed_hash);
    
    if (memcmp(computed_hash, expected_hash, 32) != 0) {
        printf( "SHA-256 verification failed!\n");
        free(ctx.file_buf);
        free(ctx.received);
        return 0;
    }
    
    printf( "File transfer completed successfully\n");
    free(ctx.received);
    *size_out = file_size;
    return ctx.file_buf;
}

// LLM-specific wrapper functions
char* fetch_model_weights(int *size_out) {
    printf( "Fetching model weights (stories15M.bin)...\n");
    return llm_fetch_file(FILE_WEIGHTS, size_out);
}

char* fetch_tokenizer(int *size_out) {
    printf( "Fetching tokenizer (tokenizer.bin)...\n");
    return llm_fetch_file(FILE_TOKENIZER, size_out);
}

// Test main function
int main(int argc, char *argv[]) {
    int weights_size, tokenizer_size;
    char *weights, *tokenizer;
    
    printf( "Starting LLM file transfer test...\n");
    
    // Fetch model weights
    weights = fetch_model_weights(&weights_size);
    if (weights) {
        printf( "Successfully fetched weights: %d bytes\n", weights_size);
        
        // Verify the hash again for good measure
        unsigned char hash[32];
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (unsigned char*)weights, weights_size);
        sha256_final(&ctx, hash);
        
        printf( "Final SHA-256: ");
        for (int i = 0; i < 32; i++) {
            printf( "%02x", hash[i] & 0xFF);
        }
        printf( "\n");
        
        free(weights);
    } else {
        printf( "Failed to fetch weights\n");
    }
    
    printf( "---\n");
    
    // Fetch tokenizer
    tokenizer = fetch_tokenizer(&tokenizer_size);
    if (tokenizer) {
        printf( "Successfully fetched tokenizer: %d bytes\n", tokenizer_size);
        
        // Verify the hash again for good measure
        unsigned char hash[32];
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, (unsigned char*)tokenizer, tokenizer_size);
        sha256_final(&ctx, hash);
        
        printf( "Final SHA-256: ");
        for (int i = 0; i < 32; i++) {
            printf( "%02x", hash[i] & 0xFF);
        }
        printf( "\n");
        
        free(tokenizer);
    } else {
        printf( "Failed to fetch tokenizer\n");
    }
    
    printf( "LLM file transfer test completed\n");
    exit(0);
}