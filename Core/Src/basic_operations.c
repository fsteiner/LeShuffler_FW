/*
 * basic_operations.c
 *
 *  Created on: Jun 28, 2024
 *      Author: François S
 */

#include <basic_operations.h>
#include <buttons.h>
#include <i2c.h>
#include "iwdg.h"
#include <ili9488.h>
#include <games.h>
#include <interface.h>
#include <main.h>
#include <math.h>
#include <PSRAM.h>
#include <rng.h>
#include <servo_motor.h>
#include <stm32_adafruit_lcd.h>
#include <TB6612FNG.h>
#include <TMC2209.h>
#include <utilities.h>

// DEBUGGING
uint16_t crsl_speed;

bool last_card_stuck_on_entry = false;

const uint32_t accelZone = 25; // steps
const uint32_t decelZone = 40; // steps

extern uint8_t n_cards_in;
extern button encoder_btn;
extern button escape_btn;
extern int8_t carousel_pos;

/**
 * @brief  waitSensor:	wait for requested IR status (with debounce)
 * @param  _S: 		sensor to monitor
 * @param	eventType: 	CARD_SEEN, CARD_CLEAR
 * @param 	delay: 		waiting time in ms, if 0 wait indefinitely until event occurs
 * @retval 			LS_OK, NOT_SEEN (if waiting time exceeded)
 */
return_code_t wait_sensor(sensor_code_t _S, bool eventType, uint32_t delay)
{
	bool event = false;
	bool escape;
	bool reading;
	uint32_t startTime;
	uint32_t startTime2;

	// Special treatment: shoe sensor wait can be escaped
	if (_S == SHOE_SENSOR)
		reset_btn(&escape_btn);
	// Mark beginning of waiting time
	startTime = HAL_GetTick();
	// Main loop
	while (!event)
	{
		watchdog_refresh();
		//Wait for sensor to either a)see requested status, b)escape or c)expiry of delay (if delay >0)
		while (!(reading = (read_sensor(_S) == eventType))
				&& !(escape = ((_S == SHOE_SENSOR) && escape_btn.short_press))
				&& !(delay > 0 && HAL_GetTick() > (startTime + delay)))
		{
			update_btn(&escape_btn);
		}
		// If no event and no escape, the delay has expired, return error
		if (!(reading || escape))
			return NOT_SEEN;
		// If escape acknowledge
		else if (escape)
			event = true;
		// Else if reading confirm
		else if (reading)
		{
			// Confirm stability
			startTime2 = HAL_GetTick();
			// Check the event is stable for at least DEBOUNCE_TIME (<< delay)
			while (!event && (read_sensor(_S) == eventType))
				if (HAL_GetTick() - startTime2 > DEBOUNCE_TIME)
					event = true;
			// If not we stay in the while(!event) loop, with the original start time.
		}

	}
	// Only if event or escape
	return LS_OK;
}

/**
 * @brief  wait_seen_entry:	wait for card seen at either entry sensor (works even if wrong insertion)
 * 		uses a longer debounce time (card transit time is about 80-90 ms)
 * @param 	delay: 		waiting time in ms, should not be zero
 * @retval 			LS_OK, NOT_SEEN (if waiting time exceeded)
 */
return_code_t wait_seen_entry(uint32_t delay)
{
	const bool eventType = CARD_SEEN;
	const uint32_t debounceTime = 30; // Time to clear poker card minimum 90 ms INCLUDING debounceTime
	bool event = false;
	bool reading;
	uint32_t startTime;
	uint32_t startTime2;

	// Mark beginning of waiting time
	startTime = HAL_GetTick();
	// Main loop
	while (!event)
	{
		watchdog_refresh();
		//Wait for either a) one of 3 entry sensors to see requested status or b) expiry of delay (if delay >0)
		while (!(reading = (read_sensor(ENTRY_SENSOR_1) == eventType
				|| read_sensor(ENTRY_SENSOR_2) == eventType
				|| read_sensor(ENTRY_SENSOR_3) == eventType))
				&& !(delay > 0 && HAL_GetTick() > (startTime + delay)))
			;
		// If no reading, the delay has expired, return error
		if (!reading)
			return NOT_SEEN;
		// Else confirm
		else
		{
			// Confirm stability
			startTime2 = HAL_GetTick();
			// Check the event is stable (at least one entry sensor sees a card) for at least debounceTime (<< delay)
			while (!event
					&& (read_sensor(ENTRY_SENSOR_1) == eventType
							|| read_sensor(ENTRY_SENSOR_2) == eventType
							|| read_sensor(ENTRY_SENSOR_3) == eventType))
			{
				if (HAL_GetTick() - startTime2 > debounceTime)
					event = true;
			}
			// If not we stay in the while(!event) loop, with the original start time.
		}

	}

	// Only if event or escape
	return LS_OK;
}

/**
 * @brief:				waits for the card to clear entry sensors 1 & 3 (2 is too close) or delay expired
 * @param 	delay: 		waiting time in ms, should not be zero
 * @retval 			LS_OK, CARD_STUCK_ON_ENTRY (if waiting time exceeded)
 */
return_code_t wait_clear_entry(uint32_t delay)
{
	const bool eventType = CARD_CLEAR;
	const uint32_t debounceTime = 30;
	bool event = false;
	bool reading;
	uint32_t startTime;
	uint32_t startTime2;

	// Mark beginning of waiting time
	startTime = HAL_GetTick();
	// Main loop
	while (!event)
	{
		watchdog_refresh();
		//Wait for either a) one of 3 entry sensors to see requested status or b) expiry of delay (if delay >0)
		while (!(reading = (read_sensor(ENTRY_SENSOR_1) == eventType
				|| read_sensor(ENTRY_SENSOR_3) == eventType))
				&& !(delay > 0 && HAL_GetTick() > (startTime + delay)))
			;
		// If no reading, the delay has expired, return error
		if (!reading)
			return CARD_STUCK_ON_ENTRY;
		// Else confirm
		else
		{
			// Confirm stability
			startTime2 = HAL_GetTick();
			// Check the event is stable (at least one entry sensor sees a card) for at least debounceTime (<< delay)
			while (!event
					&& (read_sensor(ENTRY_SENSOR_1) == eventType
							|| read_sensor(ENTRY_SENSOR_3) == eventType))
			{
				if (HAL_GetTick() - startTime2 > debounceTime)
					event = true;
			}
			// If not we stay in the while(!event) loop, with the original start time.
		}

	}

	// Only if event or escape
	return LS_OK;
}

return_code_t cards_in_tray(void)
{
	bool reading = read_sensor(TRAY_SENSOR);
	bool previous_reading;
	do
	{
		HAL_Delay(DEBOUNCE_TIME);
		previous_reading = reading;
		reading = read_sensor(TRAY_SENSOR);
	}
	while (reading != previous_reading);

	return (reading == CARD_SEEN);
}

return_code_t cards_in_shoe(void)
{
	bool reading = read_sensor(SHOE_SENSOR);
	bool previous_reading;
	uint32_t shoe_debounce_time = 20; // ms
	do
	{
		HAL_Delay(shoe_debounce_time);
		previous_reading = reading;
		reading = read_sensor(SHOE_SENSOR);
	}
	while (reading != previous_reading);

	return (reading == CARD_SEEN);
}

/**
 * @brief  waitPickUp:	wait for cards to be picked up or ESC press
 */
void wait_pickup_shoe_or_ESC(void)
{
	extern icon_set_t icon_set_void;
	bool cards_seen = false;

	if (cards_in_shoe())
	{
		flap_mid();
		cards_seen = true;
		prompt_interface(MESSAGE, PICK_UP_CARDS, "", icon_set_void, ICON_CROSS,
				SHOE_CLEAR);
	}

	if (!cards_in_shoe() && cards_seen)
	{
		HAL_Delay(SHOE_DELAY);
		flap_close();
	}

	clear_text();
	clear_icons();
	clear_picture();

	return;
}

void wait_pickup_shoe(uint8_t n_cards)
{
	extern icon_set_t icon_set_void;
	extern char display_buf[];

	if (cards_in_shoe())
	{
		flap_mid();
		if (n_cards == 0)
			prompt_interface(MESSAGE, PICK_UP_CARDS, "", icon_set_void,
					ICON_VOID, SHOE_CLEAR);
		else
		{
			snprintf(display_buf, N_DISP_MAX, "\nPick up %u card%s", n_cards,
					(n_cards > 1) ? "s" : "");
			prompt_interface(MESSAGE, CUSTOM_PICK_UP, display_buf,
					icon_set_void, ICON_VOID, SHOE_CLEAR);
		}
		HAL_Delay(SHOE_DELAY);
		flap_close();
	}

	clear_text();
	clear_icons();
	clear_picture();

	return;
}

