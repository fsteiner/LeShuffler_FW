/**
 * STM32H733VGT6 Custom USB Bootloader Implementation
 * Bootloader at 0x08000000 (Sector 0, 128KB)
 * Application at 0x08020000 (Sectors 1-7, 896KB)
 */

#include "stm32h7xx_hal.h"
#include "bootload.h"
#include "crypto.h"
#include "usbd_cdc_if.h"
#include <string.h>

// ============================================================================
// Bootloader Version Info (stored at fixed flash address 0x0800BFF0)
// Application firmware can read this directly from flash
// ============================================================================
__attribute__((section(".bootloader_version"), used))
const struct {
    uint16_t version;      // Major.Minor (0x0201 = v2.1)
    uint16_t reserved;     // For future use
    uint32_t magic;        // 0x424C5652 = "BLVR" (Bootloader Version)
    uint32_t build_date;   // Reserved for build timestamp
    uint32_t checksum;     // Reserved
} bootloader_version_info = {
    .version = BOOTLOADER_VERSION,
    .reserved = 0,
    .magic = 0x424C5652,   // "BLVR"
    .build_date = 0,
    .checksum = 0
};

// RTC Backup Register for bootloader flag (survives reset)
#define RTC_BACKUP_BOOTLOADER_FLAG  0  // Use register 0
#define BOOTLOADER_MAGIC_VALUE      0xDEADBEEF

// Memory layout - STM32H733VGT6 has 128KB sectors!
#define BOOTLOADER_START_ADDRESS    0x08000000
#define APPLICATION_START_ADDRESS   0x08020000  // Sector 1 (128KB offset)
#define FLASH_END_ADDRESS           0x080FFFFF

// Flash sector definitions for STM32H733VGT6 (1MB flash, 128KB sectors)
// Sector 0: 0x08000000 - 0x0801FFFF (Bootloader)
// Sector 1: 0x08020000 - 0x0803FFFF (App start)
// Sector 2-7: App continues
#define BL_FLASH_SECTOR_SIZE        0x20000  // 128KB sectors
#define BL_FLASH_TOTAL_SIZE         0x100000  // 1MB
#define APP_FIRST_SECTOR            1         // Application starts at sector 1

/**
 * @brief Request bootloader entry from application
 * Sets RTC backup register flag and resets system
 */
return_code_t EnterBootloaderMode(void)
{
    // Enable RTC backup register access
    // For STM32H7, enable RTCAPB clock
    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    HAL_PWR_EnableBkUpAccess();

    // Small delay to ensure backup domain is accessible
    for (volatile uint32_t i = 0; i < 10000; i++);

    // Write magic value to RTC backup register
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BACKUP_BOOTLOADER_FLAG, BOOTLOADER_MAGIC_VALUE);

    // Give user feedback (3 beeps)
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);  // Buzzer on
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
        HAL_Delay(100);
    }

    HAL_Delay(200);

    // System reset - bootloader will check flag and stay in bootloader mode
    NVIC_SystemReset();

    // Never reached
    return LS_OK;
}

/**
 * @brief Backward compatibility wrapper for EnterBootloaderMode
 * @return return_code_t Status
 */
return_code_t SetBootloaderFlag(void)
{
    return EnterBootloaderMode();
}

/**
 * @brief Check if bootloader mode was requested
 * Called by bootloader at startup - MUST work without HAL initialized!
 * @return 1 if bootloader mode requested, 0 otherwise
 */
uint8_t IsBootloaderModeRequested(void)
{
    uint32_t flag_value;

    // Enable RTC APB clock (direct register access - no HAL needed)
    RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    // Small delay for clock to stabilize
    __DSB();

    // Enable backup domain access (direct register access - no HAL needed)
    // Set DBP bit in PWR_CR1
    PWR->CR1 |= PWR_CR1_DBP;
    // Wait until backup domain is accessible
    while ((PWR->CR1 & PWR_CR1_DBP) == 0) {}

    // Read RTC backup register directly
    // RTC_BACKUP_BOOTLOADER_FLAG is register 0, RTC_BKP0R is at RTC base + 0x50
    flag_value = RTC->BKP0R;

    // Check if magic value is present
    if (flag_value == BOOTLOADER_MAGIC_VALUE) {
        // Clear the flag so we don't stay in bootloader after next reset
        RTC->BKP0R = 0;
        return 1;
    }

    return 0;
}

