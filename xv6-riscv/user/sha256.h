#ifndef SHA256_H
#define SHA256_H

#include "kernel/types.h"
#include "user.h"

// ------------------------
// Basic integer typedefs
// ------------------------
typedef unsigned char  BYTE;   // 8-bit
typedef unsigned int   WORD;   // 32-bit
typedef unsigned int   size_t; // xv6 lacks size_t, so define it

// SHA256 context
typedef struct {
  BYTE data[64];   // current 512-bit chunk being processed
  WORD datalen;    // number of bytes in 'data'
  unsigned long long bitlen; // total message length in bits
  WORD state[8];    // internal hash state (a..h)
} SHA256_CTX;

// API functions
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);
void sha256_transform(SHA256_CTX *ctx, const BYTE data[]);

#endif
