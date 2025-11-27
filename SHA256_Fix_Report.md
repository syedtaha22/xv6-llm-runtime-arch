# SHA-256 Fix Report

## Issue Description

The xv6 LLM runtime architecture was experiencing SHA-256 verification failures specifically for large file transfers, particularly the model weights file (approximately 60MB). The tokenizer file (smaller) would succeed, but the weights file consistently failed SHA-256 integrity checks.

### Root Cause Analysis

Through comprehensive testing and debugging, the issue was isolated to the SHA-256 implementation itself, not network corruption or chunking problems. Key findings:

1. **Test Isolation**: Updated SHA-256 tests to be more comprehensive, including large buffer tests (up to 60MB).
2. **Debug Output**: Added debug statements showing internal state during hash computation.
3. **State Discrepancy**: Discovered that for identical input data, the SHA-256 context state would differ between passing and failing cases.

Example debug output showing the problem:

```
Test 4: Buffer size: 10485760 bytes
Debug: Final data block: 80000000000000000000000000000000000000000000000000000000000005000
State: ea70ec0e a4865988 361cca4 5ed16de 351793dd 9f7cd4c8 5159459 2fb00aa5 
Debug: Final bitlen: 83886080
Debug: Final datalen: 0
Debug: Final hash bytes: ea70ecea4865988361cca45ed16de351793dd9f7cd4c851594592fb0aa5
SHA256 buffer test
  Input length: 10485760 bytes
  SHA256: ea70ec0ea48659880361cca405ed16de351793dd9f7cd4c8051594592fb00aa5
  Computed: ea70ec0ea48659880361cca405ed16de351793dd9f7cd4c8051594592fb00aa5
  Expected: 6f22c2e43af8bc2d16b7666deffd9a4fb99d44284fe3caa69adbaddd247d8250
FAIL   Hash mismatch

Test 4: Buffer size: 10485760 bytes
Debug: Final data block: 80000000000000000000000000000000000000000000000000000000000005000
State: 6f22c2e4 3af8bc2d 16b7666d effd9a4f b99d4428 4fe3caa6 9adbaddd 247d8250 
Debug: Final bitlen: 83886080
Debug: Final datalen: 0
Debug: Final hash bytes: 6f22c2e43af8bc2d16b7666deffd9a4fb99d44284fe3caa69adbaddd247d8250
SHA256 buffer test
  Input length: 10485760 bytes
  SHA256: 6f22c2e43af8bc2d16b7666deffd9a4fb99d44284fe3caa69adbaddd247d8250
PASS   Hash matches expected value
```

Notice that the final data block is identical, but the internal state differs, leading to different final hashes.

## Solution

After extensive debugging that couldn't pinpoint the exact bug in the existing implementation, we replaced the SHA-256 implementation with a corrected version inspired by the Lucidar blog post (https://lucidar.me/en/dev-c-cpp/sha-256-in-c-cpp/).

### Key Changes

1. **New Implementation**: Adopted a more robust SHA-256 implementation with:
   - 16-element rotating message schedule (instead of 64-element)
   - Improved block processing logic
   - Better padding and finalization handling

2. **Code Structure**:
   - Added comprehensive docstrings to all functions
   - Improved type safety with xv6-compatible types
   - Cleaner separation of concerns with helper functions
   - Added `sha256_hash()` function for one-call hashing

3. **Testing**: Updated test suite to include large buffer tests that previously failed.

### Files Modified

- `xv6-riscv/user/sha256.c`: Complete rewrite of SHA-256 implementation, added `sha256_hash()` for convenience
- `xv6-riscv/user/sha256.h`: Updated header with better documentation, types, and new function declaration
- `xv6-riscv/user/ftpclient.c`: Updated to use `sha256_hash()` instead of three-step process
- `xv6-riscv/user/testftp.c`: Updated to use `sha256_hash()` instead of three-step process
- `xv6-riscv/user/testsha.c`: Updated to use `sha256_hash()` instead of three-step process
- `xv6-riscv/Makefile`: Renamed test program from `testsha256` to `testsha`
- `xv6-riscv/user/testsha256.c`: Removed (replaced with new test)

### Verification

The new implementation passes all tests, including the previously failing large buffer tests. File transfers now succeed with correct SHA-256 verification.

## Impact

- Large file transfers (weights) now succeed
- SHA-256 integrity verification works correctly
- All existing functionality preserved
- Improved code documentation and maintainability
- Added user-friendly `sha256_hash()` function for simplified usage

The fix resolves the core cryptographic issue that was preventing reliable large file transfers in the xv6 LLM runtime architecture.