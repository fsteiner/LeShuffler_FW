/*
 * Buttons.c
 *
 *  Created on: Jun 28, 2024
 *      Author: Francois S
 */

#include <buttons.h>
#include "iwdg.h"
#include <utilities.h>

extern button escape_btn;
extern button encoder_btn;

void button_init(button *pBtn, GPIO_TypeDef *port, uint16_t pin,
		GPIO_PinState openState)
{
	pBtn->port = port;
	pBtn->pin = pin;
	pBtn->open_state = openState;
	pBtn->old_TOC = pBtn->TOC = HAL_GetTick();

	reset_btn(pBtn);

	return;
}

void reset_btn(button *pBtn)
{
	uint32_t startTime;
	bool released;
	// wait for button to be released (with debounce) except if permanentPress
	if (pBtn->permanent_press)
		released = true;
	else
		released = false;
	while (!released)
	{
		watchdog_refresh();
		startTime = HAL_GetTick();
		while (HAL_GPIO_ReadPin(pBtn->port, pBtn->pin) == pBtn->open_state)
		{
			if (HAL_GetTick() - startTime > DEBOUNCE_TIME)
			{
				released = true;
				break;
			}
		}
	}

	// Reset pBtn press flags
	pBtn->short_press = false;
	pBtn->long_press = false;
	pBtn->permanent_press = false;
	pBtn->interrupt_press = false;
	// Reset previous button state as not pressed
	pBtn->state = pBtn->open_state;
	pBtn->old_state = pBtn->open_state;
}

void reset_btns(void)
{
	reset_btn(&encoder_btn);
	reset_btn(&escape_btn);
	return;
}

// button should be reset before using update
void update_btn(button *pBtn)
{
	// Read current state
	pBtn->state = HAL_GPIO_ReadPin(pBtn->port, pBtn->pin);
	// If state has changed and more than DEBOUNCE_TIME has elapsed since last change
	if ((pBtn->state != pBtn->old_state)
			&& (HAL_GetTick() - pBtn->TOC > DEBOUNCE_TIME))
	{
		// Store current state as latest registered state
		pBtn->old_state = pBtn->state;
		// Register a legitimate change of state and TOC
		pBtn->old_TOC = pBtn->TOC;  	// Save previous time of state change
		pBtn->TOC = HAL_GetTick();    // Register latest time of state change
		// If button is released update type of press
		if (pBtn->state == pBtn->open_state)
		{
			if (pBtn->TOC - pBtn->old_TOC < LONG_PRESS_TIME)
				pBtn->short_press = true;
			else if (pBtn->TOC - pBtn->old_TOC < PERM_PRESS_TIME)
				pBtn->long_press = true;
			else
				pBtn->permanent_press = true;
		}

	}
	// else if button has been pressed for more than PERM_PRESS_TIME and not released, set permanentPress flag
	else if (pBtn->state != pBtn->open_state
			&& (HAL_GetTick() - pBtn->TOC > PERM_PRESS_TIME))
		pBtn->permanent_press = true;

	return;
}

void update_btns(void)
{
	update_btn(&encoder_btn);
	update_btn(&escape_btn);
}

void wait_btn(button *pBtn)
{
	reset_btn(pBtn);
	while (!pBtn->interrupt_press)
		;

	return;
}

void wait_btns(void)
{
	reset_btn(&encoder_btn);
	reset_btn(&escape_btn);
	while (!encoder_btn.interrupt_press && !escape_btn.interrupt_press)
		;

	return;
}

