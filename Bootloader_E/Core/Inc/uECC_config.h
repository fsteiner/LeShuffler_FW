/*
 * uECC_config.h
 * Configuration for micro-ecc library on STM32H7
 * Version D Encrypted Bootloader
 */

#ifndef UECC_CONFIG_H
#define UECC_CONFIG_H

/* Only enable secp256r1 (P-256) curve - used for ECDSA signatures */
#define uECC_SUPPORTS_secp160r1  0
#define uECC_SUPPORTS_secp192r1  0
#define uECC_SUPPORTS_secp224r1  0
#define uECC_SUPPORTS_secp256r1  1
#define uECC_SUPPORTS_secp256k1  0

/* Use portable C implementation to avoid ARM assembly r7 issues with -fomit-frame-pointer */
#define uECC_PLATFORM           uECC_arch_other

/* 32-bit word size for Cortex-M7 */
#define uECC_WORD_SIZE          4

/* Disable ARM-specific assembly (causes r7 register conflicts) */
#define uECC_ARM_USE_UMAAL      0

/* Maximum optimization level */
#define uECC_OPTIMIZATION_LEVEL 3

/* Enable faster squaring function */
#define uECC_SQUARE_FUNC        1

/* Don't need compressed point support */
#define uECC_SUPPORT_COMPRESSED_POINT 0

/* STM32 is little-endian but we handle byte order explicitly */
#define uECC_VLI_NATIVE_LITTLE_ENDIAN 0

/* Don't expose VLI API (only need signature verification) */
#define uECC_ENABLE_VLI_API     0

/* Max RNG tries before failure */
#define uECC_RNG_MAX_TRIES      64

#endif /* UECC_CONFIG_H */
