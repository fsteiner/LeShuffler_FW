/*
 *
 *  Created on: Jun 27, 2024
 *      Author: François S
 */

#include <fonts.h>
#include <i2c.h>
#include <games.h>
#include <interface.h> // TO BE REVIEWED
#include <psram.h>
#include <rng.h>
#include <stm32_adafruit_lcd.h>
#include <TMC2209.h>
#include <utilities.h>

return_code_t standard_errors[] =
		{ CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, CARD_STUCK_ON_EXIT, SFCIT,
				SENCIT, NOT_ENOUGH_CARDS_IN_SHUFFLER, TRAY_IS_EMPTY, CHECK_TRAY,
				WRONG_INSERTION };

return_code_t non_errors[] =
{ LS_OK, LS_ESC, DO_EMPTY, SLOT_IS_EMPTY, NOT_SEEN, REPEAT, CONTINUE,
		DO_RECYCLE, CUSTOM_MESSAGE, BURN_CARD, DRAW_CARD, EXTRA_CARD };

return_code_t text_errors[] =
{ NO_FAVORITES_SELECTED, NO_DEALERS_CHOICE_SELECTED };

return_code_t graphic_errors[] =
{ CARD_STUCK_IN_TRAY, CARD_STUCK_ON_ENTRY, CARD_STUCK_ON_EXIT, SFCIT, SENCIT,
		NOT_ENOUGH_CARDS_IN_SHUFFLER, TRAY_IS_EMPTY, CHECK_TRAY,
		WRONG_INSERTION, PICK_UP_CARDS, CUSTOM_PICK_UP, CUSTOM_EMPTY };

return_code_t fatal_errors[] =
{ LS_ERROR, MENU_ERROR, INVALID_CHOICE, POINTER_ERROR, INVALID_SLOT,
		HOMING_ERROR, MOVE_CRSL_ERROR, LOAD_ERROR, EJECT_ERROR, VIBRATION_ERROR,
		ALIGNMENT, ALIGNMENT_ON_ONE, MISSED_ALIGNMENT, INITIAL_ALIGNMENT,
		STEPPER_ERROR, TRNG_ERROR, EERAM_ERROR, EERAM_BUSY,
		EERAM_NOT_INITIALISED, TMC2209_BUSY, TMC2209_TIMEOUT, TMC2209_ERROR };

// Sizes
const uint16_t n_standard_errors = sizeof(standard_errors)
		/ sizeof(return_code_t);
const uint16_t n_non_errors = sizeof(non_errors) / sizeof(return_code_t);
const uint16_t n_text_errors = sizeof(text_errors) / sizeof(return_code_t);
const uint16_t n_graphic_errors = sizeof(graphic_errors)
		/ sizeof(return_code_t);
const uint16_t n_fatal_errors = sizeof(fatal_errors) / sizeof(return_code_t);

// Global variables
extern uint8_t n_cards_in;
extern union game_state_t game_state;
extern union machine_state_t machine_state;

// Define what printing function is reprogrammed (definition below)
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

/**
 * @brief  Retargets the C library printf function to the USART.
 * @param  None
 * @retval None
 */