return_code_t safe_abort(void)
{
	extern icon_set_t icon_set_check;
	return_code_t ret_val = LS_OK;
	// Check for ESC with confirmation prompt (only 2 options or errors)
	if (escape_btn.interrupt_press)
	{
		clear_message(TEXT_ERROR);
		// If continue, reset ESC button from the first press and modify return code
		if ((ret_val = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
				"\nOperation paused, continue?", icon_set_check, ICON_RED_CROSS,
				BUTTON_PRESS)) == LS_OK)
		{
			reset_btn(&escape_btn);
			ret_val = CONTINUE;
		}
		// Clean and exit
		clear_message(TEXT_ERROR);
		// Display icons
		display_encoder_icon(ICON_VOID);
		display_escape_icon(ICON_CROSS);
	}

	return ret_val;
}

/**
 * @brief  loadOneCard:	Load one card (does not check there is a card in tray)
 * @param  rand_mode: 	SEQ_MODE or RAND_MODE
 * @param  pLR: 			pointer to loadReport containing info for debugging
 * @retval 				LS_OK, CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, WRONG_INSERTION
 */
return_code_t load_one_card(rand_mode_t rand_mode)
{
	extern load_report_t LR;
	extern dc_motor_t tray_motor;
	extern dc_motor_t entry_motor;
//	extern union machine_state_t machine_state;

	// Init Load report
	lr_init(&LR);

	// Get machine state
	if ((LR.ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

	const uint32_t safe_loading_time = 0;
	const uint32_t post_loading_time = 0;
	const uint32_t tray_clearance_delay = 0; // to be calibrated - compensates weakness of tray motor
	const uint16_t n_steps_entry = 30;
	const uint16_t speed_step = (ENTRY_MOTOR_MAX_SPEED - ENTRY_MOTOR_MIN_SPEED)
			/ n_steps_entry;
	const uint32_t ramp_time = 90; //ms NOT NECESSARY THE SHORTEST, TO CATCH THE CARD MORE SMOOTHLY
	bool card_stuck = true;
//	const uint32_t insertConfirmDelay = 70; // to be calibrated, max 85 (~passage time of a card)
//	bool E1, E2, E3, F1, F2, F3;

	// Check that entry is clear
	// SENSOR 2 TOO SENSITIVE, TO BE USED ONLY FOR INSERTION CHECK
	if (read_sensor(ENTRY_SENSOR_1) == CARD_SEEN
			|| read_sensor(ENTRY_SENSOR_3) == CARD_SEEN)
	{
		LR.ret_val = CARD_STUCK_ON_ENTRY;
		LR.stuck = 1;
		goto _EXIT;
	}

	// Enable loading module
	load_enable();

	// Start motors
	LR.t_0 = HAL_GetTick();
	// Start tray motor INVERSION OF MOTOR STARTS ALLOWS TO CATCH THE CARD MORE SMOOTHLY
	set_motor_rotation(tray_motor, TRAY_MOTOR_SPEED, DC_MTR_FWD);
	// Ramp entry motor
	for (uint16_t i = 0; i < n_steps_entry; i++)
	{
		if (i != 1)
			HAL_Delay(n_steps_entry == 1 ? 0 : ramp_time / (n_steps_entry - 1));
		set_motor_rotation(entry_motor,
		ENTRY_MOTOR_MIN_SPEED + i * speed_step, DC_MTR_FWD);
	}
	// For rounding issues
	set_motor_rotation(entry_motor, ENTRY_MOTOR_MAX_SPEED, DC_MTR_FWD);
	// Wait for card to show at entry, after medium delay increase tray motor speed
	if (wait_seen_entry(S_WAIT_DELAY) == NOT_SEEN)
	{
		set_motor_rotation(tray_motor, (TRAY_MOTOR_SPEED + 100) / 2,
		DC_MTR_FWD);
		LR.sticky = true;
	}
	else
		card_stuck = false;

	// If card still stuck after long delay, abort
	if (card_stuck && wait_seen_entry(L_WAIT_DELAY) == NOT_SEEN)
	{
		dc_motor_freewheel(tray_motor);
		dc_motor_freewheel(entry_motor);
		LR.ret_val = CARD_STUCK_IN_TRAY;
		LR.stuck = 2;
		goto _EXIT;
	}
	else
	{
		// t_seen - card reaches sensor
		LR.t_seen = HAL_GetTick();
		card_stuck = false; // for consistency, not strictly necessary

		/* CHECKING SENSE OF INTRODUCTION - NEUTRALISED FOR NOW */
//		// Check sense of introduction
//		E1 = readSensor(ENTRY_SENSOR_1);
//		E2 = readSensor(ENTRY_SENSOR_2);
//		E3 = readSensor(ENTRY_SENSOR_3);
//
//		// If issue, confirm
//		if ((E1 == CARD_SEEN && E3 == CARD_CLEAR) ||
//			(E3 == CARD_SEEN && E1 == CARD_CLEAR) ||
//			(E2 == CARD_SEEN && (E1 == CARD_CLEAR || E3 == CARD_CLEAR)))
//		{
//			// Confirm
//			HAL_Delay(insertConfirmDelay); // to be calibrated
//			F1 = readSensor(ENTRY_SENSOR_1);
//			F2 = readSensor(ENTRY_SENSOR_2);
//			F3 = readSensor(ENTRY_SENSOR_3);
//
//			// Checks that the CARD_CLEAR above was not an artefact (card coming in)
//			if ((E1 == CARD_CLEAR && F1 == CARD_CLEAR) ||
//				(E2 == CARD_CLEAR && F2 == CARD_CLEAR) ||
//				(E3 == CARD_CLEAR && F3 == CARD_CLEAR))
//			{
//
//				dc_motor_freewheel(trayMotor);
//				dc_motor_freewheel(entryMotor);
//				// DEBUGGING
//		        extern char display_buf[];
//				snprintf(display_buf, N_DISP_MAX, "F1 %d F2 %d F3 %d", F1, F2, F3);
//				promptBasicItem(display_buf, 1);
//				waitBtns();
//				clearScreen();
//				LR.ret_val = WRONG_INSERTION;
//				goto _EXIT;
//			}
//		}
	}

	// If all normal clean stop tray motor after a small delay to help the card leave tray (to be calibrated)
	HAL_Delay(tray_clearance_delay);
	set_motor_rotation(tray_motor, 0, DC_MTR_FWD);

	// Wait for card to clear entry, abort if too long
	if (wait_clear_entry(L_WAIT_DELAY))
	{
		// Stop of entry motor
		dc_motor_freewheel(entry_motor);
		LR.ret_val = CARD_STUCK_ON_ENTRY;
		LR.stuck = 3;
		goto _EXIT;
	}
	// t_clear - card clears entry
	LR.t_clear = HAL_GetTick();

	// Stop entry motor after safety time
	HAL_Delay(safe_loading_time);
	// Freewheel entry motor
	dc_motor_freewheel(entry_motor);

	/*
	 * Update slot (only function with ejectOneCard)
	 * [located BEFORE _EXIT]
	 */
	LR.ret_val = update_current_slot_w_offset(1);

	_EXIT:

	// Disable loading module
	load_disable();
	// Pause to avoid jamming
	HAL_Delay(post_loading_time);
	// Show/hide n_cards_in
	if (LR.ret_val == LS_OK)
		prompt_n_cards_in();
	else
		hide_n_cards_in();

	return LR.ret_val;
}

/**
 * @brief  Initialises carousel (disable, set direction)
 * @param  none
 * @retval none
 */
return_code_t carousel_init(void)
{
	return_code_t ret_val = LS_OK;

	// Initialise driver
	if ((ret_val = tmc2209_init()) != LS_OK)
		goto _EXIT;

	// Go to a full-step stop (half-step away)
	for (int i = 0; i < MS_FACTOR / 2; i++)
		microstep_crsl(HOMING_SLOW_SPEED);

	// Give time for automatic setting of PWM_OFS_AUTO before homing
	HAL_Delay(130 + 70);

	_EXIT:

	return ret_val;
}

void set_crsl_dir_via_pin(bool crslDir)
{
	// Set direction
	HAL_GPIO_WritePin(STP_DIR_GPIO_Port, STP_DIR_Pin, crslDir);
	return;
}
/*
 * Carousel enabling and disabling used in
 * 	• carouselInit
 * 	• [homeCarousel]
 * 	• [loadMaxNcards]
 * 	• [emptyCarousel]
 */
void carousel_enable(void)
{
	HAL_GPIO_WritePin(STP_ENN_PIN, GPIO_PIN_RESET);

	return;
}

void carousel_disable(void)
{
	HAL_GPIO_WritePin(STP_ENN_PIN, GPIO_PIN_SET);

	return;
}

/**
 * @brief  microStepCarousel:	steps carousel by one micro-step
 * @param  us: 				delay between 2 micro-steps in micro seconds
 * @retval none
 */
void microstep_crsl(uint16_t us)
{
	HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, SET);
	HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, GPIO_PIN_RESET);
	delay_micro_s(max(0, (int32_t) us - (int32_t) STP_OVERHEAD));

	return;
}

