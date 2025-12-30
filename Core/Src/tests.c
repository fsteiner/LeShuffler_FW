/*
 * Tests.c
 *
 *  Created on: Oct 3, 2024
 *      Author: macbook
 */

#include <basic_operations.h>
#include <buttons.h>
#include <interface.h>
#include <ili9488.h>
#include <rng.h>
#include <stdbool.h>
#include <stdint.h>
#include <servo_motor.h>
#include <utilities.h>

extern button encoder_btn;
extern button escape_btn;
extern int8_t carousel_pos;
extern uint32_t prev_encoder_turn_time;

// Display variables
extern int16_t display_row;
extern char display_buf[];
extern uint8_t n_cards_in;

return_code_t test_images(void)
{
	return_code_t ret_val = LS_OK;

	void prompt_test_question(char *text)
	{
		BSP_LCD_Clear(LCD_COLOR_BLACK);
		BSP_LCD_SetFont(&LCD_FIXED_FONT);
		BSP_LCD_DisplayStringAtLine(7, (uint8_t*) text);
		BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
		BSP_LCD_DisplayStringAt(0, ENCODER_Y, (uint8_t*) "OUI", RIGHT_MODE);
		BSP_LCD_SetTextColor(LCD_COLOR_RED);
		BSP_LCD_DisplayStringAt(0, ESCAPE_Y, (uint8_t*) "NON", RIGHT_MODE);
		BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
		BSP_LCD_SetFont(&LCD_FIXED_SMALL_FONT);

		return;
	}

	// Test animated logo
	prompt_animated_logo();
	HAL_Delay(L_WAIT_DELAY);
	prompt_test_question("  Animation OK ?");
	wait_btns();
	if (escape_btn.interrupt_press)
	{
		ret_val = LS_ERROR;
		reset_btns();
		goto _EXIT;
	}

	// Test small logo
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	uint8_t *image_address = (uint8_t*) SMALL_LOGO_ADDRESS;
	uint16_t Xpos = (BSP_LCD_GetXSize() - SMALL_LOGO_W) / 2;
	uint16_t Ypos = (BSP_LCD_GetYSize() - SMALL_LOGO_H) / 2;
	ili9488_DrawRGBImage8bit(Xpos, Ypos, SMALL_LOGO_W, SMALL_LOGO_H,
			image_address);
	HAL_Delay(L_WAIT_DELAY);
	prompt_test_question("  Petit logo OK ?");
	wait_btns();
	if (escape_btn.interrupt_press)
	{
		ret_val = LS_ERROR;
		reset_btns();
		goto _EXIT;
	}

	// Test graphic error pictures
	uint16_t _w = (uint16_t) SILH_W;
	uint16_t _h = (uint16_t) SILH_H;
	image_address = (uint8_t*) SILH_ADDRESS;
	Xpos = (BSP_LCD_GetXSize() - _w) / 2;
	Ypos = (BSP_LCD_GetYSize() - _h) / 2;

	while (image_address <= (uint8_t*) RESTART_ADDRESS)
	{
		BSP_LCD_Clear(LCD_COLOR_BCKGND);
		if (image_address == (uint8_t*) RESTART_ADDRESS)
		{
			Xpos = (BSP_LCD_GetXSize() - RESTART_W) / 2;
			Ypos = (BSP_LCD_GetYSize() - RESTART_H) / 2;
			_w = RESTART_W;
			_h = RESTART_H;
		}
		ili9488_DrawRGBImage8bit(Xpos, Ypos, _w, _h, image_address);
		HAL_Delay(M_WAIT_DELAY);
		prompt_test_question("  Image OK ?");
		wait_btns();
		if (escape_btn.interrupt_press)
		{
			ret_val = LS_ERROR;
			reset_btns();
			goto _EXIT;
		}
		image_address += SILH_SIZE;

	}

	// Test icons
	extern icon_t icon_list[];
	extern uint16_t n_icons;
	_w = ICON_W;
	_h = ICON_H;
	uint16_t space = 20;
	uint16_t X0 = Xpos = (BSP_LCD_GetXSize() - _w * (n_icons + 1) / 2
			- space * n_icons / 2) / 2;
	Ypos = (BSP_LCD_GetYSize() - 2 * _h - space) / 2;
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	for (uint16_t i = 0; i < n_icons; i++)
	{
		ili9488_DrawRGBImage8bit(Xpos, Ypos, _w, _h, icon_list[i].address);
		Xpos += _w + space;
		if (i == n_icons / 2)
		{
			Xpos = X0;
			Ypos += _h + space;
		}
	}

	HAL_Delay(2000);
	prompt_test_question("  Icones OK ?");
	wait_btns();
	if (escape_btn.interrupt_press)
	{
		ret_val = LS_ERROR;
		reset_btns();
		goto _EXIT;
	}

	_EXIT:

	BSP_LCD_Clear(LCD_COLOR_BLACK);

	return ret_val;
}

