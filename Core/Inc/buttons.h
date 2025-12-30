/*
 * buttons.h
 *
 *  Created on: Jun 28, 2024
 *      Author: Francois S
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <definitions.h>
#include <gpio.h>
#include <stdbool.h>

typedef struct
{
	GPIO_TypeDef * port;
	uint16_t pin;
	GPIO_PinState open_state; // the state in which the button is open
	GPIO_PinState state;
	GPIO_PinState old_state;
	bool short_press;
	bool long_press;
	bool permanent_press;
	volatile bool interrupt_press;
	uint32_t TOC;
	uint32_t old_TOC;
} button;


void button_init(button * btn, GPIO_TypeDef * port, uint16_t pin, GPIO_PinState pullup);
void reset_btn(button * btn);
void reset_btns(void);
void update_btn(button * btn);
void update_btns();
void wait_btn(button * btn);
void wait_btns(void);

#ifdef __cplusplus
}
#endif


#endif /* INC_BUTTONS_H_ */
