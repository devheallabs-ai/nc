/*
 * nc_crypto.c — NC Cryptographic Primitives
 *
 * Pure C11 implementation of AES-256-GCM + SHA-256 + PBKDF2.
 * Zero external dependencies. Compiles on macOS, Linux, Windows.
 *
 * Why embedded (not OpenSSL)?
 *   1. Zero dependencies — NC engine is self-contained
 *   2. Cross-platform — same code on all OS
 *   3. Standard algorithms — FIPS 197, FIPS 180-4, NIST SP 800-38D
 *
 * NOVA key material is loaded at runtime from the NOVA runtime
 * plugin and is NOT included in this open-source file.
 *
 * Copyright 2026 DevHeal Labs AI. Apache-2.0 License.
 */

#include "nc_crypto.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════
 *  AES-256 S-Box (FIPS 197, Section 5.1.1)
 * ═══════════════════════════════════════════════════════════ */

static const uint8_t SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

/* AES round constants */
static const uint8_t RCON[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* ═══════════════════════════════════════════════════════════
 *  AES-256 Key Expansion
 * ═══════════════════════════════════════════════════════════ */

static uint32_t sub_word(uint32_t w) {
    return ((uint32_t)SBOX[(w>>24)&0xff]<<24) |
           ((uint32_t)SBOX[(w>>16)&0xff]<<16) |
           ((uint32_t)SBOX[(w>>8)&0xff]<<8)   |
           ((uint32_t)SBOX[w&0xff]);
}

static uint32_t rot_word(uint32_t w) {
    return (w << 8) | (w >> 24);
}

void nc_aes256_init(NcAesCtx *ctx, const uint8_t key[NC_AES_KEY_SIZE]) {
    uint32_t *rk = ctx->round_key;

    /* First 8 words directly from key */
    for (int i = 0; i < 8; i++) {
        rk[i] = ((uint32_t)key[4*i]<<24) | ((uint32_t)key[4*i+1]<<16) |
                 ((uint32_t)key[4*i+2]<<8) | (uint32_t)key[4*i+3];
    }

    /* Expand to 60 words */
    for (int i = 8; i < 60; i++) {
        uint32_t t = rk[i-1];
        if (i % 8 == 0) {
            t = sub_word(rot_word(t)) ^ ((uint32_t)RCON[i/8] << 24);
        } else if (i % 8 == 4) {
            t = sub_word(t);
        }
        rk[i] = rk[i-8] ^ t;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  AES-256 Encrypt Block (single 16-byte block)
 * ═══════════════════════════════════════════════════════════ */

/* Galois field multiplication tables for MixColumns */
static uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

void nc_aes256_encrypt_block(const NcAesCtx *ctx,
                              const uint8_t in[NC_AES_BLOCK_SIZE],
                              uint8_t out[NC_AES_BLOCK_SIZE]) {
    uint8_t state[4][4];
    const uint32_t *rk = ctx->round_key;

    /* Load state (column-major) */
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            state[r][c] = in[c*4+r];

    /* Initial AddRoundKey */
    for (int c = 0; c < 4; c++) {
        state[0][c] ^= (rk[c] >> 24) & 0xff;
        state[1][c] ^= (rk[c] >> 16) & 0xff;
        state[2][c] ^= (rk[c] >> 8) & 0xff;
        state[3][c] ^= rk[c] & 0xff;
    }

    /* 14 rounds for AES-256 */
    for (int round = 1; round <= NC_AES_ROUNDS; round++) {
        /* SubBytes */
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                state[r][c] = SBOX[state[r][c]];

        /* ShiftRows */
        uint8_t t;
        /* Row 1: shift left 1 */
        t = state[1][0]; state[1][0]=state[1][1]; state[1][1]=state[1][2];
        state[1][2]=state[1][3]; state[1][3]=t;
        /* Row 2: shift left 2 */
        t=state[2][0]; state[2][0]=state[2][2]; state[2][2]=t;
        t=state[2][1]; state[2][1]=state[2][3]; state[2][3]=t;
        /* Row 3: shift left 3 */
        t=state[3][3]; state[3][3]=state[3][2]; state[3][2]=state[3][1];
        state[3][1]=state[3][0]; state[3][0]=t;

        /* MixColumns (skip on last round) */
        if (round < NC_AES_ROUNDS) {
            for (int c = 0; c < 4; c++) {
                uint8_t a0=state[0][c], a1=state[1][c];
                uint8_t a2=state[2][c], a3=state[3][c];
                state[0][c] = gmul(a0,2) ^ gmul(a1,3) ^ a2 ^ a3;
                state[1][c] = a0 ^ gmul(a1,2) ^ gmul(a2,3) ^ a3;
                state[2][c] = a0 ^ a1 ^ gmul(a2,2) ^ gmul(a3,3);
                state[3][c] = gmul(a0,3) ^ a1 ^ a2 ^ gmul(a3,2);
            }
        }

        /* AddRoundKey */
        for (int c = 0; c < 4; c++) {
            uint32_t w = rk[round*4+c];
            state[0][c] ^= (w >> 24) & 0xff;
            state[1][c] ^= (w >> 16) & 0xff;
            state[2][c] ^= (w >> 8) & 0xff;
            state[3][c] ^= w & 0xff;
        }
    }

    /* Store state to output */
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            out[c*4+r] = state[r][c];
}

/* ═══════════════════════════════════════════════════════════
 *  SHA-256 (FIPS 180-4)
 * ═══════════════════════════════════════════════════════════ */

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32-(n))))
#define CH(x,y,z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)       (ROR32(x,2) ^ ROR32(x,13) ^ ROR32(x,22))
#define EP1(x)       (ROR32(x,6) ^ ROR32(x,11) ^ ROR32(x,25))
#define SIG0(x)      (ROR32(x,7) ^ ROR32(x,18) ^ ((x)>>3))
#define SIG1(x)      (ROR32(x,17) ^ ROR32(x,19) ^ ((x)>>10))

