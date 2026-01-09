#include <buttons.h>
#include <definitions.h>
#include <interface.h>
#include <iwdg.h>
#include <octospi.h>
#include <PSRAM.h>
#include <stdio.h>
#include <stm32_adafruit_lcd.h>
#include <stm32h733xx.h>
#include <stm32h7xx.h>
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_def.h>
#include <stm32h7xx_hal_gpio.h>
#include <string.h>
#include <sys/_stdint.h>
#include <usbd_cdc_if.h>
#include <utilities.h>
#include <W25Q64.h>

//#include "LCD/Fonts/Condor_Italic_30.h"
//#include "LCD/Fonts/condor_black.h"

#define START                   0x01
#define END                     0x02
#define ENDFILE                 0x04
#define ERASEDSECTOR            0x05
#define ACK                     0x41    //A
#define ERASEFLASH              0x43    //C
#define NAK                     0x4E    //N
#define ERROR                   0x45    //E
#define SENDLASTACKNOWLEDGEMENT 0x53    //S

extern char display_buf[];
extern volatile uint8_t usbBufferCounts;
extern volatile uint8_t packetReceived;
uint32_t initial_offset = 0;
uint32_t current_offset = 0;
uint16_t payload_size = 0;
uint16_t calculated_CRC = 0;
uint8_t acknowledgement;
uint8_t payload[2048] =
{ 0 };
uint8_t crc[2];
uint8_t usbBuffer[2048];
uint8_t read_buf[10] =
{ 0 };
uint8_t hospi_reset;
union
{
	struct
	{
		uint32_t address_1;
		uint32_t address_2;
	};

	uint8_t bytes[8];

} address_buffer;

/**
 * @brief Data to send over USB IN endpoint are sent over CDC interface
 *         through this function.
 * @param  Buf: Buffer of data to be sent
 * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
 */
static uint32_t Send_Byte(uint8_t c)
{
	CDC_Transmit_HS(&c, sizeof(c));
	return 0;
}

/**
 * @brief Calculate CRC16 for the data
 * @param  data: Buffer of data
 * @param  length: number of bytes
 * @retval Value of CRC16
 */
static uint16_t Calculate_CRC(uint8_t *data, uint16_t length)
{
	uint16_t CRC16 = 0xFFFF;
	uint16_t Poly = 0x1021;
	for (uint16_t i = 0; i < length; i++)
	{
		CRC16 ^= (data[i] << 8);   //XOR byte into the CRC register
		for (uint8_t j = 0; j < 8; j++)
		{  // Process each bit
			if (CRC16 & 0x8000)
			{				  // Check if the MSB is set
				CRC16 = (CRC16 << 1) ^ Poly;  // Shift left and XOR Polynomial
			}
			else
			{
				CRC16 = CRC16 << 1;  //Just shift left
			}
		}
	}
	return CRC16; //Final CRC
}

/**
 * @brief Erase the QSPI Chip(W25Q128)
 * @param  None
 * @retval Result of the operation: HAL_OK if all operations are OK else HAL_ERROR or HAL_BUSY
 */
static void erase_chip(void)
{
	W25Q64_OSPI_Erase_Chip(&hospi2);
	HAL_Delay(1000);
}

/**
 * @brief   Read  offset value
 * @param   None
 * @retval  None
 */
return_code_t read_offset(void)
{
	return_code_t ret_val = LS_OK;
	address_storage_t address_storage;

	if ((ret_val = read_eeram(EERAM_FLASH_OFFSET, address_storage.bytes,
	E_PTR_SIZE)) != LS_OK)
		return ret_val;
	else
		current_offset = initial_offset = address_storage.address;

	return ret_val;
}
static return_code_t write_offset(void)
{
	return_code_t ret_val;
	address_storage_t address_storage;

	address_storage.address = current_offset;
	ret_val = write_eeram(EERAM_FLASH_OFFSET, address_storage.bytes,
	E_PTR_SIZE);

	return ret_val;
}

// Display offset value (FS MODIF)
static void display_offset(void)
{
	int row = 18;
	BSP_LCD_Clear(LCD_COLOR_BLACK);
	BSP_LCD_DisplayStringAtLine(row--, (uint8_t*) " LeShuffler Image Utility");
	snprintf(display_buf, 99, " In flash: %8lu bytes", current_offset);
	BSP_LCD_DisplayStringAtLine(row--, (uint8_t*) display_buf);
	snprintf(display_buf, 99, " Current offset: %#08lx", current_offset);
	BSP_LCD_DisplayStringAtLine(row--, (uint8_t*) display_buf);
	snprintf(display_buf, 99, " Transferred: %5.1f%%",
			(float) current_offset * 100
					/ (FLASH_CURRENT_OFFSET - OCTOSPI2_BASE));
	BSP_LCD_DisplayStringAtLine(row--, (uint8_t*) display_buf);
}

/**
 * @brief Receive the data to write and store in QSPI Chip(W25Q128)
 *        update the value of the current offset in EERAM (at EERAM_FLASH_OFFSET)
 * @param  None
 * @retval None
 */

