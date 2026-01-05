/*
 * bootload.h
 *
 *  Created on: Nov 23, 2025
 *      Author: fs
 *
 * Custom USB Bootloader for STM32H733VGT6
 * Bootloader: 0x08000000-0x08003FFF (16KB)
 * Application: 0x08004000-0x080FFFFF (1008KB)
 */

#ifndef INC_BOOTLOAD_H_
#define INC_BOOTLOAD_H_

#include <stdint.h>
#include "stm32h7xx_hal.h"

// Define return_code_t if not already defined
#ifndef UTILITIES_H_
typedef uint32_t return_code_t;
#define LS_OK     0
#define LS_ERROR  1
#endif

// External peripheral handles (defined in main.c)
extern RTC_HandleTypeDef hrtc;
extern CRC_HandleTypeDef hcrc;

// ============================================================================
// Application Functions (called from application code)
// ============================================================================

/**
 * @brief Request bootloader entry from application
 * Sets RTC backup register flag and resets system
 * @return return_code_t Status
 */
return_code_t EnterBootloaderMode(void);

/**
 * @brief Backward compatibility wrapper for EnterBootloaderMode
 * @return return_code_t Status
 */
return_code_t SetBootloaderFlag(void);

// ============================================================================
// Bootloader Functions (called from bootloader code)
// ============================================================================

/**
 * @brief Check if bootloader mode was requested
 * Called by bootloader at startup
 * @return 1 if bootloader mode requested, 0 otherwise
 */
uint8_t IsBootloaderModeRequested(void);

/**
 * @brief Jump from bootloader to application
 * Called by bootloader when no firmware update is needed
 */
void JumpToApplication(void);

// ============================================================================
// Flash Programming Functions
// ============================================================================

/**
 * @brief Unlock flash for programming
 * @return HAL status
 */
HAL_StatusTypeDef FlashUnlock(void);

/**
 * @brief Lock flash after programming
 * @return HAL status
 */
HAL_StatusTypeDef FlashLock(void);

/**
 * @brief Erase application flash sectors
 * @return HAL status
 */
HAL_StatusTypeDef EraseApplicationFlash(void);

/**
 * @brief Write data to flash
 * @param address Flash address (must be 32-byte aligned)
 * @param data Pointer to data buffer
 * @param length Data length (must be multiple of 32)
 * @return HAL status
 */
HAL_StatusTypeDef WriteFlash(uint32_t address, uint8_t *data, uint32_t length);

/**
 * @brief Verify flash contents
 * @param address Flash address to verify
 * @param data Pointer to expected data
 * @param length Data length in bytes
 * @return 1 if match, 0 if mismatch
 */
uint8_t VerifyFlash(uint32_t address, uint8_t *data, uint32_t length);

/**
 * @brief Calculate CRC32 of application flash
 * @return CRC32 value
 */
uint32_t CalculateApplicationCRC(void);

// ============================================================================
// USB CDC Firmware Update Protocol
// ============================================================================

// Firmware update packet structure
typedef struct {
    uint32_t packet_type;  // 0x01=Start, 0x02=Data, 0x03=End
    uint32_t address;      // Flash address for this packet
    uint32_t length;       // Data length in this packet
    uint32_t crc32;        // CRC32 of data in this packet
    uint8_t data[256];     // Up to 256 bytes of firmware data
} FirmwarePacket_t;

// Packet types
#define PACKET_TYPE_START  0x01
#define PACKET_TYPE_DATA   0x02
#define PACKET_TYPE_END    0x03
#define PACKET_TYPE_STATUS 0x04  // v2: Query bootloader state
#define PACKET_TYPE_RESUME 0x05  // v2.1: Resume interrupted transfer (no erase)

// Bootloader version (v2.1 = 0x0201)
#define BOOTLOADER_VERSION_MAJOR  2
#define BOOTLOADER_VERSION_MINOR  1
#define BOOTLOADER_VERSION        ((BOOTLOADER_VERSION_MAJOR << 8) | BOOTLOADER_VERSION_MINOR)

// Status response structure (sent in response to STATUS packet)
typedef struct {
    uint8_t header;          // 0xAA
    uint8_t packet_type;     // PACKET_TYPE_STATUS
    uint8_t version_major;   // Bootloader version major
    uint8_t version_minor;   // Bootloader version minor
    uint8_t state;           // FirmwareUpdateState_t
    uint8_t progress;        // 0-100%
    uint16_t reserved;       // Padding
    uint32_t bytes_received; // Total bytes received so far
    uint32_t next_address;   // Expected next write address
    uint32_t flags;          // Bit 0: flash erased, Bit 1: transfer in progress
    uint8_t footer;          // 0x55
    uint8_t padding[3];      // Pad to 24 bytes
} BootloaderStatus_t;

// Firmware update state
typedef enum {
    FW_IDLE,
    FW_RECEIVING,
    FW_FINALIZING,  // Flash locked, doing beeps, about to complete
    FW_COMPLETE,
    FW_ERROR
} FirmwareUpdateState_t;

/**
 * @brief Process received firmware packet from USB CDC
 * @param packet Pointer to received packet
 * @return 0=Success, -1=Error
 */
int32_t ProcessFirmwarePacket(FirmwarePacket_t *packet);

/**
 * @brief Get current firmware update progress (0-100)
 * @return Progress percentage
 */
uint8_t GetFirmwareUpdateProgress(void);

/**
 * @brief Get firmware update state
 * @return Current state
 */
FirmwareUpdateState_t GetFirmwareUpdateState(void);

/**
 * @brief Reset firmware update state
 */
void ResetFirmwareUpdate(void);

/**
 * @brief Get bootloader status (v2)
 * @param status Pointer to status structure to fill
 */
void GetBootloaderStatus(BootloaderStatus_t *status);

/**
 * @brief Check if flash has been erased
 * @return 1 if erased, 0 otherwise
 */
uint8_t IsFlashErased(void);

/**
 * @brief Get bytes received so far
 * @return Number of bytes received
 */
uint32_t GetBytesReceived(void);

#endif /* INC_BOOTLOAD_H_ */