// Function pointer type for application entry
typedef void (*pFunction)(void);

/**
 * @brief Jump from bootloader to application
 * Called by bootloader when no firmware update is needed
 *
 * NOTE: This is called VERY EARLY, before MPU/HAL/peripherals are initialized.
 * Keep this function minimal - no HAL calls!
 *
 * CRITICAL FINDINGS:
 * 1. Do NOT set MSP - let the app's Reset_Handler set it (first instruction is "ldr sp, =_estack")
 * 2. MUST re-enable interrupts before jump - ExitRun0Mode() in app startup needs them
 */
void JumpToApplication(void)
{
    uint32_t app_stack;
    uint32_t app_reset_vector;
    pFunction JumpToApp;

    // Read application's stack pointer and reset vector
    app_stack = *((volatile uint32_t *)APPLICATION_START_ADDRESS);
    app_reset_vector = *((volatile uint32_t *)(APPLICATION_START_ADDRESS + 4));

    // Validate stack pointer is in valid RAM range
    // STM32H7 RAM ranges: 0x20000000-0x2001FFFF (DTCM), 0x24000000-0x2407FFFF (AXI SRAM)
    if ((app_stack & 0xFFF00000) != 0x20000000 && (app_stack & 0xFFF00000) != 0x24000000) {
        // Invalid application - stay in bootloader
        return;
    }

    // Disable interrupts temporarily while we clean up
    __disable_irq();

    // Reset SysTick to default state
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // Clear all NVIC interrupts
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  // Disable all interrupts
        NVIC->ICPR[i] = 0xFFFFFFFF;  // Clear all pending
    }

    // Clear pending SysTick and PendSV
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;

    // Set Vector Table Offset to application
    SCB->VTOR = APPLICATION_START_ADDRESS;

    // Memory barriers
    __DSB();
    __ISB();

    // CRITICAL: Re-enable interrupts before jumping
    // The app's ExitRun0Mode() function needs interrupts enabled
    __enable_irq();

    // Get the application's Reset Handler address and jump
    // NOTE: Do NOT set MSP - app's Reset_Handler sets it as first instruction
    JumpToApp = (pFunction)app_reset_vector;
    JumpToApp();

    // Never reached
    while (1);
}

// ============================================================================
// Flash Programming Functions
// ============================================================================

// Track which sectors have been erased (for progressive erase)
static uint32_t last_erased_sector = 0;

/**
 * @brief Unlock flash for programming
 * @return HAL status
 */
HAL_StatusTypeDef FlashUnlock(void)
{
    // Check if flash is already unlocked (STM32H7 uses FLASH_CR1 for bank 1)
    if ((FLASH->CR1 & FLASH_CR_LOCK) == 0) {
        // Already unlocked
        return HAL_OK;
    }

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();

    // CRITICAL: Clear all error flags before programming
    // This prevents previous errors from affecting new operations
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1);

    return status;
}

/**
 * @brief Lock flash after programming
 * @return HAL status
 */
HAL_StatusTypeDef FlashLock(void)
{
    return HAL_FLASH_Lock();
}

/**
 * @brief Erase a single flash sector
 * @param sector_num Absolute sector number (0-127)
 * @return HAL status
 */
static HAL_StatusTypeDef EraseSingleSector(uint32_t sector_num)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Banks = FLASH_BANK_1;
    erase_init.Sector = sector_num;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    if (status != HAL_OK || sector_error != 0xFFFFFFFF) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief Erase sectors as needed for the given address
 * Only erases ONE sector at a time to avoid USB timeout
 * @param address Flash address that needs to be written
 * @return HAL status
 * @note Reserved for encrypted firmware updates (progressive erase)
 */
__attribute__((unused))
static HAL_StatusTypeDef EnsureSectorErased(uint32_t address)
{
    // Calculate which sector this address belongs to
    uint32_t sector_num = (address - BOOTLOADER_START_ADDRESS) / BL_FLASH_SECTOR_SIZE;

    // Check if we need to erase this sector
    if (sector_num > last_erased_sector) {
        // Erase only the next sector (ONE at a time to keep USB alive)
        uint32_t next_sector = last_erased_sector + 1;
        HAL_StatusTypeDef status = EraseSingleSector(next_sector);
        if (status != HAL_OK) {
            return HAL_ERROR;
        }
        last_erased_sector = next_sector;
    }

    return HAL_OK;
}

