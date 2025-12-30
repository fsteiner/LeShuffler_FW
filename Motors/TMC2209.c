/*
 * TCM2209_Stepper_motor.c
 *
 *  Created on: May 31, 2024
 *      Author: Radhika S
 */

#include <basic_operations.h>
#include <string.h>
#include <stdbool.h>
#include <utilities.h>

// DEBUGGING
#include <buttons.h>
#include <interface.h>
#include <TMC2209.h>
extern char display_buf[];
// END DEBUGGING

#define TMC2209_SYNC 0x05
HAL_StatusTypeDef status;

extern UART_HandleTypeDef huart3;

void setMSpins()
{
	// Set microsteps resolution
	switch (MS_FACTOR)
	{
		case 64UL:
			HAL_GPIO_WritePin(STP_MS2_PIN, 1);
			HAL_GPIO_WritePin(STP_MS1_PIN, 0);
			break;
		case 32UL:
			HAL_GPIO_WritePin(STP_MS2_PIN, 0);
			HAL_GPIO_WritePin(STP_MS1_PIN, 1);
			break;
		case 16UL:
			HAL_GPIO_WritePin(STP_MS2_PIN, 1);
			HAL_GPIO_WritePin(STP_MS1_PIN, 1);
			break;
		case 8UL:
			HAL_GPIO_WritePin(STP_MS2_PIN, 0);
			HAL_GPIO_WritePin(STP_MS1_PIN, 0);
			break;
		default:
			LS_error_handler(LS_ERROR);
	}
}

