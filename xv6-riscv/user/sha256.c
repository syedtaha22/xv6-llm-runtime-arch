/**
 * @file sha256.c
 * @brief Minimal SHA-256 implementation for xv6 user space
 *
 * This file implements the SHA-256 hashing algorithm following the FIPS-180-4
 * specification. It contains the three main API functions exposed in
 * sha256.h:
 *
 *   - sha256_init()   — Initialize the hashing context
 *   - sha256_update() — Feed data to the hash function
 *   - sha256_final()  — Finalize the digest
 *
 * The implementation is written to be fully portable inside xv6:
 * no stdlib, no big-endian helpers, no dynamic memory
 */

#include "sha256.h"

/**
 * @def ROTLEFT(a,b)
 * @brief Rotate a 32-bit integer left by b bits
 */
#define ROTLEFT(a,b)  (((a) << (b)) | ((a) >> (32 - (b))))

/**
 * @def ROTRIGHT(a,b)
 * @brief Rotate a 32-bit integer right by b bits
 */
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32 - (b))))

/**
 * @def CH(x,y,z)
 * @brief SHA-256 "choose" function. Selects bits from y or z based on x
 */
#define CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))

/**
 * @def MAJ(x,y,z)
 * @brief SHA-256 "majority" function. Bitwise majority among x,y,z
 */
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/**
 * @def EP0(x)
 * @brief High-level mixing function Σ0 used in each compression round
 */
#define EP0(x) (ROTRIGHT(x,2)  ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))

/**
 * @def EP1(x)
 * @brief High-level mixing function Σ1 used in each compression round
 */
#define EP1(x) (ROTRIGHT(x,6)  ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))

/**
 * @def SIG0(x)
 * @brief σ0 message schedule function (rotations + shifts)
 */
#define SIG0(x)(ROTRIGHT(x,7)  ^ ROTRIGHT(x,18) ^ ((x) >> 3))

/**
 * @def SIG1(x)
 * @brief σ1 message schedule function (rotations + shifts)
 */
#define SIG1(x)(ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))


/**
 * @brief SHA-256 round constants from FIPS-180-4
 *
 * These 32-bit constants are added during each of the 64 compression rounds
 */
static const WORD k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};


/**
 * @brief Core SHA-256 compression function
 *
 * Processes exactly one 512-bit block (64 bytes) from @p data, updating the
 * internal working state stored in @p ctx
 *
 * @param ctx  Pointer to SHA-256 context whose state will be updated
 * @param data A 64-byte block of message data
 *
 * @note This function is called internally by sha256_update() and sha256_final()
 *       Users should never call it directly
 */
void sha256_transform(SHA256_CTX *ctx, const BYTE data[]) {
  WORD a, b, c, d, e, f, g, h, t1, t2, m[64];
  WORD i, j;

  // Load first 16 words (big endian)
  for (i = 0, j = 0; i < 16; ++i, j += 4) {
    m[i] = (data[j] << 24) |
           (data[j+1] << 16) |
           (data[j+2] <<  8) |
           (data[j+3]);
  }

  // Extend message schedule to 64 words
  for (; i < 64; ++i) {
    m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];
  }

  // Initialize working variables
  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];

  // The 64 compression rounds
  for (i = 0; i < 64; ++i) {
    t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
    t2 = EP0(a) + MAJ(a,b,c);

    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  // Add results back to the context state
  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

/**
 * @brief Initialize the SHA-256 hashing context
 *
 * Sets the starting hash values (initial vector) and resets internal counters
 *
 * @param ctx Pointer to an allocated SHA256_CTX structure
 */
void sha256_init(SHA256_CTX *ctx) {
  ctx->datalen = 0;
  ctx->bitlen  = 0;

  // Initial hash values (H0..H7)
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

/**
 * @brief Feed data into the SHA-256 state
 *
 * Accepts arbitrary-length message data Internally buffers bytes until
 * a 64-byte block is full, at which point sha256_transform() is called
 *
 * @param ctx  Pointer to initialized SHA-256 context
 * @param data Byte array with message data
 * @param len  Number of bytes in @p data
 */
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len) {
  WORD i;

  for (i = 0; i < len; ++i) {
    ctx->data[ctx->datalen] = data[i];
    ctx->datalen++;

    // Process full block
    if (ctx->datalen == 64) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

/**
 * @brief Finalize the SHA-256 digest and write the output hash
 *
 * Performs padding, processes any remaining data, and writes the
 * final 32-byte hash into @p hash in big-endian order
 *
 * @param ctx   Pointer to SHA-256 context
 * @param hash  Output buffer of 32 bytes
 *
 * @note After calling this the context should not be reused unless re-initialized
 */
void sha256_final(SHA256_CTX *ctx, BYTE hash[]) {
  WORD i = ctx->datalen;

  // Append 0x80 byte 
  ctx->data[i++] = 0x80;

  // Pad with zeros to 56 bytes
  if (ctx->datalen < 56) {
    while (i < 56) ctx->data[i++] = 0x00;
  } else {
    while (i < 64) ctx->data[i++] = 0x00;
    sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }

  // Final message length in bits (big endian)
  ctx->bitlen += ctx->datalen * 8;
  ctx->data[63] = ctx->bitlen;
  ctx->data[62] = ctx->bitlen >> 8;
  ctx->data[61] = ctx->bitlen >> 16;
  ctx->data[60] = ctx->bitlen >> 24;
  ctx->data[59] = ctx->bitlen >> 32;
  ctx->data[58] = ctx->bitlen >> 40;
  ctx->data[57] = ctx->bitlen >> 48;
  ctx->data[56] = ctx->bitlen >> 56;

  sha256_transform(ctx, ctx->data);

  // Convert state to final hash (big endian)
  for (i = 0; i < 4; ++i) {
    hash[i]      = (ctx->state[0] >> (24 - i*8)) & 0xff;
    hash[i + 4]  = (ctx->state[1] >> (24 - i*8)) & 0xff;
    hash[i + 8]  = (ctx->state[2] >> (24 - i*8)) & 0xff;
    hash[i + 12] = (ctx->state[3] >> (24 - i*8)) & 0xff;
    hash[i + 16] = (ctx->state[4] >> (24 - i*8)) & 0xff;
    hash[i + 20] = (ctx->state[5] >> (24 - i*8)) & 0xff;
    hash[i + 24] = (ctx->state[6] >> (24 - i*8)) & 0xff;
    hash[i + 28] = (ctx->state[7] >> (24 - i*8)) & 0xff;
  }
}
