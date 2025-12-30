/*
 * PSRAM.h
 *
 *  Created on: Jul 22, 2024
 *      Author: Radhika S
 */
/*
@mainpage  EERAM 5V Click
@{

### Device Description ###

    The Microchip Technology Inc.
    47L04/47C04/47L16/47C16 (47XXX) is a 4/16 Kbit
    SRAM with EEPROM backup. The device is organized
    as 512 x 8 bits or 2,048 x 8 bits of memory, and
    utilizes the I2C serial interface. The 47XXX provides
    infinite read and write cycles to the SRAM while
    EEPROM cells provide high-endurance nonvolatile
    storage of data.


### Features ###

    • 4 Kbit/16 Kbit SRAM with EEPROM Backup:
        - Internally organized as 512 x 8 bits (47X04)
        or 2,048 x 8 bits (47X16)
        - Automatic Store to EEPROM array upon
        power-down (using optional external
        capacitor)
        - Automatic Recall to SRAM array upon
        power-up
        - Hardware Store pin for manual Store
        operations
        - Software commands for initiating Store and
        Recall operations
        - Store time 8 ms maximum (47X04) or
        25 ms maximum (47X16)
    • Nonvolatile External Event Detect Flag
    • High Reliability:
        - Infinite read and write cycles to SRAM
        - More than one million store cycles to
        EEPROM
        - Data retention: >200 years
        - ESD protection: >4,000V

@}
*/
/*----------------------------------------------------------------------------*/

#ifndef PSRAM_H_
#define PSRAM_H_

#include <definitions.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"
#include <stdbool.h>

// EERAM ADDRESS (8bits)
#define EERAM_I2C_ADDRESS    		  0xA0  // Replace with the correct I2C address from the datasheet
#define EERAM_ADDRESS_CONTROL         0x30
#define EERAM_CHIPADDRESS_BASE_MASK   0xF0 //!< Base chip address mask
#define EERAM_CHIPADDRESS_MASK        0xFC //!< Chip address mask

#define EERAM_STORE_TIMEOUT           25 //!< Store Operation Duration: 25ms
#define EERAM_RECALL_TIMEOUT          5  //!< Recall Operation Duration: 5ms
#define EERAM_CMD_STORE   			  0b00110011 //!< Command to store SRAM data to EEPROM
#define EERAM_CMD_RECALL  			  0b11011101 //!< Command to recall data from EEPROM to SRAM

//#define A2 								1 // Chip Select bit A2
//#define A1 								0 // Chip Select bit A1
// Status Register address
#define EERAM_STATUS_REG_ADDR 		  0x00  // Assuming 8-bit addressing for the status register
#define EERAM_COMMAND_REG_ADDR  	  0x55  //!< Address to the command register

// Define the Page Size and number of pages
#define PAGE_NUM             512    // number of pages
#define EERAM_PAGE_SIZE      64    // Page size for the 47L16
#define EERAM_MEMORY_SIZE    2048  // //!< 47x16 total memory size Total memory size in bytes (16 Kbits / 8 bits)
#define EERAM_ASE_ENABLE      (0x1u << 1) //!< Enable Auto-Store feature
#define EERAM_ASE_DISABLE     (0x0u << 1) //!< Disable Auto-Store feature
#define EERAM_BP_Pos          2
#define EERAM_BP_Mask         (0x7u << EERAM_BP_Pos)
#define EERAM_BP_SET(value)   (((uint8_t)(value) << EERAM_BP_Pos) & EERAM_BP_Mask) //!< Set Block Protect bits
#define EERAM_BP_GET(value)   (((uint8_t)(value) & EERAM_BP_Mask) >> EERAM_BP_Pos) //!< Get Block Protect bits
#define EERAM_ARRAY_MODIFIED  (0x1u << 7) //!< SRAM array has been modified

// EERAM Status Register
typedef union {
    uint8_t Status;
    struct {
        uint8_t EVENT: 1; //!< Event detect bit
        uint8_t ASE  : 1; //!< Auto-Store Enable bit
        uint8_t BP   : 3; //!< Block Protect bits
        uint8_t      : 2; //!< Reserved
        uint8_t AM   : 1; //!< Array Modified bit
    } Bits;
} EERAM_Status_Register;

typedef union
{
    uint32_t address;
    uint8_t bytes[E_PTR_SIZE];
} address_storage_t;

// Block Protect enumeration
typedef enum {
    EERAM_NO_WRITE_PROTECT  = 0b000, //!< No write protect
    EERAM_PROTECT_7E0h_7FFh = 0b001, //!< Protect range: Upper 1/64
    EERAM_PROTECT_7C0h_7FFh = 0b010, //!< Protect range: Upper 1/32
    EERAM_PROTECT_780h_7FFh = 0b011, //!< Protect range: Upper 1/16
    EERAM_PROTECT_700h_7FFh = 0b100, //!< Protect range: Upper 1/8
    EERAM_PROTECT_600h_7FFh = 0b101, //!< Protect range: Upper 1/4
    EERAM_PROTECT_400h_7FFh = 0b110, //!< Protect range: Upper 1/2
    EERAM_PROTECT_000h_7FFh = 0b111, //!< Protect range: All blocks
} EERAM_BlockProtect;

HAL_StatusTypeDef EERAM_ReadSRAMData(I2C_HandleTypeDef *hi2c, uint16_t address, uint8_t* data, uint16_t size);
HAL_StatusTypeDef EERAM_WriteSRAMData(I2C_HandleTypeDef *hi2c, uint16_t address, uint8_t* data, uint16_t size);
HAL_StatusTypeDef EERAM_StoreSRAMtoEEPROM(I2C_HandleTypeDef *hi2c,  bool force, bool waitEndOfStore);
HAL_StatusTypeDef EERAM_RecallEEPROMtoSRAM(I2C_HandleTypeDef *hi2c, bool waitEndOfRecall);
HAL_StatusTypeDef EERAM_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t* data);
HAL_StatusTypeDef EERAM_WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t data);
HAL_StatusTypeDef EERAM_ActivateAutoStore(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef EERAM_DeactivateAutoStore(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef EERAM_GetStatus(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef EERAM_HardwareStore(I2C_HandleTypeDef *hi2c, uint16_t devAddr);

#endif /* PSRAM_H_ */