static void receive_store(void)
{
	switch (usbBuffer[0])
	{
		case START:
			//Getting Payload Size
			payload_size = (usbBuffer[1] << 8) | usbBuffer[2];

			//Getting Payload
			memcpy(payload, usbBuffer + 3, payload_size);

			//Calculating CRC16
			calculated_CRC = Calculate_CRC(payload, payload_size);

			//Getting CRC16
			uint16_t received_CRC = ((usbBuffer[2046] << 8) | usbBuffer[2047]);

			//Check
			if (calculated_CRC == received_CRC)
			{
				W25Q64_OSPI_Write(&hospi2, payload, current_offset,
						payload_size);
				current_offset += payload_size;
				memset(usbBuffer, 0, sizeof(usbBuffer));
				packetReceived = 0;
				acknowledgement = ACK;
				HAL_Delay(10);
				Send_Byte(acknowledgement);
			}
			else
			{
				acknowledgement = NAK;
				HAL_Delay(10);
				Send_Byte(acknowledgement);
			}
			break;

		case ERROR:
			//Error case to handle error that occurs during file transmission
			// Note current (non-valid) offset
			uint32_t tempOffset = current_offset;
			// Reset valid offset to stored value
			read_offset();
			//Clean the invalid portion that has been written
			if (W25Q64_OSPI_EraseSector(&hospi2, current_offset, tempOffset)
					!= HAL_OK)
			{
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, SET);
				HAL_Delay(200);
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, RESET);
				HAL_Delay(300);
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, SET);
				HAL_Delay(200);
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, RESET);
				break;
			}
			acknowledgement = ERASEDSECTOR;
			HAL_Delay(10);
			Send_Byte(acknowledgement);
			break;

		case ENDFILE:
			// Creating the Address Buffer that is to be send to excel
			uint8_t address_buffer[8];
			address_buffer[0] = initial_offset >> 24 & 0xFF;
			address_buffer[1] = initial_offset >> 16 & 0xFF;
			address_buffer[2] = initial_offset >> 8 & 0xFF;
			address_buffer[3] = initial_offset & 0xFF;
			address_buffer[4] = current_offset >> 24 & 0xFF;
			address_buffer[5] = current_offset >> 16 & 0xFF;
			address_buffer[6] = current_offset >> 8 & 0xFF;
			address_buffer[7] = current_offset & 0xFF;

			// Display current offset on screen
			display_offset();
			// Update current offset in EERAM
			write_offset();
			//Send the Address to excel file
			HAL_Delay(100);
			CDC_Transmit_HS(address_buffer, sizeof(address_buffer));

			//Reset initial_offset as current_offset
			initial_offset = current_offset;

			// Instruction to reset Flash at beginning of next file
			hospi_reset = 0;

			break;

		case SENDLASTACKNOWLEDGEMENT:
			//Sends last signal
			HAL_Delay(100);
			Send_Byte(acknowledgement);
			break;

		case ERASEFLASH:
			//Erase Flash
			erase_chip();
			read_offset();
			// Display Offset on screen
			display_offset();
			beep(MEDIUM_BEEP);
			break;

		default:

			break;
	}
}

void image_utility(void)
{
	return_code_t status;
	extern icon_set_t icon_set_check;

	// Get address of last byte of last image on flash
	if ((status = read_offset()) != LS_OK)
		LS_error_handler(status);

	// If images already loaded erase them (with warning if they seem OK)
	if (current_offset != 0)
	{
		if (current_offset == FLASH_CURRENT_OFFSET - OCTOSPI2_BASE)
		{
			return_code_t user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
					"This will erase all images\nAre you sure?", icon_set_check,
					ICON_CROSS, BUTTON_PRESS);

			// If ESC abort
			if (user_input == LS_ESC)
			{
				reset_btns();
				return;
			}
		}
		// Else proceed
		prompt_message("\nErasing image memory");

		// Init flash memory to enable erase
		W25Q64_OCTO_SPI_Init(&hospi2);
		HAL_Delay(10);

		// Erase all sectors with images
		// DEBUGGING
		W25Q64_OSPI_EraseSector(&hospi2, FLASH_FACTORY_OFFSET, current_offset);
		//	W25Q64_OSPI_Erase_Chip(&hospi2);
		// Update offset
		current_offset = initial_offset = FLASH_FACTORY_OFFSET;
		if ((status = write_offset()) != LS_OK)
			LS_error_handler(status);
	}

	// Enter receiving loop
	clear_message(TEXT_ERROR);
	prompt_message("\nReady to receive images");
	beep(MEDIUM_BEEP);
	while (1)
	{
		watchdog_refresh();

		if (packetReceived)
		{
			if (!hospi_reset)
			{
				W25Q64_OSPI_ResetChip(&hospi2);
				W25Q64_OCTO_SPI_Init(&hospi2);
				hospi_reset = 1;
			}
			receive_store();
			usbBufferCounts = 0;
			packetReceived = 0;

		}
	}

	return;
}
