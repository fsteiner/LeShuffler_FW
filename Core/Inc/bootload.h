/*
 * bootload.h
 *
 *  Created on: Nov 23, 2025
 *      Author: fs
 */

#ifndef INC_BOOTLOAD_H_
#define INC_BOOTLOAD_H_

#include <stdint.h>

// Bootloader version location in flash
#define BOOTLOADER_VERSION_ADDR   0x0800BFF0
#define BOOTLOADER_VERSION_MAGIC  0x424C5652  // "BLVR"

/**
 * @brief Request entry into bootloader mode
 * Sets flag and resets device
 */
return_code_t EarlyBootloaderCheck(void);

/**
 * @brief Get bootloader version from flash
 * Reads version info stored at fixed address by bootloader
 * @return Version as uint16_t (major << 8 | minor), or 0 if not found/old bootloader
 * Example: 0x0201 = v2.1, 0x0100 = v1.0
 */
uint16_t GetBootloaderVersion(void);

#endif /* INC_BOOTLOAD_H_ */