/**
 * @brief Erase application flash sectors (1-7)
 * Called from main() BEFORE USB init, so USB is not affected
 *
 * STM32H733VGT6 Memory Map (128KB sectors):
 *   Sector 0: 0x08000000 - 0x0801FFFF = Bootloader
 *   Sector 1-7: 0x08020000 - 0x080FFFFF = Application (896KB)
 *
 * @return HAL status
 */
HAL_StatusTypeDef EraseApplicationFlash(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t error;
    const uint32_t SECTORS_TO_ERASE = 7;  // Sectors 1-7

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < SECTORS_TO_ERASE; i++) {
        // Refresh watchdog before each sector erase (takes ~seconds)
        IWDG_REFRESH();

        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Banks = FLASH_BANK_1;
        erase.Sector = APP_FIRST_SECTOR + i;
        erase.NbSectors = 1;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    HAL_FLASH_Lock();

    // Mark all sectors as erased so WriteFlash doesn't try to erase again
    last_erased_sector = APP_FIRST_SECTOR + SECTORS_TO_ERASE;

    return HAL_OK;
}

/**
 * @brief Write data to flash
 * @param address Flash address to write to (must be aligned to 256-bit / 32 bytes)
 * @param data Pointer to data buffer
 * @param length Data length in bytes (must be multiple of 32)
 * @return HAL status
 */
HAL_StatusTypeDef WriteFlash(uint32_t address, uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status = HAL_OK;

    // CRITICAL SAFETY CHECK: Prevent any writes to bootloader area
    if (address < APPLICATION_START_ADDRESS) {
        return HAL_ERROR;
    }

    // Also check that the write won't overflow into invalid area
    if (address + length > FLASH_END_ADDRESS) {
        return HAL_ERROR;
    }

    // STM32H7 flash is programmed in 256-bit (32 byte) words
    // Address must be 32-byte aligned
    if ((address % 32) != 0) {
        return HAL_ERROR;
    }

    // Length must be multiple of 32 bytes
    if ((length % 32) != 0) {
        return HAL_ERROR;
    }

    // Program flash in 256-bit chunks
    for (uint32_t i = 0; i < length; i += 32) {
        IWDG_REFRESH();  // Refresh watchdog during flash writes
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                   address + i,
                                   (uint32_t)(data + i));
        if (status != HAL_OK) {
            return status;
        }
    }

    return HAL_OK;
}

/**
 * @brief Verify flash contents
 * @param address Flash address to verify
 * @param data Pointer to expected data
 * @param length Data length in bytes
 * @return 1 if match, 0 if mismatch
 */
uint8_t VerifyFlash(uint32_t address, uint8_t *data, uint32_t length)
{
    uint8_t *flash_ptr = (uint8_t *)address;

    for (uint32_t i = 0; i < length; i++) {
        if (flash_ptr[i] != data[i]) {
            return 0;  // Mismatch
        }
    }

    return 1;  // Match
}

/**
 * @brief Get application CRC32 for verification
 * @return CRC32 value
 */
uint32_t CalculateApplicationCRC(void)
{
    // Enable CRC peripheral clock
    __HAL_RCC_CRC_CLK_ENABLE();

    // Calculate CRC of application flash
    uint32_t *flash_ptr = (uint32_t *)APPLICATION_START_ADDRESS;
    uint32_t flash_size = FLASH_END_ADDRESS - APPLICATION_START_ADDRESS + 1;
    uint32_t word_count = flash_size / 4;

    // Use HAL CRC calculation
    return HAL_CRC_Calculate(&hcrc, flash_ptr, word_count);
}

// ============================================================================
// USB CDC Firmware Update Protocol
// ============================================================================

// Software CRC32 table (matches Python binascii.crc32)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/**
 * @brief Software CRC32 calculation (matches Python binascii.crc32)
 * Uses standard zlib/PNG CRC32 algorithm:
 * - Initial value: 0xFFFFFFFF
 * - Final XOR: 0xFFFFFFFF
 */
static uint32_t software_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;  // Start with all 1s
    for (uint32_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;  // Final XOR
}

