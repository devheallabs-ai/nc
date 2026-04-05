/*
 * nc_crypto.h — NOVA Cryptographic Engine
 *
 * AES-256-GCM encryption for NOVA model files.
 * Pure C11, zero external dependencies, cross-platform.
 *
 * Architecture:
 *   - AES-256 block cipher (FIPS 197)
 *   - GCM authenticated encryption (NIST SP 800-38D)
 *   - SHA-256 hash (FIPS 180-4)
 *   - PBKDF2-HMAC-SHA256 key derivation (RFC 8018)
 *   - Master key obfuscated in compiled binary
 *
 * Security:
 *   - AES-256-GCM: authenticated encryption (confidentiality + integrity)
 *   - PBKDF2 with 100,000 iterations (brute-force resistant)
 *   - Random salt per file (no two files share keys)
 *   - GCM auth tag detects tampering (16 bytes)
 *   - Master key split + XOR'd in binary (resist static analysis)
 *
 * Patent: DevHeal Labs AI — NOVA Model Format
 * Copyright 2026 DevHeal Labs AI. All rights reserved.
 */

#ifndef NC_CRYPTO_H
#define NC_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  Constants
 * ═══════════════════════════════════════════════════════════ */

#define NC_AES_BLOCK_SIZE   16
#define NC_AES_KEY_SIZE     32   /* AES-256 */
#define NC_AES_ROUNDS       14   /* AES-256 uses 14 rounds */
#define NC_GCM_IV_SIZE      12
#define NC_GCM_TAG_SIZE     16
#define NC_SHA256_SIZE      32
#define NC_PBKDF2_SALT_SIZE 32
#define NC_PBKDF2_ITER      100000

/* NOVA format v2 */
#define NC_NOVA_MAGIC_V2    "NOVAML02"
#define NC_NOVA_VERSION_2   2

/* ═══════════════════════════════════════════════════════════
 *  AES-256 Context
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t round_key[60];  /* Expanded key schedule (15 * 4 words) */
} NcAesCtx;

/* ═══════════════════════════════════════════════════════════
 *  SHA-256 Context
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[64];
    uint32_t buflen;
} NcSha256Ctx;

/* ═══════════════════════════════════════════════════════════
 *  API — SHA-256
 * ═══════════════════════════════════════════════════════════ */

void nc_sha256_init(NcSha256Ctx *ctx);
void nc_sha256_update(NcSha256Ctx *ctx, const uint8_t *data, size_t len);
void nc_sha256_final(NcSha256Ctx *ctx, uint8_t hash[NC_SHA256_SIZE]);

/* One-shot SHA-256 */
void nc_sha256(const uint8_t *data, size_t len, uint8_t hash[NC_SHA256_SIZE]);

/* ═══════════════════════════════════════════════════════════
 *  API — PBKDF2-HMAC-SHA256
 * ═══════════════════════════════════════════════════════════ */

void nc_pbkdf2_sha256(const uint8_t *password, size_t pass_len,
                       const uint8_t *salt, size_t salt_len,
                       uint32_t iterations,
                       uint8_t *out, size_t out_len);

/* ═══════════════════════════════════════════════════════════
 *  API — AES-256
 * ═══════════════════════════════════════════════════════════ */

void nc_aes256_init(NcAesCtx *ctx, const uint8_t key[NC_AES_KEY_SIZE]);
void nc_aes256_encrypt_block(const NcAesCtx *ctx,
                              const uint8_t in[NC_AES_BLOCK_SIZE],
                              uint8_t out[NC_AES_BLOCK_SIZE]);

/* ═══════════════════════════════════════════════════════════
 *  API — AES-256-GCM (Authenticated Encryption)
 *
 *  Encrypt: plaintext → ciphertext + tag
 *  Decrypt: ciphertext + tag → plaintext (or fail if tampered)
 * ═══════════════════════════════════════════════════════════ */

/*
 * nc_aes256_gcm_encrypt
 *   key:    32-byte AES-256 key
 *   iv:     12-byte initialization vector (nonce)
 *   aad:    additional authenticated data (can be NULL)
 *   plain:  plaintext to encrypt
 *   cipher: output ciphertext (same length as plain)
 *   tag:    output 16-byte authentication tag
 *   Returns: 0 on success, -1 on error
 */
int nc_aes256_gcm_encrypt(const uint8_t key[NC_AES_KEY_SIZE],
                           const uint8_t iv[NC_GCM_IV_SIZE],
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plain, size_t plain_len,
                           uint8_t *cipher,
                           uint8_t tag[NC_GCM_TAG_SIZE]);

/*
 * nc_aes256_gcm_decrypt
 *   Returns: 0 on success, -1 if tag doesn't match (tampered!)
 */
int nc_aes256_gcm_decrypt(const uint8_t key[NC_AES_KEY_SIZE],
                           const uint8_t iv[NC_GCM_IV_SIZE],
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *cipher, size_t cipher_len,
                           const uint8_t tag[NC_GCM_TAG_SIZE],
                           uint8_t *plain);

/* ═══════════════════════════════════════════════════════════
 *  API — NOVA Master Key (obfuscated)
 *
 *  The master key is NOT stored as a single constant.
 *  It's assembled at runtime from 4 XOR'd fragments
 *  scattered across the binary. Static analysis won't
 *  find a single "key" string.
 * ═══════════════════════════════════════════════════════════ */

void nc_nova_derive_key(const uint8_t salt[NC_PBKDF2_SALT_SIZE],
                         uint8_t key_out[NC_AES_KEY_SIZE]);

/* ═══════════════════════════════════════════════════════════
 *  API — NOVA File Encryption (high-level)
 *
 *  nova_encrypt_file: raw data → .nova v2 encrypted blob
 *  nova_decrypt_file: .nova v2 blob → raw data
 * ═══════════════════════════════════════════════════════════ */

/*
 * Encrypt raw data into NOVA format blob.
 * Allocates output buffer (*out). Caller must free().
 * Returns: total output size, or -1 on error.
 */
int64_t nc_nova_encrypt(const uint8_t *data, size_t data_len,
                         uint8_t **out);

/*
 * Decrypt NOVA format blob back to raw data.
 * Allocates output buffer (*out). Caller must free().
 * Returns: decrypted data size, or -1 on error (tampered, wrong key).
 */
int64_t nc_nova_decrypt(const uint8_t *blob, size_t blob_len,
                         uint8_t **out);

/* ═══════════════════════════════════════════════════════════
 *  API — Secure Random
 * ═══════════════════════════════════════════════════════════ */

void nc_random_bytes(uint8_t *buf, size_t len);

#endif /* NC_CRYPTO_H */
