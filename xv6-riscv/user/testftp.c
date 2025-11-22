/**
 * @file testftp.c
 * @author Hamna Sajid
 * @date 22nd November 2025
 * 
 * @brief Test harness for the UDP-based LLM file transfer client.
 * 
 * @details
 * This program tests the functionality of the UDP client by fetching
 * model weights and tokenizer files from a server, printing their sizes
 * and SHA-256 hashes for verification.
 */

#include "kernel/types.h"
#include "user.h"
#include "ftpclient.h"
#include "testutil.h"
#include "sha256.h"

/**
 * @brief Test harness main() that fetches weights and tokenizer via UDP.
 * @param argc Argument count (unused).
 * @param argv Argument vector (unused).
 * @return Exits with code 0 on completion.
 *
 * @details
 * Runs two consecutive transfers (weights and tokenizer), prints sizes and
 * SHA-256 hex digests for manual verification, and frees returned buffers.
 */
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