// Types are defined in bootload.h
// No need to redefine them here

static FirmwareUpdateState_t fw_update_state = FW_IDLE;
static uint32_t fw_total_bytes = 0;
static uint32_t fw_received_bytes = 0;

// Small packet buffer - we write to flash one packet at a time
// No large RAM buffer needed - just enough for one packet
// CRITICAL: Must be 4-byte aligned for flash operations
__attribute__((aligned(4)))
static uint8_t packet_buffer[288];  // 256 bytes + padding to 32-byte boundary

// ============================================================================
// Encrypted Firmware Update State (v3.0)
// ============================================================================
static uint8_t encrypted_mode = 0;           // 1 if processing encrypted .sfu
static SFU_Header_t sfu_header;              // Stored SFU header
static uint32_t enc_received_bytes = 0;      // Encrypted bytes received

// CRITICAL: These buffers MUST be 4-byte aligned for HAL_CRYP hardware
__attribute__((aligned(4)))
static uint8_t current_iv[AES_IV_SIZE];      // Current IV for CBC decryption
__attribute__((aligned(4)))
static uint8_t decrypted_buffer[256];        // Buffer for decrypted data
__attribute__((aligned(4)))
static uint8_t aligned_enc_buffer[256];      // Aligned copy of encrypted input

/**
 * @brief Process received firmware packet from USB CDC
 * @param packet Pointer to received packet
 * @return 0=Success, -1=Error
 */
