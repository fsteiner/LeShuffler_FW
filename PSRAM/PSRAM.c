/*
 * PSRAM.c
 *
 *  Created on: Jul 22, 2024
 *      Author: Radhika S
 */

#include "PSRAM.h"
#include "math.h"
#include "string.h"

/** @brief Read SRAM data from the EERAM device
 * @param[in] hi2c Pointer to the I2C handle
 * @param[in] devAddr I2C address of the EERAM device
 * @param[in] address Address to read from
 * @param[out] data Buffer to store the read data
 * @param[in] size Number of bytes to read
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_ReadSRAMData(I2C_HandleTypeDef *hi2c, uint16_t address, uint8_t* data, uint16_t size)
{
    if (hi2c == NULL || data == NULL || size == 0) {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(hi2c, EERAM_I2C_ADDRESS, address, I2C_MEMADD_SIZE_16BIT, data, size, HAL_MAX_DELAY);

}

/** @brief Write SRAM data to the EERAM device
 * @param[in] hi2c Pointer to the I2C handle
 * @param[in] devAddr I2C address of the EERAM device
 * @param[in] address Address to write to
 * @param[in] data Buffer containing the data to write
 * @param[in] size Number of bytes to write
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_WriteSRAMData(I2C_HandleTypeDef *hi2c, uint16_t address, uint8_t* data, uint16_t size)
{
    if (hi2c == NULL || data == NULL || size == 0) {
        return HAL_ERROR;
    }

    // Prepare the address and data to send
	  uint8_t buffer[size + 2];
	  buffer[0] = (uint8_t)(address >> 8); // High byte of address
	  buffer[1] = (uint8_t)(address);      // Low byte of address
	  memcpy(&buffer[2], data, size);      // Copy the data to buffer

	  // Perform the I2C transmission
	 return HAL_I2C_Master_Transmit(hi2c, EERAM_I2C_ADDRESS, buffer, size + 2, HAL_MAX_DELAY);
}


/** @brief Store SRAM data to EEPROM
 * @param[in] hi2c Pointer to the I2C handle
 * @param[in] devAddr I2C address of the EERAM device
 * @param[in] force If true, force the store operation
 * @param[in] waitEndOfStore If true, wait for the store operation to complete
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_StoreSRAMtoEEPROM(I2C_HandleTypeDef *hi2c, bool force, bool waitEndOfStore)
{
	uint8_t command = EERAM_CMD_STORE;
	HAL_StatusTypeDef status;

	// Write the store command to the command register
	status = HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_COMMAND_REG_ADDR, I2C_MEMADD_SIZE_8BIT, &command, 1, HAL_MAX_DELAY);
	if (status != HAL_OK) {
		return status;
	}

	// Wait for the store operation to complete if required
	if (waitEndOfStore) {
		HAL_Delay(25); // Adjust based on datasheet
	}

	return HAL_OK;

}

/** @brief Recall EEPROM data to SRAM
 * @param[in] hi2c Pointer to the I2C handle
 * @param[in] devAddr I2C address of the EERAM device
 * @param[in] waitEndOfRecall If true, wait for the recall operation to complete
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_RecallEEPROMtoSRAM(I2C_HandleTypeDef *hi2c, bool waitEndOfRecall)
{
    HAL_StatusTypeDef status;
    uint8_t command = EERAM_CMD_RECALL;

    // Write the recall command to the control register
    status = HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_COMMAND_REG_ADDR, I2C_MEMADD_SIZE_8BIT, &command, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status; // Return error if the command couldn't be written
    }

    // Wait for the recall operation to complete if required
    if (waitEndOfRecall) {
        HAL_Delay(25); // Adjust delay as needed based on the datasheet
    }

    return HAL_OK;
}


HAL_StatusTypeDef EERAM_ActivateAutoStore(I2C_HandleTypeDef *hi2c)
{
	EERAM_Status_Register status;
	HAL_StatusTypeDef ret = EERAM_ReadRegister(hi2c, &status.Status);
	if (ret != HAL_OK) {
		return ret;
	}
	status.Status |= (1 << 1);
	HAL_Delay(150);

	return HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_STATUS_REG_ADDR, I2C_MEMADD_SIZE_16BIT, &status.Status, 1, HAL_MAX_DELAY);
}

/** @brief Deactivate Auto-Store
 * @param[in] heeprom Pointer to the EERAM handle
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_DeactivateAutoStore(I2C_HandleTypeDef *hi2c)
{
	EERAM_Status_Register status;
	HAL_StatusTypeDef ret = EERAM_ReadRegister(hi2c, &status.Status);
	if (ret != HAL_OK) {
		return ret;
	}

	status.Status &= ~(1 << 1); // Clear ASE bit to disable Auto-Store
	return HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_STATUS_REG_ADDR, I2C_MEMADD_SIZE_16BIT, &status.Status, 1, HAL_MAX_DELAY);
}

/** @brief Trigger a Hardware Store operation
 * @param[in] hi2c Pointer to the I2C handle
 * @param[in] devAddr I2C address of the EERAM device
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_HardwareStore(I2C_HandleTypeDef *hi2c, uint16_t devAddr) {
	EERAM_Status_Register status;
	HAL_StatusTypeDef ret = EERAM_ReadRegister(hi2c, &status.Status);
    if (ret != HAL_OK) {
        return ret;
    }
    HAL_Delay(20);
    if (status.Status & (1 << 7)) { // Check if AM bit is set
        // Trigger Hardware Store operation
        // Set HS pin high for THSPW time here
    } else {
        // Only update EVENT bit
    	status.Status |= (1 << 0); // Set EVENT bit
        return HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_STATUS_REG_ADDR, I2C_MEMADD_SIZE_16BIT, &status.Status, 1, HAL_MAX_DELAY);
    }

    return HAL_OK;
}


/** @brief Get device status
 * @param[in] heeprom Pointer to the EERAM handle
 * @param[out] status Pointer to store the status register data
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_GetStatus(I2C_HandleTypeDef *hi2c)
{
    EERAM_Status_Register status = {0};
    HAL_StatusTypeDef ret = EERAM_ReadRegister(hi2c, &status.Status);
    return ret;
}

/** @brief Read a register from the EERAM device
 * @param[in] heeprom Pointer to the EERAM handle
 * @param[out] data Buffer to store the register data
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t* data)
{
    if (hi2c == NULL || data == NULL) {
        return HAL_ERROR;
    }
return HAL_I2C_Master_Receive(hi2c, EERAM_ADDRESS_CONTROL, data, 1, HAL_MAX_DELAY);
}


/** @brief Write a register in the EERAM device
 * @param[in] heeprom Pointer to the EERAM handle
 * @param[in] address Register address
 * @param[in] data Data to write
 * @return HAL status
 */
HAL_StatusTypeDef EERAM_WriteRegister(I2C_HandleTypeDef *hi2c,uint8_t data)
{
    if (hi2c == NULL) {
        return HAL_ERROR;
    }
       return HAL_I2C_Mem_Write(hi2c, EERAM_ADDRESS_CONTROL, EERAM_STATUS_REG_ADDR, I2C_MEMADD_SIZE_16BIT, &data, 1, HAL_MAX_DELAY);
}
