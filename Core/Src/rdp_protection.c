/**
 * @file rdp_protection.c
 * @brief Read Protection (RDP) Level 1 auto-setter implementation
 */

#include "rdp_protection.h"

/**
 * @brief Get current RDP level from option bytes
 */
uint32_t RDP_GetLevel(void)
{
    FLASH_OBProgramInitTypeDef ob_config;
    HAL_FLASHEx_OBGetConfig(&ob_config);
    return ob_config.RDPLevel;
}

/**
 * @brief Check if device is protected
 */
uint8_t RDP_IsProtected(void)
{
    return (RDP_GetLevel() != OB_RDP_LEVEL_0) ? 1 : 0;
}

/**
 * @brief Check and set RDP Level 1 if not already protected
 *
 * This function checks the current RDP level. If the device is at
 * RDP Level 0 (unprotected), it programs RDP Level 1 and triggers
 * a system reset. The device will reboot and this function will
 * detect RDP1 is already set, skipping the programming.
 */
void RDP_CheckAndProtect(void)
{
    /* Read current RDP level */
    if (RDP_GetLevel() == OB_RDP_LEVEL_0)
    {
        /* Device is unprotected - set RDP Level 1 */

        /* Unlock flash and option bytes */
        HAL_FLASH_Unlock();
        HAL_FLASH_OB_Unlock();

        /* Configure RDP Level 1 */
        FLASH_OBProgramInitTypeDef ob_config = {0};
        ob_config.OptionType = OPTIONBYTE_RDP;
        ob_config.RDPLevel = OB_RDP_LEVEL_1;

        /* Program the option bytes */
        HAL_StatusTypeDef status = HAL_FLASHEx_OBProgram(&ob_config);

        if (status == HAL_OK)
        {
            /* Launch option byte loading - triggers system reset */
            /* This function does not return */
            HAL_FLASH_OB_Launch();
        }
        else
        {
            /* Programming failed - lock and continue */
            /* Device remains unprotected but functional */
            HAL_FLASH_OB_Lock();
            HAL_FLASH_Lock();
        }
    }
    /* If already RDP1 or RDP2 - do nothing, continue normal boot */
}
