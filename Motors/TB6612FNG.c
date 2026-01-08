/*
 * DC_Motor_TB6612FNG.c
 *
 *  Created on: Jun 10, 2024
 *      Author: Radhika S
 */
#include <basic_operations.h>
#include <buttons.h>
#include <interface.h>
#include <iwdg.h>
#include <servo_motor.h>
#include <TB6612FNG.h>

extern dc_motor_t latch;
extern dc_motor_t tray_motor;
extern dc_motor_t entry_motor;

/**
 *@brief DCmotor_Init
 * Initialise DCmotor, set PWM to 100% and put driver in standby
 *@param motor
 *@param GPIOport_1 		port of first command pin
 *@param GPIOpin_1 		first command pin
 *@param GPIOport_2		port of second command pin
 *@param GPIOpin_2		second command pin
 *@param GPIOportStdby	port of standby pin
 *@param GPIOpinStdby	standby pin
 *@param htim			associated PWM timer
 *@param channel		PWM channel
 *@retval None
 */
void DCmotorInit(dc_motor_t *motor, uint8_t ID, GPIO_TypeDef *GPIOport_1,
		uint16_t GPIOpin_1, GPIO_TypeDef *GPIOport_2, uint16_t GPIOpin_2,
		GPIO_TypeDef *GPIOportStdby, uint16_t GPIOpinStdby,
		TIM_HandleTypeDef *htim, uint32_t channel)
{
	//Initialise motor
	motor->ID = ID;
	motor->GPIOport_1 = GPIOport_1;
	motor->GPIOpin_1 = GPIOpin_1;
	motor->GPIOport_2 = GPIOport_2;
	motor->GPIOpin_2 = GPIOpin_2;
	motor->GPIOportStdby = GPIOportStdby;
	motor->GPIOpinStdby = GPIOpinStdby;
	motor->htim = htim;
	motor->channel = channel;
	// Set driver on standby
	HAL_GPIO_WritePin(motor->GPIOportStdby, motor->GPIOpinStdby,
			GPIO_PIN_RESET);
	// Set duty cycle to 0% by default
	switch (motor->channel)
	{
		case TIM_CHANNEL_1:
			motor->htim->Instance->CCR1 = 0;
			break;
		case TIM_CHANNEL_2:
			motor->htim->Instance->CCR2 = 0;
			break;
		case TIM_CHANNEL_3:
			motor->htim->Instance->CCR3 = 0;
			break;
		case TIM_CHANNEL_4:
			motor->htim->Instance->CCR4 = 0;
			break;
		default:
			LS_error_handler(LS_ERROR);
	}
	return;
}

void setDutyCycle(dc_motor_t motor, uint32_t dutyCycle)
{
	// Normalise
	dutyCycle = min(dutyCycle, motor.htim->Instance->ARR);

	switch (motor.channel)
	{
		case TIM_CHANNEL_1:
			motor.htim->Instance->CCR1 = dutyCycle;
			break;
		case TIM_CHANNEL_2:
			motor.htim->Instance->CCR2 = dutyCycle;
			break;
		case TIM_CHANNEL_3:
			motor.htim->Instance->CCR3 = dutyCycle;
			break;
		case TIM_CHANNEL_4:
			motor.htim->Instance->CCR4 = dutyCycle;
			break;
		default:
			LS_error_handler(LS_ERROR);
	}

	return;
}
/**
 *@brief dc_motor_enable
 * Neutralise command pins, start PWM, get driver out of standby
 *@param motor
 *@retval None
 */
void dc_motor_enable(dc_motor_t motor)
{
	// Set duty cycle to 0
	setDutyCycle(motor, 0);
	// Neutralise command pins
	HAL_GPIO_WritePin(motor.GPIOport_1, motor.GPIOpin_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor.GPIOport_2, motor.GPIOpin_2, GPIO_PIN_RESET);
	// Start PWM generation
	HAL_TIM_PWM_Start(motor.htim, motor.channel);
	// Get driver out of standby
	HAL_GPIO_WritePin(motor.GPIOportStdby, motor.GPIOpinStdby, GPIO_PIN_SET);

	return;
}

/*
 *@brief DCmotorDisable
 * Put driver in standby, neutralise command pins, stop timer
 *@param motor
 *@retval None
 */
void dc_motor_disable(dc_motor_t motor)
{
	// Neutralise command pins
	HAL_GPIO_WritePin(motor.GPIOport_1, motor.GPIOpin_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor.GPIOport_2, motor.GPIOpin_2, GPIO_PIN_RESET);
	// Stop PWM generation
	HAL_TIM_PWM_Stop(motor.htim, motor.channel);
	// Get driver in standby
	HAL_GPIO_WritePin(motor.GPIOportStdby, motor.GPIOpinStdby, GPIO_PIN_RESET);

	return;
}

void dc_motor_freewheel(dc_motor_t motor)
{
	// Neutralise command pins (leave PWM on)
	HAL_GPIO_WritePin(motor.GPIOport_1, motor.GPIOpin_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor.GPIOport_2, motor.GPIOpin_2, GPIO_PIN_RESET);

	return;
}

/**
 *@brief set_motor_rotation
 * Set the rotation of DC Motor
 *@param motor
 *@param speed set speed and direction
 *@retval None
 */