PUTCHAR_PROTOTYPE
{
	/* Place your implementation of fputc here */
	/* e.g. write a character to the USART and Loop until the end of transmission */
	extern UART_HandleTypeDef huart3;
	HAL_UART_Transmit(&huart3, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
	return ch;
}

void beep(uint32_t duration)
{
	// Start timing (non blocking)
	uint32_t startTime = HAL_GetTick();
	// Start buzzer
	HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
	while (HAL_GetTick() - startTime < duration)
		;
	HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
	return;
}

// Insertion sort (in place, sorts only the first sequence_size elements)
void insertion_sort(int8_t sequence[], uint16_t sequence_size, bool order)
{
	int8_t tmp;
	uint16_t i;
	uint16_t j;
	for (i = 1; i < sequence_size; i++)
	{
		tmp = sequence[i];
		for (j = i; j > 0; j--)
		{
			if (order == ASCENDING_ORDER ?
					sequence[j - 1] > tmp : sequence[j - 1] < tmp)
				sequence[j] = sequence[j - 1];
			else
				break;
		}
		sequence[j] = tmp;
	}
	return;
}

// EERAM access
return_code_t read_eeram(uint16_t address, uint8_t *p_data, uint16_t size)
{
	return_code_t ret_val;
	HAL_StatusTypeDef HS;

	HS = EERAM_ReadSRAMData(&hi2c3, address, p_data, size);
	switch (HS)
	{
		case HAL_OK:
			ret_val = LS_OK;
			break;

		case HAL_BUSY:
			ret_val = EERAM_BUSY;
			break;

		case HAL_ERROR:
			ret_val = EERAM_ERROR;
			break;

		default:
			ret_val = INVALID_CHOICE;
	}

	return ret_val;
}

return_code_t write_eeram(uint16_t address, uint8_t *data, uint16_t size)
{
	return_code_t ret_val;
	HAL_StatusTypeDef HS;

	HS = EERAM_WriteSRAMData(&hi2c3, address, data, size);
	switch (HS)
	{
		case HAL_OK:
			ret_val = LS_OK;
			break;

		case HAL_BUSY:
			ret_val = EERAM_BUSY;
			break;

		case HAL_ERROR:
			ret_val = EERAM_ERROR;
			break;

		default:
			ret_val = INVALID_CHOICE;
	}

	return ret_val;
}

return_code_t reset_eeram(uint16_t address, uint16_t size)
{
	return_code_t ret_val;
	uint8_t w_data[size];
	// Set w_data to 0
	memset(w_data, 0, size);
	// Write memory block
	ret_val = write_eeram(address, w_data, size);

	return ret_val;
}

return_code_t read_machine_state(void)
{
	extern union machine_state_t machine_state;
	return read_eeram(EERAM_MCHN_STATE, &machine_state.bytes,
	E_MCHN_STATE_SIZE);
}

return_code_t write_machine_state(void)
{
	extern union machine_state_t machine_state;
	return write_eeram(EERAM_MCHN_STATE, &machine_state.bytes,
	E_MCHN_STATE_SIZE);
}

return_code_t reset_machine_state(void)
{
	machine_state.reset = DONT_RESET;
	machine_state.double_deck = SINGLE_DECK_STATE;
	machine_state.random_in = RANDOM_STATE;
	machine_state.seq_dschg = RANDOM_STATE;
	machine_state.cut = NOT_CUT_STATE;
	machine_state.content_error = CONTENT_OK;

	return write_machine_state();
}

return_code_t read_game_state(void)
{
	return read_eeram(EERAM_GAME_STATE, game_state.bytes, E_GAME_STATE_SIZE);
}

return_code_t write_game_state(void)
{
	return write_eeram(EERAM_GAME_STATE, game_state.bytes, E_GAME_STATE_SIZE);;
}

return_code_t reset_game_state(void)
{
	memset(game_state.bytes, 0, E_GAME_STATE_SIZE); // game_code = ROOT_MENU (not a game), current_stage = NO_GAME_RUNNING;

	return write_game_state();
}

return_code_t reset_carousel(void)
{
	return_code_t ret_val = LS_OK;

	if ((ret_val = reset_eeram(EERAM_CRSL, E_CRSL_SIZE)) != LS_OK)
		goto _EXIT;
	if ((ret_val = reset_eeram(EERAM_CRSL_2, E_CRSL_SIZE)) != LS_OK)
		goto _EXIT;

	ret_val = get_n_cards_in();

	if (n_cards_in != 0)
		ret_val = LS_ERROR;

	_EXIT:

	return ret_val;
}

return_code_t read_flag(flag_code_t flag_code, uint8_t *p_flag)
{
	return_code_t ret_val;
// Read machine state
	if ((ret_val = read_machine_state() != LS_OK))
		goto _EXIT;

	switch (flag_code)
	{
		case RESET_FLAG:
			*p_flag = machine_state.reset;
			break;

		case RANDOM_IN_FLAG:
			*p_flag = machine_state.random_in;
			break;

		case SEQ_DSCHG_FLAG:
			*p_flag = machine_state.seq_dschg;
			break;

		case CUT_FLAG:
			*p_flag = machine_state.cut;
			break;

		case DOUBLE_DECK_FLAG:
			*p_flag = machine_state.double_deck;
			break;

		default:
			ret_val = INVALID_CHOICE;
	}

	_EXIT:

	return ret_val;
}

return_code_t write_flag(flag_code_t flag_code, bool X)
{
	return_code_t ret_val;

// Read machine state
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

	switch (flag_code)
	{
		case RESET_FLAG:
			machine_state.reset = X;
			break;

		case RANDOM_IN_FLAG:
			machine_state.random_in = X;
			break;

		case SEQ_DSCHG_FLAG:
			machine_state.seq_dschg = X;
			break;

		case CUT_FLAG:
			machine_state.cut = X;
			break;

		case DOUBLE_DECK_FLAG:
			machine_state.double_deck = X;
			break;

		default:
			ret_val = INVALID_CHOICE;
			goto _EXIT;
	}

	ret_val = write_machine_state();

	_EXIT:

	return ret_val;
}

return_code_t reset_flap_defaults(void)
{
	return_code_t ret_val;
	uint8_t w_data;

// Reset flap positions to pre-factory position
	w_data = (uint8_t) (FLAP_OPEN_DEFAULT / FLAP_FACTOR);
	if ((ret_val = write_eeram(EERAM_FLAP_OPEN, &w_data, 1)) != LS_OK)
		goto _EXIT;
	w_data = (uint8_t) (FLAP_CLOSED_DEFAULT / FLAP_FACTOR);
	if ((ret_val = write_eeram(EERAM_FLAP_CLOSED, &w_data, 1)) != LS_OK)
		goto _EXIT;

	_EXIT:

	return ret_val;
}

// Get number of cards in, based on carousel ;ap and reset machine state if shuffler empty
return_code_t get_n_cards_in(void)
{
	return_code_t ret_val;
	carousel_t carousel;

	// Initialise n_cards_in
	n_cards_in = 0;

	// Read 2 EERAM logical carousel blocks
	for (uint8_t i = 0; i < 2; i++)
	{
		if ((ret_val = read_eeram(i == 0 ? EERAM_CRSL : EERAM_CRSL_2,
				carousel.bytes, E_CRSL_SIZE)) != LS_OK)
			goto _EXIT;

		// Count n_cards_in
		for (uint8_t slot_index = 0; slot_index < N_SLOTS; slot_index++)
			n_cards_in += (carousel.drum >> slot_index) & 1;
	}
// If empty reset machine state
	if (n_cards_in == 0)
		ret_val = reset_machine_state();

	_EXIT:

	return ret_val;
}

// Get number of cards in, based on carousel map but don't reset double deck state
return_code_t read_n_cards_in(void)
{
	return_code_t ret_val;
	carousel_t carousel;

	// Initialise n_cards_in
	n_cards_in = 0;

	// Read 2 EERAM logical carousel blocks
	for (uint8_t i = 0; i < 2; i++)
	{
		if ((ret_val = read_eeram(i == 0 ? EERAM_CRSL : EERAM_CRSL_2,
				carousel.bytes, E_CRSL_SIZE)) != LS_OK)
			goto _EXIT;

		// Count n_cards_in
		for (uint8_t slot_index = 0; slot_index < N_SLOTS; slot_index++)
			n_cards_in += (carousel.drum >> slot_index) & 1;
	}
// If empty reset machine state except double deck status
	if (n_cards_in == 0)
	{
		machine_state.reset = DONT_RESET;
		// machine_state.double_deck UNCHANGED
		machine_state.random_in = RANDOM_STATE;
		machine_state.seq_dschg = RANDOM_STATE;
		machine_state.cut = NOT_CUT_STATE;
		machine_state.content_error = CONTENT_OK;

		ret_val = write_machine_state();
	}

	_EXIT:

	return ret_val;
}

return_code_t some_double_slots(bool *p_bool)
{
	return_code_t ret_val;
	// Get the second logical carousel (containing second cards)
	carousel_t carousel;

	if ((ret_val = read_eeram(EERAM_CRSL_2, carousel.bytes, E_CRSL_SIZE))
			!= LS_OK)
		goto _EXIT;

	// Assert if some slots have 2 cards
	*p_bool = (carousel.drum != 0);

	_EXIT:

	return ret_val;
}

/* GET DEFAULT DECK SIZE */
return_code_t read_default_n_deck(uint8_t *pN)
{
	return read_eeram(EERAM_N_DECK, pN, E_N_DECK_SIZE);
}

/* SET DEFAULT DECK SIZE */
return_code_t write_default_n_deck(uint8_t N)
{
	return write_eeram(EERAM_N_DECK, &N, E_N_DECK_SIZE);
}

/* GET DEFAULT DOUBLE DECK SIZE */
return_code_t read_default_n_double_deck(uint8_t *pN)
{
	return read_eeram(EERAM_N_DOUBLE_DECK, pN, E_N_DOUBLE_DECK_SIZE);
}

/* WRITE DEFAULT DOUBLE DECK SIZE */
return_code_t write_default_n_double_deck(uint8_t N)
{
	return write_eeram(EERAM_N_DOUBLE_DECK, &N, E_N_DOUBLE_DECK_SIZE);
}

/* READ  GENERAL PREFERENCES */
return_code_t read_gen_prefs(void)
{
	extern gen_prefs_t gen_prefs;
	return read_eeram(EERAM_GEN_PREFS, &gen_prefs.byte, E_GEN_PREFS_SIZE);
}

/* WRITE  DEAL GAP  INDEX */
return_code_t write_deal_gap_index(uint8_t N)
{
	extern gen_prefs_t gen_prefs;
	gen_prefs.deal_gap_index = N;

	return write_eeram(EERAM_GEN_PREFS, &gen_prefs.byte, E_GEN_PREFS_SIZE);
}

/* WRITE CUT CARD FLAG */
return_code_t write_cut_card_flag(bool BB)
{
	extern gen_prefs_t gen_prefs;
	gen_prefs.cut_card_flag = BB;

	return write_eeram(EERAM_GEN_PREFS, &gen_prefs.byte, E_GEN_PREFS_SIZE);
}

// To update bit # index after address
return_code_t write_eeram_bit(uint16_t address, uint16_t index, uint8_t bit)
{
	return_code_t ret_val;
// Get the location index = A x BYTE + B
	uint16_t A = index / BYTE;
	uint16_t B = index % BYTE;
// Variable to store results
	uint8_t X;

// Read memory block
	if ((ret_val = read_eeram(address + A, &X, 1)) != LS_OK)
		goto _EXIT;
// Update value in byte
	if (bit == 1)
		X = X | (1 << B);
	else
		X = X & ~(1 << B);
// Write memory block
	if ((ret_val = write_eeram(address + A, &X, 1)) != LS_OK)
		goto _EXIT;

	_EXIT:

	return ret_val;
}

// To update bit # index after address
return_code_t read_eeram_bit(uint16_t address, uint16_t index, uint8_t *pBit)
{
	return_code_t ret_val;
// Get the location index = A x BYTE + B
	uint16_t A = index / BYTE;
	uint16_t B = index % BYTE;
// Variable to store results
	uint8_t X;

// Read memory block
	if ((ret_val = read_eeram(address + A, &X, 1)) != LS_OK)
		goto _EXIT;

	*pBit = (X & (1 << B)) >> B;

	_EXIT:

	return ret_val;
}

/*
 * update_current_slot_w_offset(): set current position (with offset if needed) to either full (1) or empty (0)
 * only for single deck
 * @param slot_number:   slot desired value (to be set)
 */
return_code_t update_current_slot_w_offset(slot_status_t set_value)
{
	extern int8_t carousel_pos; // used in expansion of CRSL_POS_WITH_OFFSET
	return_code_t ret_val;
	// If loading (FULL_SLOT), derive slot being loaded from carousel position
	// If emptying (EMPTY_SLOT), slot number is carousel position
	uint16_t slot_with_offset = (uint16_t) CRSL_POS_WITH_OFFSET(set_value);
	uint8_t *p_bit = 0;
	slot_status_t slot_status_1 = EMPTY_SLOT;
	slot_status_t slot_status_2 = EMPTY_SLOT;

	// Get machine state
	extern union machine_state_t machine_state;
	if ((ret_val = read_machine_state()) != LS_OK)
		goto _EXIT;

	// Get first logical slot content
	if ((ret_val = read_slot(slot_with_offset, p_bit)) != LS_OK)
		goto _EXIT;
	slot_status_1 = (slot_status_t) *p_bit;

	// Get second logical slot content if double deck
	if (machine_state.double_deck == DOUBLE_DECK_STATE)
	{
		if ((ret_val = read_slot(slot_with_offset + N_SLOTS, p_bit)) != LS_OK)
			goto _EXIT;
		slot_status_2 = (slot_status_t) *p_bit;
	}

	// Single deck operation
	if (machine_state.double_deck == SINGLE_DECK_STATE)
	{
		// Loading an empty slot
		if (set_value == FULL_SLOT && slot_status_1 == EMPTY_SLOT)
		{
			if ((ret_val = write_eeram_bit(EERAM_CRSL, slot_with_offset,
					FULL_SLOT)) != LS_OK)
				goto _EXIT;
			n_cards_in++;
		}
		// Emptying a full slot
		else if (set_value == EMPTY_SLOT && slot_status_1 == FULL_SLOT)
		{
			if ((ret_val = write_eeram_bit(EERAM_CRSL, slot_with_offset,
					EMPTY_SLOT)) != LS_OK)
				goto _EXIT;
			n_cards_in--;
		}
	}
	// Double deck operation
	else
	{
		// Load into a slot either empty or with only one card
		// Update first logical slot if empty
		if (set_value == FULL_SLOT && (slot_status_1 == EMPTY_SLOT))
		{
			if ((ret_val = write_eeram_bit(EERAM_CRSL, slot_with_offset,
					FULL_SLOT)) != LS_OK)
				goto _EXIT;
			n_cards_in++;
		}
		// Update second logical slot if empty and first is full
		else if (set_value == FULL_SLOT && (slot_status_2 == EMPTY_SLOT))
		{
			if ((ret_val = write_eeram_bit(EERAM_CRSL_2, slot_with_offset,
					FULL_SLOT)) != LS_OK)
				goto _EXIT;
			n_cards_in++;
		}
		// Empty: set both logical slots to zero and update n_cards_in accordingly
		else
		{
			if (set_value == EMPTY_SLOT && slot_status_1 == FULL_SLOT)
			{
				if ((ret_val = write_eeram_bit(EERAM_CRSL, slot_with_offset,
						EMPTY_SLOT)) != LS_OK)
					goto _EXIT;
				n_cards_in--;
			}
			if (set_value == EMPTY_SLOT && slot_status_2 == FULL_SLOT)
			{
				if ((ret_val = write_eeram_bit(EERAM_CRSL_2, slot_with_offset,
						EMPTY_SLOT)) != LS_OK)
					goto _EXIT;
				n_cards_in--;
			}
		}
	}
	_EXIT:

	return ret_val;
}

// Read slot with no offset, a number above N_SLOTS reads the 2nd logical slot
return_code_t read_slot(int8_t slot_number, slot_status_t *p_slot_status)
{
	return_code_t ret_val;
	uint8_t read_bit = 0;
	uint16_t eeram_address;
	uint16_t slot_index;

	if (slot_number < N_SLOTS)
	{
		eeram_address = EERAM_CRSL;
		slot_index = (uint16_t) slot_number;
	}
	else
	{
		eeram_address = EERAM_CRSL_2;
		slot_index = (uint16_t) slot_number - N_SLOTS;
	}

	if ((ret_val = read_eeram_bit(eeram_address, slot_index, &read_bit))
			== LS_OK)
		*p_slot_status = (slot_status_t) read_bit;

	return ret_val;
}

// Only for single deck
return_code_t read_slots_w_offset(int8_t target_list[],
		slot_status_t slot_status)
{
	return_code_t ret_val;
	uint16_t target_index = 0;
	slot_status_t reading;
	carousel_t carousel;

// Get carousel contents
	if ((ret_val = read_eeram(EERAM_CRSL, carousel.bytes, E_CRSL_SIZE))
			!= LS_OK)
		goto _EXIT;

// Populate list of target slots
	for (uint16_t i = 0; i < N_SLOTS; i++)
	{
		reading = (slot_status_t) (carousel.drum >> i) & 1;
		// If selected status, put current slot address (with offset for empty compartments) into target list
		if (reading == slot_status)
			target_list[target_index++] = (i + N_SLOTS
					- (slot_status == EMPTY_SLOT ? IN_OFFSET : 0)) % N_SLOTS;
	}

// Flag remaining positions as invalid
	for (uint16_t i = target_index; i < N_SLOTS; i++)
		target_list[i] = -1;

	_EXIT:

	return ret_val;
}

return_code_t read_random_slots_w_offset(int8_t target_list[],
		slot_status_t slot_status)
{
	const uint16_t n_items = (
			slot_status == FULL_SLOT ? n_cards_in : N_SLOTS - n_cards_in);
	return_code_t ret_val;
	uint16_t list_length;
	int8_t interim_list[N_SLOTS];
	uint8_t rdm;

// Get list of slots
	if ((ret_val = read_slots_w_offset(interim_list, slot_status)) != LS_OK)
		goto _EXIT;

// Align list_length to the length of the list (excluding invalid values at the end)
	list_length = n_items;
// Populate target list from the end (useful for bounded random)
	while (list_length > 0)
	{
		// Determine random target (in interim list range)
		if (bounded_random(&rdm, list_length) != HAL_OK)
		{
			ret_val = TRNG_ERROR;
			goto _EXIT;
		}
		// Check for invalid slots (specifically -1 could be present if bug)
		if (interim_list[rdm] < 0 || interim_list[rdm] >= N_SLOTS)
		{
			ret_val = INVALID_SLOT;
			goto _EXIT;
		}
		// Put at end of target list the value selected at random from interim list
		target_list[--list_length] = interim_list[rdm];

		// Remove item # rdm from  interim_list (after this, length of interim_list == list_length again)
		for (uint16_t i = rdm; i < list_length; i++)
			interim_list[i] = interim_list[i + 1];
		// Erase first value after last in interim_list for clarity
		interim_list[list_length] = -1;
	}

	_EXIT:

	return ret_val;
}

// Orders first n items of a position list from a reference position (in place)
return_code_t order_positions(int8_t pos_list[], uint16_t n_items,
		int8_t ref_pos, sort_order_t sort_order)
{
	return_code_t ret_val = LS_OK;

// Check ref_pos is valid
	if (ref_pos < 0 || ref_pos >= N_SLOTS)
	{
		ret_val = INVALID_SLOT;
		goto _EXIT;
	}

// Index first n items of list relative to reference position
	for (uint16_t i = 0; i < n_items; i++)
		pos_list[i] = (pos_list[i] + N_SLOTS - ref_pos) % N_SLOTS;

// Sort them in ascending/descending order
	insertion_sort(pos_list, n_items, sort_order);

// Restore absolute index
	for (uint16_t i = 0; i < n_items; i++)
		pos_list[i] = (pos_list[i] + ref_pos) % N_SLOTS;

	_EXIT:

	return ret_val;
}

return_code_t update_card_tally(uint32_t n_cards)
{
	return_code_t ret_val;
	union
	{
		uint32_t value;
		uint8_t bytes[E_CARDS_TALLY_SIZE];
	} tally;

	if ((ret_val = read_eeram(EERAM_CARDS_TALLY, tally.bytes,
	E_CARDS_TALLY_SIZE)) != LS_OK)
		goto _EXIT;

	tally.value += n_cards;

	if ((ret_val = write_eeram(EERAM_CARDS_TALLY, tally.bytes,
	E_CARDS_TALLY_SIZE)) != LS_OK)
		goto _EXIT;

	_EXIT:

	return ret_val;
}

return_code_t reset_card_tally(void)
{
	return_code_t ret_val;
	uint8_t bytes[E_CARDS_TALLY_SIZE];

	memset(bytes, 0, E_CARDS_TALLY_SIZE);
	ret_val = write_eeram(EERAM_CARDS_TALLY, bytes, E_CARDS_TALLY_SIZE);

	return ret_val;
}

return_code_t check_eeram(void)
{
	return_code_t ret_val = LS_OK;
	union
	{
		uint16_t value;
		uint8_t bytes[2];
	} eeram_status;

// Get EERAM status
	if ((ret_val = read_eeram(EERAM_STATUS, eeram_status.bytes,
	E_STATUS_SIZE)) != LS_OK)
		goto _EXIT;

// Act according to value
	switch (eeram_status.value)
	{
		// If already at V1.0 do nothing
		case FW_LS_V1_00:
			break;

		default:
			// Full reset including card tally
			// Flag if test was not performed (set test counter to 0xFEEB)
			HAL_Delay(L_WAIT_DELAY);
			clear_message(TEXT_ERROR);
			prompt_message("Full reset in progress");

			// Reset card tally and flap defaults
			if ((ret_val = reset_card_tally()) != LS_OK || (ret_val =
					reset_flap_defaults()) != LS_OK)
				goto _EXIT;

			// Force machine state reset
			machine_state.reset = DO_RESET;
			if ((ret_val = write_machine_state()) != LS_OK)
				goto _EXIT;

			// If factory test was not performed flag
			if (eeram_status.value != FW_LS_TESTED)
			{
				clear_message(TEXT_ERROR);
				prompt_message("Resetting test counter");
				uint8_t w_data[] = {0xFE, 0xEB};
				if ((ret_val = write_eeram(EERAM_TEST_COUNTER, w_data,
				E_TEST_COUNTER_SIZE)) != LS_OK)
					goto _EXIT;
			}

			HAL_Delay(L_WAIT_DELAY);
			clear_message(TEXT_ERROR);

//			// Set flash offset to zero
//			if ((ret_val = write_eeram(EERAM_FLASH_OFFSET, w_data,
//			E_FLASH_OFFSET_SIZE)) != LS_OK)
//				goto _EXIT;

			// Update EERAM status
			eeram_status.value = FW_LS_V1_00;
			if ((ret_val = write_eeram(EERAM_STATUS, eeram_status.bytes,
			E_STATUS_SIZE)) != LS_OK)
				goto _EXIT;
	}

	_EXIT:

	return ret_val;
}

void reset_encoder(void)
{
	extern int16_t prev_encoder_pos;
	prev_encoder_pos = 0;
	ENCODER_REFERENCE = 0;
}

// Delay in microseconds
void delay_micro_s(uint16_t us)
{
	MICRO_SECONDS = 0;  			// set the counter value a 0
	while (MICRO_SECONDS < us)
		; // wait for the counter to reach the us input in the parameter
	return;
}

// Read sensor by sensor code
GPIO_PinState read_sensor(sensor_code_t _S)
{
	switch (_S)
	{
		case TRAY_SENSOR:
			return HAL_GPIO_ReadPin(TRAY_SENSOR_PIN);
			break;
		case ENTRY_SENSOR_1:
			return HAL_GPIO_ReadPin(ENTRY_SENSOR_1_PIN);
			break;
		case ENTRY_SENSOR_2:
			return HAL_GPIO_ReadPin(ENTRY_SENSOR_2_PIN);
			break;
		case ENTRY_SENSOR_3:
			return HAL_GPIO_ReadPin(ENTRY_SENSOR_3_PIN);
			break;
		case EXIT_SENSOR:
			return HAL_GPIO_ReadPin(EXIT_SENSOR_PIN);
			break;
		case SHOE_SENSOR:
			return HAL_GPIO_ReadPin(SHOE_SENSOR_PIN);
			break;
		case HOMING_SENSOR:
			return HAL_GPIO_ReadPin(HOMING_SENSOR_PIN);
			break;
		case SLOT_SENSOR:
			return HAL_GPIO_ReadPin(SLOT_SENSOR_PIN);
			break;
		default:
			LS_error_handler(LS_ERROR);
			return GPIO_PIN_RESET; // Never reached
	}

}

void LS_error_handler(return_code_t ret_val)
{
	carousel_disable();
	// Disable motors (only if post-initialisation)
	extern dc_motor_t entry_motor;
	if (entry_motor.channel != 0)
		load_disable();
	extern icon_set_t icon_set_void;
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	prompt_interface(ALERT, ret_val, "", icon_set_void, ICON_VOID, NO_HOLD);
	Error_Handler();
}

error_type_t error_type(return_code_t error_code)
{

	error_type_t error_type_val = ERROR_NOT_FOUND;

	for (uint16_t i = 0; i < n_non_errors; i++)
	{
		if (error_code == non_errors[i])
		{
			error_type_val = NON_ERROR;
			break;
		}
	}

	if (error_type_val == ERROR_NOT_FOUND)
	{
		for (uint16_t i = 0; i < n_text_errors; i++)
		{
			if (error_code == text_errors[i])
			{
				error_type_val = TEXT_ERROR;
				break;
			}
		}
	}

	if (error_type_val == ERROR_NOT_FOUND)
	{
		for (uint16_t i = 0; i < n_graphic_errors; i++)
		{
			if (error_code == graphic_errors[i])
			{
				error_type_val = GRAPHIC_ERROR;
				break;
			}
		}
	}
	if (error_type_val == ERROR_NOT_FOUND)
	{
		for (uint16_t i = 0; i < n_fatal_errors; i++)
		{
			if (error_code == fatal_errors[i])
			{
				error_type_val = FATAL_ERROR;
				break;
			}
		}
	}

	return error_type_val;
}

char* error_message(return_code_t error_code)
{
	error_type_t error_type_val = error_type(error_code);

	// Should not happen
	if (error_type_val == NON_ERROR)
		return "Operation OK";

	if (error_type_val == TEXT_ERROR)
		switch (error_code)
		{
			case NO_FAVORITES_SELECTED:
				return "No games selected.\nUse Settings > Set Favorites";

			case NO_DEALERS_CHOICE_SELECTED:
				return "No games selected.\nUse Settings > Set Dealer's Choice";

			default:
				return "TE Error code unknown";
		}

	if (error_type_val == GRAPHIC_ERROR)
		switch (error_code)
		{
			case CARD_STUCK_IN_TRAY:
				return "Rearrange tray.";

			case CARD_STUCK_ON_ENTRY:
				return "Open front cover and check tray.";

			case CARD_STUCK_ON_EXIT:
				return "Card stuck in exit tray.";

			case SFCIT:
				return "Shuffler full. Check tray.";

			case SENCIT:
				return "Shuffler empty.";

			case NOT_ENOUGH_CARDS_IN_SHUFFLER:
				return "Not enough cards in Shuffler";

			case TRAY_IS_EMPTY:
				return "Load cards.";

			case CHECK_TRAY:
				return "Cards in tray.";

			case WRONG_INSERTION:
				return "Check card orientation.";

			case PICK_UP_CARDS:
				return "Pick up cards.";

			default:
				return "GE Error code unknown";
		}

	if (error_type_val == FATAL_ERROR)
	{

//		// DEBUGGING explicit error messages for fatal errors
//		extern char display_buf[];
//		snprintf(display_buf, N_DISP_MAX, "Error #%d", error_code);
//		return display_buf;

		if (error_code == HOMING_ERROR || error_code == MOVE_CRSL_ERROR
				|| error_code == ALIGNMENT || error_code == ALIGNMENT_ON_ONE
				|| error_code == MISSED_ALIGNMENT
				|| error_code == INITIAL_ALIGNMENT)
			return "Open carousel cover.\nCheck for obstruction and restart.";
		else
			return "Restart your machine";
	}

	return "Error code unknown";

}
