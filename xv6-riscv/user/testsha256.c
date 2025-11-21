#include "kernel/types.h"
#include "user.h"
#include "sha256.h"

#define GREEN "\033[1;32m"
#define RED   "\033[1;31m"
#define RESET "\033[0m"

// Convert two hex chars → byte
static BYTE hex_to_byte(char high, char low) {
  BYTE h = (high <= '9') ? high - '0' : (high - 'a' + 10);
  BYTE l = (low <= '9') ? low - '0' : (low - 'a' + 10);
  return (h << 4) | l;
}

// Convert 64-char hex string → 32-byte array to compare generated with expected
void hex_to_bytes(const char *hex, BYTE out[32]) {
  for (int i = 0; i < 32; i++) {
    out[i] = hex_to_byte(hex[2*i], hex[2*i+1]);
  }
}

void print_hash(BYTE hash[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 16) printf("0");
    printf("%x", hash[i]);
  }
  printf("\n");
}

int hashes_equal(BYTE h1[32], BYTE h2[32]) {
  for (int i = 0; i < 32; i++)
    if (h1[i] != h2[i])
      return 0;
  return 1;
}

void run_test(char *title, char *msg, char *expected_hex) {

  SHA256_CTX ctx;
  BYTE hash[32];
  BYTE expected[32];

  // compute actual hash
  sha256_init(&ctx);
  sha256_update(&ctx, (BYTE*)msg, strlen(msg));
  sha256_final(&ctx, hash);

  // convert expected from hex string → bytes
  hex_to_bytes(expected_hex, expected);

  printf("=== %s ===\n", title);
  printf("Input: \"%s\"\n", msg);
  printf("SHA256: ");
  print_hash(hash);

  if (hashes_equal(hash, expected)) {
    printf(GREEN "PASS\n" RESET);
  }
  else {
    printf(RED "FAIL\n" RESET);
  }

  printf("\n");
}

int main() {

  // 1. Empty string
  run_test("Test 1: Empty string", "",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  // 2. Single character
  run_test("Test 2: Single character", "a",
    "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");

  // 3. Short string
  run_test("Test 3: \"hello world\"", "hello world",
    "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");

  // 4. Block-boundary (62 bytes)
  char *block_boundary =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
    "QRSTUVWXYZ0123456";

  run_test("Test 4: Block-boundary string", block_boundary,
    "60d0ba2d3510c243f1b619dac382d6a7dee50eb02f871e59c1066f728c7bd802");

  // 5. Multi-block (>64 bytes)
  char *multi_block =
    "The quick brown fox jumps over the lazy dog. "
    "This is a longer test string that spans multiple blocks.";

  run_test("Test 5: Multi-block string", multi_block,
    "65dab9c0a2772f0ea4654aabc5cb63c83a6ee018249ef5d104bed2ad7141a9e1");

  exit(0);
}