int32_t ProcessFirmwarePacket(FirmwarePacket_t *packet)
{
    HAL_StatusTypeDef status;

    switch (packet->packet_type) {
        case PACKET_TYPE_START:
            // Start of firmware update
            // v2: ALWAYS reset state and re-erase flash
            // This enables safe restart-from-beginning after USB disconnect
            IWDG_REFRESH();
            fw_update_state = FW_RECEIVING;
            fw_total_bytes = packet->length;  // Total firmware size
            fw_received_bytes = 0;

            // v2: Always erase flash on START (enables safe restart)
            // This is critical for handling USB disconnects - ensures clean slate
            status = EraseApplicationFlash();
            if (status != HAL_OK) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            return 0;

        case PACKET_TYPE_RESUME:
            // v2.1: Resume interrupted transfer WITHOUT erasing flash
            // Only valid if transfer was already in progress
            if (fw_update_state != FW_RECEIVING) {
                // No transfer in progress - cannot resume
                // Caller should send START instead
                return -1;
            }

            // Validate that we have a partial transfer to resume
            if (fw_received_bytes == 0 || last_erased_sector == 0) {
                // Nothing to resume - need fresh START
                return -1;
            }

            // Keep existing state (fw_total_bytes, fw_received_bytes unchanged)
            // Flash is NOT erased - continue from where we left off
            // Response will include next_address via STATUS query
            return 0;

        case PACKET_TYPE_STATUS:
            // v2: Status query - handled separately via GetBootloaderStatus()
            // Just return success, actual response is built by caller
            return 0;

        case PACKET_TYPE_DATA:
            // ULTRA-CONSERVATIVE APPROACH: Write each packet immediately with long delays
            IWDG_REFRESH();

            if (fw_update_state != FW_RECEIVING) {
                return -1;
            }

            // Safety check: validate length
            if (packet->length == 0 || packet->length > 256) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Verify packet CRC
            uint32_t calc_crc = software_crc32(packet->data, packet->length);
            if (calc_crc != packet->crc32) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Calculate flash address based on bytes received so far
            uint32_t flash_address = APPLICATION_START_ADDRESS + fw_received_bytes;

            // Safety check: verify address
            if (flash_address < APPLICATION_START_ADDRESS) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Prepare data with padding
            uint32_t padded_length = ((packet->length + 31) / 32) * 32;
            memcpy(packet_buffer, packet->data, packet->length);
            for (uint32_t i = packet->length; i < padded_length; i++) {
                packet_buffer[i] = 0xFF;
            }

            // Flash write operations
            // Memory barrier before flash operations
            __DSB();

            // Unlock flash
            status = FlashUnlock();
            if (status != HAL_OK) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Flash is erased before USB init, no need for progressive erase here

            // Write to flash
            status = WriteFlash(flash_address, packet_buffer, padded_length);

            // Lock flash immediately
            FlashLock();

            if (status != HAL_OK) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Memory barriers after flash operations
            __DSB();
            __ISB();

            // v2: Removed HAL_Delay(5) - was causing USB disconnects on macOS
            // The DSB/ISB barriers are sufficient for flash write completion

            fw_received_bytes += packet->length;

            return 0;

        case PACKET_TYPE_END:
            // End of firmware update - all data already written to flash

            if (fw_update_state != FW_RECEIVING) {
                return -1;
            }

            // Verify total bytes written
            if (fw_received_bytes != fw_total_bytes) {
                fw_update_state = FW_ERROR;
                return -1;
            }

            // Set completion state (main loop will beep and reset device)
            fw_update_state = FW_COMPLETE;

            return 0;

        // ====================================================================
        // Encrypted Firmware Update Packets (v3.0)
        // ====================================================================

        case PACKET_TYPE_ENC_START:
            // Encrypted firmware update - packet contains SFU header
            {
                // Clear any leftover packet data from interrupted USB transfer
                CDC_ClearPacketState();
                IWDG_REFRESH();

                // Reset crypto state first (handles interrupted transfers)
                Crypto_Reset();
                encrypted_mode = 0;

                // Verify we have enough data for header
                if (packet->length < sizeof(SFU_Header_t)) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Copy and validate SFU header
                memcpy(&sfu_header, packet->data, sizeof(SFU_Header_t));

                // Check magic
                if (sfu_header.magic != SFU_MAGIC) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Validate sizes
                if (sfu_header.firmware_size == 0 || sfu_header.firmware_size > (896 * 1024)) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }
                if (sfu_header.original_size == 0 || sfu_header.original_size > (896 * 1024)) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Initialize encrypted mode
                encrypted_mode = 1;
                enc_received_bytes = 0;
                fw_total_bytes = sfu_header.original_size;
                fw_received_bytes = 0;
                fw_update_state = FW_RECEIVING;

                // Copy IV for CBC decryption
                memcpy(current_iv, sfu_header.iv, AES_IV_SIZE);

                // Start incremental hash for signature verification
                if (Crypto_SHA256_Start() != CRYPTO_OK) {
                    encrypted_mode = 0;
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Erase application flash
                status = EraseApplicationFlash();
                if (status != HAL_OK) {
                    encrypted_mode = 0;
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                return 0;
            }

        case PACKET_TYPE_ENC_DATA:
            // Full encrypted data handler: validate, hash, decrypt, flash
            {
                IWDG_REFRESH();

                // State check
                if (fw_update_state != FW_RECEIVING || !encrypted_mode) {
                    return -1;
                }

                // Length checks
                if (packet->length == 0 || packet->length > 256) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }
                if ((packet->length % AES_BLOCK_SIZE) != 0) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // CRC check (over encrypted data)
                uint32_t enc_crc = software_crc32(packet->data, packet->length);
                if (enc_crc != packet->crc32) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Copy to aligned buffer for crypto operations
                memcpy(aligned_enc_buffer, packet->data, packet->length);

                // Update hash with encrypted data (for signature verification)
                if (Crypto_SHA256_Update(aligned_enc_buffer, packet->length) != CRYPTO_OK) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Decrypt the data
                if (Crypto_DecryptFirmwareBlock(aligned_enc_buffer, decrypted_buffer,
                                                 packet->length, current_iv) != CRYPTO_OK) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                enc_received_bytes += packet->length;

                // Calculate actual data to write (handle final packet padding)
                uint32_t data_to_write = packet->length;
                uint32_t remaining = sfu_header.original_size - fw_received_bytes;
                if (data_to_write > remaining) {
                    data_to_write = remaining;  // Trim PKCS7 padding on final packet
                }

                // Calculate flash address
                uint32_t flash_address = APPLICATION_START_ADDRESS + fw_received_bytes;

                // Safety check
                if (flash_address < APPLICATION_START_ADDRESS) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Prepare data with 32-byte alignment for flash write
                uint32_t padded_length = ((data_to_write + 31) / 32) * 32;
                memcpy(packet_buffer, decrypted_buffer, data_to_write);
                for (uint32_t i = data_to_write; i < padded_length; i++) {
                    packet_buffer[i] = 0xFF;
                }

                // Write to flash
                __DSB();
                status = FlashUnlock();
                if (status != HAL_OK) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                status = WriteFlash(flash_address, packet_buffer, padded_length);
                FlashLock();

                if (status != HAL_OK) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                __DSB();
                __ISB();

                fw_received_bytes += data_to_write;

                return 0;
            }

        case PACKET_TYPE_ENC_END:
            // End of encrypted firmware update - verify signature
            {
                IWDG_REFRESH();

                if (fw_update_state != FW_RECEIVING || !encrypted_mode) {
                    return -1;
                }

                // Verify we received all encrypted data
                if (enc_received_bytes != sfu_header.firmware_size) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Finalize hash
                // CRITICAL: Must be 4-byte aligned for HASH DMA output
                __attribute__((aligned(4)))
                static uint8_t computed_hash[SHA256_DIGEST_SIZE];
                if (Crypto_SHA256_Finish(computed_hash) != CRYPTO_OK) {
                    fw_update_state = FW_ERROR;
                    return -1;
                }

                // Verify ECDSA signature (this is the slow part - ~1-2 seconds)
                IWDG_REFRESH();
                if (Crypto_ECDSA_VerifyHash(computed_hash, sfu_header.signature) != CRYPTO_OK) {
                    // Signature verification failed - firmware is NOT authentic!
                    fw_update_state = FW_ERROR;
                    encrypted_mode = 0;
                    return -1;
                }

                // Signature valid - firmware is authentic
                encrypted_mode = 0;
                fw_update_state = FW_COMPLETE;

                return 0;
            }

        default:
            return -1;
    }
}