// Initialise stepper motor driver
// DEBUGGING, MINIMAL SETUP GCONF + IHOLD_IRUN WITH MS VIA UART
return_code_t tmc2209_init()
{
	return_code_t ret_val = LS_OK;
	tmc2209_gconf_t gconf_;
	tmc2209_ihold_irun_t ihold_irun_;
	tmc2209_chopconf_t chopconf_;
	tmc2209_pwmconf_t pwmconf_;
	uint32_t mRes;
	uint32_t check_bytes;
	const uint16_t n_trials_max = 15;
	uint16_t n_trials;

	switch (MS_FACTOR)
	{
		case 256UL:
			mRes = MRES_256;
			break;
		case 128UL:
			mRes = MRES_128;
			break;
		case 64UL:
			mRes = MRES_064;
			break;
		case 32UL:
			mRes = MRES_032;
			break;
		case 16UL:
			mRes = MRES_016;
			break;
		case 8UL:
			mRes = MRES_008;
			break;
		case 4UL:
			mRes = MRES_004;
			break;
		case 2UL:
			mRes = MRES_002;
			break;
		case 1UL:
			mRes = MRES_001;
			break;
		default:
			ret_val = LS_ERROR;
			goto _EXIT;
	}

	gconf_.bytes = 0;
	gconf_.i_scale_analog = I_SCALE_ANALOG_DEFAULT;
	gconf_.internal_rsense = INTERNAL_RSENSE_DEFAULT;
	gconf_.en_spread_cycle = EN_SPREAD_CYCLE_DEFAULT;
	gconf_.shaft = SHAFT_DEFAULT;
	gconf_.index_otpw = INDEX_OTPW_DEFAULT;
	gconf_.index_step = INDEX_STEP_DEFAULT;
	gconf_.pdn_disable = PDN_DISABLE_DEFAULT;
	gconf_.mstep_reg_select = MSTEP_REG_SELECT_DEFAULT;
	gconf_.multistep_filt = MULTISTEP_FILT_DEFAULT;
	gconf_.test_mode = TEST_MODE_DEFAULT;

	ihold_irun_.bytes = 0;
	ihold_irun_.ihold = IHOLD_DEFAULT;
	ihold_irun_.irun = IRUN_DEFAULT;
	ihold_irun_.iholddelay = IHOLDDELAY_DEFAULT;

	chopconf_.bytes = 0;
	chopconf_.diss2vs = DISS2VS_DEFAULT;
	chopconf_.diss2g = DISS2G_DEFAULT;
	chopconf_.dedge = DEGDE_DEFAULT;
	chopconf_.intpol = INTPOL_DEFAULT;
	chopconf_.mres = mRes;
	chopconf_.vsense = VSENSE_DEFAULT;
	chopconf_.tbl = TBL_DEFAULT;
	chopconf_.hend = HEND_DEFAULT;
	chopconf_.hstrt = HSTRT_DEFAULT;
	chopconf_.toff = TOFF_DEFAULT;

	pwmconf_.bytes = 0;
	pwmconf_.pwm_lim = PWM_LIM_DEFAULT;
	pwmconf_.pwm_reg = PWM_REG_DEFAULT;
	pwmconf_.freewheel = FREEWHEEL_DEFAULT;
	pwmconf_.pwm_autograd = PWM_AUTOGRAD_DEFAULT;
	pwmconf_.pwm_autoscale = PWM_AUTOSCALE_DEFAULT;
	pwmconf_.pwm_freq = PWM_FREQ_DEFAULT;
	pwmconf_.pwm_grad = PWM_GRAD_DEFAULT;
	pwmconf_.pwm_ofs = PWM_OFS_DEFAULT;

	// Reset registers
	carousel_disable(); 				// set ENN pin high to switch motor off
	HAL_GPIO_WritePin(STP_STDBY_PIN, GPIO_PIN_SET); // set STDBY pin high to enter standby mode
	HAL_Delay(15);										// small delay
	HAL_GPIO_WritePin(STP_STDBY_PIN, GPIO_PIN_RESET); // set STDBY pin low to exit standby mode
	carousel_enable();					// set ENN pin low to switch motor on

	// Write registers with check
	// GCONF
	n_trials = 0;
	do
	{
		if ((ret_val = tmc2209_write(ADDRESS_GCONF, gconf_.bytes)) != LS_OK)
			goto _EXIT;

		do
			check_bytes = tmc2209_read(ADDRESS_GCONF);
		while ((check_bytes == 0xffffffff));

		if (n_trials++ > n_trials_max)
		{
			ret_val = TMC2209_ERROR;
			goto _EXIT;
		}

	}
	while ((check_bytes != gconf_.bytes));

	// PWMCONF
	n_trials = 0;
	do
	{
		if ((ret_val = tmc2209_write(ADDRESS_PWMCONF, pwmconf_.bytes)) != LS_OK)
			goto _EXIT;

		do
			check_bytes = tmc2209_read(ADDRESS_PWMCONF);
		while ((check_bytes == 0xffffffff));

		if (n_trials++ > n_trials_max)
		{
			ret_val = TMC2209_ERROR;
			goto _EXIT;
		}

	}
	while ((check_bytes != pwmconf_.bytes));

	// CHOPCONF
	n_trials = 0;
	do
	{
		if ((ret_val = tmc2209_write(ADDRESS_CHOPCONF, chopconf_.bytes))
				!= LS_OK)
			goto _EXIT;

		do
			check_bytes = tmc2209_read(ADDRESS_CHOPCONF);
		while ((check_bytes == 0xffffffff));

		if (n_trials++ > n_trials_max)
		{
			ret_val = TMC2209_ERROR;
			goto _EXIT;
		}

	}
	while ((check_bytes != chopconf_.bytes));

	// IHOLD_IRUN
	if ((ret_val = tmc2209_write(ADDRESS_IHOLD_IRUN, ihold_irun_.bytes))
			!= LS_OK)
		goto _EXIT;

	// TPOWERDOWN
	if ((ret_val = tmc2209_write(ADDRESS_TPOWERDOWN, TPOWERDOWN_DEFAULT))
			!= LS_OK)
		goto _EXIT;


	_EXIT:

	return ret_val;
}

return_code_t tmc2209_set_standstill_mode(
		tmc2209_standstill_mode_t standstill_mode)
{
	tmc2209_pwmconf_t pwmconf_;
	do
		pwmconf_.bytes = tmc2209_read(ADDRESS_PWMCONF);
	while (pwmconf_.bytes == 0xFFFFFFFF);
	pwmconf_.freewheel = standstill_mode;

	return tmc2209_write(ADDRESS_PWMCONF, pwmconf_.bytes);
}

