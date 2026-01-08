/*
 * Servo_Motor.c
 *
 *  Created on: Jun 26, 2024
 *      Author: Radhika S
 */

#include <basic_operations.h>
#include <buttons.h>
#include <servo_motor.h>
#include <utilities.h>
#include <interface.h>

extern servo_motor_t flap;
uint32_t flap_open_pos;
uint32_t flap_closed_pos;
uint32_t flap_mid_pos;

/**
 *@brief servo_motor_init
 * Initialization of DC Motor
 *
 *@param *pServo 	Servo Motor handle Structure definition
 *@param htim		TIM handle
 *@param Channel 	TIM Channels to be enabled
 *          This parameter can be one of the following values:
 *            @arg TIM_CHANNEL_1: TIM Channel 1 selected
 *            @arg TIM_CHANNEL_2: TIM Channel 2 selected
 *            @arg TIM_CHANNEL_3: TIM Channel 3 selected
 *            @arg TIM_CHANNEL_4: TIM Channel 4 selected
 *            @arg TIM_CHANNEL_5: TIM Channel 5 selected
 *            @arg TIM_CHANNEL_6: TIM Channel 6 selected
 *
 *@retval None
 */
void servo_motor_init(servo_motor_t *pServo, TIM_HandleTypeDef *htim,
        uint32_t channel)
{
    pServo->htim = htim;
    pServo->channel = channel;

    return;
}
// Similar to set_flap but without the intermediary steps (if there already)

return_code_t flap_init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    return_code_t ret_val = LS_OK;
    uint8_t r_data;

    if ((ret_val = read_eeram(EERAM_FLAP_OPEN, &r_data, 1)) != LS_OK)
        goto _EXIT;
    flap_open_pos = (uint32_t) FLAP_FACTOR * r_data;
    if ((ret_val = read_eeram(EERAM_FLAP_CLOSED, &r_data, 1)) != LS_OK)
        goto _EXIT;
    flap_closed_pos = (uint32_t) FLAP_FACTOR * r_data;
    flap_mid_pos = flap_closed_pos - 100;

    servo_motor_init(&flap, htim, channel);
    servo_motor_enable(&flap);
    set_servo_pos(&flap, flap_closed_pos);

    _EXIT:

    return ret_val;
}

void servo_motor_enable(servo_motor_t *pServo)
{
    HAL_TIM_PWM_Start(pServo->htim, pServo->channel);
    return;
}

void servo_motor_disable(servo_motor_t *pServo)
{
    HAL_TIM_PWM_Stop(pServo->htim, pServo->channel);
    return;
}
/**
 * @brief  Operate servo
 * @param  Position from 0 to 1000 (
 * @retval None
 */
void set_servo_pos(servo_motor_t *pServo, uint32_t ccrValue)
{
    // Normalise ccrValue
    if (ccrValue > pServo->htim->Instance->ARR)
        ccrValue = pServo->htim->Instance->ARR;
    // @50Hz, period is 20 ms
    // Per data sheet position goes from from 0.5ms/20ms = 2.5% to 3ms/20ms = 6%, centre at 1.75 ms

    // Select channel associated to servo object
    switch (pServo->channel)
    {
        case TIM_CHANNEL_1:
            pServo->htim->Instance->CCR1 = ccrValue;
        break;
        case TIM_CHANNEL_2:
            pServo->htim->Instance->CCR2 = ccrValue;
        break;
        case TIM_CHANNEL_3:
            pServo->htim->Instance->CCR3 = ccrValue;
        break;
        case TIM_CHANNEL_4:
            pServo->htim->Instance->CCR4 = ccrValue;
        break;
        default:
            LS_error_handler(LS_ERROR);
    }

    // Store servo position in servo object
    pServo->position = ccrValue;
    return;
}

// Set flap position in steps, based on set_servo_pos
void set_flap(uint32_t pos)
{
    const uint32_t step = 5;
    const uint32_t lag = FLAP_DELAY * step / (SERVO_MAX - SERVO_MIN);
    servo_motor_enable(&flap);
    if (flap.position < pos)
        while (flap.position < pos)
        {
            set_servo_pos(&flap, flap.position + step);
            HAL_Delay(lag);
        }
    else if (flap.position > pos)
        while (flap.position > pos)
        {
            set_servo_pos(&flap, flap.position - step);
            HAL_Delay(lag);
        }
    //servo_motor_disable(&flap); DEBUGGING

    return;
}

void flap_open(void)
{
    set_flap(flap_open_pos);
    return;
}

void flap_close(void)
{
    set_flap(flap_closed_pos);
    return;
}

void flap_mid(void)
{
    set_flap(flap_mid_pos);
    return;
}

return_code_t adjust_card_flap(int8_t min_adjust, int8_t max_adjust)
{
	// p_ref_pos is either &flap_open_pos or &flap_closed_pos

	return_code_t ret_val;

    // Don't reset flap to defaults
    // reset_flap_defaults();
    flap_init(SERVO_PWM_CH);
    prompt_basic_item("OPEN    ", 0);
    if ((ret_val = adjust_flap(&flap_open_pos, min_adjust, max_adjust)) != LS_OK)
    	goto _EXIT;
    prompt_basic_item("CLOSED    ", 0);
    if ((ret_val = adjust_flap(&flap_closed_pos, min_adjust, max_adjust)) != LS_OK)
       	goto _EXIT;
    flap_mid_pos = flap_closed_pos - FLAP_CLOSED_TO_MID;
    clear_text();

    _EXIT:

	return ret_val;

}

return_code_t adjust_card_flap_limited(void)
{
	// Limited adjustment: -3 to +3 range (for MAINTENANCE menu)
	return adjust_card_flap(-3, 3);
}

return_code_t adjust_card_flap_full(void)
{
	// Full adjustment: no limits (for MAINTENANCE_LEVEL_2 menu)
	return adjust_card_flap(0, 0);
}
