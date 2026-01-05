/*
 * utilities.h
 *
 *  Created on: Jun 27, 2024
 *      Author: Francois S
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <definitions.h>
#include <main.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <octospi.h>
#include <W25Q64.h>

// item_code_t
typedef enum
{
	ROOT_MENU,
	POKER,
	CASINO,
	CLASSICS,
	SHUFFLE,
	FAVORITES,
	CUSTOM_GAMES,
	DEAL_N_CARDS,
	EMPTY,
	DOUBLE_DECK_SHUFFLE,
	LOAD,
	SETTINGS,
	_5_CARD_DRAW,
	_5_CARD_STUD,
	_6_CARD_STUD,
	_7_CARD_DRAW,
	_7_CARD_STUD,
	CHINESE_POKER,
	DBL_BRD_HOLDEM,
	IRISH_POKER,
	OMAHA,
	PINEAPPLE,
	TEXAS_HOLDEM,
	DEALERS_CHOICE,
	SHUFFLERS_CHOICE,
	_3_CD_POKER,
	_4_CD_POKER,
	BACCARAT,
	BLACKJACK,
	LETEM_RIDE,
	MAXIMUM_FLUSH,
	PAIGOW,
	SOUTHERN_STUD,
	BRIDGE,
	CRIBBAGE,
	HEARTS,
	SPADES,
	LANDLORD,
	GIN_RUMMY,
	RUMMY,
	TIEN_LEN,
	MATATU,
	CUSTOM_1,
	CUSTOM_2,
	CUSTOM_3,
	CUSTOM_4,
	CUSTOM_5,
	SET_FAVORITES,
	SET_DEALERS_CHOICE,
	RESET_CONTENT,
	RESET_PREFS,
	RESET_CUSTOM_GAMES,
	SET_PREFS,
	ADJUST_DEAL_GAP,
	SET_CUT_CARD_FLAG,
	SET_CUSTOM_GAMES,
	ABOUT,
	MAINTENANCE,
	MAINTENANCE_LEVEL_2,
	IMAGE_UTILITY,
	FIRMWARE_UPDATE,
	TEST_MENU,
	ROTATE_TRAY_ROLLER,
	ROTATE_ENTRY_ROLLER,
	TEST_CAROUSEL,
	TEST_CAROUSEL_DRIVER,
	TEST_EXIT_LATCH,
	ACCESS_EXIT_CHUTE,
	TEST_BUZZER,
	ADJUST_CARD_FLAP,
	DISPLAY_SENSORS,
	TEST_IMAGES,
	TEST_WATCHDOG,
	GAMES_LIST,
	POKER_LIST
} item_code_t;

// return_code_t
typedef enum
{
	// Non errors
	LS_OK,
	LS_ESC,			// prompt calling menu in error catch
	DO_EMPTY,		// option to force empty in error catch
	SLOT_IS_EMPTY, 	// managed via content_error
	NOT_SEEN,		// internal
	REPEAT,			// internal
	CONTINUE,		// internal
	CUSTOM_MESSAGE,	// internal
	BURN_CARD,		// internal
	DRAW_CARD,		// internal
	EXTRA_CARD,		// internal
	DO_RECYCLE,		// internal

	//Text errors
	NO_FAVORITES_SELECTED,
	NO_DEALERS_CHOICE_SELECTED,

	//Graphic errors
	CARD_STUCK_IN_TRAY,
	CARD_STUCK_ON_ENTRY,
	CARD_STUCK_ON_EXIT,
	SFCIT,
	SENCIT,
	NOT_ENOUGH_CARDS_IN_SHUFFLER,
	TRAY_IS_EMPTY,
	CHECK_TRAY,
	WRONG_INSERTION,
	PICK_UP_CARDS,
	CUSTOM_PICK_UP,
	CUSTOM_EMPTY,

	// Fatal errors
	HOMING_ERROR,
	MOVE_CRSL_ERROR,
	ALIGNMENT,
	ALIGNMENT_ON_ONE,
	MISSED_ALIGNMENT,
	INITIAL_ALIGNMENT,
	LS_ERROR,
	MENU_ERROR,
	INVALID_CHOICE,
	POINTER_ERROR,
	INVALID_SLOT,
	LOAD_ERROR,
	EJECT_ERROR,
	VIBRATION_ERROR,
	STEPPER_ERROR,
	TRNG_ERROR,
	EERAM_ERROR,
	EERAM_BUSY,
	EERAM_NOT_INITIALISED,
	TMC2209_BUSY,
	TMC2209_TIMEOUT,
	TMC2209_ERROR

} return_code_t;

// error_type_t
typedef enum
{
	NON_ERROR, TEXT_ERROR, GRAPHIC_ERROR, FATAL_ERROR, ERROR_NOT_FOUND
} error_type_t;

// sensor_code_t
typedef enum
{
	TRAY_SENSOR,
	ENTRY_SENSOR_1,
	ENTRY_SENSOR_2,
	ENTRY_SENSOR_3,
	EXIT_SENSOR,
	SHOE_SENSOR,
	HOMING_SENSOR,
	SLOT_SENSOR
} sensor_code_t;

// flag_code_t
// Functions write_flag and read_flag must be consistent with flag list
typedef enum
{
	RESET_FLAG, RANDOM_IN_FLAG, SEQ_DSCHG_FLAG, CUT_FLAG, DOUBLE_DECK_FLAG,
} flag_code_t;

// slot_status_t
typedef enum
{
	EMPTY_SLOT, FULL_SLOT
} slot_status_t;

//sort_order_t
typedef enum
{
	DESCENDING_ORDER, ASCENDING_ORDER
} sort_order_t;

// double_deck_mode_t
typedef enum
{
	SDCK_MODE, DDCK_MODE
} double_deck_mode_t;

// machine_state_t
union machine_state_t
{
	struct
	{
		uint8_t reset :1;
		uint8_t random_in :1;
		uint8_t seq_dschg :1;
		uint8_t cut :1;
		uint8_t double_deck :1;
		uint8_t content_error :1;
		uint8_t reserved :2;
	};

	uint8_t bytes;
};

void image_utility(void);           // in image_utility.c
return_code_t read_offset(void);    // in image_utility.c

GPIO_PinState read_sensor(sensor_code_t);
void beep(uint32_t);
void delay_micro_s(uint16_t);
void reset_encoder(void);
void OLD_prompt_LS_error(return_code_t retValue);
void LS_error_handler(return_code_t retValue);
void insertion_sort(int8_t sequence[], uint16_t, bool);
return_code_t order_positions(int8_t*, uint16_t, int8_t, sort_order_t);
return_code_t read_eeram(uint16_t, uint8_t*, uint16_t);
return_code_t write_eeram(uint16_t, uint8_t*, uint16_t);
return_code_t reset_eeram(uint16_t, uint16_t);
return_code_t read_eeram_bit(uint16_t, uint16_t, uint8_t*);
return_code_t write_eeram_bit(uint16_t, uint16_t, uint8_t);
return_code_t read_flag(flag_code_t, uint8_t*);
return_code_t write_flag(flag_code_t, bool);
return_code_t read_slot(int8_t, slot_status_t*);
return_code_t update_current_slot_w_offset(slot_status_t);
return_code_t read_machine_state(void);
return_code_t write_machine_state(void);
return_code_t reset_machine_state(void);
return_code_t read_game_state(void);
return_code_t write_game_state(void);
return_code_t reset_game_state(void);
return_code_t read_default_n_deck(uint8_t*);
return_code_t write_default_n_deck(uint8_t);
return_code_t read_default_n_double_deck(uint8_t*);
return_code_t write_default_n_double_deck(uint8_t);
return_code_t read_gen_prefs(void);
return_code_t write_deal_gap_index(uint8_t);
return_code_t write_cut_card_flag(bool);
return_code_t read_slots_w_offset(int8_t list[], slot_status_t);
return_code_t read_random_slots_w_offset(int8_t list[], slot_status_t);
return_code_t get_n_cards_in(void);
return_code_t read_n_cards_in(void);
return_code_t some_double_slots(bool*);
return_code_t reset_carousel(void);
return_code_t reset_flap_defaults(void);
return_code_t update_card_tally(uint32_t);
return_code_t reset_card_tally();
return_code_t check_eeram(void);
error_type_t error_type(return_code_t);
char* error_message(return_code_t);

#endif /* UTILITIES_H_ */