/**
 * @brief Get current firmware update progress (0-100)
 * @return Progress percentage
 */
uint8_t GetFirmwareUpdateProgress(void)
{
    if (fw_total_bytes == 0) {
        return 0;
    }

    return (uint8_t)((fw_received_bytes * 100) / fw_total_bytes);
}

/**
 * @brief Get firmware update state
 * @return Current state
 */
FirmwareUpdateState_t GetFirmwareUpdateState(void)
{
    return fw_update_state;
}

/**
 * @brief Reset firmware update state (for timeout/error recovery)
 */
void ResetFirmwareUpdate(void)
{
    fw_update_state = FW_IDLE;
    fw_total_bytes = 0;
    fw_received_bytes = 0;
    last_erased_sector = 0;
    // Reset encrypted mode state
    encrypted_mode = 0;
    enc_received_bytes = 0;
    memset(&sfu_header, 0, sizeof(sfu_header));
    memset(current_iv, 0, sizeof(current_iv));
    FlashLock();
}

// ============================================================================
// Bootloader v2 Functions
// ============================================================================

/**
 * @brief Check if flash has been erased
 * @return 1 if erased, 0 otherwise
 */
uint8_t IsFlashErased(void)
{
    return (last_erased_sector > 0) ? 1 : 0;
}

/**
 * @brief Get bytes received so far
 * @return Number of bytes received
 */
uint32_t GetBytesReceived(void)
{
    return fw_received_bytes;
}

/**
 * @brief Get bootloader status (v2)
 * Fills status structure with current bootloader state
 * @param status Pointer to status structure to fill
 */
void GetBootloaderStatus(BootloaderStatus_t *status)
{
    status->header = 0xAA;
    status->packet_type = PACKET_TYPE_STATUS;
    status->version_major = BOOTLOADER_VERSION_MAJOR;
    status->version_minor = BOOTLOADER_VERSION_MINOR;
    status->state = (uint8_t)fw_update_state;
    status->progress = GetFirmwareUpdateProgress();
    status->reserved = 0;
    status->bytes_received = fw_received_bytes;
    status->next_address = APPLICATION_START_ADDRESS + fw_received_bytes;
    status->flags = 0;
    if (last_erased_sector > 0) status->flags |= 0x01;  // Bit 0: flash erased
    if (fw_update_state == FW_RECEIVING) status->flags |= 0x02;  // Bit 1: transfer in progress
    if (encrypted_mode) status->flags |= 0x04;  // Bit 2: encrypted mode (v3.0)
    status->footer = 0x55;
    status->padding[0] = 0;
    status->padding[1] = 0;
    status->padding[2] = 0;
}
