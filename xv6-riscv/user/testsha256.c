#include "kernel/types.h"
#include "user.h"
#include "sha256.h"


void print_hash(BYTE hash[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 16) printf("0");   // manual zero padding because xsprintf doesn't handle width
    printf("%x", hash[i]);
  }
  printf("\n");
}

void run_test(char *title, char *msg) {
  SHA256_CTX ctx;
  BYTE hash[32];

  sha256_init(&ctx);
  sha256_update(&ctx, (BYTE*)msg, strlen(msg));
  sha256_final(&ctx, hash);

  printf("=== %s ===\n", title);
  printf("Input: \"%s\"\n", msg);
  printf("SHA256: ");
  print_hash(hash);
  printf("\n");
}

int main() {

  // 1. Empty string
  run_test("Test 1: Empty string", "");

  // 2. Single character
  run_test("Test 2: Single character", "a");

  // 3. Short string
  run_test("Test 3: \"hello world\"", "hello world");

  // 4. Block-boundary string 
  // this is 62 bytes (fits the boundary specification)
  char *block_boundary =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
    "QRSTUVWXYZ0123456";
  run_test("Test 4: Block-boundary string", block_boundary);

  // 5. Multi-block string (>64 bytes)
  char *multi_block =
    "The quick brown fox jumps over the lazy dog. "
    "This is a longer test string that spans multiple blocks.";
  run_test("Test 5: Multi-block string", multi_block);

  exit(0);
}

