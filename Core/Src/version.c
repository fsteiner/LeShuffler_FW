/**
 * @file version.c
 * @brief Firmware version implementation
 *
 * Stores version information in flash with a magic number for identification.
 * Provides getter functions for runtime version access.
 */

#include "version.h"
#include <stdio.h>

// ============================================================================
// Version Info Structure (stored in flash, .rodata section)
// ============================================================================
// The 'used' attribute prevents the linker from removing this constant
// even if it's not directly referenced in the code.
__attribute__((used))
const FirmwareVersion_t firmware_version_info = {
    .magic = FW_VERSION_MAGIC,
    .major = FW_VERSION_MAJOR,
    .minor = FW_VERSION_MINOR,
    .patch = FW_VERSION_PATCH,
    .reserved = 0,
    .reserved2 = 0,
    .reserved3 = 0
};

// ============================================================================
// Static Buffers for String Formatting
// ============================================================================
static char version_string[16];         // "vX.Y.Z"
static char version_display[48];        // "LeShuffler FW version X.Y.Z"

// ============================================================================
// Getter Function Implementations
// ============================================================================

const char* GetFirmwareVersionString(void)
{
    snprintf(version_string, sizeof(version_string), "v%d.%d.%d",
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    return version_string;
}

const char* GetFirmwareVersionDisplay(void)
{
    snprintf(version_display, sizeof(version_display),
             "LeShuffler FW version %d.%d.%d",
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    return version_display;
}

uint8_t GetFirmwareVersionMajor(void)
{
    return FW_VERSION_MAJOR;
}

uint8_t GetFirmwareVersionMinor(void)
{
    return FW_VERSION_MINOR;
}

uint8_t GetFirmwareVersionPatch(void)
{
    return FW_VERSION_PATCH;
}

uint32_t GetFirmwareVersion(void)
{
    return FW_VERSION;
}