void show_images(void)
{

	// Show animated logo
	prompt_animated_logo();
	display_encoder_icon(ICON_CHECK);
	wait_btns();

	// Test small logo
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	uint8_t *image_address = (uint8_t*) SMALL_LOGO_ADDRESS;
	uint16_t Xpos = (BSP_LCD_GetXSize() - SMALL_LOGO_W) / 2;
	uint16_t Ypos = (BSP_LCD_GetYSize() - SMALL_LOGO_H) / 2;
	ili9488_DrawRGBImage8bit(Xpos, Ypos, SMALL_LOGO_W, SMALL_LOGO_H,
			image_address);
	display_encoder_icon(ICON_CHECK);
	wait_btns();

	// Test graphic error pictures
	uint16_t _w = (uint16_t) SILH_W;
	uint16_t _h = (uint16_t) SILH_H;
	image_address = (uint8_t*) SILH_ADDRESS;
	Xpos = (BSP_LCD_GetXSize() - _w) / 2;
	Ypos = (BSP_LCD_GetYSize() - _h) / 2;

	while (image_address <= (uint8_t*) RESTART_ADDRESS)
	{
		BSP_LCD_Clear(LCD_COLOR_BCKGND);
		if (image_address == (uint8_t*) RESTART_ADDRESS)
		{
			Xpos = (BSP_LCD_GetXSize() - RESTART_W) / 2;
			Ypos = (BSP_LCD_GetYSize() - RESTART_H) / 2;
			_w = RESTART_W;
			_h = RESTART_H;
		}
		ili9488_DrawRGBImage8bit(Xpos, Ypos, _w, _h, image_address);
		display_encoder_icon(ICON_CHECK);
		wait_btns();
		image_address += SILH_SIZE;

	}

	// Test icons
	extern icon_t icon_list[];
	extern uint16_t n_icons;
	_w = ICON_W;
	_h = ICON_H;
	uint16_t space = 20;
	uint16_t X0 = Xpos = (BSP_LCD_GetXSize() - _w * (n_icons + 1) / 2
			- space * n_icons / 2) / 2;
	Ypos = (BSP_LCD_GetYSize() - 2 * _h - space) / 2;
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	for (uint16_t i = 0; i < n_icons; i++)
	{
		ili9488_DrawRGBImage8bit(Xpos, Ypos, _w, _h, icon_list[i].address);
		Xpos += _w + space;
		if (i == n_icons / 2)
		{
			Xpos = X0;
			Ypos += _h + space;
		}
	}
	display_encoder_icon(ICON_CHECK);
	wait_btns();

	BSP_LCD_Clear(LCD_COLOR_BLACK);

	return;
}

// display function
static int16_t next_row(void)
{
	//sFONT* pCurrentFont;
	//pCurrentFont = BSP_LCD_GetFont();

	if (display_row < LCD_N_ROWS)
		display_row++;
	else
	{
		clear_text();
		display_row = 0;
	}

	return display_row;
}

// testCarousel measures the optical disc parameters
/* RESULTS
 * @12RPM can miss slots
 *  @6RPM OK
 *  @1PRM
 * Homing zone 8 initial align 4 (between end of homing zone and light)
 * Light: 1-3  avg 152/54 =  2.8 min  1 max 12 (154 157)
 * Dark:  7-8  avg 387/54 =  7.2 min 19 max  9 (385 382)
 * Slot:  9-11 avg 539/54 = 10.0 min  9 max 60
 *
 */