return_code_t tmc2209_set_stepper_direction(bool direction)
{
	tmc2209_gconf_t gconf_;
	uint16_t counter = 0;

	do
	{
		gconf_.bytes = tmc2209_read(ADDRESS_GCONF);
		counter++;
	}
	while (gconf_.bytes == 0xFFFFFFFF);
	gconf_.shaft = direction;
	if (counter > 1)
	{
		snprintf(display_buf, N_DISP_MAX, "%d tmc2209 reading errors", counter);
		prompt_basic_item(display_buf, 1);
	}

	return tmc2209_write(ADDRESS_GCONF, gconf_.bytes);
}

return_code_t tmc2209_write(uint8_t register_address, uint32_t data)
{
	return_code_t ret_val;
	HAL_StatusTypeDef HAL_status;
	uint32_t timeOut = 1000; //ms
	write_read_reply_datagram_t write_datagram;
	write_datagram.bytes = 0;
	write_datagram.sync = SYNC;
	write_datagram.serial_address = SERIAL_ADDRESS_0;
	write_datagram.register_address = register_address;
	write_datagram.rw = RW_WRITE;
	write_datagram.data = reverseData(data);
	write_datagram.crc = calculate_crc_write(&write_datagram,
	WRITE_READ_REPLY_DATAGRAM_SIZE);
	uint8_t datagram_bytes[8];

	// Store datagram uint64_t bytes into array of 8 bytes
	for (int i = 0; i < 8; i++)
		datagram_bytes[i] = (write_datagram.bytes >> (i * BITS_PER_BYTE))
				& BYTE_MAX_VALUE;
	// Transmit
	HAL_HalfDuplex_EnableTransmitter(&huart3);
	HAL_status = HAL_UART_Transmit(&huart3, datagram_bytes,
	WRITE_READ_REPLY_DATAGRAM_SIZE, timeOut);

	switch ((int) HAL_status)
	{
		case HAL_OK:
			ret_val = LS_OK;
			break;

		case HAL_ERROR:
			ret_val = TMC2209_ERROR;
			break;

		case HAL_BUSY:
			ret_val = TMC2209_BUSY;
			break;

		case HAL_TIMEOUT:
			ret_val = TMC2209_TIMEOUT;
			break;

		default:
			ret_val = STEPPER_ERROR;

	}

	return ret_val;

}

uint32_t tmc2209_read(uint8_t register_address)
{
	HAL_StatusTypeDef HAL_status_val;

	read_request_datagram_t read_request_datagram;
	read_request_datagram.bytes = 0;
	read_request_datagram.sync = SYNC;
	read_request_datagram.serial_address = SERIAL_ADDRESS_0;
	read_request_datagram.register_address = register_address;
	read_request_datagram.rw = RW_READ;
	read_request_datagram.crc = calculate_crc_read(&read_request_datagram,
	READ_REQUEST_DATAGRAM_SIZE);

	uint32_t timeOut = 1000;

	uint8_t datagram_bytes[WRITE_READ_REPLY_DATAGRAM_SIZE];
	for (int i = 0; i < READ_REQUEST_DATAGRAM_SIZE; ++i)
		datagram_bytes[i] = (read_request_datagram.bytes >> (i * BITS_PER_BYTE))
				& BYTE_MAX_VALUE;

	// Transmit
	HAL_HalfDuplex_EnableTransmitter(&huart3);
	HAL_UART_Transmit(&huart3, datagram_bytes,
	READ_REQUEST_DATAGRAM_SIZE, timeOut);

	uint8_t byte = 0;
	uint8_t byte_count = 0;
	write_read_reply_datagram_t read_reply_datagram;
	read_reply_datagram.bytes = 0;

	// Receive
	HAL_HalfDuplex_EnableReceiver(&huart3);
	for (uint8_t i = 0; i < WRITE_READ_REPLY_DATAGRAM_SIZE; ++i)
	{
		if ((HAL_status_val = HAL_UART_Receive(&huart3, (uint8_t*) &byte, 1,
				1000)) != HAL_OK)
			return 0xFFFFFFFF;
		else
			datagram_bytes[i] = byte;
	}
	HAL_HalfDuplex_EnableTransmitter(&huart3);

	for (uint8_t i = 0; i < WRITE_READ_REPLY_DATAGRAM_SIZE; ++i)
		read_reply_datagram.bytes |= ((uint64_t) datagram_bytes[i]
				<< (byte_count++ * BITS_PER_BYTE));

	uint32_t reversed_data = reverseData(read_reply_datagram.data);
	uint8_t crc = calculate_crc_write(&read_reply_datagram,
	WRITE_READ_REPLY_DATAGRAM_SIZE);
	if (crc != read_reply_datagram.crc)
		return 0xEEEEEEEE;
	else
		return reversed_data;
}