/**
 * @brief  rotateOneStep:	rotates carousel by one full step
 * @param  RPM: 			rotational speed
 * @retval none
 */

void rotate_one_step(double rpm)
{
	uint16_t us = (uint16_t) round(
			(double) US_PER_STEP_AT_1_RPM / MS_FACTOR / rpm);
	for (int i = 0; i < MS_FACTOR; i++)
		microstep_crsl(us);

	return;
}

bool crsl_at_home(void)
{
	return (read_sensor(HOMING_SENSOR) == CAROUSEL_HOMED);
}

bool slot_dark(void)
{
	return read_sensor(SLOT_SENSOR) == SLOT_DARK;
}

bool slot_light(void)
{
	return !slot_dark();
}

/**
 * @brief  Home carousel
 * @retval LS_OK, ALIGNMENT
 */
return_code_t home_carousel(void)

{
	// Accelerations and decelerations linear as slow speed not sensitive to multiplicative coefs
	extern move_report_t MR;
	const uint32_t homingMaxTime = 3000UL;  // ms
	const uint16_t stepUp = (HOMING_SPEED - HOMING_SLOW_SPEED) / HOM_ACCEL_ZONE;
	const uint16_t stepDn = (HOMING_SPEED - HOMING_SLOW_SPEED) / HOM_DECEL_ZONE;
	const double step_up = pow(HOMING_SPEED / HOMING_SLOW_SPEED,
			1.0 / HOM_ACCEL_ZONE);
	double rpm = HOMING_SLOW_SPEED; 		// Start slow
	uint32_t startTime;

	mr_init(&MR);
	MR.ret_val = LS_OK;

	// Check that no card is stuck on entry with debounce
	if (read_sensor(ENTRY_SENSOR_2) == CARD_SEEN)
	{
		HAL_Delay(DEBOUNCE_TIME);
		if (read_sensor(ENTRY_SENSOR_2) == CARD_SEEN)
			return CARD_STUCK_ON_ENTRY;
	}

	// Check that no card is stuck on exit with debounce
	if (read_sensor(EXIT_SENSOR) == CARD_SEEN)
	{
		HAL_Delay(DEBOUNCE_TIME);
		if (read_sensor(EXIT_SENSOR) == CARD_SEEN)
			return CARD_STUCK_ON_EXIT;
	}

	// Close latch
	set_latch(LATCH_CLOSED);

	// Move if homed already, accelerating through to homing speed [homing zone is 8-9 steps]
	startTime = HAL_GetTick();
	while (crsl_at_home() && HAL_GetTick() < startTime + homingMaxTime)
	{
		watchdog_refresh();
		//rpm = min(HOMING_SPEED, rpm + stepUp);
		rpm = min(HOMING_SPEED, rpm * step_up);
		rotate_one_step(rpm);
		MR.initial_move++;
		MR.total_steps++;
	}
	if (crsl_at_home())
	{
		MR.ret_val = HOMING_ERROR;
		goto _EXIT;
	}

	// Realign to start of homing position, accelerating at the beginning
	startTime = HAL_GetTick();
	while (!crsl_at_home() && HAL_GetTick() < startTime + homingMaxTime)
	{
		watchdog_refresh();
		rpm = min(HOMING_SPEED, rpm + stepUp);
		rotate_one_step(rpm);
		MR.initial_move++;
		MR.total_steps++;
	}
	// Check that we detected the homing sensor
	if (!crsl_at_home())
	{
		MR.ret_val = HOMING_ERROR;
		goto _EXIT;
	}
	// If it is the case, realign precisely to end of home position while decelerating through [about 8-9 steps]
	while (crsl_at_home())
	{
		watchdog_refresh();
		if (rpm > HOMING_SLOW_SPEED && rpm > stepDn)
			rpm = max(HOMING_SLOW_SPEED, rpm - stepDn);
		rotate_one_step(rpm);
		MR.blind_zone++;
		MR.total_steps++;
	}

	// Then align slot (about 4 steps, machine-dependent)
	align_to_light();

	// Update carousel position
	carousel_pos = HOMING_POS;

	_EXIT:

	return MR.ret_val;
}

/**
 * @brief  Rotate carousel to position (aligned with exit) requires enable/disable carousel
 * @param  pos: position from 0 to N_SLOTS-1
 * @retval LS_OK, any error from moveNslots (ALIGNMENT), MOVE_CRSL_ERROR (if fed invalid position)
 */
return_code_t go_to_position(int8_t pos)
{
	int8_t delta;
	extern move_report_t MR;

	mr_init(&MR);
	MR.target = pos;
	return_code_t ret_val = LS_OK;

	if (pos < 0 || pos >= N_SLOTS)
		return INVALID_SLOT;
	if ((pos - carousel_pos) % N_SLOTS != 0)
	{
		delta = (pos - carousel_pos + N_SLOTS) % N_SLOTS;
		ret_val = move_n_slots(delta); // this function updates carouselPos
	}
	return ret_val;
}

void prime_tray(void)
{
	extern dc_motor_t tray_motor;

	load_enable();
	set_motor_rotation(tray_motor, 20, DC_MTR_FWD);
	HAL_Delay(TRAY_PRIMING_TIME);
	set_motor_rotation(tray_motor, 0, DC_MTR_FWD);
}

/**
 * @brief  loadMaxNcards: 	atomic function, loads cards in carousel until tray empty
 * 							or _N cards in
 * @param  _N: 				max n_cards_in
 * @param  rand_mode: 		SEQ_MODE or RAND_MODE
 * @param	p_card_count: 	pointer to integer reporting number of cards loaded
 * @retval 					LS_OK, DO_EMPTY, CHECK_TRAY, SFCIT, LS_ESC
 * 							any error from goToPosition (ALIGNMENT)
 * 							any error from loadOneCard (CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, WRONG_INSERTION)
 */

