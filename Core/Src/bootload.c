/**
 * STM32H7V33VGT Bootloader Jump Implementation
 * Allows entering system bootloader without BOOT pin access
 */

#include "stm32h7xx_hal.h"
#include "utilities.h"

/**
 * @brief External peripheral handle declarations
 */
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart3;
extern I2C_HandleTypeDef hi2c3;
extern OSPI_HandleTypeDef hospi2;
extern RNG_HandleTypeDef hrng;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim12;
extern TIM_HandleTypeDef htim15;

// Forward declarations of EERAM read/write functions
extern return_code_t read_eeram(uint16_t address, uint8_t *data, uint16_t size);
extern return_code_t write_eeram(uint16_t address, uint8_t *data, uint16_t size);

// System memory bootloader address for STM32H7
#define BOOTLOADER_ADDRESS  0x1FF09800  // STM32H7V33VGT and STM32H743/753
// Note: STM32H7A3/7B3 use 0x1FF0A000
// Note: STM32H72x/73x use 0x1FF09000

/**
 * @brief Deinitialize all initialized peripherals
 * Must match your MX_xxx_Init() calls
 */
static void DeinitializePeripherals(void)
{
    // Disable EXTI interrupts first (from your GPIO init)
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);      // ESC_BTN (PC0)
    HAL_NVIC_DisableIRQ(EXTI1_IRQn);      // ENT_BTN (PC1)
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);    // IR_IN (PE8)
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);  // STP_DIAG (PE10)

    // Deinitialize USB Device first (important for USB bootloader)
    // USB peripheral will be reinitialized by bootloader
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12); // USB_DM, USB_DP

    // Deinitialize UART5 (may interfere with bootloader UART detection)
    HAL_UART_DeInit(&huart5);
    __HAL_RCC_UART5_CLK_DISABLE();

    // Deinitialize USART3
    HAL_UART_DeInit(&huart3);
    __HAL_RCC_USART3_CLK_DISABLE();

    // Deinitialize Timers
    HAL_TIM_Base_DeInit(&htim2);
    __HAL_RCC_TIM2_CLK_DISABLE();

    HAL_TIM_Base_DeInit(&htim3);
    __HAL_RCC_TIM3_CLK_DISABLE();

    HAL_TIM_Base_DeInit(&htim12);
    __HAL_RCC_TIM12_CLK_DISABLE();

    HAL_TIM_Base_DeInit(&htim15);
    __HAL_RCC_TIM15_CLK_DISABLE();

    // Deinitialize RNG
    HAL_RNG_DeInit(&hrng);
    __HAL_RCC_RNG_CLK_DISABLE();

    // Deinitialize OCTOSPI2 (includes OSPIM configuration)
    HAL_OSPI_DeInit(&hospi2);
    __HAL_RCC_OSPI2_CLK_DISABLE();
    // Note: OCTOSPI2 GPIO pins are auto-configured by CubeMX based on your OSPIM port settings
    // They will be reset to analog with the bulk GPIO reset below

    // Deinitialize I2C3
    HAL_I2C_DeInit(&hi2c3);
    __HAL_RCC_I2C3_CLK_DISABLE();
    // Deinitialize I2C3 GPIO pins (SCL: PA8, SDA: PC9)
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8);  // I2C_SCL
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_9);  // I2C_SDA

    // Deinitialize all GPIO pins with EXTI interrupts
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0 | GPIO_PIN_1);    // ESC_BTN, ENT_BTN
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_8 | GPIO_PIN_10);   // IR_IN, STP_DIAG

    // Reset all GPIO to analog input (low power, high impedance)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Pin = GPIO_PIN_All;

    // Only deinitialize GPIO ports that were actually initialized
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    // Disable GPIO clocks (only ports that were enabled in your init)
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();
    __HAL_RCC_GPIOE_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();
}

/**
 * @brief Prepare system for bootloader jump
 * Disables all interrupts and peripherals
 */