uint32_t reverseData(uint32_t data)
{
	uint32_t reversed_data = 0;
	uint8_t right_shift;
	uint8_t left_shift;
	for (uint8_t i = 0; i < DATA_SIZE; ++i)
	{
		right_shift = (DATA_SIZE - i - 1) * BITS_PER_BYTE;
		left_shift = i * BITS_PER_BYTE;
		reversed_data |= ((data >> right_shift) & BYTE_MAX_VALUE) << left_shift;
	}
	return reversed_data;
}

uint8_t calculate_crc_write(write_read_reply_datagram_t *datagram,
		uint8_t datagram_size)
{
	uint8_t crc = 0;
	uint8_t byte;
	uint8_t datagram_bytes[7];
	for (int i = 0; i < datagram_size - 1; i++)
	{
		datagram_bytes[i] = (datagram->bytes >> (i * BITS_PER_BYTE))
				& BYTE_MAX_VALUE;
	}
	for (uint8_t i = 0; i < (datagram_size - 1); ++i)
	{
		byte = datagram_bytes[i];
		for (uint8_t j = 0; j < BITS_PER_BYTE; ++j)
		{
			if ((crc >> 7) ^ (byte & 0x01))
			{
				crc = (crc << 1) ^ 0x07;
			}
			else
			{
				crc = crc << 1;
			}
			byte = byte >> 1;
		}
	}
	return crc;
}

uint8_t calculate_crc_read(read_request_datagram_t *datagram,
		uint8_t datagram_size)
{
	uint8_t crc = 0;
	uint8_t byte;
	uint8_t datagram_bytes[4];
	for (int i = 0; i < (datagram_size - 1); i++)
	{
		datagram_bytes[i] = (datagram->bytes >> (i * BITS_PER_BYTE))
				& BYTE_MAX_VALUE;
	}
	for (uint8_t i = 0; i < (datagram_size - 1); ++i)
	{
		byte = datagram_bytes[i];
		for (uint8_t j = 0; j < BITS_PER_BYTE; ++j)
		{
			if ((crc >> 7) ^ (byte & 0x01))
			{
				crc = (crc << 1) ^ 0x07;
			}
			else
			{
				crc = crc << 1;
			}
			byte = byte >> 1;
		}
	}
	return crc;
}

return_code_t test_carousel_motor(void)
{
	return_code_t ret_val = LS_OK;
	const uint32_t rpm = 80;

	bool toggle = false;
	extern button encoder_btn;
	extern button escape_btn;

	reset_btns();
	carousel_enable();
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		if (toggle)
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nRunning...");
			while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
				rotate_one_step(rpm);
		}
		else
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nStopped.");

		}
		toggle = !toggle;
		while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
			;
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	clear_message(TEXT_ERROR);

	return ret_val;
}

void test_carousel_driver(void)
{

	bool toggle = true;
	extern button encoder_btn;
	extern button escape_btn;

	reset_btns();
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		if (toggle)
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nDriver enabled.");
			carousel_enable();
		}
		else
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nDriver disabled.");
			carousel_disable();
		}
		toggle = !toggle;
		while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
			;
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	clear_message(TEXT_ERROR);
	carousel_enable();

	return;
}