return_code_t load_max_n_cards(uint8_t _N, rand_mode_t rand_mode,
		uint16_t *p_card_count)
{
	return_code_t ret_val = LS_OK;
	int8_t target;
	int8_t targets[N_SLOTS];
	const uint8_t n_items = N_SLOTS - n_cards_in; // capacity
	uint16_t index;
	bool do_close_flap = false;
	extern bool fresh_load;
	extern load_report_t LR;
	extern union game_state_t game_state;
	extern icon_set_t icon_set_void;
	extern union machine_state_t machine_state;

	// Normalise _N
	_N = min(_N, N_SLOTS);

	// Make sure latch is closed
	set_latch(LATCH_CLOSED);

	// Read machine state
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

	// Check if that is a proper game
	if (is_proper_game(game_state.game_code))
	{
		// Check the card distribution preference
		user_prefs_t user_prefs;
		if ((ret_val = read_user_prefs(&user_prefs, game_state.game_code))
				!= LS_OK)
			goto _EXIT;
		// If hole cards or CC in progress and game prefs are distribution to internal tray, close flap
		if ((game_state.current_stage == HOLE_CARDS
				&& user_prefs.hole_cards_dest == DEST_INTERNAL_TRAY)
				|| (game_state.current_stage >= CC_1
						&& game_state.current_stage <= CC_5
						&& user_prefs.community_cards_dest == DEST_INTERNAL_TRAY))
			do_close_flap = true;
	}
	// Otherwise check if we are shuffling
	else if (game_state.game_code == SHUFFLE)
	{
		do_close_flap = true;
		// Close flap if needed and if there were no cards in shoe already
		if (!cards_in_shoe())
			flap_close();
	}

	// If maximum number of cards reached or exceeded, jump to exit
	if (n_cards_in >= _N)
	{
		ret_val = (n_cards_in > _N ? DO_EMPTY : LS_OK);
		goto _EXIT;
	}
	// Otherwise, but if there are no cards in tray, give an option
	else if (!cards_in_tray())
	{
		fresh_load = true;
		extern icon_set_t icon_set_void;
		if ((ret_val = prompt_interface(MESSAGE,
				(n_cards_in == 0) ? SENCIT : TRAY_IS_EMPTY, "", icon_set_void,
				ICON_CROSS, TRAY_LOAD)) == LS_ESC)
			goto _EXIT;
	}

	/* This is the only point when we break random state
	 * hence the only starting point for loading sequentally
	 */
	if (machine_state.seq_dschg == SEQUENTIAL_STATE)
	{
		machine_state.random_in = SEQUENTIAL_STATE;
		if ((ret_val = write_machine_state()) != LS_OK)
			goto _EXIT;
	}

// If sequential state, force sequential loading
	if (machine_state.random_in == SEQUENTIAL_STATE)
		rand_mode = SEQ_MODE;

// Prepare targets
	if (rand_mode == RAND_MODE)
	{
		// Get random targets
		if ((ret_val = read_random_slots_w_offset(targets, EMPTY_SLOT))
				!= LS_OK)
			goto _EXIT;
	}
	else if (rand_mode == SEQ_MODE)
	{
		// Get empty slots
		if ((ret_val = read_slots_w_offset(targets, EMPTY_SLOT)) != LS_OK)
			goto _EXIT;
		// Order targets from current position
		order_positions(targets, n_items, carousel_pos, ASCENDING_ORDER);
	}

// Prime tray
	if (fresh_load || last_card_stuck_on_entry)
	{
		prime_tray();
		fresh_load = last_card_stuck_on_entry = false;
	}

// Prompt
	prompt_interface(MESSAGE, CUSTOM_MESSAGE, "\nLoading...", icon_set_void,
			ICON_CROSS, NO_HOLD);

// Load as long as cards in tray and maximum not reached
	reset_btns();
	index = 0;
// If loading sequentially break randomness
	if (rand_mode == SEQ_MODE
			&& (ret_val = write_flag(RANDOM_IN_FLAG, SEQUENTIAL_STATE))
					!= LS_OK)
		goto _EXIT;
	while (cards_in_tray() && n_cards_in < _N)
	{
		// Allow for ESC
		if (escape_btn.interrupt_press)
		{
			ret_val = LS_ESC;
			goto _EXIT;
		}

		// Check flap if closing suspended
		if (do_close_flap && !cards_in_shoe())
			flap_close();

		// Define next target
		target = targets[index++];

		// Go to position
		if ((ret_val = go_to_position(target)) != LS_OK)
			goto _EXIT;

		// Load one card, check a second time if card stuck before aborting
		if ((ret_val = load_one_card(rand_mode)) == CARD_STUCK_ON_ENTRY)
		{
			if (read_sensor(ENTRY_SENSOR_1) == CARD_SEEN
					|| read_sensor(ENTRY_SENSOR_3) == CARD_SEEN)
			{
				last_card_stuck_on_entry = true;
				goto _EXIT;
			}
			// If cleared restore ret_val
			else
				ret_val = LR.ret_val = LS_OK;
		}
		// Update card count if OK after check
		if (ret_val == LS_OK)
			(*p_card_count)++;
		// Abort if still in error
		else
			goto _EXIT;
	}

// If all OK otherwise but spare cards in tray, flag
	if (cards_in_tray())
		ret_val = (_N == N_SLOTS ? SFCIT : CHECK_TRAY);
	else
		fresh_load = true;

	_EXIT:

// Signal that the deck is uncut
	write_flag(CUT_FLAG, NOT_CUT_STATE);

// Update card tally
	update_card_tally((uint32_t) *p_card_count);

	return ret_val;
} // end of load_max_n_cards

/**
 * @brief  loadCarousel:	Loads cards in carousel until full. Uses loadMaxNcards
 * @param  rand_mode: 	SEQ_MODE or RAND_MODE
 * @param	p_card_count: pointer to integer reporting number of cards loaded
 * @retval LS_OK, SFCIT, LS_ESC
 * 		any error from goToPosition (ALIGNMENT)
 * 		any error from loadOneCard (CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, WRONG_INSERTION)
 */
return_code_t load_carousel(rand_mode_t rand_mode, uint16_t *p_card_count)
{
	return_code_t ret_val;

// Load up to N_SLOTS cards
	ret_val = load_max_n_cards(N_SLOTS, rand_mode, p_card_count);

	return ret_val;
}

/*
 loadToN uses loadMaxNcards to load exactly to N cards
 *
 * return codes:
 * DO_EMPTY if n_cards_in > _N
 * CHECK_TRAY if n_cards_in == _N < N_SLOTS and cards in tray
 * SFCIT if 	 n_cards_in == N_SLOTS and cards in tray
 * LS_ESC if ESC while loading cards
 * @retval LS_OK, DO_EMPTY, CHECK_TRAY, SFCIT, LS_ESC
 * 		any error from goToPosition (ALIGNMENT)
 * 		any error from loadOneCard (CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, WRONG_INSERTION)
 */

return_code_t load_to_n_cards(uint8_t _N, rand_mode_t rand_mode,
		uint16_t *p_card_count)
{
	extern char display_buf[];
	extern icon_set_t icon_set_check;
	extern icon_set_t icon_set_void;
	extern bool fresh_load;
	return_code_t ret_val = LS_OK;
	return_code_t user_input;
	uint32_t cardLoadWait = L_WAIT_DELAY;

// Check that we are not already there
	if (n_cards_in == _N)
	{
		if (cards_in_tray())
			ret_val = (_N == N_SLOTS ? SFCIT : CHECK_TRAY);
		else
			fresh_load = true;
		goto _EXIT;
	}

// Check there is no excess of cards
	if (n_cards_in > _N)
	{
		warn_n_cards_in();
		ret_val = DO_EMPTY;
		goto _EXIT;
	}

// Otherwise loop while insufficient number of cards
	while (n_cards_in < _N)
	{
		// If cards in tray, load until _N reached
		if (cards_in_tray())
		{
			// Allow for ESC
			prompt_interface(MESSAGE, CUSTOM_MESSAGE, "\nLoading...",
					icon_set_void, ICON_CROSS, NO_HOLD);
			ret_val = load_max_n_cards(_N, rand_mode, p_card_count);
			if (ret_val == CARD_STUCK_IN_TRAY)
			{
				user_input = prompt_interface(MESSAGE, ret_val, "",
						icon_set_check, ICON_VOID, BUTTON_PRESS);
				// Abort if escape
				if (user_input == LS_ESC)
				{
					ret_val = user_input;
					break;
				}
			}
			else if (ret_val != LS_OK)
				break;
		}
		// Else if tray empty, suggest loading, then either exit or load based on user command
		else
		{

			fresh_load = true;
			warn_n_cards_in();

			// Prompt to load( _N-n_cards_in) missing cards
			// Wait either for cards to be loaded or ESC
			snprintf(display_buf, N_DISP_MAX, "\nLoad %d card%s ",
					_N - n_cards_in, (_N - n_cards_in == 1 ? "" : "s"));
			user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
					icon_set_void, ICON_CROSS, TRAY_LOAD);
			// Continue based on user input, either exit the loop
			if (user_input == LS_ESC)
			{
				ret_val = user_input;
				break;
			}
			// Or stay in loop after safety delay
			else if (cards_in_tray())
				HAL_Delay(cardLoadWait);
		}
	}

	_EXIT:

	return ret_val;
}

void mr_init(move_report_t *pMR)
{
	pMR->ret_val = MOVE_CRSL_ERROR;
	pMR->align_OK = false;
	pMR->one_slot = false;
	pMR->total_steps = 0;
	pMR->blind_zone = 0;
	pMR->first_light_zone = 0;
	pMR->dark_zone = 0;
	pMR->watch_zone = 0;
	pMR->optical_offset = 0;
	pMR->full_step = 0;
	pMR->post_correction = 0;
	pMR->initial_move = 0;
	pMR->homing_zone = 0;
	pMR->target = -2;

	return;
}

void er_init(eject_report_t *pER)
{
	pER->ret_val = EJECT_ERROR;
	pER->alignIssue = false;
	pER->wiggle = 0;
	pER->wiggleMin = 0;
	pER->wiggleMax = 0;
	pER->vib = false;
	pER->vibDuration = 0;
	return;
}

