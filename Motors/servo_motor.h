/*
 * Servo_Motor.h
 *
 *  Created on: Jun 26, 2024
 *      Author: Radhika S
 */

#ifndef SERVOMOTOR_H_
#define SERVOMOTOR_H_

#include "stm32h7xx_hal.h"
#include "stdio.h"
#include "stm32h7xx_hal_def.h"
#include "string.h"
#include <stdbool.h>
#include "tim.h"
#include "gpio.h"
#include <utilities.h>

typedef struct {
    TIM_HandleTypeDef* htim;
    uint32_t channel;
    uint16_t position;
} servo_motor_t;


void servo_motor_init(servo_motor_t *, TIM_HandleTypeDef *, uint32_t);
void servo_motor_enable(servo_motor_t *);
void servo_motor_disable(servo_motor_t *);
void set_servo_pos(servo_motor_t *, uint32_t );
void set_flap(uint32_t);
void flap_open(void);
void flap_close(void);
void flap_mid(void);
return_code_t flap_init(TIM_HandleTypeDef *, uint32_t);
return_code_t adjust_card_flap(void);

#endif /* SERVOMOTOR_H_ */