void set_motor_rotation(dc_motor_t motor, uint16_t speed, bool direction)
{
	// Set duty cycle
	setDutyCycle(motor, (motor.htim->Instance->ARR + 1) * speed / 100);
	// Energise command pin based on direction
	if (direction == DC_MTR_FWD)
		HAL_GPIO_WritePin(motor.GPIOport_2, motor.GPIOpin_2, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(motor.GPIOport_1, motor.GPIOpin_1, GPIO_PIN_SET);

	return;
}

void clean_roller(dc_motor_t dc_motor)
{
	bool DC_toggle = true;
	extern button encoder_btn;
	extern button escape_btn;
	uint16_t clean_speed = 30;


	reset_btns();
	dc_motor_enable(dc_motor);
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		if (DC_toggle)
		{
			set_motor_rotation(dc_motor, clean_speed, DC_MTR_FWD);
			clear_message(TEXT_ERROR);
			prompt_message("\nRunning...");
		}
		else
		{
			dc_motor_freewheel(dc_motor);
			clear_message(TEXT_ERROR);
			prompt_message("\nStopped.");
		}
		DC_toggle = !DC_toggle;
		while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
			;
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	dc_motor_freewheel(dc_motor);
	clear_message(TEXT_ERROR);

	return;
}
void load_enable(void)
{
	dc_motor_enable(tray_motor);
	dc_motor_enable(entry_motor);
	return;
}

void load_disable(void)
{
	dc_motor_disable(tray_motor);
	dc_motor_disable(entry_motor);
	return;
}

void set_latch(bool status)
{
	extern bool latch_position;
	if (status != latch_position)
	{
		dc_motor_enable(latch);
		set_motor_rotation(latch,
				status == LATCH_OPEN ? SOLENOID_PCT_OPEN : SOLENOID_PCT_CLOSE,
				status);
		HAL_Delay(status == LATCH_OPEN ?
		SOLENOID_IMPULSE_OPEN : SOLENOID_IMPULSE_CLOSE);
		set_motor_rotation(latch, 0, status);
		latch_position = !latch_position;
	}

	return;
}


void test_exit_latch(void)
{
	bool latch_position = LATCH_OPEN;
	extern button encoder_btn;
	extern button escape_btn;

	reset_btns();
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	while (!escape_btn.interrupt_press
			&& !encoder_btn.interrupt_press)
	{
		if (latch_position == LATCH_OPEN)
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nOpen.");
			set_latch(latch_position);

		}
		else
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nClosed.");
			set_latch(latch_position);
		}
		latch_position = !latch_position;
		while (!escape_btn.interrupt_press
				&& !encoder_btn.interrupt_press)
			;
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	set_latch(LATCH_CLOSED);
	clear_message(TEXT_ERROR);

	return;
}

void access_exit_chute(void)
{
	bool latch_position = LATCH_CLOSED;
	extern button encoder_btn;
	extern button escape_btn;

	reset_btns();
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	while (!escape_btn.interrupt_press
			&& !encoder_btn.interrupt_press)
	{
		if (latch_position == LATCH_OPEN)
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nOpen.");
			set_latch(latch_position);
			flap_open();

		}
		else
		{
			clear_message(TEXT_ERROR);
			prompt_message("\nClosed.");
			set_latch(latch_position);
			flap_close();
		}
		latch_position = !latch_position;
		while (!escape_btn.interrupt_press
				&& !encoder_btn.interrupt_press)
			;
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	clear_message(TEXT_ERROR);

	return;
}

void dc_motors_run_in(void)
{
	bool DC_toggle = true;
	extern button encoder_btn;
	extern button escape_btn;
	extern dc_motor_t tray_motor;
	extern dc_motor_t entry_motor;
	uint32_t start_time;
	uint32_t remaining_time;
	uint32_t prev_remaining_time;
	const uint32_t running_time = 60000;
	char display_buf[N_DISP_MAX + 1];
	const uint16_t rodage_speed = 70;

	load_enable();
	carousel_enable();
	// Display icons
	display_encoder_icon(ICON_CHECK);
	display_escape_icon(ICON_CROSS);
	reset_btns();
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		if (DC_toggle)
		{
			start_time = HAL_GetTick();
			set_motor_rotation(tray_motor, rodage_speed, DC_MTR_FWD);
			set_motor_rotation(entry_motor, rodage_speed, DC_MTR_FWD);
			clear_message(TEXT_ERROR);
			prev_remaining_time = 0;
			do
			{
				watchdog_refresh();
				remaining_time =
						HAL_GetTick() < start_time + running_time ?
								(start_time + running_time - HAL_GetTick())
										/ 1000 :
								0;

				snprintf(display_buf, N_DISP_MAX, "Running In Roller Motors\n%2lu s",
						remaining_time);
				// y position for the second line (where countdown appears)
				uint16_t y_pos = LCD_TOP_ROW
						+ LCD_ROW_HEIGHT * (MSG_ROW + 1) + V_ADJUST;

				if (remaining_time != prev_remaining_time)
				{
					// Clear the countdown line before redrawing
					BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
					BSP_LCD_FillRect(0, y_pos, BUTTON_ICON_X, LCD_ROW_HEIGHT);
					BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
					prompt_message(display_buf);
					prev_remaining_time = remaining_time;
				}
				if (escape_btn.interrupt_press || encoder_btn.interrupt_press)
					break;

			}
			while (remaining_time > 0);

			// Stop
			dc_motor_freewheel(tray_motor);
			dc_motor_freewheel(entry_motor);
			clear_message(TEXT_ERROR);
			prompt_message("Stopped.");

		}
		else
		{
			// Stop
			dc_motor_freewheel(tray_motor);
			dc_motor_freewheel(entry_motor);
			clear_message(TEXT_ERROR);
			prompt_message("Stopped.");
		}
		DC_toggle = !DC_toggle;
		while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
			watchdog_refresh();
		reset_btn(&encoder_btn);

	}
	reset_btn(&escape_btn);
	dc_motor_freewheel(tray_motor);
	dc_motor_freewheel(entry_motor);
	clear_message(TEXT_ERROR);

	return;
}