void lr_init(load_report_t *pLR)
{
	pLR->t_0 = 0;
	pLR->t_seen = 0;
	pLR->t_clear = 0;
	pLR->sticky = false;
	pLR->stuck = 0;
	pLR->ret_val = LOAD_ERROR;

	return;
}

void vr_init(vibration_report_t *pVR)
{
	pVR->ret_val = VIBRATION_ERROR;
	pVR->vibrationDuration = 0;
	return;
}

return_code_t carousel_vibration(uint32_t duration)
{
	const uint16_t halfRange = MS_FACTOR;
	const uint16_t stepDelay = 10;
	bool direction = CRSL_FWD;
	uint32_t startTime;
	uint32_t endTime;
	bool sensorState = read_sensor(EXIT_SENSOR);
	bool noChange;
	vibration_report_t VR;

	vr_init(&VR);
	startTime = HAL_GetTick();
// Stops when sensor status changes or after duration whichever is sooner
	while (((endTime = HAL_GetTick()) < startTime + duration)
			&& (noChange = (sensorState == read_sensor(EXIT_SENSOR))))
	{
		// go half way in current direction
		for (uint16_t i = 0; i < halfRange; i++)
			microstep_crsl(stepDelay);
		// go back and stay afterwards in this new direction
		direction = !direction;
		set_crsl_dir_via_pin(direction);
		for (uint16_t i = 0; i < halfRange; i++)
			microstep_crsl(stepDelay);
		// small wait
		HAL_Delay(10);
	}
	VR.vibrationDuration = endTime - startTime;
// Restore direction
	set_crsl_dir_via_pin(CRSL_FWD);

	VR.ret_val = noChange ? VIBRATION_ERROR : LS_OK;

	return VR.ret_val;
}

/**
 * @brief  Eject one card from the carousel
 * @param  rand_mode: SEQ_MODE or RAND_MODE (opens and closes latch or not)
 * @param	gap: DEAL_GAP, NO_DEAL_GAP (minimum gap between 2 cards, when dealing individual hands on the table
 * @param	pER: pointer to ejectReport containing info for debugging
 * @retval LS_OK, SLOT_IS_EMPTY, CARD_STUCK_ON_EXIT
 */
return_code_t eject_one_card(safe_mode_t safe_mode, rand_mode_t rand_mode,
bool gap)
{
	vibration_report_t VR;
	extern eject_report_t ER;
	extern uint32_t last_deal_time;
	extern gen_prefs_t gen_prefs;
	bool cardCleared = false;
	bool cardSeen = false;
	uint32_t startTime;
	const int16_t maxMS = 0;
	const int16_t minMS = 0;
	const uint32_t stepDelay = 1500UL;
	const uint32_t waitSeenFirstTime = 200UL;
	const uint32_t waitSeenPostWiggle = (rand_mode == SEQ_MODE ? 20 : 100UL);
	const uint32_t waitSeenPostVibration = 50UL;
	const uint32_t waitClearExit = 300UL;
	const uint32_t vibDelay = (safe_mode == SAFE_MODE ? 50UL : 300UL);
	bool direction;

// Double safety
	if (safe_mode == SAFE_MODE)
		rand_mode = SEQ_MODE;

	er_init(&ER);
	vr_init(&VR);

// Open Latch - with minimum delay if gap (used when random dealing)
	if (gap)
		while (HAL_GetTick()
				< last_deal_time
						+ (uint32_t) gen_prefs.deal_gap_index
								* DEAL_GAP_MULTIPLIER)
			;
	set_latch(LATCH_OPEN);

// Wait D1 for card to get to sensor
	if (wait_sensor(EXIT_SENSOR, CARD_SEEN, waitSeenFirstTime) == LS_OK)
		goto _CARD_SEEN;

// If not, flag
	ER.alignIssue = true;

// Wiggle (if range ≠ 0)
	if (maxMS != 0 || minMS != 0)
	{
		// Wiggle back and forth to release (interrupt => polled)
		while (!(cardSeen = (read_sensor(EXIT_SENSOR) == CARD_SEEN))
				&& ER.wiggle < maxMS)
		{
			microstep_crsl(stepDelay);
			ER.wiggle++;
		}
		ER.wiggleMax = ER.wiggle;

		// If card seen, end wiggle
		if (cardSeen)
			goto _WIGGLE_HOUSEKEEPING;

		// If not, turn carousel backwards
		set_crsl_dir_via_pin(CRSL_BWD);
		// Wait for card seen on exit (interrupt => polled)
		while (!(cardSeen = (read_sensor(EXIT_SENSOR) == CARD_SEEN))
				&& ER.wiggle > minMS)
		{
			microstep_crsl(stepDelay);
			ER.wiggle--;
		}
		ER.wiggleMin = ER.wiggle;

		// If card seen, end wiggle
		if (cardSeen)
			goto _WIGGLE_HOUSEKEEPING;

		// else wait  (interrupt  => polled)
		startTime = HAL_GetTick();
		while (!(cardSeen = (read_sensor(EXIT_SENSOR) == CARD_SEEN))
				&& HAL_GetTick() < startTime + waitSeenPostWiggle)

			_WIGGLE_HOUSEKEEPING:
			// Restore position
			direction = (ER.wiggle <= 0 ? CRSL_FWD : CRSL_BWD);
		set_crsl_dir_via_pin(direction);
		while (ER.wiggle != 0)
			ER.wiggle > 0 ? ER.wiggle-- : ER.wiggle++;
		// Restore direction if necessary
		if (direction == CRSL_BWD)
			set_crsl_dir_via_pin(CRSL_FWD);

		if (cardSeen)
			goto _CARD_SEEN;

	}

// if card not seen, try vibration
	cardSeen = (carousel_vibration(vibDelay) == LS_OK);
	if (cardSeen)
		goto _CARD_SEEN;

// If card was not seen wait (interrupt  => polled)
	startTime = HAL_GetTick();
	while (!(cardSeen = (read_sensor(EXIT_SENSOR) == CARD_SEEN))
			&& HAL_GetTick() < startTime + waitSeenPostVibration)
		if (cardSeen)
			goto _CARD_SEEN;

// Flag slot empty if did not work
	ER.ret_val = SLOT_IS_EMPTY;
	goto _EXIT;

// _CARD SEEN
	_CARD_SEEN:

// Wait for card to clear sensor
	cardCleared =
			(wait_sensor(EXIT_SENSOR, CARD_CLEAR, waitClearExit) == LS_OK);

// If card cleared, move on
	if (cardCleared)
	{
		ER.ret_val = LS_OK;
		goto _EXIT;
	}

// if it doesn'work, try vibration
	cardCleared = (carousel_vibration(1000) == LS_OK);

	ER.vib = true;
	ER.vibDuration = VR.vibrationDuration;

// If card cleared, move
	if (cardCleared)
	{
		ER.ret_val = LS_OK;
		goto _EXIT;
	}
// Otherwise flag
	else
		ER.ret_val = CARD_STUCK_ON_EXIT;

	_EXIT:

// Update slot (only function with loadOneCard)
	if (ER.ret_val != SLOT_IS_EMPTY)
	{
		if (update_current_slot_w_offset(EMPTY_SLOT) != LS_OK)
			LS_error_handler(EERAM_ERROR);
	}

// Except if card stuck or slot empty, close latch and note time (only if random operation)
	if ((rand_mode == RAND_MODE) && (ER.ret_val != CARD_STUCK_ON_EXIT)
			&& (ER.ret_val != SLOT_IS_EMPTY || safe_mode == SAFE_MODE))
	{
		set_latch(LATCH_CLOSED);
		last_deal_time = HAL_GetTick();
	}

	return ER.ret_val;
}

typedef struct moveSummary
{
	uint32_t microsteps;
	double elapsedTime;
	double averageRPM;
} moveSummary;

