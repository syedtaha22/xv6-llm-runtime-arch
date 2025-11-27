# SHA256 Syscall Implementation Summary

## Overview
This implementation adds SHA256 cryptographic hashing support to the xv6-riscv kernel via a new system call. The SHA256 functionality has been moved from user space to kernel space for security and efficiency, providing user programs with a syscall interface to compute SHA256 hashes of arbitrary data.

## Changes Made

### Kernel Space Additions
- **xv6-riscv/kernel/sha256.h**: Header file defining SHA256 structures and function prototypes
- **xv6-riscv/kernel/sha256.c**: Complete SHA256 implementation adapted for kernel environment
- **xv6-riscv/kernel/syscall.h**: Added SYS_sha256 = 35
- **xv6-riscv/kernel/syscall.c**: Added extern declaration and syscall table entry for sys_sha256
- **xv6-riscv/kernel/sysproc.c**: Implemented sys_sha256 function with incremental hashing for large buffers
- **xv6-riscv/Makefile**: Added kernel/sha256.o to OBJS for compilation

### User Space Modifications
- **xv6-riscv/user/user.h**: Added sha256() function declaration
- **xv6-riscv/user/usys.pl**: Added sha256 entry for syscall assembly generation
- **xv6-riscv/user/usys.S**: Regenerated to include sha256 syscall
- **xv6-riscv/user/testsha.c**: Modified to use syscall instead of user-space implementation
- **xv6-riscv/user/ftpclient.c**: Updated to use syscall for SHA256 hashing
- **xv6-riscv/user/testftp.c**: Adjusted for syscall usage
- **xv6-riscv/user/ulib.c**: Removed user-space SHA256 wrapper (syscall called directly)

### User Space Removals
- **xv6-riscv/user/sha256.c**: Deleted (moved to kernel)
- **xv6-riscv/user/sha256.h**: Deleted (moved to kernel)

## Technical Details

### Syscall Interface
- **Prototype**: `int sha256(const void *data, int len, void *hash)`
- **Parameters**:
  - `data`: Pointer to input data buffer
  - `len`: Length of input data in bytes
  - `hash`: Pointer to 32-byte output buffer for SHA256 digest
- **Return**: 0 on success, -1 on error

### Implementation Features
- Supports arbitrary data sizes through incremental hashing
- Uses kernel memory allocation (kalloc) for temporary buffers
- Handles large buffers efficiently by processing in 4096-byte chunks
- Maintains compatibility with standard SHA256 output

### Testing
- All existing SHA256 tests pass (9/9)
- Supports buffer sizes from 1 byte to 62MB
- Verified correct hash computation for various input sizes

## Security and Performance
- Hashing performed in kernel space prevents user-space tampering
- Incremental processing avoids large memory allocations
- Maintains cryptographic integrity of SHA256 algorithm

## Files Affected
- Added: 2 files
- Modified: 10 files  
- Deleted: 2 files
- Total: 14 files changed

## Verification
- Kernel compiles without errors
- All SHA256 tests pass
- Syscall handles large buffers correctly
- No kernel panics or memory issues