void measureSlots(bool MS)
{
	uint16_t homing_zone = 0;
	uint8_t status;
	bool OK = true;
	enum
	{
		LIGHT, DARK, SLOT
	};
	char *labels[] =
	{ "LIGHT", "DARK", "SLOT" };
	uint16_t zoneCount[3], max[3], min[3], accSteps[3], currentSteps[3],
			maxPos[3], minPos[3];
	uint16_t initialAlign = 0;
	const double speed = 0.25;

	void statusUpdate(uint8_t sts)
	{
		zoneCount[sts]++;
		accSteps[sts] += currentSteps[sts];
		if (currentSteps[sts] > max[sts])
		{
			max[sts] = currentSteps[sts];
			maxPos[sts] = accSteps[LIGHT] + accSteps[DARK];
		}
		if (currentSteps[sts] < min[sts])
		{
			min[sts] = currentSteps[sts];
			minPos[sts] = accSteps[LIGHT] + accSteps[DARK];
		}
		return;
	}

	bool testStep(bool MS)
	{
		// If status change, update records and invert status
		if (read_sensor(SLOT_SENSOR) != status)
		{
			statusUpdate(status);
			// If we have done a full slot, update slot
			if (slot_light())
			{
				currentSteps[SLOT] = currentSteps[LIGHT] + currentSteps[DARK];
				statusUpdate(SLOT);
				if (currentSteps[SLOT] < 5)
					return 1;
			}
			else
				beep(1);
			// Toggle status
			status = !status;
			// Reset current steps for next status
			currentSteps[status] = 0;
		}
		// Step carousel and increment current steps
		MS ? microstep_crsl(US_PER_STEP_AT_1_RPM / MS_FACTOR / speed) : rotate_one_step(
						speed);
		currentSteps[status]++;

		return 0;
	}

	// initial display
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	display_row = -1;
	snprintf(display_buf, N_DISP_MAX, "Press - measuring in %s @%.1f RPM",
			MS ? "micro steps" : "full steps", speed);
	prompt_basic_text(display_buf, next_row(), LCD_FIXED_SMALL_FONT);

	wait_btns();

	// Initialise arrays
	for (uint8_t i = LIGHT; i <= SLOT; i++)
	{
		zoneCount[i] = 0;
		max[i] = 0;
		min[i] = 0xFFFF;
		maxPos[i] = 0;
		minPos[i] = 0;
		accSteps[i] = 0;
		currentSteps[i] = 0;
		accSteps[i] = 0;
	}

	// Disalign homing if aligned
	while (crsl_at_home())
		MS ? microstep_crsl(
		US_PER_STEP_AT_1_RPM / MS_FACTOR / CRSL_SLOW_SPEED) :
				rotate_one_step(CRSL_SLOW_SPEED);

	// Realign to beginning of homing zone in 2 phases (blind fast, then slow)
	const uint16_t slotMargin = 3;
	for (uint16_t i = 0;
			i < (N_SLOTS - slotMargin) * STEPS_PER_SLOT * (MS ? MS_FACTOR : 1);
			i++)
		MS ? microstep_crsl(
		US_PER_STEP_AT_1_RPM / (CRSL_FAST_SPEED / 4) / MS_FACTOR) :
				rotate_one_step(CRSL_FAST_SPEED / 4);
	while (!crsl_at_home())
		MS ? microstep_crsl(
		US_PER_STEP_AT_1_RPM / CRSL_SLOW_SPEED / MS_FACTOR) :
				rotate_one_step(CRSL_SLOW_SPEED);
	beep(5);

	// Count steps of homing zone
	while (crsl_at_home())
	{
		homing_zone++;
		MS ? microstep_crsl(
		US_PER_STEP_AT_1_RPM / CRSL_SLOW_SPEED / MS_FACTOR) :
				rotate_one_step(CRSL_SLOW_SPEED);
	}
	beep(5);

	// Optical align to light
	while (slot_dark())
	{
		initialAlign++;
		MS ? microstep_crsl(
		US_PER_STEP_AT_1_RPM / CRSL_SLOW_SPEED / MS_FACTOR) :
				rotate_one_step(CRSL_SLOW_SPEED);
	}
	beep(5);

	// Note initial status (SLOT_LIGHT)
	status = read_sensor(SLOT_SENSOR);

	// Rotate until homing zone
	while (OK && !crsl_at_home())
		if (testStep(MS))
			OK = false;
	beep(5);

	// Rotate until end of homing zone
	while (OK && crsl_at_home())
		testStep(MS);
	beep(5);

	// Rotate until optically aligned to light
	while (OK && slot_dark())
		testStep(MS);
	beep(5);

	// Border effect
	if (OK)
		testStep(MS);

	// Display
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	display_row = -1;
	snprintf(display_buf, N_DISP_MAX, "Homing zone %d Initial align %d",
			homing_zone, initialAlign);
	prompt_basic_text(display_buf, next_row(), LCD_FIXED_SMALL_FONT);
	wait_btns();
	for (status = LIGHT; status <= SLOT; status++)
	{
		BSP_LCD_Clear(LCD_COLOR_BCKGND);
		display_row = -1;
		snprintf(display_buf, N_DISP_MAX, "%s cnt %d stp %d avg %2.1f",
				labels[status], zoneCount[status], accSteps[status],
				(float) accSteps[status] / zoneCount[status]);
		prompt_basic_text(display_buf, next_row(), LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX, "%s min %d max %d", labels[status],
				min[status], max[status]);
		prompt_basic_text(display_buf, next_row(), LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX, "%s minPos %d maxPos %d",
				labels[status], minPos[status], maxPos[status]);
		prompt_basic_text(display_buf, next_row(), LCD_FIXED_SMALL_FONT);
		wait_btns();
	}
	return;
}