static void sha256_transform(NcSha256Ctx *ctx, const uint8_t data[64]) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)data[4*i]<<24) | ((uint32_t)data[4*i+1]<<16) |
               ((uint32_t)data[4*i+2]<<8) | (uint32_t)data[4*i+3];
    for (int i = 16; i < 64; i++)
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];

    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + K256[i] + w[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void nc_sha256_init(NcSha256Ctx *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

void nc_sha256_update(NcSha256Ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->buflen = 0;
        }
    }
}

void nc_sha256_final(NcSha256Ctx *ctx, uint8_t hash[NC_SHA256_SIZE]) {
    uint64_t total = ctx->bitcount + ctx->buflen * 8;
    ctx->buffer[ctx->buflen++] = 0x80;
    if (ctx->buflen > 56) {
        while (ctx->buflen < 64) ctx->buffer[ctx->buflen++] = 0;
        sha256_transform(ctx, ctx->buffer);
        ctx->buflen = 0;
    }
    while (ctx->buflen < 56) ctx->buffer[ctx->buflen++] = 0;
    for (int i = 7; i >= 0; i--)
        ctx->buffer[ctx->buflen++] = (uint8_t)(total >> (i*8));
    sha256_transform(ctx, ctx->buffer);

    for (int i = 0; i < 8; i++) {
        hash[4*i]   = (ctx->state[i] >> 24) & 0xff;
        hash[4*i+1] = (ctx->state[i] >> 16) & 0xff;
        hash[4*i+2] = (ctx->state[i] >> 8) & 0xff;
        hash[4*i+3] = ctx->state[i] & 0xff;
    }
}

void nc_sha256(const uint8_t *data, size_t len, uint8_t hash[NC_SHA256_SIZE]) {
    NcSha256Ctx ctx;
    nc_sha256_init(&ctx);
    nc_sha256_update(&ctx, data, len);
    nc_sha256_final(&ctx, hash);
}

/* ═══════════════════════════════════════════════════════════
 *  HMAC-SHA256
 * ═══════════════════════════════════════════════════════════ */

