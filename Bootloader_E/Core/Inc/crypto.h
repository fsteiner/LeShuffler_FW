/*
 * crypto.h
 * Cryptographic functions for Version D Encrypted Bootloader
 */

#ifndef INC_CRYPTO_H_
#define INC_CRYPTO_H_

#include "main.h"
#include <stdint.h>

/* Return codes */
#define CRYPTO_OK           0
#define CRYPTO_ERROR       -1
#define CRYPTO_INVALID_SIG -2
#define CRYPTO_INVALID_KEY -3

/* Constants */
#define AES_KEY_SIZE        32
#define AES_IV_SIZE         16
#define AES_BLOCK_SIZE      16
#define SHA256_DIGEST_SIZE  32
#define ECDSA_SIG_SIZE      64
#define ECDSA_PUBKEY_SIZE   64

/* .sfu file header magic */
#define SFU_MAGIC           0x5546534C  /* "LSFU" in little-endian */

/* Secure Firmware Update header structure */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t firmware_size;
    uint32_t original_size;
    uint8_t  iv[AES_IV_SIZE];
    uint8_t  signature[ECDSA_SIG_SIZE];
    uint32_t header_crc;
} SFU_Header_t;

int32_t Crypto_Init(void);
void Crypto_Reset(void);  /* Reset crypto state after interrupted transfer */
int32_t Crypto_AES256_Decrypt(const uint8_t *encrypted, uint8_t *decrypted, uint32_t length, const uint8_t *iv);
int32_t Crypto_SHA256(const uint8_t *data, uint32_t length, uint8_t *hash);
int32_t Crypto_ECDSA_Verify(const uint8_t *data, uint32_t data_length, const uint8_t *signature);
int32_t Crypto_ValidateSFUHeader(const SFU_Header_t *header);
int32_t Crypto_DecryptFirmwareBlock(const uint8_t *encrypted_block, uint8_t *decrypted_block, uint32_t length, uint8_t *iv);

/* Incremental SHA-256 for large data (signature verification) */
int32_t Crypto_SHA256_Start(void);
int32_t Crypto_SHA256_Update(const uint8_t *data, uint32_t length);
int32_t Crypto_SHA256_Finish(uint8_t *hash);

/* ECDSA verification with pre-computed hash */
int32_t Crypto_ECDSA_VerifyHash(const uint8_t *hash, const uint8_t *signature);

/* DEBUG: Check if incremental hash is in progress */
uint8_t Crypto_IsHashStarted(void);
uint8_t Crypto_GetHashState(void);

#endif /* INC_CRYPTO_H_ */
