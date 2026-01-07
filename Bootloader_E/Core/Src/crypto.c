#include "crypto.h"
#include "crypto_keys.h"
#include "cryp.h"
#include "hash.h"
#include "rng.h"
#include "uECC.h"
#include <string.h>

// CRITICAL: Must be 4-byte aligned for HAL_CRYP hardware operations
__attribute__((aligned(4)))
static uint8_t crypto_temp_buffer[AES_BLOCK_SIZE];
static uint8_t hash_started = 0;
static uint8_t key_converted = 0;

/* Key and IV buffers as 32-bit words (CRYP requires big-endian word format) */
__attribute__((aligned(4)))
static uint32_t key_words[8];   // 32 bytes = 8 words

__attribute__((aligned(4)))
static uint32_t iv_words[4];    // 16 bytes = 4 words

/* Convert 4 bytes to big-endian 32-bit word (required by CRYP peripheral) */
static inline uint32_t bytes_to_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           (uint32_t)bytes[3];
}

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

void Crypto_Reset(void) {
    /* Reset incremental hash state flag */
    hash_started = 0;
    /* Reset key converted flag so it re-converts on next decrypt */
    key_converted = 0;

    /* Reset HASH peripheral using HAL (safer than RCC reset)
     * This clears any interrupted incremental hash state */
    HAL_HASH_DeInit(&hhash);
    hhash.State = HAL_HASH_STATE_RESET;

    /* Note: Don't DeInit CRYP here - leave it initialized */
}

int32_t Crypto_AES256_Decrypt(const uint8_t *encrypted, uint8_t *decrypted,
                              uint32_t length, const uint8_t *iv) {
    if ((length % AES_BLOCK_SIZE) != 0) return CRYPTO_ERROR;

    /* Convert key from byte array to big-endian 32-bit words (only once) */
    if (!key_converted) {
        for (int i = 0; i < 8; i++) {
            key_words[i] = bytes_to_be32(&AES_KEY[i * 4]);
        }
        key_converted = 1;
    }

    /* Convert IV from byte array to big-endian 32-bit words (every call, IV changes) */
    for (int i = 0; i < 4; i++) {
        iv_words[i] = bytes_to_be32(&iv[i * 4]);
    }

    /* Use HAL_CRYP_Init to properly initialize with real key (triggers key derivation)
     * SetConfig doesn't properly set up decryption key schedule */
    hcryp.Init.DataType = CRYP_DATATYPE_8B;
    hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
    hcryp.Init.Algorithm = CRYP_AES_CBC;
    hcryp.Init.pKey = key_words;
    hcryp.Init.pInitVect = iv_words;

    /* Force reinitialization by resetting state */
    hcryp.State = HAL_CRYP_STATE_RESET;

    if (HAL_CRYP_Init(&hcryp) != HAL_OK) return CRYPTO_ERROR;

    /* Perform AES-256-CBC decryption
     * Size is in 32-bit words, so divide length by 4 */
    if (HAL_CRYP_Decrypt(&hcryp, (uint32_t*)encrypted, length / 4,
                         (uint32_t*)decrypted, HAL_MAX_DELAY) != HAL_OK) {
        return CRYPTO_ERROR;
    }

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
int32_t Crypto_SHA256_Start(void) {
    /* Reset HAL state so it will accept reinitialization */
    hhash.State = HAL_HASH_STATE_RESET;
    hhash.Init.DataType = HASH_DATATYPE_8B;

    if (HAL_HASH_Init(&hhash) != HAL_OK) {
        return CRYPTO_ERROR;
    }

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

// DEBUG: Expose HAL state for diagnostics
uint8_t Crypto_GetHashState(void) {
    return (uint8_t)hhash.State;
}

int32_t Crypto_SHA256_Finish(uint8_t *hash) {
    if (!hash_started) return -10;  // Distinct error: hash not started
    hash_started = 0;

    // CRITICAL: Some HAL versions require non-NULL buffer even when Size=0
    // Use a dummy buffer to avoid HAL_ERROR from NULL pointer check
    static uint8_t dummy_buffer[4] __attribute__((aligned(4))) = {0};

    HAL_StatusTypeDef hal_result = HAL_HASHEx_SHA256_Accmlt_End(&hhash, dummy_buffer, 0, hash, HAL_MAX_DELAY);
    if (hal_result != HAL_OK) {
        // Return negative HAL status + 100 for distinction
        return -(100 + (int32_t)hal_result);  // -101=ERROR, -102=BUSY, -103=TIMEOUT
    }
    return CRYPTO_OK;
}

// DEBUG: Check if hash is in progress
uint8_t Crypto_IsHashStarted(void) {
    return hash_started;
}

int32_t Crypto_ECDSA_VerifyHash(const uint8_t *hash, const uint8_t *signature) {
    const struct uECC_Curve_t *curve = uECC_secp256r1();
    if (uECC_verify(ECDSA_PUBLIC_KEY, hash, SHA256_DIGEST_SIZE, signature, curve)) {
        return CRYPTO_OK;
    }
    return CRYPTO_INVALID_SIG;
}