static void hmac_sha256(const uint8_t *key, size_t key_len,
                         const uint8_t *msg, size_t msg_len,
                         uint8_t out[NC_SHA256_SIZE]) {
    uint8_t k_pad[64];
    uint8_t k_hash[NC_SHA256_SIZE];
    NcSha256Ctx ctx;

    /* If key > 64 bytes, hash it first */
    if (key_len > 64) {
        nc_sha256(key, key_len, k_hash);
        key = k_hash;
        key_len = NC_SHA256_SIZE;
    }

    /* ipad */
    memset(k_pad, 0x36, 64);
    for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];

    nc_sha256_init(&ctx);
    nc_sha256_update(&ctx, k_pad, 64);
    nc_sha256_update(&ctx, msg, msg_len);
    uint8_t inner[NC_SHA256_SIZE];
    nc_sha256_final(&ctx, inner);

    /* opad */
    memset(k_pad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];

    nc_sha256_init(&ctx);
    nc_sha256_update(&ctx, k_pad, 64);
    nc_sha256_update(&ctx, inner, NC_SHA256_SIZE);
    nc_sha256_final(&ctx, out);

    /* Wipe sensitive data */
    memset(k_pad, 0, 64);
    memset(inner, 0, NC_SHA256_SIZE);
}

/* ═══════════════════════════════════════════════════════════
 *  PBKDF2-HMAC-SHA256 (RFC 8018)
 * ═══════════════════════════════════════════════════════════ */