static void PrepareForBootloaderJump(void)
{
    // Disable all interrupts
    __disable_irq();

    // Disable SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // Deinitialize all peripherals
    DeinitializePeripherals();

    // CRITICAL: Configure USB pins to signal USB connection to bootloader
    // The bootloader checks PA12 (USB_DP) state to detect USB presence
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure PA9 (USB_OTG_FS_VBUS) if available - for VBUS detection
    // Some bootloaders check VBUS to enable USB
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // Simulate VBUS present
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure PA12 (USB_DP) as input with pull-down then pull-up
    // This creates a "disconnect-reconnect" sequence the bootloader expects
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);  // Pull low
    for (volatile uint32_t i = 0; i < 50000; i++);  // Delay for USB disconnect

    // Now configure as input with pull-up for bootloader detection
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure PA11 (USB_DM) as input with no pull
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Small delay to let pins settle
    for (volatile uint32_t i = 0; i < 10000; i++);

    // Keep GPIO clock enabled - bootloader needs it for USB detection!
    // Don't disable it this time

    // Disable and invalidate instruction cache
    SCB_DisableICache();
    SCB_InvalidateICache();

    // Disable and clean data cache
    SCB_DisableDCache();
    SCB_CleanInvalidateDCache();

    // Disable MPU if enabled
    #if defined(__MPU_PRESENT) && (__MPU_PRESENT == 1U)
    if (MPU->CTRL & MPU_CTRL_ENABLE_Msk)
    {
        HAL_MPU_Disable();
    }
    #endif

    // Deinitialize HAL
    HAL_RCC_DeInit();
    HAL_DeInit();

    // Clear all interrupt enable registers
    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // Disable all fault handlers
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                    SCB_SHCSR_BUSFAULTENA_Msk |
                    SCB_SHCSR_MEMFAULTENA_Msk);
}

/**
 * @brief Boot into system bootloader using clean reset
 * This method is more reliable for USB DFU detection
 */
void JumpToBootloader_SystemReset(void)
{
    // First, fully deinitialize everything
    PrepareForBootloaderJump();

    // Small delay to ensure everything is settled
    for (volatile uint32_t i = 0; i < 100000; i++);

    // Now do the actual jump to bootloader using the direct method
    // but with a completely clean system state
    uint32_t bootloader_stack = *((uint32_t *)BOOTLOADER_ADDRESS);
    uint32_t bootloader_entry = *((uint32_t *)(BOOTLOADER_ADDRESS + 4));
    void (*BootloaderEntry)(void) = (void (*)(void))bootloader_entry;

    // Set Main Stack Pointer
    __set_MSP(bootloader_stack);

    // Set Vector Table Offset Register to bootloader
    SCB->VTOR = BOOTLOADER_ADDRESS;

    // Memory barriers
    __DSB();
    __ISB();

    // Jump to bootloader
    BootloaderEntry();

    // Never reached
    while(1);
}



/**
 * @brief Call this in main() AFTER I2C3 initialization
 * For use with the persistent flag method using external 47C16 EERAM
 *
 * IMPORTANT: This must be called AFTER MX_I2C3_Init() because it needs
 * I2C3 to communicate with the external 47C16 chip
 *
 * @return return_code_t Status of the check operation
 */
return_code_t EarlyBootloaderCheck(void)
{
    return_code_t ret_val;
    union
    {
        uint32_t value;
        uint8_t bytes[4];
    } flag;

    // Read flag from 47C16
    if ((ret_val = read_eeram(EERAM_BOOTLOADER_FLAG, flag.bytes, 4)) == LS_OK)
    {
        // Check if bootloader flag is set
        if (flag.value == BOOTLOADER_MAGIC_VALUE)
        {
            // Clear flag in EERAM
            flag.value = 0;
            if ((ret_val = write_eeram(EERAM_BOOTLOADER_FLAG, flag.bytes, 4)) == LS_OK)
            {
                // Jump to bootloader using system reset method for reliable USB DFU
                JumpToBootloader_SystemReset();
            }
        }
    }

    return ret_val;
}




