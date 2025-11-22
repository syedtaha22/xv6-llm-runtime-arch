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
#include "xstrlib.h"

/* Minimal hex helper that uses xsprintf */
static void bytes_to_hex(const unsigned char *in, int len, char *out) {
    for (int i = 0; i < len; i++) {
        xsprintf(out + i*2, "%02x", in[i] & 0xFF);
    }
    out[len*2] = '\0';
}

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
    char buf[256];
    char hex[65];
    
    /* set a global tag for testutil output */
    set_tag("LLMFTP");

    info(0, "Starting LLM file transfer test...");

    /* Fetch model weights */
    weights = fetch_model_weights(&weights_size);
    if (weights) {
        xsprintf(buf, "Successfully fetched weights: %d bytes", weights_size);
        pass(1, buf);

        /* Verify the hash again for good measure */
        unsigned char hash[32];
        SHA256_CTX ctx;

        sha256_init(&ctx);
        sha256_update(&ctx, (unsigned char*)weights, weights_size);
        sha256_final(&ctx, hash);

        /* build hex string */
        bytes_to_hex(hash, 32, hex);

        xsprintf(buf, "Final SHA-256: %s", hex);
        info(2, buf);;

        free(weights);
    } else {
        failnoex(1, "Failed to fetch weights");
    }

    info(0, "---");

    /* Fetch tokenizer */
    tokenizer = fetch_tokenizer(&tokenizer_size);
    if (tokenizer) {
        xsprintf(buf, "Successfully fetched tokenizer: %d bytes", tokenizer_size);
        pass(1, buf);

        /* Verify the hash again for good measure */
        unsigned char hash2[32];
        SHA256_CTX ctx2;

        sha256_init(&ctx2);
        sha256_update(&ctx2, (unsigned char*)tokenizer, tokenizer_size);
        sha256_final(&ctx2, hash2);

        bytes_to_hex(hash2, 32, hex);

        xsprintf(buf, "Final SHA-256: %s", hex);
        info(2, buf);

        free(tokenizer);
    } else {
        failnoex(1, "Failed to fetch tokenizer");
    }

    pass(0, "LLM file transfer test completed");
    exit(0);
}