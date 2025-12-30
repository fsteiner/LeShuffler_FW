/*
 * basic_operations.h
 *
 *  Created on: Jun 28, 2024
 *      Author: François S
 */

#ifndef INC_BASIC_OPERATIONS_H_
#define INC_BASIC_OPERATIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <interface.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <TB6612FNG.h>
#include <TMC2209.h>
#include <utilities.h>

// move_report_t
typedef struct
{
	return_code_t ret_val;
	bool align_OK;
	bool one_slot;
	int16_t total_steps;
	int16_t first_light_zone;
	int16_t blind_zone;
	int16_t watch_zone;	int16_t dark_zone;
	int16_t full_step;
	int16_t post_correction;
	int16_t initial_move;
	int16_t homing_zone;
	int16_t optical_offset;

	int8_t target;
} move_report_t;

// eject_report_t
typedef struct
{
	return_code_t ret_val;
	bool alignIssue;
	int16_t wiggle;
	int16_t wiggleMax;
	int16_t wiggleMin;
	bool vib;
	uint32_t vibDuration;
} eject_report_t;

// load_report_t
typedef struct
{
	uint32_t t_0;
	uint32_t t_seen;
	uint32_t t_clear;
	bool sticky;
	uint16_t stuck;
	return_code_t ret_val;
} load_report_t;

// vibration_report_t
typedef struct
{
	return_code_t ret_val;
	uint32_t vibrationDuration;
} vibration_report_t;

// carousel_t
typedef union
{
	struct
	{
		uint64_t drum : N_SLOTS;
		uint64_t reserved :64 - N_SLOTS;
	};
	uint8_t bytes[E_CRSL_SIZE];
} carousel_t;

// rand_mode_t
typedef enum
{
	SEQ_MODE, RAND_MODE
} rand_mode_t;

// safe_mode_t
typedef enum
{
	NON_SAFE_MODE, SAFE_MODE
} safe_mode_t;

void carousel_enable(void);
void carousel_disable(void);
return_code_t carousel_init(void);
void set_crsl_dir_via_pin(bool);
void microstep_crsl(uint16_t);
void rotate_one_step(double);
bool crsl_at_home(void);
bool slot_dark(void);
bool slot_light(void);
return_code_t wait_sensor(sensor_code_t, bool, uint32_t);
return_code_t cards_in_tray(void);
return_code_t cards_in_shoe(void);
void wait_pickup_shoe_or_ESC(void);
void wait_pickup_shoe(uint8_t);
return_code_t safe_abort(void);
return_code_t carousel_vibration(uint32_t);
return_code_t align_to_light(void);
void audioCheck(void);
return_code_t alignmentTest();
void prime_tray(void);
return_code_t load_one_card(rand_mode_t);
return_code_t home_carousel(void);
return_code_t move_n_slots(uint8_t);
return_code_t go_to_position(int8_t);
return_code_t eject_one_card(safe_mode_t, rand_mode_t, bool);
return_code_t load_max_n_cards(uint8_t, rand_mode_t, uint16_t*);
return_code_t load_carousel(rand_mode_t, uint16_t*);
return_code_t load_to_n_cards(uint8_t, rand_mode_t, uint16_t*);
return_code_t load_double_deck(uint16_t*);
return_code_t adjust_flap(uint32_t*);
void mr_init(move_report_t*);
void er_init(eject_report_t*);
void lr_init(load_report_t*);
void vr_init(vibration_report_t*);
int16_t read_encoder(bool);
void test_btns(void);
void test_dc_motor(dc_motor_t motor);
void test_buzzer(void);
return_code_t test_random_deal(void);
return_code_t force_empty(char*, prompt_mode_t);
void rotateStepperWithRamp(uint32_t, double, double);

#ifdef __cplusplus
}
#endif

#endif /* INC_BASIC_OPERATIONS_H_ */
