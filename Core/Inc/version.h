/**
 * @file version.h
 * @brief Firmware version information
 *
 * Provides version constants and getter functions for firmware versioning.
 * Version is stored in flash with a magic number for external tool detection.
 */

#ifndef VERSION_H_
#define VERSION_H_

#include <stdint.h>

// ============================================================================
// Firmware Version - UPDATE THESE FOR EACH RELEASE
// ============================================================================
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  1

// ============================================================================
// Encoded Version (0x00MMmmPP format)
// ============================================================================
// Examples: v1.0.1 = 0x00010001, v2.3.15 = 0x0002030F
#define FW_VERSION  ((FW_VERSION_MAJOR << 16) | (FW_VERSION_MINOR << 8) | FW_VERSION_PATCH)

// Magic number for version struct identification in binary
#define FW_VERSION_MAGIC  0x46575652  // "FWVR" (Firmware Version)

// ============================================================================
// Version Info Structure (16 bytes)
// ============================================================================
typedef struct {
    uint32_t magic;         // 0x46575652 = "FWVR"
    uint8_t  major;         // Major version
    uint8_t  minor;         // Minor version
    uint8_t  patch;         // Patch version
    uint8_t  reserved;      // Padding
    uint32_t reserved2;     // Reserved for future use
    uint32_t reserved3;     // Reserved for future use
} FirmwareVersion_t;

// ============================================================================
// Getter Functions
// ============================================================================

/**
 * @brief Get firmware version as formatted string
 * @return Pointer to static string in format "vX.Y.Z" (e.g., "v1.0.1")
 */
const char* GetFirmwareVersionString(void);

/**
 * @brief Get firmware version as display string for About screen
 * @return Pointer to static string "LeShuffler FW version X.Y.Z"
 */
const char* GetFirmwareVersionDisplay(void);

/**
 * @brief Get major version number
 * @return Major version (0-255)
 */
uint8_t GetFirmwareVersionMajor(void);

/**
 * @brief Get minor version number
 * @return Minor version (0-255)
 */
uint8_t GetFirmwareVersionMinor(void);

/**
 * @brief Get patch version number
 * @return Patch version (0-255)
 */
uint8_t GetFirmwareVersionPatch(void);

/**
 * @brief Get encoded version number
 * @return Version as 0x00MMmmPP (e.g., 0x00010001 for v1.0.1)
 */
uint32_t GetFirmwareVersion(void);

#endif /* VERSION_H_ */