void nc_pbkdf2_sha256(const uint8_t *password, size_t pass_len,
                       const uint8_t *salt, size_t salt_len,
                       uint32_t iterations,
                       uint8_t *out, size_t out_len) {
    uint32_t block = 1;
    size_t offset = 0;

    while (offset < out_len) {
        /* U1 = HMAC(password, salt || INT32_BE(block)) */
        size_t msg_len = salt_len + 4;
        uint8_t *msg = (uint8_t *)malloc(msg_len);
        if (!msg) return;
        memcpy(msg, salt, salt_len);
        msg[salt_len]   = (block >> 24) & 0xff;
        msg[salt_len+1] = (block >> 16) & 0xff;
        msg[salt_len+2] = (block >> 8) & 0xff;
        msg[salt_len+3] = block & 0xff;

        uint8_t U[NC_SHA256_SIZE], T[NC_SHA256_SIZE];
        hmac_sha256(password, pass_len, msg, msg_len, U);
        memcpy(T, U, NC_SHA256_SIZE);
        free(msg);

        /* U2..Uc: iterate and XOR */
        for (uint32_t i = 1; i < iterations; i++) {
            uint8_t prev[NC_SHA256_SIZE];
            memcpy(prev, U, NC_SHA256_SIZE);
            hmac_sha256(password, pass_len, prev, NC_SHA256_SIZE, U);
            for (int j = 0; j < NC_SHA256_SIZE; j++) T[j] ^= U[j];
        }

        /* Copy to output */
        size_t copy_len = out_len - offset;
        if (copy_len > NC_SHA256_SIZE) copy_len = NC_SHA256_SIZE;
        memcpy(out + offset, T, copy_len);
        offset += copy_len;
        block++;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  GCM — Galois/Counter Mode (NIST SP 800-38D)
 *
 *  GF(2^128) multiplication for GHASH.
 *  Uses the "schoolbook" method. Not constant-time
 *  (timing side-channels not a concern for file encryption).
 * ═══════════════════════════════════════════════════════════ */

/* 128-bit block as two 64-bit halves */
typedef struct { uint64_t hi, lo; } Block128;

static Block128 block_from_bytes(const uint8_t b[16]) {
    Block128 r;
    r.hi = ((uint64_t)b[0]<<56) | ((uint64_t)b[1]<<48) | ((uint64_t)b[2]<<40) |
           ((uint64_t)b[3]<<32) | ((uint64_t)b[4]<<24) | ((uint64_t)b[5]<<16) |
           ((uint64_t)b[6]<<8)  | (uint64_t)b[7];
    r.lo = ((uint64_t)b[8]<<56) | ((uint64_t)b[9]<<48) | ((uint64_t)b[10]<<40) |
           ((uint64_t)b[11]<<32) | ((uint64_t)b[12]<<24) | ((uint64_t)b[13]<<16) |
           ((uint64_t)b[14]<<8) | (uint64_t)b[15];
    return r;
}

static void block_to_bytes(Block128 b, uint8_t out[16]) {
    for (int i = 7; i >= 0; i--) { out[7-i] = (b.hi >> (i*8)) & 0xff; }
    for (int i = 7; i >= 0; i--) { out[15-i] = (b.lo >> (i*8)) & 0xff; }
}

static Block128 block_xor(Block128 a, Block128 b) {
    return (Block128){ a.hi ^ b.hi, a.lo ^ b.lo };
}

/* GF(2^128) multiplication with reduction polynomial x^128 + x^7 + x^2 + x + 1 */
static Block128 gf128_mul(Block128 X, Block128 Y) {
    Block128 Z = {0, 0};
    Block128 V = Y;

    for (int i = 0; i < 128; i++) {
        /* Check bit i of X (MSB first, GCM bit ordering) */
        uint64_t word = (i < 64) ? X.hi : X.lo;
        int bit = 63 - (i % 64);
        if ((word >> bit) & 1) {
            Z = block_xor(Z, V);
        }
        /* V = V >> 1 in GF(2^128) */
        int lsb = V.lo & 1;
        V.lo = (V.lo >> 1) | (V.hi << 63);
        V.hi >>= 1;
        if (lsb) {
            V.hi ^= 0xe100000000000000ULL; /* reduction: XOR with R */
        }
    }
    return Z;
}

/* GHASH: hash AAD and ciphertext under key H */
static void ghash(const uint8_t H[16],
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t *cipher, size_t cipher_len,
                   uint8_t out[16]) {
    Block128 h = block_from_bytes(H);
    Block128 y = {0, 0};
    uint8_t block[16];

    /* Process AAD */
    size_t i = 0;
    while (i < aad_len) {
        size_t n = aad_len - i;
        if (n > 16) n = 16;
        memset(block, 0, 16);
        memcpy(block, aad + i, n);
        y = block_xor(y, block_from_bytes(block));
        y = gf128_mul(y, h);
        i += n;
    }

    /* Process ciphertext */
    i = 0;
    while (i < cipher_len) {
        size_t n = cipher_len - i;
        if (n > 16) n = 16;
        memset(block, 0, 16);
        memcpy(block, cipher + i, n);
        y = block_xor(y, block_from_bytes(block));
        y = gf128_mul(y, h);
        i += n;
    }

    /* Length block: [aad_len_bits || cipher_len_bits] */
    uint64_t aad_bits = aad_len * 8;
    uint64_t ct_bits = cipher_len * 8;
    memset(block, 0, 16);
    for (int j = 7; j >= 0; j--) { block[7-j] = (aad_bits >> (j*8)) & 0xff; }
    for (int j = 7; j >= 0; j--) { block[15-j] = (ct_bits >> (j*8)) & 0xff; }
    y = block_xor(y, block_from_bytes(block));
    y = gf128_mul(y, h);

    block_to_bytes(y, out);
}

/* AES-CTR: encrypt/decrypt using counter mode */
static void aes_ctr(const NcAesCtx *aes, const uint8_t iv[NC_GCM_IV_SIZE],
                     uint32_t counter_start,
                     const uint8_t *in, size_t len, uint8_t *out) {
    uint8_t counter_block[16];
    uint8_t keystream[16];

    /* Initial counter block: IV || counter (big-endian 32-bit) */
    memcpy(counter_block, iv, NC_GCM_IV_SIZE);

    size_t offset = 0;
    uint32_t ctr = counter_start;
    while (offset < len) {
        counter_block[12] = (ctr >> 24) & 0xff;
        counter_block[13] = (ctr >> 16) & 0xff;
        counter_block[14] = (ctr >> 8) & 0xff;
        counter_block[15] = ctr & 0xff;

        nc_aes256_encrypt_block(aes, counter_block, keystream);

        size_t n = len - offset;
        if (n > 16) n = 16;
        for (size_t i = 0; i < n; i++) {
            out[offset + i] = in[offset + i] ^ keystream[i];
        }
        offset += n;
        ctr++;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  AES-256-GCM Encrypt
 * ═══════════════════════════════════════════════════════════ */

int nc_aes256_gcm_encrypt(const uint8_t key[NC_AES_KEY_SIZE],
                           const uint8_t iv[NC_GCM_IV_SIZE],
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plain, size_t plain_len,
                           uint8_t *cipher,
                           uint8_t tag[NC_GCM_TAG_SIZE]) {
    if (!key || !iv || !cipher || !tag) return -1;
    if (plain_len > 0 && !plain) return -1;

    NcAesCtx aes;
    nc_aes256_init(&aes, key);

    /* H = AES(K, 0^128) — hash subkey */
    uint8_t H[16] = {0};
    uint8_t zero_block[16] = {0};
    nc_aes256_encrypt_block(&aes, zero_block, H);

    /* Encrypt plaintext with CTR starting at counter=2 */
    if (plain_len > 0) {
        aes_ctr(&aes, iv, 2, plain, plain_len, cipher);
    }

    /* GHASH over AAD and ciphertext */
    uint8_t ghash_out[16];
    ghash(H, aad, aad ? aad_len : 0, cipher, plain_len, ghash_out);

    /* Tag = GHASH XOR AES(K, IV||0^31||1) — counter=1 */
    uint8_t j0[16];
    memcpy(j0, iv, NC_GCM_IV_SIZE);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
    uint8_t enc_j0[16];
    nc_aes256_encrypt_block(&aes, j0, enc_j0);
    for (int i = 0; i < 16; i++) tag[i] = ghash_out[i] ^ enc_j0[i];

    /* Wipe key schedule */
    memset(&aes, 0, sizeof(aes));
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  AES-256-GCM Decrypt
 * ═══════════════════════════════════════════════════════════ */

int nc_aes256_gcm_decrypt(const uint8_t key[NC_AES_KEY_SIZE],
                           const uint8_t iv[NC_GCM_IV_SIZE],
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *cipher, size_t cipher_len,
                           const uint8_t tag[NC_GCM_TAG_SIZE],
                           uint8_t *plain) {
    if (!key || !iv || !tag || !plain) return -1;
    if (cipher_len > 0 && !cipher) return -1;

    NcAesCtx aes;
    nc_aes256_init(&aes, key);

    /* H = AES(K, 0^128) */
    uint8_t H[16] = {0};
    uint8_t zero_block[16] = {0};
    nc_aes256_encrypt_block(&aes, zero_block, H);

    /* Compute expected tag BEFORE decrypting (authenticate first) */
    uint8_t ghash_out[16];
    ghash(H, aad, aad ? aad_len : 0, cipher, cipher_len, ghash_out);

    uint8_t j0[16];
    memcpy(j0, iv, NC_GCM_IV_SIZE);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
    uint8_t enc_j0[16];
    nc_aes256_encrypt_block(&aes, j0, enc_j0);

    uint8_t expected_tag[16];
    for (int i = 0; i < 16; i++) expected_tag[i] = ghash_out[i] ^ enc_j0[i];

    /* Constant-time tag comparison */
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= expected_tag[i] ^ tag[i];
    if (diff != 0) {
        memset(&aes, 0, sizeof(aes));
        return -1;  /* AUTHENTICATION FAILED — tampered or wrong key */
    }

    /* Decrypt ciphertext with CTR starting at counter=2 */
    if (cipher_len > 0) {
        aes_ctr(&aes, iv, 2, cipher, cipher_len, plain);
    }

    memset(&aes, 0, sizeof(aes));
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Secure Random Bytes
 *
 *  Uses OS-provided CSPRNG:
 *  - macOS/Linux: /dev/urandom
 *  - Windows: BCryptGenRandom
 * ═══════════════════════════════════════════════════════════ */

void nc_random_bytes(uint8_t *buf, size_t len) {
#ifdef _WIN32
    /* Windows: use BCryptGenRandom or RtlGenRandom */
    extern int __stdcall SystemFunction036(void *, unsigned long);
    SystemFunction036(buf, (unsigned long)len);
#else
    /* POSIX: /dev/urandom (always available, non-blocking) */
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(buf, 1, len, f);
        fclose(f);
        if (n == len) return;
    }
    /* Fallback: time-based PRNG (weak, but better than nothing) */
    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)buf;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (seed >> 16) & 0xff;
    }
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  NOVA Key Derivation — Plugin Interface
 *
 *  The NOVA master key is NOT included in the open-source build.
 *  Key material is loaded at runtime from the NOVA runtime plugin
 *  (nc_nova_runtime.dll / libncnovaruntime.so / libncnovaruntime.dylib).
 *
 *  To use NOVA encryption features, install the NOVA runtime:
 *    nc nova install
 *
 *  For enterprise/commercial licensing:
 *    https://nova.devheallabs.in
 * ═══════════════════════════════════════════════════════════ */

/* Function pointer for key provider loaded from NOVA runtime plugin */
typedef void (*nc_nova_key_provider_fn)(const uint8_t *salt, size_t salt_len,
                                         uint8_t *key_out, size_t key_len);

static nc_nova_key_provider_fn g_nova_key_provider = NULL;

/* ── Platform-specific dynamic loading ── */
#ifdef _WIN32
#include <windows.h>
static void nc_nova_load_runtime(void) {
    if (g_nova_key_provider) return;
    HMODULE lib = LoadLibraryA("nc_nova_runtime.dll");
    if (!lib) lib = LoadLibraryA("lib/nc_nova_runtime.dll");
    if (lib) {
        g_nova_key_provider = (nc_nova_key_provider_fn)GetProcAddress(lib, "nc_nova_derive_key_impl");
    }
}
#else
#include <dlfcn.h>
static void nc_nova_load_runtime(void) {
    if (g_nova_key_provider) return;
    void *lib = dlopen("libncnovaruntime.so", RTLD_LAZY);
    if (!lib) lib = dlopen("libncnovaruntime.dylib", RTLD_LAZY);
    if (!lib) lib = dlopen("./lib/libncnovaruntime.so", RTLD_LAZY);
    if (!lib) lib = dlopen("./lib/libncnovaruntime.dylib", RTLD_LAZY);
    if (lib) {
        g_nova_key_provider = (nc_nova_key_provider_fn)dlsym(lib, "nc_nova_derive_key_impl");
    }
}
#endif

void nc_nova_derive_key(const uint8_t salt[NC_PBKDF2_SALT_SIZE],
                         uint8_t key_out[NC_AES_KEY_SIZE]) {
    nc_nova_load_runtime();

    if (g_nova_key_provider) {
        /* Delegate to NOVA runtime plugin */
        g_nova_key_provider(salt, NC_PBKDF2_SALT_SIZE, key_out, NC_AES_KEY_SIZE);
    } else {
        fprintf(stderr,
            "\n  NOVA runtime not found.\n"
            "  NOVA encryption requires the NOVA runtime plugin.\n"
            "  Install it with: nc nova install\n"
            "  More info: https://nova.devheallabs.in\n\n");
        memset(key_out, 0, NC_AES_KEY_SIZE);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  NOVA File Encryption — High-Level API
 *
 *  File format (.nova v2):
 *    Magic:     8 bytes   "NOVAML02"
 *    Version:   4 bytes   uint32_le (2)
 *    Salt:      32 bytes  random (PBKDF2 salt)
 *    IV:        12 bytes  random (GCM nonce)
 *    Data len:  8 bytes   uint64_le (plaintext length)
 *    Tag:       16 bytes  GCM authentication tag
 *    Ciphertext: variable (encrypted payload)
 *
 *  Total header: 8 + 4 + 32 + 12 + 8 + 16 = 80 bytes
 * ═══════════════════════════════════════════════════════════ */

#define NOVA_V2_HEADER_SIZE 80

static void write_le32(uint8_t *p, uint32_t v) {
    p[0]=v&0xff; p[1]=(v>>8)&0xff; p[2]=(v>>16)&0xff; p[3]=(v>>24)&0xff;
}

static void write_le64(uint8_t *p, uint64_t v) {
    for (int i=0;i<8;i++) p[i]=(v>>(i*8))&0xff;
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static uint64_t read_le64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i=0;i<8;i++) v |= (uint64_t)p[i] << (i*8);
    return v;
}

int64_t nc_nova_encrypt(const uint8_t *data, size_t data_len, uint8_t **out) {
    if (!data || !out) return -1;

    size_t total = NOVA_V2_HEADER_SIZE + data_len;
    uint8_t *blob = (uint8_t *)malloc(total);
    if (!blob) return -1;

    /* Build header */
    memcpy(blob, NC_NOVA_MAGIC_V2, 8);                     /* Magic */
    write_le32(blob + 8, NC_NOVA_VERSION_2);                /* Version */
    nc_random_bytes(blob + 12, NC_PBKDF2_SALT_SIZE);       /* Salt */
    nc_random_bytes(blob + 44, NC_GCM_IV_SIZE);            /* IV */
    write_le64(blob + 56, (uint64_t)data_len);             /* Data length */
    /* Tag at offset 64, filled after encryption */

    /* Derive encryption key from master + salt */
    uint8_t key[NC_AES_KEY_SIZE];
    nc_nova_derive_key(blob + 12, key);

    /* Encrypt with AES-256-GCM */
    /* AAD = header bytes 0..55 (magic + version + salt + IV + length) */
    uint8_t tag[NC_GCM_TAG_SIZE];
    int rc = nc_aes256_gcm_encrypt(key, blob + 44,
                                    blob, 56,           /* AAD */
                                    data, data_len,
                                    blob + 80,          /* ciphertext */
                                    tag);

    memcpy(blob + 64, tag, NC_GCM_TAG_SIZE);           /* Store tag */

    memset(key, 0, NC_AES_KEY_SIZE);  /* Wipe key */

    if (rc != 0) { free(blob); return -1; }

    *out = blob;
    return (int64_t)total;
}

int64_t nc_nova_decrypt(const uint8_t *blob, size_t blob_len, uint8_t **out) {
    if (!blob || !out || blob_len < NOVA_V2_HEADER_SIZE) return -1;

    /* Verify magic */
    if (memcmp(blob, NC_NOVA_MAGIC_V2, 8) != 0) {
        fprintf(stderr, "NOVA: Invalid file (wrong magic)\n");
        return -1;
    }

    /* Verify version */
    uint32_t version = read_le32(blob + 8);
    if (version != NC_NOVA_VERSION_2) {
        fprintf(stderr, "NOVA: Unsupported format version %u\n", version);
        return -1;
    }

    /* Read header fields */
    const uint8_t *salt = blob + 12;
    const uint8_t *iv = blob + 44;
    uint64_t data_len = read_le64(blob + 56);
    const uint8_t *tag = blob + 64;
    const uint8_t *ciphertext = blob + 80;

    if (blob_len < NOVA_V2_HEADER_SIZE + data_len) {
        fprintf(stderr, "NOVA: File truncated\n");
        return -1;
    }

    /* Derive key */
    uint8_t key[NC_AES_KEY_SIZE];
    nc_nova_derive_key(salt, key);

    /* Decrypt and authenticate */
    uint8_t *plain = (uint8_t *)malloc((size_t)data_len);
    if (!plain) { memset(key, 0, NC_AES_KEY_SIZE); return -1; }

    int rc = nc_aes256_gcm_decrypt(key, iv,
                                    blob, 56,           /* AAD */
                                    ciphertext, (size_t)data_len,
                                    tag, plain);
    memset(key, 0, NC_AES_KEY_SIZE);

    if (rc != 0) {
        fprintf(stderr, "NOVA: Decryption failed — file tampered or wrong key\n");
        free(plain);
        return -1;
    }

    *out = plain;
    return (int64_t)data_len;
}