// Accelerate or decelerate stepper for nMicroSteps
// starting by rpmInitial using calculated coefs based on carousel speeds
// returns final velocity
double rampStepper(double rpmInitial, uint32_t nMicroSteps,
bool accelDecel, moveSummary *pSummary)
{
	double usCalc = US_PER_STEP_AT_1_RPM / MS_FACTOR / rpmInitial - STP_OVERHEAD;
	uint32_t usCurrent = round(usCalc);
	uint32_t startTime;
	const double accelK = pow((float) CRSL_SLOW_SPEED / CRSL_FAST_SPEED,
			1.0 / (accelZone * MS_FACTOR));
	const double decelK = pow((float) CRSL_FAST_SPEED / CRSL_SLOW_SPEED,
			1.0 / (decelZone * MS_FACTOR));

	pSummary->microsteps = 0;
	pSummary->averageRPM = 0;
	pSummary->elapsedTime = 0;
	startTime = HAL_GetTick();

	while (pSummary->microsteps < nMicroSteps)
	{
		// step motor
		HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, GPIO_PIN_RESET);
		// wait for usCurrent
		delay_micro_s(usCurrent);
		// update usCurrent including overhead
		usCalc = (usCalc + STP_OVERHEAD)
				* ((accelDecel == ACCELERATE) ? accelK : decelK) - STP_OVERHEAD;
		usCurrent = (uint32_t) round(usCalc);
		// increment microsteps
		pSummary->microsteps++;
	}

	pSummary->elapsedTime = (double) (HAL_GetTick() - startTime) / 1000;
	pSummary->averageRPM = (double) pSummary->microsteps / pSummary->elapsedTime
			* SEC_PER_MINUTE / MS_FACTOR / CRSL_RESOLUTION;

// Return final calculated velocity
	return (double) US_PER_STEP_AT_1_RPM / MS_FACTOR
			/ (usCurrent + STP_OVERHEAD);
}

// Rotate stepper at rpm constant speed for nMicroSteps
void coastStepper(double rpm, uint32_t nMicroSteps, moveSummary *pSummary)
{
	const double usCalc = US_PER_STEP_AT_1_RPM / MS_FACTOR / rpm - STP_OVERHEAD;
	const uint32_t usCurrent = (uint32_t) round(usCalc);
	uint32_t startTime;

	pSummary->microsteps = 0;
	pSummary->averageRPM = 0;
	pSummary->elapsedTime = 0;
	startTime = HAL_GetTick();

	while (pSummary->microsteps < nMicroSteps)
	{
		HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, SET);
		HAL_GPIO_WritePin(STP_STEP_GPIO_Port, STP_STEP_Pin, GPIO_PIN_RESET);
		delay_micro_s(usCurrent);
		pSummary->microsteps++;
	}
	pSummary->elapsedTime = (double) (HAL_GetTick() - startTime) / 1000;
	pSummary->averageRPM = (double) pSummary->microsteps / pSummary->elapsedTime
			* SEC_PER_MINUTE / MS_FACTOR / CRSL_RESOLUTION;

	return;
}

/*
 * Rotate N microsteps with exponential acceleration and deceleration
 * starting and ending at the SAME (low) speed rpmStartStop
 * limited by rpmMax in the middle
 * ADD RETURN CODE AND STALL
 */
void rotateStepperWithRamp(uint32_t nMicroSteps, double rpmStartStop,
		double rpmMax)
{
	const double accelK = pow((float) CRSL_SLOW_SPEED / CRSL_FAST_SPEED,
			1.0 / (accelZone * MS_FACTOR));
	const double decelK = pow((float) CRSL_FAST_SPEED / CRSL_SLOW_SPEED,
			1.0 / (decelZone * MS_FACTOR));
// accelSplit: moment when we go from accel to decel, before rpmMax reached (short move)
	const double accelSplitPct = log(decelK) / (log(decelK) - log(accelK));

// NaMax: full acceleration from rpmStartStop to rpmMax (maximum # of accelerating steps)
	const uint32_t NaMax = (uint32_t) round(
			(log(rpmStartStop) - log(rpmMax)) / log(accelK));
// NdMax: same going from rpmMax to rpmStartStop
	const uint32_t NdMax = (uint32_t) round(
			(log(rpmMax) - log(rpmStartStop)) / log(decelK));

//	// Logging
	const int nPhases = 3;
	moveSummary summaries[nPhases + 1];
//	char * phaseLabels[] = {"acceleration", "coast", "deceleration", "grand total"};
	for (int i = 0; i < nPhases + 1; i++)
	{
		summaries[i].microsteps = 0;
		summaries[i].elapsedTime = 0;
		summaries[i].averageRPM = 0;
	}

// actual nAccel: NaMax or less if we can't reach rpmMax
	const uint32_t nAccel = min((uint32_t ) round(accelSplitPct * nMicroSteps),
			NaMax);
// actual nDecel: NdMax or less if we couldn't reach rpmMax
	const uint32_t nDecel = min(nMicroSteps - nAccel, NdMax);
// nCoast by difference
	const uint32_t nCoast =
			nAccel + nDecel < nMicroSteps ? nMicroSteps - (nAccel + nDecel) : 0;
	double rpmCoast;
//

	int phase = 0;
	rpmCoast = rampStepper(rpmStartStop, nAccel, ACCELERATE,
			&summaries[phase++]);
	coastStepper(rpmCoast, nCoast, &summaries[phase++]);
	rampStepper(rpmCoast, nDecel, DECELERATE, &summaries[phase++]);

//	// stats
//	for (int i = 0; i < nPhases; i++)
//	{
//		summaries[nPhases].microsteps  += summaries[i].microsteps;
//		summaries[nPhases].elapsedTime += summaries[i].elapsedTime;
//	}
//	summaries[nPhases].averageRPM = (double) summaries[nPhases].microsteps/summaries[nPhases].elapsedTime*SEC_PER_MINUTE/MS_FACTOR/CRSL_RESOLUTION;
//
//
//	for (int i = 0; i < nPhases+1; i ++)
//	{
//		BSP_LCD_Clear(LCD_COLOR_BCKGND);
//		int row = 0;
//		snprintf(display_buf, N_DISP_MAX, "%s", phaseLabels[i]);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "%lu microsteps", summaries[i].microsteps);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "elapsed:     %7.2f", summaries[i].elapsedTime);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "average rpm: %6.1f", summaries[i].averageRPM);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "usCalc:      %6.1f", summaries[i].usCalc);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "usCurrent:   %6lu", summaries[i].usCurrent);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "rpm coast %5.1f", rpmCoast);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "rpm final %5.1f", rpmFinal);
//		promptBasicItem(display_buf, row++);
//		waitBtns();
//		resetBtns();
//	}
	return;
}

/**
 * @brief  Rotates carousel by N slots. Needs carousel enabled
 * @param  N: integer number of slots
 * @retval LS_OK, ALIGNMENT
 */