/*
 * audioCheck
 *
 * ENT short press: one step forward
 * ESC short press: one step backward
 * ESC long press: 	jump to next light
 * ENT permanent press: toggle carouselArm()
 * ESC permanent press: exit
 *
 */

void audioCheck(void)
{
	// Audio check optical alignment
	bool carouselEnabled = true;
	reset_btns();
	while (1)
	{
		update_btns();
		// ENT - ONE STEP FORWARD
		if (encoder_btn.short_press)
		{
			// Step forward one step
			rotate_one_step(CRSL_SLOW_SPEED);
			reset_btns();
			beep(1);
		}
		// ESC_LONG - ONE STEP BACKWARD
		else if (escape_btn.long_press)
		{
			// Step backward one step
			set_crsl_dir_via_pin(CRSL_BWD);
			rotate_one_step(CRSL_SLOW_SPEED);
			set_crsl_dir_via_pin(CRSL_FWD);
			reset_btns();
			beep(1);
			HAL_Delay(10);
			beep(3);
		}
		// ENT-PERM - TOGGLE CAROUSE ENABLE
		else if (encoder_btn.permanent_press)
		{
			if (carouselEnabled)
			{
				carousel_disable();
				carouselEnabled = false;
				beep(200);
				HAL_Delay(100);
				beep(200);
			}
			else
			{
				carousel_enable();
				carouselEnabled = true;
				beep(400);
			}
			reset_btns();
		}
		// ESC - GO TO BEGINNING OF LIGHT
		else if (escape_btn.short_press)
		{
			// go direct to beginning of light
			while (slot_light())
				rotate_one_step(CRSL_SLOW_SPEED);
			while (slot_dark())
				rotate_one_step(CRSL_SLOW_SPEED);
			reset_btns();
		}
		// EXIT function
		else if (escape_btn.permanent_press)
		{
			reset_btns();
			beep(20);
			break;
		}

		// Signal when light
		if (slot_light())
		{
			beep(1);
			HAL_Delay(30);
		}
	}

	return;
}

