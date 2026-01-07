/*
 * TB6612FNG.h
 *
 *  Created on: Jun 10, 2024
 *      Author: Radhika S
 */

#ifndef TB6612FNG_H_
#define TB6612FNG_H_

#include "stm32h7xx_hal.h"
#include "stdio.h"
#include "stm32h7xx_hal_def.h"
#include "string.h"
#include <stdbool.h>
#include "tim.h"
#include "gpio.h"

typedef struct {
	uint8_t ID;
	GPIO_TypeDef * GPIOport_1;
	uint16_t GPIOpin_1;
	GPIO_TypeDef * GPIOport_2;
	uint16_t GPIOpin_2;
	GPIO_TypeDef * GPIOportStdby;
	uint16_t GPIOpinStdby;
    TIM_HandleTypeDef* htim;
    uint32_t channel;
} dc_motor_t;

void DCmotorInit(
	dc_motor_t * motor,
	uint8_t ID,
	GPIO_TypeDef * , uint16_t ,
	GPIO_TypeDef * , uint16_t ,
	GPIO_TypeDef * , uint16_t ,
	TIM_HandleTypeDef* , uint32_t );
void setDutyCycle(dc_motor_t, uint32_t);
void dc_motor_enable(dc_motor_t);
void dc_motor_disable(dc_motor_t);
void dc_motor_freewheel(dc_motor_t);
void clean_roller(dc_motor_t dc_motor);
void load_enable(void);
void load_disable(void);
void set_motor_rotation(dc_motor_t, uint16_t, bool);
void set_latch(bool status);
void test_exit_latch(void);
void access_exit_chute(void);
void dc_motors_run_in(void);


#endif /* TB6612FNG_H_ */