return_code_t move_n_slots(uint8_t N)
{
	/*Sequence as follows:
	 * • Move forward blind zone, leaving watch zone at the end
	 * • Accelerate at beginning of blind zone, decelerate at end
	 * • Find the light in watch zone with tolerance
	 * General parameters:
	 * • light window is 2 to 4 full steps, average 3.2
	 * • dark  window is 6 to 8 full steps, average 6.8
	 * • light + dark is 9 to 11 full steps, average 10 (== STEPS_PER_SLOT)
	 * Total steps in revolution 540 (= theoretical)
	 * */

	extern move_report_t MR;
	extern union machine_state_t machine_state;
	const int32_t blind_zone =
			(N * STEPS_PER_SLOT - WATCH_ZONE - OPTICAL_OFFSET) * MS_FACTOR;
	bool previous_reading;
	bool reading;

// Initialise report
	mr_init(&MR);

// Get machine state
	if ((MR.ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;
// DOUBLE DECK LOGIC TO BE REVIEWED (RAMP ACCEL CONSISTENCY?)
	const double slow_speed =
			(machine_state.double_deck == DOUBLE_DECK_STATE) ?
			DDECK_SLOW_SPEED : CRSL_SLOW_SPEED;
	const double max_speed =
			(machine_state.double_deck == DOUBLE_DECK_STATE) ?
			DDECK_MAX_SPEED : CRSL_FAST_SPEED;
	const uint32_t slow_lag = (uint32_t) round(
	US_PER_STEP_AT_1_RPM / MS_FACTOR / slow_speed - STP_OVERHEAD);
	const int16_t max_steps_for_one = (STEPS_PER_SLOT + 2) * MS_FACTOR;

	// Check we are still aligned to the light window with debouncing
	reading = slot_light();
	do
	{
		HAL_Delay(DEBOUNCE_TIME);
		previous_reading = reading;
		reading = slot_light();
	}
	while (reading != previous_reading);
	MR.align_OK = reading;

	if (N == 0)
	{
		MR.ret_val = LS_OK;
		return MR.ret_val;
	}
	else if (N == 1)
	{
		// Flag one slot
		MR.one_slot = true;
		// Go to the end of light
		while (MR.total_steps < max_steps_for_one)
		{
			if (slot_dark())
			{
				HAL_Delay(DEBOUNCE_TIME);
				if (slot_dark())
					break;

			}

			microstep_crsl(slow_lag);
			MR.total_steps++;
			MR.first_light_zone++;
		}
		// Go to the beginning of light
		while (MR.total_steps < max_steps_for_one)
		{
			if (slot_light())
			{
				HAL_Delay(DEBOUNCE_TIME);
				if (slot_light())
					break;

			}
			microstep_crsl(slow_lag);
			MR.total_steps++;
			MR.dark_zone++;
		}
	}
	else
	{
		// Move in the blind zone with no feedback.
		rotateStepperWithRamp(blind_zone, slow_speed, max_speed);
		MR.blind_zone = blind_zone;
		MR.total_steps += blind_zone;
	}
	if (N == 1 && MR.total_steps >= max_steps_for_one)
	{
		MR.ret_val = ALIGNMENT_ON_ONE;
		return MR.ret_val;
	}

// If OK updates carouselPos (atomic function, only one to do this with homeCarousel())
	MR.ret_val = align_to_light();
	if (MR.ret_val == LS_OK)
		carousel_pos = (carousel_pos + N) % N_SLOTS;

	_EXIT:

//	// DEBUGGING - show PWM_GRAD_AUTO and PWM_OFS_AUTO
//	while (1)
//	{
//		int row = 0;
//		promptBasicItem("moveNslots debugging", row++);
//		tmc2209_pwm_auto_t pwm_auto_;
//		pwm_auto_.bytes =tmc2209_read(ADDRESS_PWM_AUTO);
//		snprintf(display_buf, N_DISP_MAX, "PWM_GRAD_AUTO: %#X", pwm_auto_.pwm_grad_auto);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "PWM_OFS_AUTO:  %#X", pwm_auto_.pwm_ofs_auto);
//		promptBasicItem(display_buf, row++);
//		moveNslots(N_SLOTS);
//		pwm_auto_.bytes =tmc2209_read(ADDRESS_PWM_AUTO);
//		snprintf(display_buf, N_DISP_MAX, "PWM_GRAD_AUTO: %#X", pwm_auto_.pwm_grad_auto);
//		promptBasicItem(display_buf, row++);
//		snprintf(display_buf, N_DISP_MAX, "PWM_OFS_AUTO:  %#X", pwm_auto_.pwm_ofs_auto);
//		promptBasicItem(display_buf, row++);
//
//		waitBtns();
//		resetBtns();
//	}
//

	return MR.ret_val;
}	// end move_n_slots

// Aligns to beginning of light with optical offset
// Origin of all ALIGNMENT flags
return_code_t align_to_light()
{

	extern move_report_t MR;
	const double align_speed = 3.7;
	extern union machine_state_t machine_state;
	const double slow_speed =
			(machine_state.double_deck == DOUBLE_DECK_STATE) ?
			DDECK_SLOW_SPEED :
																CRSL_SLOW_SPEED;
	const uint32_t slow_lag = (uint32_t) round(
	US_PER_STEP_AT_1_RPM / MS_FACTOR / slow_speed - STP_OVERHEAD);
	uint16_t us = (uint16_t) round(
	US_PER_STEP_AT_1_RPM / MS_FACTOR / align_speed);

	// Find the BEGINNING OF LIGHT with debounce
	// Observed in move (1): up to 220 microsteps
	while (MR.watch_zone < (WATCH_ZONE + TOLERANCE) * MS_FACTOR)
	{
		if (slot_light())
		{
			HAL_Delay(DEBOUNCE_TIME);
			if (slot_light())
				break;

		}

		microstep_crsl(us);
		MR.watch_zone++;
		MR.total_steps++;

	}
	if (MR.watch_zone >= (WATCH_ZONE + TOLERANCE) * MS_FACTOR)
		MR.ret_val = MISSED_ALIGNMENT;

	// Finish on a full step
	while (MR.total_steps % MS_FACTOR != 0)
	{
		microstep_crsl(us);
		MR.total_steps++;
		MR.full_step++;
	}
	// If all OK take into account optical offset if applicable
	if (OPTICAL_OFFSET != 0 && MR.ret_val == LS_OK)
	{
		while (MR.optical_offset < OPTICAL_OFFSET * MS_FACTOR)
		{
			microstep_crsl(us);
			MR.optical_offset++;
			MR.total_steps++;
		}
		if (MR.optical_offset >= OPTICAL_OFFSET * MS_FACTOR)
		{
			MR.ret_val = MISSED_ALIGNMENT;
			goto _EXIT;
		}
	}

	// Check we still are in light and correct up to an arbitrary number of µsteps with debounce
	uint16_t max_post_correction = MS_FACTOR * STEPS_PER_SLOT;
	while (MR.post_correction < max_post_correction)
	{
		if (slot_light())
		{
			HAL_Delay(DEBOUNCE_TIME);
			if (slot_light())
				break;

		}
		microstep_crsl(slow_lag);
		MR.total_steps++;
		MR.post_correction++;
	}

	// If we hit the limit, flag error
	if (MR.post_correction >= max_post_correction)
		MR.ret_val = ALIGNMENT;

	_EXIT:

	return MR.ret_val;
}

int16_t read_encoder(bool direction)
{
	extern int16_t prev_encoder_pos;
	int16_t increment;
	int16_t encoderPosition;

// Calculate increment
	encoderPosition = ENCODER_POSITION;
	increment = (direction ? 1 : -1) * (encoderPosition - prev_encoder_pos);
// Reset previous encoder position
	prev_encoder_pos = encoderPosition;
//ENCODER_REFERENCE = (int32_t) encoderPosition;

	return increment;
}

return_code_t adjust_flap(uint32_t *p_ref_pos, int8_t min_adjust, int8_t max_adjust)
{
	return_code_t ret_val = LS_OK;
	int16_t increment;
	uint32_t final_pos = *p_ref_pos;
	uint32_t start_pos = *p_ref_pos;
	uint8_t w_data;
	extern char display_buf[];
	extern uint32_t flap_open_pos;

	// Check if unlimited mode (0,0 means no limits)
	bool unlimited = (min_adjust == 0 && max_adjust == 0);

	// Calculate min/max allowed positions
	uint32_t min_pos = unlimited ? 0 :
			(start_pos > (uint32_t)(-min_adjust * FLAP_FACTOR)) ?
			start_pos + min_adjust * FLAP_FACTOR : 0;
	uint32_t max_pos = unlimited ? UINT32_MAX :
			start_pos + max_adjust * FLAP_FACTOR;

	set_flap(*p_ref_pos);
	reset_btns();
	snprintf(display_buf, N_DISP_MAX, "Position:  %d",
			(uint8_t) (final_pos / FLAP_FACTOR));
	prompt_basic_item(display_buf, 1);
	prompt_basic_item("      ", 2);
	while (!encoder_btn.interrupt_press && !escape_btn.interrupt_press)
	{
		watchdog_refresh();
		while ((increment = read_encoder(CLK_WISE)) == 0
				&& !encoder_btn.interrupt_press && !escape_btn.interrupt_press)
			watchdog_refresh();

		// Calculate new position
		int32_t new_pos = (int32_t)final_pos + increment * FLAP_FACTOR;

		// Prevent negative
		if (new_pos < 0)
			new_pos = 0;

		// Clamp to min/max range (skip if unlimited)
		if (!unlimited)
		{
			if ((uint32_t)new_pos < min_pos)
				new_pos = min_pos;
			if ((uint32_t)new_pos > max_pos)
				new_pos = max_pos;
		}

		if ((uint32_t)new_pos != final_pos)
		{
			final_pos = (uint32_t)new_pos;
			set_flap(final_pos);
			snprintf(display_buf, N_DISP_MAX, "Position:  %d",
					(uint8_t) (final_pos / FLAP_FACTOR));
			prompt_basic_item(display_buf, 1);
		}
		else if (encoder_btn.interrupt_press && final_pos != *p_ref_pos)
		{
			// Update default position
			*p_ref_pos = final_pos;
			// Store it in EERAM
			w_data = (uint8_t) (final_pos / FLAP_FACTOR);
			ret_val = write_eeram(
					p_ref_pos == &flap_open_pos ?
					EERAM_FLAP_OPEN :
													EERAM_FLAP_CLOSED, &w_data,
					1);
			prompt_basic_item("Saved", 2);
			HAL_Delay(200);
		}
		// Note: removed "else set_flap(*p_ref_pos)" which incorrectly reset
		// position when hitting min/max limits
	}
	reset_btns();

	return ret_val;
}

/**
 * @brief  Switches to double deck mode and loads cards until full or button press
 * @retval LS_OK, LS_ESC, DO_LOAD
 * @retval any error from loadOneCard and goToPosition (also captured in LR and MR)
 */
return_code_t load_double_deck(uint16_t *p_n_loaded)
{
	return_code_t ret_val;
	return_code_t user_input;
	int8_t interim_list[2 * N_SLOTS];
	int8_t target_list[2 * N_SLOTS];
	uint16_t target_index = 0;
	int8_t target;
	uint8_t rdm;
	uint32_t cardLoadWait = L_WAIT_DELAY;

	extern bool fresh_load;
	extern union machine_state_t machine_state;
	extern char display_buf[N_DISP_MAX + 1];

// Get machine state
	if ((ret_val = read_machine_state() != LS_OK))
		goto _EXIT;

// If shuffler content either not random or sequentially discharged, abort
	if (machine_state.random_in == SEQUENTIAL_STATE
			|| machine_state.seq_dschg == SEQUENTIAL_STATE)
	{
		if ((ret_val = force_empty(
				"Shuffler will be emptied\nfor 2-deck shuffle.", TIME_LAG))
				!= LS_OK)
			goto _EXIT;
	}

	// CREATE TARGET LIST
	// Initialise double_carousel
	carousel_t double_carousel[2];
	memset(double_carousel, 0, 2 * E_CRSL_SIZE);

	// If cards in carousel, read carousel contents from EERAM
	if (n_cards_in != 0)
	{
		if ((ret_val = read_eeram(EERAM_CRSL, double_carousel[0].bytes,
		E_CRSL_SIZE)) != LS_OK)
			goto _EXIT;
		if ((ret_val = read_eeram(EERAM_CRSL_2, double_carousel[1].bytes,
		E_CRSL_SIZE)) != LS_OK)
			goto _EXIT;
	}

// Populate interim list with all positions where there is space (twice if space for 2 cards)
// taking into account position offset for go_to_position
	for (uint16_t i = 0; i < 2; i++)
		for (uint16_t j = 0; j < N_SLOTS; j++)
		{
			if (((double_carousel[i].drum >> j) & 1) == EMPTY_SLOT)
				interim_list[target_index++] = (j + N_SLOTS - IN_OFFSET)
						% N_SLOTS;
		}

// Fill the rest of interim list with -1
	for (uint16_t i = target_index; i < 2 * N_SLOTS; i++)
		interim_list[i] = -1;

// Populate random target list from the end (useful for bounded random)
	while (target_index > 0)
	{
		// Random target (in interim list range)
		if (bounded_random(&rdm, target_index) != HAL_OK)
		{
			ret_val = TRNG_ERROR;
			goto _EXIT;
		}

		// Put at end of target list a random value of interim list
		target_list[--target_index] = interim_list[rdm];

		// Remove item # rdm from  interim list (after operation length = target index again)
		for (uint16_t i = rdm; i < target_index; i++)
			interim_list[i] = interim_list[i + 1];
	}

// LOAD CARDS
	*p_n_loaded = 0;
// Set double deck flag
	if ((ret_val = write_flag(DOUBLE_DECK_FLAG, DOUBLE_DECK_STATE)) != LS_OK)
		goto _EXIT;

// Take note of default double deck
	uint8_t default_NDD;
	if ((ret_val = read_default_n_double_deck(&default_NDD)) != LS_OK)
		goto _EXIT;
	reset_btns();

// Load cards
	while (n_cards_in < 2 * N_SLOTS)
	{
		// If cards in tray, load
		if (cards_in_tray())
		{
			extern icon_set_t icon_set_void;
			prompt_interface(MESSAGE, CUSTOM_MESSAGE, "\nLoading...",
					icon_set_void, ICON_CROSS, NO_HOLD);
			// Allow for ESC at each cycle
			if ((ret_val = safe_abort()) == LS_ESC)
				break;

			// Prime tray if needed
			if (fresh_load)
			{
				prime_tray();
				fresh_load = false;
			}

			// Got to next target
			target = target_list[(*p_n_loaded)++];
			if ((ret_val = go_to_position(target)) != LS_OK)
				break;

			// Load one card (updates both logical slots in EERAM)
			do
			{
				if (cards_in_tray())
				{
					// Load one card, check a second time if card stuck before aborting
					if ((ret_val = load_one_card(RAND_MODE))
							== CARD_STUCK_ON_ENTRY)
					{
						if (read_sensor(ENTRY_SENSOR_1) == CARD_SEEN
								|| read_sensor(ENTRY_SENSOR_3) == CARD_SEEN)
							last_card_stuck_on_entry = true;
						// If cleared restore ret_val
						else
							ret_val = LS_OK;
					}

					extern icon_set_t icon_set_check;
					if (ret_val == CARD_STUCK_IN_TRAY)
						prompt_interface(MESSAGE, ret_val, "\nRearrange tray",
								icon_set_check, ICON_VOID, BUTTON_PRESS);
				}
				else
					ret_val = LS_OK;
			}
			while (ret_val == CARD_STUCK_IN_TRAY);

			if (ret_val != LS_OK)
				break;
		}
		// If no cards in tray, check if we have the right number
		else
		{
			// If empty load or abort
			if (n_cards_in == 0)
			{
				clear_message(TEXT_ERROR);
				extern icon_set_t icon_set_void;
				user_input = prompt_interface(MESSAGE, SENCIT, "",
						icon_set_void, ICON_RED_CROSS, TRAY_LOAD);
				if (user_input == LS_ESC)
				{
					ret_val = user_input;
					break;
				}
			}
			// Otherwise exit if it is the right number
			else if (n_cards_in == default_NDD)
				break;
			// If not update default deck or keep loading
			else
			{
				warn_n_cards_in();
				snprintf(display_buf, N_DISP_MAX, "\n%d cards in, OK?",
						n_cards_in);
				clear_message(TEXT_ERROR);
				extern icon_set_t icon_set_check;
				user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE,
						display_buf, icon_set_check, ICON_CROSS, TRAY_LOAD);

				// If user ESC, break and exit
				if (user_input == LS_ESC)
				{
					ret_val = user_input;
					break;
				}
				// If ENT pressed or cards loaded
				else
				{
					// If no cards in tray and more than 54 cards, update default double deck and break loop
					if (!cards_in_tray() && n_cards_in > N_SLOTS)
					{
						ret_val = write_default_n_double_deck(n_cards_in);
						break;
					}
					// If cards in tray keep in loading loop
					else
						HAL_Delay(cardLoadWait);
				}
			}
		}
		prompt_n_cards_in();
	}

	_EXIT:

	return ret_val;
}

return_code_t force_empty(char *text, prompt_mode_t prompt_mode)
{
	return_code_t ret_val;
	extern union machine_state_t machine_state;
	extern union game_state_t game_state;
	extern game_rules_t void_game_rules;
	extern icon_set_t icon_set_check;
	extern icon_set_t icon_set_void;
	return_code_t user_input = LS_OK;

	if (prompt_mode == BUTTON_PRESS)
		user_input = prompt_interface(ALERT, CUSTOM_EMPTY, text, icon_set_check,
				ICON_CROSS, BUTTON_PRESS);
	else
		prompt_interface(ALERT, CUSTOM_EMPTY, text, icon_set_void, ICON_VOID,
				TIME_LAG);
	if (user_input != LS_OK)
	{
		ret_val = user_input;
		goto _EXIT;
	}
	prompt_menu(DEFAULT_SELECT, EMPTY);
	// Update game state to (safe) emptying
	reset_game_state();
	if (machine_state.content_error == CONTENT_OK)
		game_state.current_stage = EMPTYING;
	else
		game_state.current_stage = SAFE_EMPTYING;
	// Write game state
	if ((ret_val = write_game_state()) != LS_OK)
		goto _EXIT;

	// Force empty carousel
	ret_val = discharge_cards(void_game_rules);
	if (ret_val != CARD_STUCK_ON_EXIT)
		set_latch(LATCH_CLOSED);

	_EXIT:

	return ret_val;
}

void test_buzzer(void)
{
	clear_message(TEXT_ERROR);
	prompt_message("Beep beep beep.");
	for (int i = 0; i < 3; i++)
	{
		beep(20);
		if (i < 2)
			HAL_Delay(300);
	}
	clear_message(TEXT_ERROR);
}
