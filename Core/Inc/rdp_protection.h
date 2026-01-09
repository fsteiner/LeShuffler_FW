/**
 * @file rdp_protection.h
 * @brief Read Protection (RDP) Level 1 auto-setter
 *
 * Automatically sets RDP Level 1 on first boot if device is unprotected.
 * This prevents firmware from being read via ST-LINK debugger.
 *
 * RDP Levels:
 *   - Level 0: No protection (development)
 *   - Level 1: Flash read protected, reversible via mass erase
 *   - Level 2: Permanent protection (IRREVERSIBLE - not used here)
 *
 * Behavior:
 *   - If RDP0: Sets RDP1 and triggers system reset
 *   - If RDP1: Does nothing, continues normal boot
 *   - Runs once per power cycle, idempotent
 */

#ifndef INC_RDP_PROTECTION_H_
#define INC_RDP_PROTECTION_H_

#include "stm32h7xx_hal.h"

/**
 * @brief Check and set RDP Level 1 if not already protected
 *
 * Call this early in main() after HAL_Init() and SystemClock_Config().
 * If device is at RDP Level 0, this function sets RDP Level 1 and
 * triggers a system reset. The function will not return in that case.
 *
 * If device is already at RDP Level 1 or higher, this function
 * returns immediately and normal boot continues.
 *
 * @note This function may trigger a system reset and not return.
 */
void RDP_CheckAndProtect(void);

/**
 * @brief Get current RDP level
 * @return Current RDP level (OB_RDP_LEVEL_0, OB_RDP_LEVEL_1, or OB_RDP_LEVEL_2)
 */
uint32_t RDP_GetLevel(void);

/**
 * @brief Check if device is protected (RDP Level 1 or higher)
 * @return 1 if protected, 0 if not protected
 */
uint8_t RDP_IsProtected(void);

#endif /* INC_RDP_PROTECTION_H_ */
