#include "crypto.h"
#include "crypto_keys.h"
#include "cryp.h"
#include "hash.h"
#include "rng.h"
#include "uECC.h"
#include <string.h>

static uint8_t crypto_temp_buffer[AES_BLOCK_SIZE];

static int uECC_RNG_Callback(uint8_t *dest, unsigned size) {
    uint32_t random_word;
    unsigned i = 0;
    while (i < size) {
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random_word) != HAL_OK) {
            return 0;
        }
        unsigned bytes_to_copy = (size - i > 4) ? 4 : size - i;
        memcpy(&dest[i], &random_word, bytes_to_copy);
        i += bytes_to_copy;
    }
    return 1;
}

int32_t Crypto_Init(void) {
    uECC_set_rng(uECC_RNG_Callback);
    if (hcryp.Instance == NULL) return CRYPTO_ERROR;
    if (hrng.Instance == NULL) return CRYPTO_ERROR;
    return CRYPTO_OK;
}

int32_t Crypto_AES256_Decrypt(const uint8_t *encrypted, uint8_t *decrypted,
                              uint32_t length, const uint8_t *iv) {
    if ((length % AES_BLOCK_SIZE) != 0) return CRYPTO_ERROR;

    hcryp.Init.DataType = CRYP_DATATYPE_8B;
    hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
    hcryp.Init.Algorithm = CRYP_AES_CBC;
    hcryp.Init.pKey = (uint32_t*)AES_KEY;
    hcryp.Init.pInitVect = (uint32_t*)iv;

    if (HAL_CRYP_Init(&hcryp) != HAL_OK) return CRYPTO_ERROR;
    if (HAL_CRYP_Decrypt(&hcryp, (uint32_t*)encrypted, length,
                         (uint32_t*)decrypted, HAL_MAX_DELAY) != HAL_OK) return CRYPTO_ERROR;
    return CRYPTO_OK;
}

int32_t Crypto_SHA256(const uint8_t *data, uint32_t length, uint8_t *hash) {
    hhash.Init.DataType = HASH_DATATYPE_8B;
    if (HAL_HASH_Init(&hhash) != HAL_OK) return CRYPTO_ERROR;
    if (HAL_HASHEx_SHA256_Start(&hhash, (uint8_t*)data, length,
                                 hash, HAL_MAX_DELAY) != HAL_OK) return CRYPTO_ERROR;
    return CRYPTO_OK;
}

int32_t Crypto_ECDSA_Verify(const uint8_t *data, uint32_t data_length,
                            const uint8_t *signature) {
    uint8_t hash[SHA256_DIGEST_SIZE];
    if (Crypto_SHA256(data, data_length, hash) != CRYPTO_OK) return CRYPTO_ERROR;
    const struct uECC_Curve_t *curve = uECC_secp256r1();
    if (uECC_verify(ECDSA_PUBLIC_KEY, hash, sizeof(hash), signature, curve)) {
        return CRYPTO_OK;
    }
    return CRYPTO_INVALID_SIG;
}

int32_t Crypto_ValidateSFUHeader(const SFU_Header_t *header) {
    if (header->magic != SFU_MAGIC) return CRYPTO_ERROR;
    if (header->firmware_size == 0 || header->firmware_size > (1024 * 1024)) return CRYPTO_ERROR;
    uint32_t header_data_size = offsetof(SFU_Header_t, signature);
    return Crypto_ECDSA_Verify((const uint8_t*)header, header_data_size, header->signature);
}

int32_t Crypto_DecryptFirmwareBlock(const uint8_t *encrypted_block,
                                     uint8_t *decrypted_block,
                                     uint32_t length, uint8_t *iv) {
    int32_t result;
    if (length >= AES_BLOCK_SIZE) {
        memcpy(crypto_temp_buffer, &encrypted_block[length - AES_BLOCK_SIZE], AES_BLOCK_SIZE);
    }
    result = Crypto_AES256_Decrypt(encrypted_block, decrypted_block, length, iv);
    if (result == CRYPTO_OK && length >= AES_BLOCK_SIZE) {
        memcpy(iv, crypto_temp_buffer, AES_BLOCK_SIZE);
    }
    return result;
}

/* Incremental SHA-256 hashing for signature verification */
static uint8_t hash_started = 0;

int32_t Crypto_SHA256_Start(void) {
    hhash.Init.DataType = HASH_DATATYPE_8B;
    if (HAL_HASH_Init(&hhash) != HAL_OK) return CRYPTO_ERROR;
    hash_started = 1;
    return CRYPTO_OK;
}

int32_t Crypto_SHA256_Update(const uint8_t *data, uint32_t length) {
    if (!hash_started) return CRYPTO_ERROR;
    if (length == 0) return CRYPTO_OK;
    if (HAL_HASHEx_SHA256_Accmlt(&hhash, (uint8_t*)data, length) != HAL_OK) {
        return CRYPTO_ERROR;
    }
    return CRYPTO_OK;
}

int32_t Crypto_SHA256_Finish(uint8_t *hash) {
    if (!hash_started) return CRYPTO_ERROR;
    hash_started = 0;
    if (HAL_HASHEx_SHA256_Accmlt_End(&hhash, NULL, 0, hash, HAL_MAX_DELAY) != HAL_OK) {
        return CRYPTO_ERROR;
    }
    return CRYPTO_OK;
}

int32_t Crypto_ECDSA_VerifyHash(const uint8_t *hash, const uint8_t *signature) {
    const struct uECC_Curve_t *curve = uECC_secp256r1();
    if (uECC_verify(ECDSA_PUBLIC_KEY, hash, SHA256_DIGEST_SIZE, signature, curve)) {
        return CRYPTO_OK;
    }
    return CRYPTO_INVALID_SIG;
}