return_code_t alignmentTest()
{
	extern move_report_t MR;
	extern eject_report_t ER;
	mr_init(&MR);
	er_init(&ER);
	clear_text();
	uint16_t counter = 0;
	uint16_t nAligns = 0;
	uint16_t nMisaligns = 0;
	uint16_t nFalseAligns = 0;
	uint16_t nFalseMisaligns = 0;
	uint16_t missed = 0;
	uint16_t nEjectAdjust = 0;
	int8_t target;
	int8_t targets[N_SLOTS];
	int16_t wiggleMax = -1000;
	int16_t wiggleMin = 1000;

	while (home_carousel() != LS_OK)
		beep(5);

	// initialise targets randomly
	if ((MR.ret_val = read_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
		goto _EXIT;

	order_positions(targets, n_cards_in, carousel_pos, ASCENDING_ORDER);
	for (uint8_t i = n_cards_in; i < N_SLOTS; i++)
		targets[i] = -1;

	clear_text();
	display_row = 0;

	reset_btns();
	while (n_cards_in)
	{
		bool initialAlign;
		target = targets[counter++];

		snprintf(display_buf, N_DISP_MAX, "%-3d:-> %2d", counter, target);
		prompt_menu_item(display_buf, 0);

		initialAlign = slot_light();
		go_to_position(target);

		if (MR.ret_val != LS_OK)
		{
			snprintf(display_buf, N_DISP_MAX, "MV: %s at pos %2d",
					error_message(MR.ret_val), carousel_pos);
			prompt_menu_item(display_buf, next_row());
			break;
		}
		else if (MR.align_OK)
		{
			if (initialAlign)
				nAligns++;
			else
				nFalseAligns++;
		}
		else if (!MR.align_OK)
		{
			if (!initialAlign)
				nMisaligns++;
			else
				nFalseMisaligns++;
		}

		if (MR.ret_val == LS_OK)
			eject_one_card(NON_SAFE_MODE, SEQ_MODE, NO_DEAL_GAP);
		if (ER.ret_val != LS_OK)
		{
			snprintf(display_buf, N_DISP_MAX, "EJ: %s at pos %2d",
					error_message(ER.ret_val), carousel_pos);
			prompt_menu_item(display_buf, next_row());
			break;
		}
		else if (ER.wiggleMax != 0 || ER.wiggleMin != 0)
		{
			wiggleMax = max(wiggleMax, ER.wiggleMax);
			wiggleMin = min(wiggleMin, ER.wiggleMin);
			nEjectAdjust++;
			while (display_row < 2)
				display_row++;
			snprintf(display_buf, N_DISP_MAX, "wiggle [%d , %d]", wiggleMin,
					wiggleMax);
			prompt_menu_item(display_buf, next_row());
		}
	}

	if (ER.ret_val != CARD_STUCK_ON_EXIT)
		set_latch(LATCH_CLOSED);

	if (MR.ret_val == LS_OK && ER.ret_val == LS_OK)
	{
		clear_text();
		snprintf(display_buf, N_DISP_MAX, "%d iterations without fail",
				counter);
		prompt_menu_item(display_buf, next_row());
		snprintf(display_buf, N_DISP_MAX, "AL: %d MA: %d FA: %d FM: %d",
				nAligns, nMisaligns, nFalseAligns, nFalseMisaligns);
		prompt_menu_item(display_buf, next_row());

		if (nEjectAdjust)
		{
			snprintf(display_buf, N_DISP_MAX, "Eject adjusted %d times",
					nEjectAdjust);
			prompt_menu_item(display_buf, next_row());
			snprintf(display_buf, N_DISP_MAX, "wiggle [%d , %d]", wiggleMin,
					wiggleMax);
			prompt_menu_item(display_buf, next_row());
		}
		if (missed)
		{
			snprintf(display_buf, N_DISP_MAX, "Missed: %d", missed);
			prompt_menu_item(display_buf, next_row());
		}
	}

	_EXIT:

	return MR.ret_val;
}

// simultaneaously read pins of GPIO port
// pPort is the port, mask the pins we want to read
void readPortIDR(GPIO_TypeDef *pPort, uint32_t mask)
{
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	uint32_t prevValue = 0, currentValue;
	uint32_t values[8];
	uint16_t N = 0;
	while (1)
	{
		update_btns();
		currentValue = pPort->IDR & mask;
		if (currentValue != prevValue)
		{
			values[N++] = currentValue;
			prevValue = currentValue;
		}

		if (N >= 8)
		{
			for (uint16_t i = 0; i < 8; i++)
			{
				snprintf(display_buf, N_DISP_MAX, "%08lx", values[i]);
				prompt_menu_item(display_buf, display_row++);
			}
			wait_btns();
			BSP_LCD_Clear(LCD_COLOR_BCKGND);
			display_row = 0;
			N = 0;
		}
		if (escape_btn.short_press)
		{
			BSP_LCD_Clear(LCD_COLOR_BCKGND);
			display_row = 0;
			N = 0;
			reset_btns();
		}
	}
}

return_code_t test_random_deal(void)
{
	return_code_t ret_val;
	int8_t target;
	int8_t targets[N_SLOTS];
	int8_t **pTargets = NULL;
	uint16_t index;
	const uint16_t cardsPerPlayer = 4;
	const uint16_t nPlayers = 3;
	int8_t exitTargets[nPlayers * cardsPerPlayer];

	// "FILL" carousel
	for (carousel_pos = 0; carousel_pos < N_SLOTS; carousel_pos++)
		update_current_slot_w_offset(1);
	// "HOME_CRSL" carousel
	carousel_pos = 0;

	// Randomise cards from the end of the list targets[n_cards_in-1]
	if ((ret_val = read_random_slots_w_offset(targets, FULL_SLOT)) != LS_OK)
		goto _EXIT;

	clear_text();
	prompt_basic_text("Targets", ++display_row, Font16);
	for (uint16_t i = 0; i < 2; i++)
	{
		snprintf(display_buf, N_DISP_MAX, "%2d %2d %2d %2d %2d %2d",
				targets[N_SLOTS - 6 * i - 1], targets[N_SLOTS - 6 * i - 2],
				targets[N_SLOTS - 6 * i - 3], targets[N_SLOTS - 6 * i - 4],
				targets[N_SLOTS - 6 * i - 5], targets[N_SLOTS - 6 * i - 6]);
		prompt_basic_text(display_buf, ++display_row, Font16);
	}

	// Structure pTargets, variable holding players'cards carousel positions
	pTargets = (int8_t**) malloc(nPlayers * sizeof(targets)); // size of any pointer will do
	if (pTargets == NULL)
	{
		ret_val = LS_ERROR;
		goto _EXIT;
	}
	for (uint16_t i = 0; i < nPlayers; i++)
	{
		pTargets[i] = (int8_t*) malloc(cardsPerPlayer * sizeof(int8_t));
		if (pTargets[i] == NULL)
		{
			ret_val = LS_ERROR;
			goto _EXIT;
		}
	}

	//Initialise pTargets starting by the end of targets list
	uint8_t counter = n_cards_in;
	for (uint16_t nP = 0; nP < nPlayers; nP++)
		for (uint16_t nC = 0; nC < cardsPerPlayer; nC++)
			pTargets[nP][nC] = targets[--counter];

	prompt_basic_text("pTargets", ++display_row, Font16);
	for (uint16_t nP = 0; nP < nPlayers; nP++)
	{
		snprintf(display_buf, N_DISP_MAX, "P%d: %2d %2d %2d %2d", nP,
				pTargets[nP][0], pTargets[nP][1], pTargets[nP][2],
				pTargets[nP][3]);
		prompt_basic_text(display_buf, ++display_row, Font16);
	}

	// Deal Cards
	index = 0;
	for (uint16_t nC = 0; nC < cardsPerPlayer; nC++)
	{
		for (uint16_t nP = 0; nP < nPlayers; nP++)
		{

			// Order targets of current player based on current carousel position
			order_positions(pTargets[nP], cardsPerPlayer - nC, carousel_pos,
					DESCENDING_ORDER);
			// Take the last one as target
			target = pTargets[nP][cardsPerPlayer - nC - 1];
			// "GO" there
			exitTargets[index++] = carousel_pos = target;
		}

	}

	prompt_basic_text("exitTargets", ++display_row, Font16);
	for (uint16_t nC = 0; nC < cardsPerPlayer; nC++)
	{
		snprintf(display_buf, N_DISP_MAX, "C%d: %2d %2d %2d", nC,
				exitTargets[nPlayers * nC], exitTargets[nPlayers * nC + 1],
				exitTargets[nPlayers * nC + 2]);
		prompt_basic_text(display_buf, ++display_row, Font16);
	}

	_EXIT:

	// Free pointers
	for (uint16_t i = 0; i < nPlayers; i++)
	{
		free(pTargets[i]);
		pTargets[i] = NULL;
	}
	free(pTargets);
	pTargets = NULL;

	return ret_val;
}
