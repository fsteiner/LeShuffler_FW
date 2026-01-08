/*
 * screen.c
 *
 *  Created on: Jul 1, 2024
 *      Author: Francois S
 */

#include <basic_operations.h>
#include <bootload.h>
#include "iwdg.h"
#include <buttons.h>
#include <fonts.h>
#include <games.h>
#include <i2c.h>
#include <ili9488.h>
#include <interface.h>
#include "PSRAM.h"
#include <servo_motor.h>
#include <stdlib.h>
#include <utilities.h>
#include <version.h>
//#include "condor_black_regular_24.h"
//#include "condor_black_italic_30.h"

extern button encoder_btn;
extern button escape_btn;
extern uint8_t n_cards_in;

uint8_t LCD_row;
uint8_t LCD_scroll;
int16_t display_row; // To be removed eventually (games_old and test)
char display_buf[N_DISP_MAX + 1];

// CUSTOM GAMES NAMES in memory
char custom_game_names[N_CUSTOM_GAMES][GAME_NAME_MAX_CHAR + 1];

// ICONS
const icon_t icon_void =
{ .code = ICON_VOID, .address = (uint8_t*) VOID_ADDRESS };
const icon_t icon_back =
{ .code = ICON_BACK, .address = (uint8_t*) BACK_ADDRESS };
const icon_t icon_card =
{ .code = ICON_CARD, .address = (uint8_t*) CARD_ADDRESS };
const icon_t icon_check =
{ .code = ICON_CHECK, .address = (uint8_t*) CHECK_ADDRESS };
const icon_t icon_cross =
{ .code = ICON_CROSS, .address = (uint8_t*) CROSS_ADDRESS };
const icon_t icon_edit =
{ .code = ICON_EDIT, .address = (uint8_t*) EDIT_ADDRESS };
const icon_t icon_flame =
{ .code = ICON_FLAME, .address = (uint8_t*) FLAME_ADDRESS };
const icon_t icon_player =
{ .code = ICON_PLAYER, .address = (uint8_t*) PLAYER_ADDRESS };
const icon_t icon_red_card =
{ .code = ICON_RED_CARD, .address = (uint8_t*) RED_CARD_ADDRESS };
const icon_t icon_red_cross =
{ .code = ICON_RED_CROSS, .address = (uint8_t*) RED_CROSS_ADDRESS };
const icon_t icon_save =
{ .code = ICON_SAVE, .address = (uint8_t*) SAVE_ADDRESS };

icon_t icon_list[] =
{ icon_void, icon_back, icon_card, icon_check, icon_cross, icon_edit,
		icon_flame, icon_player, icon_red_card, icon_red_cross, icon_save };

const uint16_t n_icons = sizeof(icon_list) / sizeof(icon_t);

icon_code_t icon_list_A[] =
{ ICON_CARD, ICON_FLAME, ICON_RED_CARD };
icon_set_t icon_set_A =
{ .icon_codes = icon_list_A, .n_items = sizeof(icon_list_A)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_B[] =
{ ICON_FLAME, ICON_CARD, ICON_RED_CARD };
icon_set_t icon_set_B =
{ .icon_codes = icon_list_B, .n_items = sizeof(icon_list_B)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_C[] =
{ ICON_CARD, ICON_FLAME };
icon_set_t icon_set_C =
{ .icon_codes = icon_list_C, .n_items = sizeof(icon_list_C)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_D[] =
{ ICON_FLAME, ICON_CARD };
icon_set_t icon_set_D =
{ .icon_codes = icon_list_D, .n_items = sizeof(icon_list_D)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_E[] =
{ ICON_CARD, ICON_RED_CARD };
icon_set_t icon_set_E =
{ .icon_codes = icon_list_E, .n_items = sizeof(icon_list_E)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_card[] =
{ ICON_CARD };
icon_set_t icon_set_card =
{ .icon_codes = icon_list_card, .n_items = sizeof(icon_list_card)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_check[] =
{ ICON_CHECK };
icon_set_t icon_set_check =
{ .icon_codes = icon_list_check, .n_items = sizeof(icon_list_check)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_save[] =
{ ICON_SAVE };
icon_set_t icon_set_save =
{ .icon_codes = icon_list_save, .n_items = sizeof(icon_list_save)
		/ sizeof(icon_code_t) };

icon_code_t icon_list_void[] =
{ ICON_VOID };
icon_set_t icon_set_void =
{ .icon_codes = icon_list_void, .n_items = sizeof(icon_list_void)
		/ sizeof(icon_code_t) };

// SUBMENUS (arrays of menuCode)
// _menu _items
item_code_t root_items[] =
{ POKER, CASINO, CLASSICS, SHUFFLE, FAVORITES, CUSTOM_GAMES, DEAL_N_CARDS,
		EMPTY, LOAD, DOUBLE_DECK_SHUFFLE, SETTINGS };

item_code_t poker_items[] =
{ _5_CARD_DRAW, _5_CARD_STUD, _6_CARD_STUD, _7_CARD_DRAW, _7_CARD_STUD,
		CHINESE_POKER, DBL_BRD_HOLDEM, IRISH_POKER, OMAHA, PINEAPPLE,
		TEXAS_HOLDEM, DEALERS_CHOICE, SHUFFLERS_CHOICE };

item_code_t casino_items[] =
{ _3_CD_POKER, _4_CD_POKER, BACCARAT, BLACKJACK, LETEM_RIDE, MAXIMUM_FLUSH,
		PAIGOW, SOUTHERN_STUD };

item_code_t classic_items[] =
{ BRIDGE, CRIBBAGE, GIN_RUMMY, HEARTS, LANDLORD, MATATU, RUMMY, SPADES,
		TIEN_LEN, CUSTOM_GAMES };

item_code_t setting_items[] =
{ SET_FAVORITES, SET_DEALERS_CHOICE, SET_PREFS, ADJUST_DEAL_GAP,
		SET_CUT_CARD_FLAG, SET_CUSTOM_GAMES, RESET_CONTENT, RESET_PREFS,
		RESET_CUSTOM_GAMES, MAINTENANCE, ABOUT };

item_code_t maintenance_items[] =
{ ROTATE_TRAY_ROLLER, ROTATE_ENTRY_ROLLER, ACCESS_EXIT_CHUTE, ADJUST_CARD_FLAP_LIMITED,
		TEST_MENU, FIRMWARE_UPDATE };

item_code_t maintenance_level_2_items[] =
{ ABOUT, IMAGE_UTILITY, TEST_IMAGES, TEST_CAROUSEL, ADJUST_CARD_FLAP, ACCESS_EXIT_CHUTE, DC_MOTORS_RUN_IN, SHUFFLE,
		EMPTY, LOAD, TEST_EXIT_LATCH, TEST_BUZZER, DISPLAY_SENSORS, TEST_WATCHDOG };

item_code_t test_items[] =
{ TEST_CAROUSEL, TEST_EXIT_LATCH, TEST_BUZZER, TEST_IMAGES, DISPLAY_SENSORS };

// Internal lists - to classify menus in categories
// List for dynamic favourites menu and to determine proper games (run by run_game)
// Custom items must be kept at the end
item_code_t games_items[] =
{ _5_CARD_DRAW, _5_CARD_STUD, _6_CARD_STUD, _7_CARD_DRAW, _7_CARD_STUD,
		CHINESE_POKER, DBL_BRD_HOLDEM, IRISH_POKER, OMAHA, PINEAPPLE,
		TEXAS_HOLDEM, _3_CD_POKER, _4_CD_POKER, BACCARAT, BLACKJACK, LETEM_RIDE,
		MAXIMUM_FLUSH, PAIGOW, SOUTHERN_STUD, BRIDGE, CRIBBAGE, GIN_RUMMY,
		HEARTS, LANDLORD, MATATU, RUMMY, SPADES, TIEN_LEN, CUSTOM_1, CUSTOM_2,
		CUSTOM_3, CUSTOM_4, CUSTOM_5 };

const uint8_t n_games = sizeof(games_items) / sizeof(item_code_t);

menu_t games_list =
{ GAMES_LIST, "INTERNAL LIST", n_games, games_items };

//List for dynamic dealer's choice menu (more than strict poker menu exc. dealer/shuffler's choice)
item_code_t dealers_choice_items[] =
{ _5_CARD_DRAW, _5_CARD_STUD, _6_CARD_STUD, _7_CARD_DRAW, _7_CARD_STUD,
		CHINESE_POKER, DBL_BRD_HOLDEM, IRISH_POKER, OMAHA, PINEAPPLE,
		TEXAS_HOLDEM };

const uint8_t n_dealers_choice = sizeof(dealers_choice_items)
		/ sizeof(item_code_t);

menu_t dealers_choice_list =
{ POKER_LIST, "INTERNAL LIST", n_dealers_choice, dealers_choice_items };

// Custom games
item_code_t custom_items[] =
{ CUSTOM_1, CUSTOM_2, CUSTOM_3, CUSTOM_4, CUSTOM_5 };

// MENUS (are retrieved by their code, used in setCurrent_menu)

// Menus with sub-menus
menu_t root_menu =
		{ ROOT_MENU, "LeShuffler", sizeof(root_items) / sizeof(item_code_t),
				root_items };
menu_t poker_menu =
{ POKER, "Poker", sizeof(poker_items) / sizeof(item_code_t), poker_items };
menu_t casino_menu =
{ CASINO, "Casino", sizeof(casino_items) / sizeof(item_code_t), casino_items };
menu_t classics_menu =
{ CLASSICS, "Classics", sizeof(classic_items) / sizeof(item_code_t),
		classic_items };
menu_t settings_menu =
{ SETTINGS, "Settings", sizeof(setting_items) / sizeof(item_code_t),
		setting_items };
menu_t custom_menu =
{ CUSTOM_GAMES, "Custom Games", sizeof(custom_items) / sizeof(item_code_t),
		custom_items };
menu_t setFavorites_menu =
{ SET_FAVORITES, "Set Favorites", n_games, games_items };
menu_t setDLRC_menu =
{ SET_DEALERS_CHOICE, "Set Dealer's Choice", n_dealers_choice,
		dealers_choice_items };

menu_t set_prefs_menu =
{ SET_PREFS, "Set Preferences", n_games, games_items };

menu_t maintenance_menu =
{ MAINTENANCE, "Maintenance", sizeof(maintenance_items) / sizeof(item_code_t),
		maintenance_items };
menu_t maintenance_level_2_menu =
{ MAINTENANCE_LEVEL_2, "Technical Support Only",
		sizeof(maintenance_level_2_items) / sizeof(item_code_t),
		maintenance_level_2_items };
menu_t test_menu =
{ TEST_MENU, "Tests", sizeof(test_items) / sizeof(item_code_t), test_items };

// Dynamic menus
menu_t favorites_menu =
{ FAVORITES, "Favorites", 0, NULL };
menu_t dealers_choice_menu =
{ DEALERS_CHOICE, "Dealer's Choice", 0, NULL };
menu_t set_custom_rules_menu =
{ SET_CUSTOM_GAMES, "Set Custom Games", sizeof(custom_items)
		/ sizeof(item_code_t), custom_items }; // ADD CONTEXT - ALSO ACTIVATED ?

// Action menus
menu_t shuffle_menu =
{ SHUFFLE, "Shuffle", 0, NULL };
menu_t deal_n_cards_menu =
{ DEAL_N_CARDS, "Deal N Cards", 0, NULL };
menu_t empty_menu =
{ EMPTY, "Empty", 0, NULL };
menu_t double_deck_menu =
{ DOUBLE_DECK_SHUFFLE, "2-Deck Shuffle", 0, NULL };
menu_t load_menu =
{ LOAD, "Load", 0, NULL };
menu_t shufflers_choice_menu =
{ SHUFFLERS_CHOICE, "Shuffler's Choice", 0, NULL };

// Game menus
menu_t _5draw_menu =
{ _5_CARD_DRAW, "5 Card Draw", 0, NULL };
menu_t _5stud_menu =
{ _5_CARD_STUD, "5 Card Stud", 0, NULL };
menu_t _6stud_menu =
{ _6_CARD_STUD, "6 Card Stud", 0, NULL };
menu_t _7draw_menu =
{ _7_CARD_DRAW, "7 Card Draw", 0, NULL };
menu_t _7stud_menu =
{ _7_CARD_STUD, "7 Card Stud", 0, NULL };
menu_t chinese_menu =
{ CHINESE_POKER, "Chinese Poker", 0, NULL };
menu_t dbl_brd_hldm_menu =
{ DBL_BRD_HOLDEM, "2 Board HoldEm", 0, NULL };
menu_t irish_menu =
{ IRISH_POKER, "Irish Poker", 0, NULL };
menu_t omaha_menu =
{ OMAHA, "Omaha", 0, NULL };
menu_t pineapple_menu =
{ PINEAPPLE, "Pineapple", 0, NULL };
menu_t holdem_menu =
{ TEXAS_HOLDEM, "Texas Hold'em", 0, NULL };
menu_t _3card_menu =
{ _3_CD_POKER, "3 Card Poker", 0, NULL };
menu_t _4card_menu =
{ _4_CD_POKER, "4 Card Poker", 0, NULL };
menu_t baccarat_menu =
{ BACCARAT, "Baccarat", 0, NULL };
menu_t blackjack_menu =
{ BLACKJACK, "Blackjack", 0, NULL };
menu_t letemride_menu =
{ LETEM_RIDE, "Let'Em Ride", 0, NULL };
menu_t max_flush_menu =
{ MAXIMUM_FLUSH, "Maximum Flush", 0, NULL };
menu_t paigow_menu =
{ PAIGOW, "Pai Gow", 0, NULL };
menu_t southern_menu =
{ SOUTHERN_STUD, "Southern Stud", 0, NULL };
menu_t bridge_menu =
{ BRIDGE, "Bridge", 0, NULL };
menu_t cribbage_menu =
{ CRIBBAGE, "Cribbage", 0, NULL };
menu_t hearts_menu =
{ HEARTS, "Hearts", 0, NULL };
menu_t spades_menu =
{ SPADES, "Spades", 0, NULL };
menu_t landlord_menu =
{ LANDLORD, "Landlord", 0, NULL };
menu_t gin_rummy_menu =
{ GIN_RUMMY, "Gin Rummy", 0, NULL };
menu_t rummy_menu =
{ RUMMY, "Rummy", 0, NULL };
menu_t tien_len_menu =
{ TIEN_LEN, "Tien Len", 0, NULL };
menu_t matatu_menu =
{ MATATU, "Matatu", 0, NULL };
menu_t custom_1_menu =
{ CUSTOM_1, custom_game_names[0], 0, NULL };
menu_t custom_2_menu =
{ CUSTOM_2, custom_game_names[1], 0, NULL };
menu_t custom_3_menu =
{ CUSTOM_3, custom_game_names[2], 0, NULL };
menu_t custom_4_menu =
{ CUSTOM_4, custom_game_names[3], 0, NULL };
menu_t custom_5_menu =
{ CUSTOM_5, custom_game_names[4], 0, NULL };

// Maintenance and test
menu_t image_utility_menu =
{ IMAGE_UTILITY, "Image Utility", 0, NULL };
menu_t firmware_update_menu =
{ FIRMWARE_UPDATE, "Firmware Update", 0, NULL };
menu_t about_menu =
{ ABOUT, "About", 0, NULL };
menu_t reset_user_prefs_menu =
{ RESET_PREFS, "Reset Preferences", 0, NULL };
menu_t adjust_deal_gap_menu =
{ ADJUST_DEAL_GAP, "Adjust Deal Pace", 0, NULL };
menu_t set_cut_card_flag_menu =
{ SET_CUT_CARD_FLAG, "Use Cut Card", 0, NULL };
menu_t reset_content_menu =
{ RESET_CONTENT, "Reset Shuffler Content", 0, NULL };
menu_t reset_custom_games_menu =
{ RESET_CUSTOM_GAMES, "Reset Custom Games", 0, NULL };
menu_t clean_tray_roller_menu =
{ ROTATE_TRAY_ROLLER, "Rotate Tray Roller", 0, NULL };
menu_t clean_entry_roller_menu =
{ ROTATE_ENTRY_ROLLER, "Rotate Entry Roller", 0, NULL };
menu_t test_carousel_menu =
{ TEST_CAROUSEL, "Test Carousel Motor", 0, NULL };
menu_t test_carousel_driver_menu =
{ TEST_CAROUSEL_DRIVER, "Test Carousel Driver", 0, NULL };
menu_t test_exist_latch_menu =
{ TEST_EXIT_LATCH, "Test Card Exit Latch", 0, NULL };
menu_t access_exit_chute_menu =
{ ACCESS_EXIT_CHUTE, "Access Exit Chute", 0, NULL };
menu_t dc_motors_run_in_menu =
{ DC_MOTORS_RUN_IN, "DC Motors Run In", 0, NULL };
menu_t adjust_card_flap_menu =
{ ADJUST_CARD_FLAP, "Adjust Card Flap", 0, NULL };
menu_t adjust_card_flap_limited_menu =
{ ADJUST_CARD_FLAP_LIMITED, "Adjust Card Flap", 0, NULL };
menu_t test_buzzer_menu =
{ TEST_BUZZER, "Test Buzzer", 0, NULL };
menu_t display_sensors_menu =
{ DISPLAY_SENSORS, "Monitor Sensors", 0, NULL };
menu_t test_images_menu =
{ TEST_IMAGES, "Test Images", 0, NULL };
menu_t test_watchdog_menu =
{ TEST_WATCHDOG, "Test Watchdog", 0, NULL };

// menu_list MUST CONTAIN THE ADDRESSES OF ALL MENUS ABOVE
menu_t *menu_list[] =
{ &root_menu,

&poker_menu, &casino_menu, &classics_menu, &shuffle_menu, &favorites_menu,
		&custom_menu, &deal_n_cards_menu, &empty_menu, &double_deck_menu,
		&load_menu, &settings_menu,

		&_5draw_menu, &_5stud_menu, &_6stud_menu, &_7draw_menu, &_7stud_menu,
		&chinese_menu, &dbl_brd_hldm_menu, &irish_menu, &omaha_menu,
		&pineapple_menu, &holdem_menu, &shufflers_choice_menu,
		&dealers_choice_menu,

		&_3card_menu, &_4card_menu, &baccarat_menu, &blackjack_menu,
		&letemride_menu, &max_flush_menu, &paigow_menu, &southern_menu,

		&bridge_menu, &cribbage_menu, &hearts_menu, &spades_menu,
		&landlord_menu, &gin_rummy_menu, &rummy_menu, &tien_len_menu,
		&matatu_menu,

		&custom_1_menu, &custom_2_menu, &custom_3_menu, &custom_4_menu,
		&custom_5_menu,

		&setFavorites_menu, &setDLRC_menu, &reset_content_menu,
		&reset_user_prefs_menu, &set_prefs_menu, &adjust_deal_gap_menu,
		&set_cut_card_flag_menu, &set_custom_rules_menu,
		&reset_custom_games_menu, &about_menu,

		&maintenance_menu, &maintenance_level_2_menu, &image_utility_menu,
		&firmware_update_menu, &test_menu,

		&clean_tray_roller_menu, &clean_entry_roller_menu, &test_carousel_menu,
		&test_carousel_driver_menu, &test_exist_latch_menu,
		&access_exit_chute_menu, &dc_motors_run_in_menu, &adjust_card_flap_menu,
		&adjust_card_flap_limited_menu, &test_buzzer_menu,
		&display_sensors_menu, &test_images_menu, &test_watchdog_menu,

		&games_list, &dealers_choice_list };

const uint16_t n_menus = sizeof(menu_list) / sizeof(menu_t*);
void atomic_clear_message(error_type_t);

// Set dynamic menu
// arg 1 is the address of the menu to be modified
// arg 2 is the address of the pointer set up to store the items of the relevant dynamic menu
return_code_t set_dynamic_menu(menu_t *p_dynamic_menu)
{
	return_code_t ret_val = LS_OK;
	menu_t reference_list;
	uint16_t address;
	uint16_t e_size;
	uint16_t n_items = 0;
	uint8_t ZERO = 0;

	switch (p_dynamic_menu->code)
	{
		case FAVORITES:
			reference_list = games_list;
			address = EERAM_FAVS;
			e_size = E_FAVS_SIZE;
			break;

		case DEALERS_CHOICE:
			reference_list = dealers_choice_list;
			address = EERAM_DLRS;
			e_size = E_DLRS_SIZE;
			break;

		default:
			ret_val = INVALID_CHOICE;
	}

	uint8_t r_data[e_size];

	if (ret_val == LS_ERROR)
		goto _EXIT;

	// Get selected items in permanent memory
	if ((ret_val = EERAM_ReadSRAMData(&hi2c3, address, r_data, e_size))
			!= LS_OK)
		goto _EXIT;

	// Calculate number of active items in dynamic menu
	for (int i = 0; i < e_size; i++)
	{
		uint8_t X = r_data[i];
		while (X)
		{
			n_items += (X & 1);
			X >>= 1;
		}
	}
	// Update dynamic menu n_items
	p_dynamic_menu->n_items = n_items;

	// Allocate memory for the dynamic items
	free(p_dynamic_menu->items);
	p_dynamic_menu->items = (item_code_t*) malloc(
			n_items * sizeof(item_code_t*));

	// Fill items in dynamic menu
	uint8_t index = 0;
	for (uint16_t i = 0; i < reference_list.n_items; i++)
	{
		uint16_t A = i / BYTE;
		uint16_t B = i % BYTE;
		if (r_data[A] & (1 << B))
			*(p_dynamic_menu->items + index++) = reference_list.items[i];
	}

	// Reset default scroll and row of dynamic menu
	if ((ret_val = write_eeram(EERAM_LCD_ROW + p_dynamic_menu->code, &ZERO, 1))
			!= LS_OK)
		goto _EXIT;
	if ((ret_val = write_eeram(EERAM_LCD_SCROLL + p_dynamic_menu->code, &ZERO,
			1)) != LS_OK)
		goto _EXIT;

	_EXIT: return ret_val;
}

return_code_t interface_init(void)
{
	return_code_t ret_val = LS_OK;
	extern menu_t current_menu;
	char text[GAME_NAME_MAX_CHAR + 1];

	// Load custom game names
	for (item_code_t custom_game_code = CUSTOM_1;
			custom_game_code < CUSTOM_1 + N_CUSTOM_GAMES; custom_game_code++)
	{
		if ((ret_val = read_custom_game_name(text, custom_game_code)) != LS_OK)
			break;
		strncpy(custom_game_names[custom_game_code - CUSTOM_1], text,
		GAME_NAME_MAX_CHAR);
	}

	// Initialise Favorites and Dealers Choice menu
	set_dynamic_menu(&favorites_menu);
	set_dynamic_menu(&dealers_choice_menu);

	// Set current_menu.code to non-menu value (for prompt_menu)
	current_menu.code = -1;

	return ret_val;
}

void prompt_hole_cards(uint16_t player, uint16_t n_players, uint16_t card,
		uint16_t n_cards, hole_change_t hole_change, bool first_deal)
{
	const uint16_t x_pos = 85;
	const uint16_t y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;
	const uint16_t x_space = 28;
	const uint16_t x_space_2 = 200;
	const uint16_t y_icon = 10;
	uint16_t color;

	if (first_deal)
	{
		// Clear message zone
		clear_message(TEXT_ERROR);
		// Draw icons
		ili9488_DrawRGBImage8bit(x_pos, y_pos - y_icon, ICON_W, ICON_H,
				icon_player.address);
		ili9488_DrawRGBImage8bit(x_pos + x_space_2, y_pos - y_icon, ICON_W,
		ICON_H, icon_card.address);
		display_escape_icon(ICON_CROSS);
	}
	// If the player number is changing, clear player space
	if (hole_change == PLAYER_CHANGE || hole_change == BOTH_CHANGE)
	{
		BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
		BSP_LCD_FillRect(x_pos + 42, y_pos + 3, 30, 25);
		BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
		color = LCD_COLOR_RED_SHFLR;
	}
	else
		color = LCD_COLOR_TEXT;
	// Fill player
	snprintf(display_buf, N_DISP_MAX, "%2d", player);
	display_text(x_pos + 45, y_pos, display_buf, LCD_BOLD_FONT, color);

	// Print separator and fixed number
	snprintf(display_buf, N_DISP_MAX, "/");
	display_text(x_pos + x_space + 45, y_pos, display_buf, LCD_REGULAR_FONT,
	LCD_COLOR_TEXT);
	snprintf(display_buf, N_DISP_MAX, "%-2d", n_players);
	display_text(x_pos + x_space + 60, y_pos, display_buf, LCD_REGULAR_FONT,
	LCD_COLOR_TEXT);

	// If the card number is changing, clear card space
	if (hole_change == CARD_CHANGE || hole_change == BOTH_CHANGE)
	{
		BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
		BSP_LCD_FillRect(x_pos + x_space_2 + 42, y_pos + 3, 30, 25);
		BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
		color = LCD_COLOR_RED_SHFLR;
	}
	else
		color = LCD_COLOR_TEXT;

	// Fill card
	snprintf(display_buf, N_DISP_MAX, "%2d", card);
	display_text(x_pos + x_space_2 + 45, y_pos, display_buf, LCD_BOLD_FONT,
			color);

	// Print separator and fixed number
	snprintf(display_buf, N_DISP_MAX, "/");
	display_text(x_pos + x_space_2 + x_space + 45, y_pos, display_buf,
	LCD_REGULAR_FONT, LCD_COLOR_TEXT);
	snprintf(display_buf, N_DISP_MAX, "%-2d", n_cards);
	display_text(x_pos + x_space_2 + x_space + 60, y_pos, display_buf,
	LCD_REGULAR_FONT, LCD_COLOR_TEXT);

	return;
}

void prompt_com_cards(uint16_t card, uint16_t n_cards)
{
	const uint16_t x_pos = 85;
	const uint16_t y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;
	const uint16_t x_space = 28;
	const uint16_t x_space_2 = 200 / 2;
	const uint16_t y_icon = 10;
	uint16_t color;

	// Only prompt if more than 1 card
	if (n_cards == 1)
		return;

	// Clear message zone
	clear_message(TEXT_ERROR);
	// Draw icon
	ili9488_DrawRGBImage8bit(x_pos + x_space_2, y_pos - y_icon, ICON_W, ICON_H,
			icon_card.address);

	// As the card number is changing, clear card space
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillRect(x_pos + x_space_2 + 42, y_pos + 3, 30, 25);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
	color = LCD_COLOR_RED_SHFLR;

	// Fill card
	snprintf(display_buf, N_DISP_MAX, "%2d", card);
	display_text(x_pos + x_space_2 + 45, y_pos, display_buf, LCD_BOLD_FONT,
			color);

	// Print separator and fixed number
	snprintf(display_buf, N_DISP_MAX, "/");
	display_text(x_pos + x_space_2 + x_space + 45, y_pos, display_buf,
	LCD_REGULAR_FONT, LCD_COLOR_TEXT);
	snprintf(display_buf, N_DISP_MAX, "%-2d", n_cards);
	display_text(x_pos + x_space_2 + x_space + 60, y_pos, display_buf,
	LCD_REGULAR_FONT, LCD_COLOR_TEXT);

	return;
}

// Atomic prompt - atomic function for prompts
// Does not clear messages at the end, does at the beginning if mode != NO_HOLD
// No wait
void atomic_prompt(return_code_t message_code, char *text, uint8_t prompt_row,
		prompt_mode_t prompt_mode, TFTSTCustomFontData font,
		uint16_t text_color,
		bool do_beep)
{
	uint16_t x;
	uint16_t y;
	const uint16_t x_max =
			BSP_LCD_GetXSize() - LCD_X_MARGIN_MESSAGE_L - LCD_X_MARGIN_MESSAGE_R;
	int16_t last_space;
	int16_t last_space_x;
	int16_t line_end;  // can be negative
	int16_t line_end_x;
	int16_t index_1;
	int16_t index_2;
	bool space_found;
	bool end_reached = false;
	char c;

	void prompt_char(void)
	{
		// Replace illegal characters and space by blank (we don't overwrite)
		if (c < ' ' || c > '~' || c == ' ')
		{
			c = ' ';
		}
		else
		{
			// Draw character if not space
			tftstDrawCharWithFont(&font, x, y, c, text_color, LCD_COLOR_BCKGND);
		}
		// Increment char index
		index_1++;
		// Move cursor by width of c or a space
		//(font.charData has an indexing offset of 32 from ASCII's)
		x += font.charData[c - FONT_INDEX_OFFSET].width;
		x += font.charData[c - FONT_INDEX_OFFSET].left;

		return;
	}

	// Clear message box except if NO_HOLD
	if (prompt_mode != NO_HOLD)
		clear_message(error_type(message_code));

	// Fill text if standard message
	if (message_code != CUSTOM_MESSAGE && message_code != CUSTOM_PICK_UP
			&& message_code != CUSTOM_EMPTY)
		text = error_message(message_code);

	// Prompt characters routine
	// Initialise main running index
	index_1 = 0;
	// Parse message to the end
	do
	{
		// Find the last space before row end in current segment
		index_2 = index_1;
		space_found = false;
		x = LCD_X_MARGIN_MESSAGE_L;

		// Skip initial blanks
		while ((c = *(text + index_2)) < ' ' || c > '~' || c == ' ')
			index_2++;

		// Continue until either end of string, line feed, row end or max char limit
		while ((c = *(text + index_2++)) != 0 && c != '\n'
				&& index_2 < MAX_MSG_CHAR && x < x_max)
		{
			// Take note of last space
			if (c < ' ' || c > '~' || c == ' ')
			{
				c = ' ';
				space_found = true;
				last_space = index_2;
				last_space_x = x;
			}

			// Move logical cursor
			x += font.charData[c - FONT_INDEX_OFFSET].width;
			x += font.charData[c - FONT_INDEX_OFFSET].left;
		}

		// Normal end of message
		if (c == 0)
		{
			line_end = index_2 - 1;
			line_end_x = x;
			end_reached = true;
		}
		// Message truncated
		else if (index_2 == MAX_MSG_CHAR)
		{
			line_end = index_2;
			line_end_x = x;
			end_reached = true;
		}
		// Line feed
		else if (c == '\n')
		{
			line_end = index_2 - 1;
			line_end_x = x;
		}
		// Reaching end of row
		else if (x >= x_max)
		{
			// Normal case
			if (space_found)
			{
				line_end = last_space - 1;
				line_end_x = last_space_x;
			}
			// No space in text, truncate
			else
			{
				line_end = index_2;
				line_end_x = x;
			}
		}

		// Reset x and y
		x = LCD_X_MARGIN_MESSAGE_L
				+ (BSP_LCD_GetXSize() - line_end_x - LCD_X_MARGIN_MESSAGE_R)
						/ 2;
		y = LCD_TOP_ROW + LCD_ROW_HEIGHT * prompt_row + V_ADJUST;

		// Print segment, skipping blanks
		// Skip initial blanks
		while ((c = *(text + index_1)) < ' ' || c > '~' || c == ' ')
			index_1++;
		while (index_1 < line_end)
		{
			c = *(text + index_1);
			prompt_char();
		}
		index_1++;

		// Go to next row
		prompt_row++;

	}
	while (!end_reached);

	// Beep if required
	if (do_beep)
		beep(MEDIUM_BEEP);

	return;
} // atomic_prompt()

// Prompt message without interaction, has to be cleared by clear_message(TEXT_ERROR)
void prompt_message(char *text)
{
	atomic_prompt(CUSTOM_MESSAGE, text, MSG_ROW, NO_HOLD, LCD_REGULAR_FONT,
	LCD_COLOR_TEXT, false);

	return;
}

return_code_t graphic_address(uint8_t *eeram_addresses[2],
		return_code_t error_code)
{
	if (error_type(error_code) != GRAPHIC_ERROR)
		return INVALID_CHOICE;

	switch (error_code)
	{
		case CARD_STUCK_IN_TRAY:
			eeram_addresses[0] = (uint8_t*) CHECK_TRAY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case CARD_STUCK_ON_ENTRY:
			eeram_addresses[0] = (uint8_t*) CSO_ENTRY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case CARD_STUCK_ON_EXIT:
			eeram_addresses[0] = (uint8_t*) CSO_EXIT_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case SFCIT:
			eeram_addresses[0] = (uint8_t*) SH_FULL_ADDRESS;
			eeram_addresses[1] = (uint8_t*) CHECK_TRAY_ADDRESS;
			break;

		case SENCIT:
			eeram_addresses[0] = (uint8_t*) SH_EMPTY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) TRAY_EMPTY_ADDRESS;
			break;

		case NOT_ENOUGH_CARDS_IN_SHUFFLER:
			eeram_addresses[0] = (uint8_t*) CONTENT_ADDRESS;
			eeram_addresses[1] = (uint8_t*) TRAY_EMPTY_ADDRESS;
			break;

		case TRAY_IS_EMPTY:
			eeram_addresses[0] = (uint8_t*) TRAY_EMPTY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case CHECK_TRAY:
			eeram_addresses[0] = (uint8_t*) CHECK_TRAY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case WRONG_INSERTION:
			eeram_addresses[0] = (uint8_t*) CHECK_TRAY_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case PICK_UP_CARDS:
			eeram_addresses[0] = (uint8_t*) PICK_UP_CARDS_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case CUSTOM_PICK_UP:
			eeram_addresses[0] = (uint8_t*) PICK_UP_CARDS_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		case CUSTOM_EMPTY:
			eeram_addresses[0] = (uint8_t*) CONTENT_ADDRESS;
			eeram_addresses[1] = (uint8_t*) SILH_ADDRESS;
			break;

		default:
			return INVALID_CHOICE;
	}

	return LS_OK;

}

void clear_picture(void)
{
	uint16_t silh_x = (BSP_LCD_GetXSize() - SILH_W) / 2;
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
//	BSP_LCD_SetTextColor(LCD_COLOR_GREEN);  // SCREEN DEBUGGING
	BSP_LCD_FillRect(silh_x, (uint16_t) SILH_Y, (uint16_t) SILH_W,
			(uint16_t) SILH_H);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
}

/*
 * Prompt dynamic buttons
 * @retval: • return codes linked to icons: LS_OK, LS_ESC, DRAW_CARD, BURN_CARD, EXTRA_CARD
 *          • LS_OK if auto_trigger
 *          • error codes: INVALID_CHOICE if invalid icon code
 */
return_code_t prompt_dynamic_buttons(return_code_t message_code,
		icon_set_t icon_set, icon_code_t esc_icon_code,
		uint8_t *eeram_addresses[2], uint16_t silh_x, prompt_mode_t prompt_mode)
{
	return_code_t ret_val = LS_OK;
	int8_t idx;
	icon_code_t sel_icon_code;
	int8_t increment;
	bool auto_trigger = false;
	uint32_t start_time;
	uint32_t start_time_2;
	uint32_t blink_lag = 500;
	bool toggle_blink = 0;
	bool initial_escape = escape_btn.interrupt_press;
	bool initial_encoder = encoder_btn.interrupt_press;
	error_type_t error_type_val = error_type(message_code);
	reset_btns();

// Display icons
	display_encoder_icon(icon_set.icon_codes[idx = 0]);
	display_escape_icon(esc_icon_code);

// Start timer
	start_time = start_time_2 = HAL_GetTick();

// If NO_HOLD (LS_error_handler only),
// exit (button interaction will be managed down the line)
	if (prompt_mode == NO_HOLD)
		goto _EXIT;

// Cycle icon set with encoder or wait for auto trigger - blinks image if graphic error
	while (1)
	{
		watchdog_refresh();
		// Blink image if graphic error
		if (error_type_val == GRAPHIC_ERROR
				&& HAL_GetTick() > start_time_2 + blink_lag)
		{

			// Toggle
			toggle_blink = !toggle_blink;
			// Draw picture
			ili9488_DrawRGBImage8bit(silh_x, (uint16_t) SILH_Y,
					(uint16_t) SILH_W, (uint16_t) SILH_H,
					eeram_addresses[toggle_blink]);
			// Reset blink timer
			start_time_2 = HAL_GetTick();
		}

		bool change = false;
		// Cycle icon if encoder rotation
		if ((increment = read_encoder(CLK_WISE)))
		{
			if (increment > 0 && idx < icon_set.n_items - 1)
			{
				idx = min(idx + increment, icon_set.n_items - 1);
				change = true;
			}
			else if (increment < 0 && index > 0)
			{
				idx = max(idx + increment, 0);
				change = true;
			}
			// If change display icon
			if (change)
				display_encoder_icon(icon_set.icon_codes[idx]);
		}

		// If encoder a valid option and encoder press, select and exit
		if (encoder_btn.interrupt_press
				&& icon_set.icon_codes[idx] != ICON_VOID)
		{
			sel_icon_code = icon_set.icon_codes[idx];
			break;
		}

		// If escape a valid option and escape press, select and exit
		if (escape_btn.interrupt_press && esc_icon_code != ICON_VOID)
		{
			sel_icon_code = esc_icon_code;
			break;
		}

		// If on auto trigger, break if triggered
		if ((prompt_mode == TIME_LAG
				&& HAL_GetTick() > start_time + L_WAIT_DELAY)
				|| (prompt_mode == SHOE_CLEAR && !cards_in_shoe())
				|| (prompt_mode == TRAY_LOAD && cards_in_tray())
				|| (prompt_mode == SHOE_CLEAR_OR_TRAY_LOAD
						&& (!cards_in_shoe() || cards_in_tray())))
		{
			auto_trigger = true;
			break;
		}
	}

// Return value linked to icon or LS_OK if auto trigger
	if (!auto_trigger)

		switch (sel_icon_code)
		{
			case ICON_BACK:
				ret_val = LS_ESC;
				break;

			case ICON_CARD:
				ret_val = DRAW_CARD;
				break;

			case ICON_CHECK:
				ret_val = LS_OK;
				break;

			case ICON_CROSS:
				ret_val = LS_ESC;
				break;

			case ICON_EDIT:
				ret_val = LS_OK;
				break;

			case ICON_FLAME:
				ret_val = BURN_CARD;
				break;

			case ICON_RED_CARD:
				ret_val = EXTRA_CARD;
				break;

			case ICON_RED_CROSS:
				ret_val = LS_ESC;
				break;

			case ICON_SAVE:
				ret_val = LS_OK;
				break;

			default:
				ret_val = INVALID_CHOICE;
		}

	_EXIT:

// Clears icons, message and picture associated with prompt except if NO_HOLD or any pick up
	if (prompt_mode != NO_HOLD && message_code != PICK_UP_CARDS
			&& message_code != CUSTOM_PICK_UP)
	{
		clear_message(error_type_val);
		clear_icons();
		if (error_type_val == GRAPHIC_ERROR)
			clear_picture();
	}

// Reset buttons interrupt to initial state (transparency)
	escape_btn.interrupt_press = initial_escape;
	encoder_btn.interrupt_press = initial_encoder;

	return ret_val;
}

return_code_t prompt_interface(alert_mode_t alert_mode,
		return_code_t message_code, char *text, icon_set_t icon_set,
		icon_code_t esc_icon_code, prompt_mode_t prompt_mode)
{
	return_code_t user_input;
	uint16_t prompt_row;
	uint16_t color =
			(alert_mode == MESSAGE) ? LCD_COLOR_TEXT : LCD_COLOR_RED_SHFLR;
	uint16_t silh_x = (BSP_LCD_GetXSize() - SILH_W) / 2;
	bool do_beep = (alert_mode == ALERT);
	uint8_t *eeram_addresses[2];
	error_type_t error_type_val = error_type(message_code);

	if (error_type_val == GRAPHIC_ERROR)
	{
		// Set text row to bottom row
		prompt_row = BOTTOM_MSG_ROW;

		// Clear middle screen
		clear_message(TEXT_ERROR);

		// Graphic prompt
		if (graphic_address(eeram_addresses, message_code) != LS_OK)
			return INVALID_CHOICE;

		ili9488_DrawRGBImage8bit(silh_x, (uint16_t) SILH_Y, (uint16_t) SILH_W,
				(uint16_t) SILH_H, eeram_addresses[0]);
	}
	else if (error_type_val == FATAL_ERROR)
	{
		// Set text row to bottom row
		prompt_row = BOTTOM_MSG_ROW;

		// Clear middle screen
		clear_message(TEXT_ERROR);

		uint16_t restart_x = (BSP_LCD_GetXSize() - RESTART_W) / 2;

		// Graphic prompt
		ili9488_DrawRGBImage8bit(restart_x, (uint16_t) SILH_Y,
				(uint16_t) RESTART_W, (uint16_t) RESTART_H,
				(uint8_t*) RESTART_ADDRESS);
	}
	else
	{
		// Set text row to medium row
		prompt_row = MSG_ROW;
		if (prompt_mode != NO_HOLD)
			clear_text();
	}

	// Text prompt
	atomic_prompt(message_code, text, prompt_row, prompt_mode, LCD_REGULAR_FONT,
			color, do_beep);

	// prompt dynamic buttons manages the interface and the icons except NO_HOLD and pick ups
	user_input = prompt_dynamic_buttons(message_code, icon_set, esc_icon_code,
			eeram_addresses, silh_x, prompt_mode);

	return user_input;
}

void clear_message(error_type_t error_type_val)
{
	// Frame parameters
	uint16_t bold = 2;
	uint16_t frame_x_pos = LCD_X_MARGIN_MESSAGE_L - 20;
	uint16_t frame_y_pos;
	if (error_type_val == GRAPHIC_ERROR || error_type_val == FATAL_ERROR)
		frame_y_pos = LCD_TOP_ROW - 16 + V_ADJUST
				+ LCD_ROW_HEIGHT * (BOTTOM_MSG_ROW + 2 - MSG_ROW);
	else
		frame_y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT - 16 + V_ADJUST;

	uint16_t frame_width =
			BSP_LCD_GetXSize()
					- (LCD_X_MARGIN_MESSAGE_L + LCD_X_MARGIN_MESSAGE_R
							+ BUTTON_X_SPACE);
	uint16_t frame_height =
			(error_type_val == GRAPHIC_ERROR || error_type_val == FATAL_ERROR) ?
					LCD_ROW_HEIGHT * 3 :
					BSP_LCD_GetYSize() - (LCD_Y_TITLE + LCD_TITLE_HEIGHT + LCD_BOTTOM_SPACE
							+ LCD_ROW_HEIGHT - 4) - 2 * V_ADJUST;

	// Clear message
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
//  DEBUGGING SCREEN
//	BSP_LCD_SetTextColor(
//			(error_type_val == GRAPHIC_ERROR || error_type_val == FATAL_ERROR) ?
//					LCD_COLOR_GREEN : LCD_COLOR_BLUE);
	BSP_LCD_FillRect(frame_x_pos - (bold - 1), frame_y_pos - (bold - 1),
			frame_width + bold, frame_height + bold);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
}

void clear_icons(void)
{
	// Clear buttons without using ICON_VOID (useful if not already loaded)
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillRect(BUTTON_ICON_X, ENCODER_ICON_Y, ICON_W, ICON_H);
	BSP_LCD_FillRect(BUTTON_ICON_X, ESCAPE_ICON_Y, ICON_W, ICON_H);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	return;
}

void prompt_basic_item(char *text, uint8_t row)
{
	BSP_LCD_SetFont(&LCD_FIXED_FONT);
	uint16_t Xpos = LCD_X_TEXT;
	uint16_t Ypos = LCD_FIXED_TOP_ROW - LCD_ROW_HEIGHT * row;
	uint32_t textSize = 0;
	uint32_t rowSize;
	char *ptr = text;
	char nextChar;

	/* Get the text size */
	while (*ptr++)
		textSize++;

	/* Number of characters available on row */
	rowSize = (BSP_LCD_GetXSize() - Xpos) / LCD_FIXED_FONT.Width;

	/* Send the string character by character on LCD
	 * completing the row with spaces
	 * */
	for (uint8_t i = 0; i < rowSize; i++)
	{
		if (i < textSize)
			nextChar = *(text++);
		else
			nextChar = ' ';
		/* Display next character on LCD */
		BSP_LCD_DisplayChar(Xpos, Ypos, nextChar);
		/* Increment the column position by one character width */
		Xpos += LCD_FIXED_FONT.Width;
	}

	return;
}

// Clear row and prompt text
void prompt_menu_item(char *text, uint8_t row)
{
	TFTSTCustomFontData font;
	uint16_t colour = LCD_COLOR_TEXT;
	uint16_t x = LCD_X_TEXT;
	uint16_t y = LCD_TOP_ROW + LCD_ROW_HEIGHT * row;
	char c;

	// Clear line
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillRect(0, LCD_TOP_ROW + LCD_ROW_HEIGHT * row + 5,
	BUTTON_ICON_X, LCD_ROW_HEIGHT);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	// Print character until the end
	for (uint8_t i = 0; i < MAX_ITEM_CHAR; i++)
	{
		// Stop if end of string
		if ((c = text[i]) == 0)
			break;
		// Replace illegal characters by underscore
		if (c < ' ' || c > '~')
			c = '_';
		// Draw character - first one in bold
		font = (i == 0 ? LCD_BOLD_FONT : LCD_REGULAR_FONT);
		tftstDrawCharWithFont(&font, x, y, c, colour, LCD_COLOR_BCKGND);
		x += font.charData[c - FONT_INDEX_OFFSET].width;
		x += font.charData[c - FONT_INDEX_OFFSET].left;
	}

	return;
}

// Prompt text at the beginning of row without clearing row beforehand
void prompt_text(const char *text, uint8_t row, TFTSTCustomFontData font)
{
	uint16_t colour = LCD_COLOR_TEXT;
	uint16_t x = LCD_X_TEXT;
	uint16_t y = LCD_TOP_ROW + LCD_ROW_HEIGHT * row;

	display_text(x, y, text, font, colour);

	return;
}

// Display text in TFTST custom font anywhere on the screen
void display_text(uint16_t x, uint16_t y, const char *text,
		TFTSTCustomFontData font, uint16_t colour)
{
	char c;
	TFTSTCustomFontCharData c_data;

	// Print character until the end
	for (uint8_t i = 0; i < MAX_ITEM_CHAR; i++)
	{
		// Stop if end of string
		if ((c = text[i]) == 0)
			break;
		// Replace illegal characters by underscore
		if (c < ' ' || c > '~')
			(c = '_');
		// Draw character
		c_data = font.charData[c - FONT_INDEX_OFFSET];
		tftstDrawCharWithFont(&font, x, y, c, colour, LCD_COLOR_BCKGND);
		x += c_data.left;
		x += c_data.width;

	}
}

// same as promptBasicItem but no filling with spaces and choice of font
void prompt_basic_text(char *text, uint8_t row, sFONT font)
{
	BSP_LCD_SetFont(&font);
	uint16_t Xpos = LCD_X_TEXT;
	uint16_t Ypos = LCD_FIXED_TOP_ROW - LCD_ROW_HEIGHT * row;
	uint32_t textSize = 0;
	uint32_t rowSize;
	char *ptr = text;

	/* Number of characters available on row */
	rowSize = (BSP_LCD_GetXSize() - Xpos) / font.Width;

	/* Get the text size */
	while (*ptr++ && textSize++ < rowSize)
		;

	/* Send the string character by character on LCD
	 * completing the row with spaces
	 * */
	for (uint8_t i = 0; i < textSize; i++)
	{
		/* Display next character on LCD */
		BSP_LCD_DisplayChar(Xpos, Ypos, *(text++));
		/* Increment the column position by one character width */
		Xpos += font.Width;
	}

	return;
}

void display_basic_text(char *text, uint16_t Xpos, uint16_t Ypos, sFONT font,
		uint16_t color)
{
	// Set font
	BSP_LCD_SetFont(&font);
	// Set color
	BSP_LCD_SetTextColor(color);

	uint32_t textSize = 0;
	uint32_t rowSize;
	char *ptr = text;

	/* Number of characters available on row */
	rowSize = (BSP_LCD_GetXSize() - Xpos) / font.Width;

	/* Get the text size */
	while (*ptr++ && textSize++ < rowSize)
		;
	/* Send the string character by character on LCD
	 * completing the row with spaces
	 * */
	for (uint8_t i = 0; i < textSize; i++)
	{
		/* Display next character on LCD */
		BSP_LCD_DisplayChar(Xpos, Ypos, *(text++));
		/* Increment the column position by one character width */
		Xpos += font.Width;
	}

	// Reset color
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	return;
}

void prompt_title(char *text)
{
	TFTSTCustomFontData font;
	uint16_t colour = LCD_COLOR_RED_SHFLR;

	//= LCD_TITLE_CAP_FONT;
	//TFTSTCustomFontData font2 = LCD_TITLE_FONT;

	uint16_t x = LCD_X_TITLE;
	uint16_t y = 0;
	char c;

	// Clear top space
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
//	BSP_LCD_SetTextColor(LCD_COLOR_YELLOW); // DEBUGGING SCREEN

	BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), LCD_TITLE_HEIGHT);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	// Special case of root menu, prompt small logo
	if (text == root_menu.label)
	{
		y = LCD_Y_TITLE;
		ili9488_DrawRGBImage8bit(x, y, (uint16_t) SMALL_LOGO_W,
				(uint16_t) SMALL_LOGO_H, (uint8_t*) SMALL_LOGO_ADDRESS);
	}

	// General case
	else
		for (uint8_t i = 0; i < MAX_TITLE_ITEM_CHAR; i++)
		{
			// Stop if end of string
			if ((c = text[i]) == 0)
				break;
			if (c >= 'a' && c <= 'z') // convert small letters in caps - regular title font
			{
				c -= 32; 						// conversion in caps
				font = LCD_TITLE_FONT;
				y = LCD_Y_TITLE;
			}
			else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) // capital letters or numbers - capital title font (bigger italics)
			{
				font = LCD_TITLE_CAP_FONT;
				y = LCD_Y_TITLE + CAP_TITLE_ADJUSTMENT;
			}
			else 							// other characters
			{
				// Replace illegal characters by underscore
				if (c < ' ' || c > '~')
					c = '_';
				font = LCD_TITLE_FONT;  	// write in regular title font
				y = LCD_Y_TITLE;
			}

			tftstDrawCharWithFont(&font, x, y, c, colour, LCD_COLOR_BCKGND);
			x += font.charData[c - FONT_INDEX_OFFSET].width;
			x += font.charData[c - FONT_INDEX_OFFSET].left;
		}
	return;
}

void prompt_current_title(void)
{
	extern menu_t current_menu;
	prompt_title(current_menu.label);
	return;
}

void prompt_dot(uint8_t row)
{
	extern menu_t current_menu;
	uint16_t X = LCD_TOP_DOT_X;
	uint16_t Y = LCD_TOP_DOT_Y + LCD_ROW_HEIGHT * row;

	if (current_menu.code == SET_FAVORITES
			|| current_menu.code == SET_DEALERS_CHOICE
			|| current_menu.code == SET_CUSTOM_GAMES)
		BSP_LCD_DrawCircle(X, Y, 7);
	else
		BSP_LCD_FillCircle(X, Y, 5);

	return;
}

void clear_dot(uint8_t row)
{
	extern menu_t current_menu;
	uint16_t X = LCD_TOP_DOT_X;
	uint16_t Y = LCD_TOP_DOT_Y + LCD_ROW_HEIGHT * row;
	uint16_t textColor = BSP_LCD_GetTextColor();

	// Set color to black
	BSP_LCD_SetTextColor(BSP_LCD_GetBackColor());

	if (current_menu.code == SET_FAVORITES
			|| current_menu.code == SET_DEALERS_CHOICE
			|| current_menu.code == SET_CUSTOM_GAMES)
		BSP_LCD_DrawCircle(X, Y, 7);
	else
		BSP_LCD_FillCircle(X, Y, 5);

	// Set color to text
	BSP_LCD_SetTextColor(textColor);
	return;
}

void prompt_check(uint8_t row)
{
	uint16_t X = LCD_TOP_DOT_X;
	uint16_t Y = LCD_TOP_DOT_Y + LCD_ROW_HEIGHT * row;
	uint16_t textColor = BSP_LCD_GetTextColor();
	BSP_LCD_SetTextColor(LCD_COLOR_RED_SHFLR);
	BSP_LCD_FillCircle(X, Y, 5);
	BSP_LCD_SetTextColor(textColor);
	return;
}

void clear_check(uint8_t row)
{
	uint16_t X = LCD_TOP_DOT_X;
	uint16_t Y = LCD_TOP_DOT_Y + LCD_ROW_HEIGHT * row;
	uint16_t textColor = BSP_LCD_GetTextColor();
	BSP_LCD_SetTextColor(BSP_LCD_GetBackColor());
	BSP_LCD_FillCircle(X, Y, 5);
	BSP_LCD_SetTextColor(textColor);
	return;
}

// Get requested menu and load its address in p_menu
return_code_t set_menu(menu_t *p_menu, item_code_t x)
{
	bool found = false;
	for (uint16_t i = 0; i < n_menus; i++)
	{
		if (menu_list[i]->code == x)
		{
			found = true;
			p_menu->code = menu_list[i]->code;
			p_menu->label = menu_list[i]->label;
			p_menu->n_items = menu_list[i]->n_items;
			p_menu->items = menu_list[i]->items;
		}
	}
	return found ? LS_OK : MENU_ERROR;
}

return_code_t set_current_menu(item_code_t item_code)
{
	extern menu_t current_menu;
	return set_menu(&current_menu, item_code);
}

// prompt_menu displays menu using current row and offset
// to be used on startup with ROOT_MENU
return_code_t prompt_menu(prompt_menu_mode_t prompt_menu_mode,
		item_code_t item_code)
{
	extern menu_t current_menu;
	return_code_t ret_val = LS_OK;
	menu_t running_menu;
	uint8_t index;

// If menu is not the current menu, set current menu to item_code and prompt title
	if (item_code != current_menu.code)
	{
		if ((ret_val = set_current_menu(item_code)) != LS_OK)
			goto _EXIT;

		// Prompt new menu title
		prompt_title(current_menu.label);

		// Clear medium part of the screen
		clear_text();

		// Get parameters from memory (if same keep as is)
		// If default select, set cursor based on last visit, otherwise keep as is
		//[we would have used set_param_sub_menu() before calling prompt_menu()]
		if (prompt_menu_mode == DEFAULT_SELECT)
		{
			if (EERAM_ReadSRAMData(&hi2c3,
			EERAM_LCD_ROW + current_menu.code, &LCD_row, 1) != HAL_OK)
				return EERAM_ERROR;
			if (EERAM_ReadSRAMData(&hi2c3,
			EERAM_LCD_SCROLL + current_menu.code, &LCD_scroll, 1) != HAL_OK)
				return EERAM_ERROR;
		}

	}

	// Set context
	extern context_t context;
	// Get calling menu
	item_code_t calling_menu_code;
	if ((ret_val = get_calling_menu(&calling_menu_code)) != LS_OK)
		goto _EXIT;

	if (calling_menu_code == SET_PREFS)
		context = CONTEXT_SET_PREFS;
	else if (calling_menu_code == SET_CUSTOM_GAMES)
		context = CONTEXT_SET_CUSTOM_GAMES;
	else if (calling_menu_code == DEALERS_CHOICE)
		context = CONTEXT_DEALERS_CHOICE;
	else
		context = CONTEXT_ROOT;

// If there are sub-items, prompt icons, menu items and checks/dots
	if (current_menu.n_items != 0)
	{
		// Display icons (specific ones for setting dynamic menus)
		if (current_menu.code == SET_FAVORITES
				|| current_menu.code == SET_DEALERS_CHOICE)
		{
			display_encoder_icon(ICON_EDIT);
			display_escape_icon(ICON_SAVE);
		}
		else
		{
			display_encoder_icon(ICON_CHECK);
			display_escape_icon(
					(current_menu.code == ROOT_MENU) ? ICON_VOID : ICON_BACK);
		}

		// Go down row by row
		for (uint8_t row = 0; row < LCD_N_ROWS; row++)
		{
			index = LCD_scroll + row;
			// Until we get to last item
			if (index < current_menu.n_items)
			{
				// identify menu on current row
				if ((ret_val = set_menu(&running_menu,
						current_menu.items[index])) != LS_OK)
					goto _EXIT;
				// Prompt corresponding menu label on row
				prompt_menu_item(running_menu.label, row);
				// Special case for set favourites and set DLR's choice
				if (current_menu.code == SET_FAVORITES
						|| current_menu.code == SET_DEALERS_CHOICE)
				{
					uint16_t address =
							(current_menu.code == SET_FAVORITES) ?
							EERAM_FAVS :
																	EERAM_DLRS;
					uint8_t currently_selected;
					// Assess is running menu is selected
					if ((ret_val = read_eeram_bit(address, index,
							&currently_selected)) != LS_OK)
						goto _EXIT;
					// Update check accordingly
					currently_selected ? prompt_check(row) : clear_check(row);
				}
			}
			else
				break;
		}
		// Prompt dot
		prompt_dot(LCD_row);
	}
// Else clear icons
	else
		clear_icons();

	_EXIT:

	return ret_val;
}

return_code_t get_calling_menu(item_code_t *p_menu_code)
{
	return_code_t ret_val = LS_OK;
	extern menu_t current_menu;

// Except if we are at the root already
	if (current_menu.code != ROOT_MENU)
	{
		// Determine code of menu calling current menu_t
		if ((ret_val = read_eeram(EERAM_CLL_MEN + current_menu.code,
				p_menu_code, 1)) != LS_OK)
			goto _EXIT;
	}
	else
		*p_menu_code = ROOT_MENU;

	_EXIT:

	return ret_val;
}

// promptCalling_menu displays menu using menu-specific DEFAULT row and offset
return_code_t prompt_calling_menu(void)
{
	extern menu_t current_menu;
	item_code_t calling_code;

// Except if we are at the root already
	if (current_menu.code != ROOT_MENU)
	{
		get_calling_menu(&calling_code);
		// Prompt calling menu_t
		prompt_menu(DEFAULT_SELECT, calling_code);
	}
// In root menu, closes flap if tray empty
	else if (!cards_in_shoe())
		flap_close();

	return LS_OK;
}

/*
 * Update_menu moves the cursor on the menu lines
 * updates LCFrow and LCDscroll
 * and returns selected item
 */
item_code_t update_menu(void)
{

	extern menu_t current_menu;
	int16_t increment = 0;
	int8_t target = LCD_row;

// Check encoder position
	if ((increment = read_encoder(CLK_WISE)))
	{
		// Detect CW turn of encoder
		if (increment > 0)
		{
			// Calculate target based on encoder rotation
			target = min((int8_t ) (LCD_row + LCD_scroll) + (int8_t ) increment,
					(int8_t ) (current_menu.n_items - 1));
			// If no scroll necessary move to target
			if (target < (int8_t) (LCD_scroll + LCD_N_ROWS))
			{
				clear_dot(LCD_row);
				LCD_row = (uint8_t) (target - LCD_scroll);
				prompt_dot(LCD_row);
			}
			// If scroll down necessary, scroll to have target on last row and prompt dot there
			else
			{
				LCD_row = LCD_N_ROWS - 1;
				LCD_scroll = (uint8_t) (target - LCD_row);
				prompt_menu(DEFAULT_SELECT, current_menu.code);
			}
		}
		// Detect CCW turn of encoder
		else if (increment < 0)
		{
			// Calculate target based on encoder rotation
			target = (uint8_t) max(
					(int16_t ) (LCD_row + LCD_scroll) + increment, 0);
			// If no scroll necessary move to target
			if (target >= LCD_scroll)
			{
				clear_dot(LCD_row);
				LCD_row = (uint8_t) (target - LCD_scroll);
				prompt_dot(LCD_row);
			}
			// If scroll up necessary, scroll to have target on first row and prompt dot there
			else
			{
				LCD_row = 0;
				LCD_scroll = (uint8_t) (target - LCD_row);
				prompt_menu(DEFAULT_SELECT, current_menu.code);
			}
		}
	}

// Return current item
	return current_menu.items[LCD_row + LCD_scroll];
}

/*
 * menu_cycle
 *
 * Cycles update_menu() within a menu with sub-items
 * Also keeps updating fresh_load main variable
 * Takes action upon button press:
 * ESC:
 *      -> goes back to previous (calling) menu - except if root menu (does nothing)
 *      -> permanent press goes back to root menu
 * ENT:
 *
 *      -> sets current menu as calling menu for selected menu
 *      -> sets selected menu as default cursor for current menu
 *      -> sets selected menu as new current menu
 *      if selected (now current) menu has sub items and is not dynamic:
 *          -> opens it with cursor positioned at latest selection (stays in menu_cycle)
 *      else (selected menu has no sub-items and not dynamic)
 *          -> does not update display and EXIT

 * 	ENT in case of dynamic menus: selects and de-selects
 * 	ESC in case of dynamic menus: exit
 *
 * Global variables used:	escape_btn, encoder_btn, current_menu, LCD_row, LCD_scroll
 */
return_code_t menu_cycle(void)
{
	extern menu_t current_menu;
	extern bool fresh_load;
	return_code_t ret_val = LS_OK;
	menu_t selected_menu;
	item_code_t running_code;
	uint32_t start_time;
	bool cards_seen_in_shoe = false;
	bool shoe_cleared = false;

	reset_encoder();
	reset_btns();

// Display icons (specific ones for setting dynamic menus)
	if (current_menu.code == SET_FAVORITES
			|| current_menu.code == SET_DEALERS_CHOICE)
	{
		display_encoder_icon(ICON_EDIT);
		display_escape_icon(ICON_SAVE);
	}
	else
	{
		display_encoder_icon(ICON_CHECK);
		display_escape_icon(
				current_menu.code == ROOT_MENU ? ICON_VOID : ICON_BACK);
	}

// Update menus until selection, keep selection
	while (!escape_btn.short_press && !escape_btn.long_press
			&& !escape_btn.permanent_press && !encoder_btn.short_press
			&& !encoder_btn.long_press && !encoder_btn.permanent_press)
	{
		watchdog_refresh();
		// Update screen and check buttons
		running_code = update_menu();
		update_btns();
		// Keep track of tray status
		if (!cards_in_tray())
			fresh_load = true;

		// Keep track of shoe status
		if (cards_in_shoe())
		{
			cards_seen_in_shoe = true;
			shoe_cleared = false;
			flap_mid();
		}
		else if (cards_seen_in_shoe && !shoe_cleared)
		{
			cards_seen_in_shoe = false;
			shoe_cleared = true;
			start_time = HAL_GetTick();
		}

		if (shoe_cleared && HAL_GetTick() > start_time + SHOE_DELAY)
		{
			shoe_cleared = false;
			flap_close();
		}
	}

// Store selected sub-menu for later use
	if ((ret_val = set_menu(&selected_menu, running_code) != LS_OK))
		goto _EXIT;

// If any press on ENT...
	if (encoder_btn.short_press || encoder_btn.long_press
			|| encoder_btn.permanent_press)
	{
		// Special treatment if we are in SET_FAVORITES and SET_DEALERS_CHOICE
		// Update checks on screen and in memory
		if (current_menu.code == SET_FAVORITES
				|| current_menu.code == SET_DEALERS_CHOICE)
		{
			uint16_t address =
					(current_menu.code == SET_FAVORITES) ?
					EERAM_FAVS :
															EERAM_DLRS;
			uint8_t r_data;

			// Get status (selected/not selected) of running menu
			if ((ret_val = read_eeram_bit(address, LCD_scroll + LCD_row,
					&r_data)) != LS_OK)
				goto _EXIT;

			bool is_selected = (bool) r_data;

			// Toggle red dot (hence the !is_selected)
			is_selected ? clear_check(LCD_row) : prompt_check(LCD_row);
			if ((ret_val = write_eeram_bit(address, LCD_row + LCD_scroll,
					!is_selected)) != LS_OK)
				goto _EXIT;
		}
		// General case
		else
		{
			// If permanent press on MAINTENANCE, select secret menu_t
			if (selected_menu.code == MAINTENANCE
					&& encoder_btn.permanent_press)
			{
				selected_menu.code = MAINTENANCE_LEVEL_2;
				LCD_scroll = 0;
				LCD_row = 0;
			}

			/* Update current menu as latest calling menu of selected menu_t */
			if ((ret_val = write_eeram(EERAM_CLL_MEN + selected_menu.code,
					&current_menu.code, 1)) != LS_OK)
				goto _EXIT;
			/* Update current row and offset as default for current menu
			 * except for non-repeatable menus (Empty, Settings)
			 */
			if (selected_menu.code != EMPTY && selected_menu.code != SETTINGS)

			{
				if ((ret_val = write_eeram(
				EERAM_LCD_ROW + current_menu.code, &LCD_row, 1)) != LS_OK)
					goto _EXIT;
				if ((ret_val = write_eeram(
				EERAM_LCD_SCROLL + current_menu.code, &LCD_scroll, 1)) != LS_OK)
					goto _EXIT;
			}

			// If there are sub-menus in the selected item, display it and make it the current menu_t
			if (selected_menu.n_items)
			{
				// Set cursor position
				// Prompt sub-menu (will set selected_menu as the current menu_t)
				if ((ret_val = prompt_menu(DEFAULT_SELECT, selected_menu.code))
						!= LS_OK)
					goto _EXIT;
			}
			// If dynamic menu with no sub-menus, go back to root_menu (don't change current menu_t)
			else if (selected_menu.code == FAVORITES)
			{
				ret_val = NO_FAVORITES_SELECTED;
				goto _EXIT;
			}
			else if (selected_menu.code == DEALERS_CHOICE)
			{
				ret_val = NO_DEALERS_CHOICE_SELECTED;
				goto _EXIT;
			}
			//Otherwise (action menu) prompt title and make it the current menu_t
			else
			{
				// Prompt selected menu_t
				if ((ret_val = prompt_menu(DEFAULT_SELECT, selected_menu.code))
						!= LS_OK)
					goto _EXIT;
			}
		}
	}
// If short or long press on escape, exit to calling menu (do nothing if in root_menu)
// If permanent press on escape, exit to root menu
// Special case: set dynamic menus if we are in "set <dynamic menu_t>"
	else if (escape_btn.short_press || escape_btn.long_press)
	{
		// Set favorites/DLRC if in "setting favorites/DLRC" menu_t
		if (current_menu.code == SET_FAVORITES)
			set_dynamic_menu(&favorites_menu);
		else if (current_menu.code == SET_DEALERS_CHOICE)
			set_dynamic_menu(&dealers_choice_menu);
		// Go back to calling menu (does nothing if at root)
		if ((ret_val = prompt_calling_menu()) != LS_OK)
			goto _EXIT;
	}
	else if (escape_btn.permanent_press)
	{
		if (current_menu.code != ROOT_MENU)
			if ((ret_val = prompt_menu(DEFAULT_SELECT, ROOT_MENU)) != LS_OK)
				goto _EXIT;
	}

	_EXIT:

	/*
	 * Main calling sequence will take care of the rest:
	 * • Action if no sub-menus
	 * • menu_cycle in newly opened menu otherwise
	 * • button status is transferred
	 * • error management
	 */

	return ret_val;
}

void prompt_animated_logo(void)
{

	uint16_t _w;
	uint16_t _h;
	uint16_t Xpos;
	uint16_t Ypos;
	uint8_t *_add;

	_w = (uint16_t) LOGO_W;
	_h = (uint16_t) LOGO_H;
	_add = (uint8_t*) LOGO_ADDRESS;

	Xpos = (BSP_LCD_GetXSize() - _w) / 2;
	Ypos = (BSP_LCD_GetYSize() - _h) / 2;

	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	HAL_Delay(500);

	for (uint32_t i = 0; i < LOGO_N_FRAMES; i++)
	{
		_add = (uint8_t*) (LOGO_ANIM_ADDRESS + i * LOGO_SIZE);
		ili9488_DrawRGBImage8bit(Xpos, Ypos, _w, _h, _add);
	}

	return;
}

bool is_proper_game(item_code_t game_code)
{
	bool found = false;
	for (uint16_t i = 0; i < games_list.n_items; i++)
		if (game_code == games_list.items[i])
		{
			found = true;
			break;
		}

	return found;
}
/*SORT OUT DEAL_N_CARDS
 CHECK IF GET_GAME_INDEX CHANGE DOES NOT BREAK THINGS
 REST USER PREFS IS BROKEN*/
// Used for EERAM storage addressing
return_code_t get_game_index(uint16_t *p_index, item_code_t game_code)
{
	extern uint16_t n_presets;
	extern game_rules_t rules_list[];
	bool found = false;
	for (*p_index = 0; *p_index < n_presets; (*p_index)++)
		if (game_code == rules_list[*p_index].game_code)
		{
			found = true;
			break;
		}
	return (found ? LS_OK : LS_ERROR);
}

// clear_text lives the title on and the bottom space unchanged
void clear_text(void)
{

	int left = 0;
	int top = LCD_TITLE_HEIGHT + 1;
	int width = BSP_LCD_GetXSize() - ICON_W - LCD_RIGHT_MARGIN;
	int height = BSP_LCD_GetYSize() - top - LCD_BOTTOM_SPACE;
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
//	BSP_LCD_SetTextColor(LCD_COLOR_GREEN); // DEBUGGING SCREEN
	BSP_LCD_FillRect(left, top, width, height);
//	BSP_LCD_SetTextColor(LCD_COLOR_BLUE); // DEBUGGING SCREEN
	BSP_LCD_FillRect(left, ESCAPE_ICON_Y + ICON_H, BSP_LCD_GetXSize(),
	ENCODER_ICON_Y - (ESCAPE_ICON_Y + ICON_H));
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	return;
}

// Parameters for prompt n_cards_in
const uint16_t right_space = 440;
const uint16_t c_rad = 15;
const uint16_t bold = 2;
const uint16_t c_xpos = c_rad + bold + right_space;
const uint16_t c_ypos = c_rad + bold;
const uint16_t n_y_pos = c_ypos - 11;
const double k_radius = 2.0 / 3.0;

void hide_n_cards_in(void)
{
// Clear double circle content
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillCircle(BSP_LCD_GetXSize() - c_xpos, BSP_LCD_GetYSize() - c_ypos,
			c_rad + bold);
	BSP_LCD_FillCircle(
			BSP_LCD_GetXSize() + (uint16_t) (c_rad * k_radius) - c_xpos,
			BSP_LCD_GetYSize() - c_ypos, c_rad + bold);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	return;
}

// atomic prompt_n_cards_in()
static return_code_t atomic_pnci(uint16_t color)
{
	return_code_t ret_val;
	static bool previous_double_deck_state = DOUBLE_DECK_STATE;
	extern union machine_state_t machine_state;
	uint16_t n_x_pos;

// Get n_cards_in
	if ((ret_val = read_n_cards_in()) != LS_OK)
		goto _EXIT;

// Small font
	BSP_LCD_SetFont(&LCD_FIXED_SMALL_FONT);

	if (machine_state.double_deck == SINGLE_DECK_STATE)
	{
		n_x_pos = c_xpos - (n_cards_in > 99 ? 24 : n_cards_in > 9 ? 18 : 13);

		// If coming from double deck state clear zone
		if (previous_double_deck_state == DOUBLE_DECK_STATE)
			hide_n_cards_in();
		// Otherwise just clear inside of circle
		else
		{
			// Clear circle content
			BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
			BSP_LCD_FillCircle(BSP_LCD_GetXSize() - c_xpos,
					BSP_LCD_GetYSize() - c_ypos, c_rad - 1);
			BSP_LCD_SetTextColor(color);
		}

		// Show nb cards in specified colour
		snprintf(display_buf, N_DISP_MAX, "%2d", n_cards_in);
		BSP_LCD_DisplayStringAt(n_x_pos, n_y_pos, (uint8_t*) display_buf,
				RIGHT_MODE);
		// Draw bold circle
		for (uint16_t i = 0; i < bold; i++)
			BSP_LCD_DrawCircle(BSP_LCD_GetXSize() - c_xpos,
					BSP_LCD_GetYSize() - c_ypos, c_rad + i);
	}

	else
	{
		n_x_pos = c_xpos
				- (n_cards_in > 99 ? 34 - 5 : n_cards_in > 9 ? 28 - 5 : 23 - 5);

		// Clear double circle content
		BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
		BSP_LCD_FillCircle(BSP_LCD_GetXSize() - c_xpos,
				BSP_LCD_GetYSize() - c_ypos, c_rad - 1);
		BSP_LCD_FillCircle(
				BSP_LCD_GetXSize() + +(uint16_t) (c_rad * k_radius) - c_xpos,
				BSP_LCD_GetYSize() - c_ypos, c_rad - 1);
		BSP_LCD_SetTextColor(color);
		// Draw double circle in bold
		for (uint16_t i = 0; i < bold; i++)
			draw_double_circle(BSP_LCD_GetXSize() - c_xpos,
					BSP_LCD_GetYSize() - c_ypos, c_rad + i);

		// Show nb cards in specified colour
		snprintf(display_buf, N_DISP_MAX, "%2d", n_cards_in);
		BSP_LCD_DisplayStringAt(n_x_pos, n_y_pos, (uint8_t*) display_buf,
				RIGHT_MODE);
	}
// Restore text colour
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
// Back to normal font
	BSP_LCD_SetFont(&LCD_FIXED_FONT);

	_EXIT:

	// Save current double deck state as previous
	previous_double_deck_state = machine_state.double_deck;

	return ret_val;
}

void prompt_n_cards_in(void)
{
	atomic_pnci(LCD_COLOR_TEXT);

	return;
}

void warn_n_cards_in(void)
{
	atomic_pnci(LCD_COLOR_RED_SHFLR);

	return;
}

return_code_t set_param_sub_menu(item_code_t top_menuCode,
		item_code_t sub_menuCode)
{
	return_code_t ret_val;
	menu_t top_menu;
	uint16_t i;
	if ((ret_val = set_menu(&top_menu, top_menuCode)) != LS_OK)
		return ret_val;
	for (i = 0; i < top_menu.n_items; i++)
	{
		if (top_menu.items[i] == sub_menuCode)
			break;
	}
	if (i == top_menu.n_items)
		return LS_ERROR;
	if (i < LCD_N_ROWS)
	{
		LCD_scroll = 0;
		LCD_row = i;
	}
	else
	{
		LCD_scroll = i - (LCD_N_ROWS - 1);
		LCD_row = LCD_N_ROWS - 1;
	}

	return LS_OK;
}

// Get icon address from code
uint8_t* icon_address(icon_code_t code)
{
	int16_t idx;
// find icon code in (reference) icon list
	for (idx = 0; idx < n_icons; idx++)
	{
		if (icon_list[idx].code == code)
			break;
	}
	if (idx >= n_icons)
		return NULL;
	else
		return icon_list[idx].address;
}

void prompt_uid(void)
{
	uint16_t row = 0;
	prompt_text("UID", row++, LCD_REGULAR_FONT);
	snprintf(display_buf, N_DISP_MAX, "%04X-%04X-%04X",
			(uint16_t) (HAL_GetUIDw0() >> 16),
			(uint16_t) (HAL_GetUIDw0() & 0XFFFFFFFF),
			(uint16_t) (HAL_GetUIDw1() >> 16));
	prompt_text(display_buf, row++, LCD_REGULAR_FONT);
	snprintf(display_buf, N_DISP_MAX, "%04X-%04X-%04X",
			(uint16_t) (HAL_GetUIDw1() & 0XFFFFFFFF),
			(uint16_t) (HAL_GetUIDw2() >> 16),
			(uint16_t) (HAL_GetUIDw2() & 0XFFFFFFFF));
	prompt_text(display_buf, row++, LCD_REGULAR_FONT);
}

void prompt_firmware_version(void)
{
	uint16_t row = 3;
	char bl_version_str[32];

	prompt_text(GetFirmwareVersionDisplay(), row++, LCD_REGULAR_FONT);

	// Display bootloader version
	uint16_t bl_ver = GetBootloaderVersion();
	if (bl_ver != 0) {
		uint8_t major = bl_ver >> 8;
		uint8_t minor = bl_ver & 0xFF;
		snprintf(bl_version_str, sizeof(bl_version_str), "Bootloader v%d.%d", major, minor);
	} else {
		snprintf(bl_version_str, sizeof(bl_version_str), "Bootloader v1.0");
	}
	prompt_text(bl_version_str, row++, LCD_REGULAR_FONT);

	return;

}

static void prompt_with_commas(uint32_t N, uint16_t x_pos, uint16_t row)
{
	uint32_t n_blocks = 0;
	uint32_t running = N;
	char *p_buffer = NULL;
	const uint16_t period = 4;
	const uint16_t block_size = period + 1;
	char first_block[period];
	char block[block_size];
	uint8_t buffer_size;

	memset(block, 0, block_size);

	do
	{
		running /= 1000UL;
		n_blocks++;
	}
	while (running != 0);

// Manage overflow
	if (n_blocks > 3)
	{
		n_blocks = 3;
		N = 999999999;
	}

// Set up string buffer
	buffer_size = n_blocks * period;
	p_buffer = (char*) malloc(buffer_size);
// Set last character to 0
	*(p_buffer + buffer_size - 1) = 0;

	running = N;
// Filling n_blocks - 1
	for (uint32_t i = 1; i < n_blocks; i++)
	{
		// Put next digits in a block
		snprintf(block, block_size, ".%03u", (uint16_t) (running % 1000));
		// Transfer block to buffer
		for (uint16_t j = 0; j < period; j++)
			*(p_buffer + (buffer_size - 1) - period * i + j) = block[j];
		running /= 1000UL;
		__NOP();
	}

// Put last digits in a block
	snprintf(first_block, period, "%3u", (uint16_t) (running % 1000));
// Transfer block to buffer
	for (uint16_t j = 0; j < period - 1; j++)
		*(p_buffer + j) = first_block[j];

	display_text(x_pos, LCD_TOP_ROW + LCD_ROW_HEIGHT * row, p_buffer,
	LCD_REGULAR_FONT, LCD_COLOR_TEXT),

	free(p_buffer);

	return;
}

return_code_t prompt_tally(void)
{
	return_code_t ret_val;
	uint16_t row = 6;
	union
	{
		uint32_t value;
		uint8_t bytes[E_CARDS_TALLY_SIZE];
	} tally;

	if ((ret_val = read_eeram(EERAM_CARDS_TALLY, tally.bytes,
	E_CARDS_TALLY_SIZE)) != LS_OK)
		goto _EXIT;

	prompt_text("Card Tally:", row, LCD_REGULAR_FONT);
	prompt_with_commas(tally.value, 160, row);

	_EXIT:

	return ret_val;

}

return_code_t prompt_about(void)
{
	return_code_t ret_val = LS_OK;

	prompt_uid();
	prompt_firmware_version();
	if ((ret_val = prompt_tally()) != LS_OK)
		return ret_val;
	reset_btns();
	uint32_t start_time = HAL_GetTick();
	while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
	{
		watchdog_refresh();
		if ((HAL_GetTick() - start_time) > 3 * L_WAIT_DELAY)
			break;
	}

	return ret_val;
}

void name_input(char name_buffer[], uint16_t text_len)
{
	char flash_buffer[] =
	{ ' ', 0 };
	int16_t cursor;
	uint16_t x_pos_0 = LCD_X_TEXT;
	uint16_t x_pos;
	uint16_t y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;
	sFONT sfont = LCD_FIXED_FONT;
	uint32_t flash_time = 400;
	int16_t increment;
	bool in_loop[3];
	typedef enum
	{
		TOP, SELECT, EDIT
	} loop_level_t;

	void enter_loop(loop_level_t loop_level)
	{
		// Reset in_loop
		memset(in_loop, false, 3);
		// Flag current loop
		in_loop[loop_level] = true;
		// Select icons
		switch (loop_level)
		{
			case TOP:
				// Display icons
				display_encoder_icon(ICON_EDIT);
				display_escape_icon(ICON_CHECK);
				break;

			case SELECT:
				// Display icons
				display_encoder_icon(ICON_EDIT);
				display_escape_icon(ICON_CHECK);
				break;

			case EDIT:
				// Display icons
				display_encoder_icon(ICON_SAVE);
				display_escape_icon(ICON_BACK);
				break;

		}

	}

	// Error-proof name_buffer (ensure proper termination)
	uint16_t char_index;
	for (char_index = 0; char_index < text_len; char_index++)
	{
		if (name_buffer[char_index] == 0)
			break;
	}
	for (uint16_t i = char_index; i < text_len + 1; i++)
		name_buffer[i] = 0;

	enter_loop(TOP);
	// Display text
	display_basic_text(name_buffer, x_pos_0, y_pos, sfont, LCD_COLOR_TEXT);

	// Wait for button press and exit if ESC
	wait_btns();
	if (escape_btn.interrupt_press)
		goto _EXIT;

	reset_btns();
	// Initialise cursor to 0
	cursor = 0;
	x_pos = x_pos_0 + cursor * sfont.Width;
	flash_buffer[0] = name_buffer[cursor];
	bool flash_toggle = false;

	// Enter select loop
	enter_loop(SELECT);

	reset_encoder();
	while (in_loop[SELECT])
	{
		watchdog_refresh();
		/* Move flashing character*/
		reset_btns();
		uint32_t previous_time = HAL_GetTick();
		while (!escape_btn.interrupt_press && !encoder_btn.interrupt_press)
		{
			watchdog_refresh();
			// Flash current character
			if (HAL_GetTick() - previous_time > flash_time)
			{
				if (flash_toggle)
					display_basic_text(
							flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
									"_" : flash_buffer, x_pos, y_pos, sfont,
							LCD_COLOR_TEXT);
				else
					display_basic_text(" ", x_pos, y_pos, sfont,
					LCD_COLOR_TEXT);
				previous_time = HAL_GetTick();
				flash_toggle = !flash_toggle;
			}

			// Move cursor if encoder turn
			if ((increment = read_encoder(CLK_WISE)))
			{
				// Display permanently current flashing character or space
				display_basic_text(
						flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
								" " : flash_buffer, x_pos, y_pos, sfont,
						LCD_COLOR_TEXT);
				// Detect CW turn of encoder
				if (increment > 0 && cursor < text_len - 1)
				{
					cursor = min(text_len - 1, cursor + (uint16_t ) increment);
				}
				// Detect CCW turn of encoder
				else if (increment < 0 && cursor > 0)
				{
					increment = max(increment, -(int16_t ) cursor);
					cursor -= (uint16_t) (-increment);
				}
				// Update on screen cursor
				x_pos = x_pos_0 + cursor * sfont.Width;
				flash_buffer[0] = name_buffer[cursor];

				// Force flash
				previous_time = 0;
				flash_toggle = false;
			}
		}

		// If ENT enter edit loop
		if (encoder_btn.interrupt_press)
		{
			reset_btns();

			// Enter edit loop
			enter_loop(EDIT);

			while (in_loop[EDIT])
			{
				watchdog_refresh();
				// Save current character
				char original_char = flash_buffer[0];

				// Show selected character in red
				display_basic_text(
						flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
								"_" : flash_buffer, x_pos, y_pos, sfont,
						LCD_COLOR_RED_SHFLR);

				// Edit current character until button press
				while (!escape_btn.short_press && !encoder_btn.short_press
						&& !encoder_btn.permanent_press)
				{
					watchdog_refresh();
					// Change character if encoder turns
					if ((increment = read_encoder(CLK_WISE)))
					{
						flash_buffer[0] = (flash_buffer[0] + ('~' - ' ')
								+ increment - ' ') % ('~' - ' ') + ' ';
						display_basic_text(
								flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
										"_" : flash_buffer, x_pos, y_pos, sfont,
								LCD_COLOR_RED_SHFLR);
					}
					update_btns();
				}
				// If ENT permanent press, update, beep and exit both edit and select loop
				if (encoder_btn.permanent_press)
				{
					display_basic_text(
							flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
									" " : flash_buffer, x_pos, y_pos, sfont,
							LCD_COLOR_TEXT);
					// Update name_buffer
					name_buffer[cursor] = flash_buffer[0];

					beep(SHORT_BEEP);
					// Back to top loop
					enter_loop(TOP);
				}
				// If ESC restore original character and exit edit loop
				else if (escape_btn.interrupt_press)
				{
					// Restore original character
					flash_buffer[0] = original_char;
					// Show selected character in white
					display_basic_text(
							flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
									" " : flash_buffer, x_pos, y_pos, sfont,
							LCD_COLOR_TEXT);
					/// Back to select loop
					enter_loop(SELECT);
				}
				// Else if ENT (but not permanent press) save and move one char down stay in loop
				else
				{
					// Show selected character in white
					display_basic_text(
							flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
									" " : flash_buffer, x_pos, y_pos, sfont,
							LCD_COLOR_TEXT);
					// Update name_buffer
					name_buffer[cursor] = flash_buffer[0];
					// Update cursor - with possible overflow
					cursor++;
					// Update on-screen cursor
					if (cursor < text_len)
					{
						x_pos = x_pos_0 + cursor * sfont.Width;
						flash_buffer[0] = name_buffer[cursor];
					}
					// Or back to select loop if end of text
					else
					{
						enter_loop(SELECT);
						// Update cursor
						cursor = 0;
						x_pos = x_pos_0 + cursor * sfont.Width;
						flash_buffer[0] = name_buffer[cursor];
					}
				}

				reset_btns();

			}

		}
		// Else reset display and exit select loop
		else
		{
			display_basic_text(
					flash_buffer[0] == ' ' || flash_buffer[0] == 0 ?
							" " : flash_buffer, x_pos, y_pos, sfont,
					LCD_COLOR_TEXT);

			// Back to top loop
			enter_loop(TOP);
		}
	}

	// Clean name_buffer
	// Put zeroes at the end
	int16_t last_char;
	for (last_char = text_len; last_char >= 0; last_char--)
	{
		if (name_buffer[last_char] == ' ' || name_buffer[last_char] == 0)
			name_buffer[last_char] = 0;
		else
			break;
	}

// Replace inside zeroes with space
	for (int16_t i = 0; i < last_char; i++)
		if (name_buffer[i] == 0)
			name_buffer[i] = ' ';

// If empty set default name
	if (last_char < 0)
		strcpy(name_buffer, "Custom");

	_EXIT:

	display_basic_text(name_buffer, x_pos_0, y_pos, sfont, LCD_COLOR_TEXT);
	HAL_Delay(M_WAIT_DELAY);

	return;
}

void draw_double_circle(uint16_t x_pos, uint16_t y_pos, uint16_t radius)
{
	extern LCD_DrawPropTypeDef DrawProp;
	int32_t D; /* Decision Variable */
	uint32_t CurX; /* Current X Value */
	uint32_t CurY; /* Current Y Value */

	D = 3 - (radius << 1);
	CurX = 0;
	CurY = radius;

	while (CurX <= CurY)
	{
		BSP_LCD_DrawPixel((x_pos + CurX), (y_pos - CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurX), (y_pos - CurY), DrawProp.TextColor);
		//BSP_LCD_DrawPixel((x_pos + CurY), (y_pos - CurX), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurY), (y_pos - CurX), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos + CurX), (y_pos + CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurX), (y_pos + CurY), DrawProp.TextColor);
		//BSP_LCD_DrawPixel((x_pos + CurY), (y_pos + CurX), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurY), (y_pos + CurX), DrawProp.TextColor);

		if (D < 0)
			D += (CurX << 2) + 6;
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}

	D = 3 - (radius << 1);
	CurX = 0;
	CurY = radius;
	x_pos += (uint16_t) (c_rad * k_radius); //4 * radius / 3;

	while (CurX <= CurY)
	{
		BSP_LCD_DrawPixel((x_pos + CurX), (y_pos - CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurX), (y_pos - CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos + CurY), (y_pos - CurX), DrawProp.TextColor);
		//BSP_LCD_DrawPixel((x_pos - CurY), (y_pos - CurX), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos + CurX), (y_pos + CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos - CurX), (y_pos + CurY), DrawProp.TextColor);
		BSP_LCD_DrawPixel((x_pos + CurY), (y_pos + CurX), DrawProp.TextColor);
		//BSP_LCD_DrawPixel((x_pos - CurY), (y_pos + CurX), DrawProp.TextColor);

		if (D < 0)
			D += (CurX << 2) + 6;
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}

	return;
}

// Prompts labels with plural
void prompt_labels(const char *label_1, const char *label_2, uint16_t N,
		uint16_t number_width)
{
	const uint16_t n_max_chars = 25; // arbitrary limit;
	uint16_t label_1_width = calculate_width(label_1, LCD_BOLD_FONT);
	uint16_t label_2_width = calculate_width(label_2, LCD_BOLD_FONT);

// Calculate y position
	uint16_t y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;

// Clear row
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillRect(0, y_pos + LCD_ROW_HEIGHT / 4, BUTTON_ICON_X,
	LCD_ROW_HEIGHT);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

// Calculate x position
	uint16_t x_pos = (BSP_LCD_GetXSize()
			- (label_1_width + PRE_NUMBER_SPACE + number_width
					+ POST_NUMBER_SPACE + label_2_width)) / 2;

// Show label 1
	display_text(x_pos, y_pos, label_1, LCD_REGULAR_FONT, LCD_COLOR_TEXT);

// Check number or chars in label_2
	uint16_t n_chars_label_2 = 0;
	for (uint16_t i = 0; i < n_max_chars; i++)
	{
		if (label_2[i] == 0)
			break;
		else
			n_chars_label_2++;
	}

// Allocate memory to label_2_amended (for s at the plural)
	char *label_2_amended = malloc((n_chars_label_2 + 3) * sizeof(char));
// Copy
	strncpy(label_2_amended, label_2, n_chars_label_2 + 2);

// Add some spaces at the end
	uint16_t char_index = n_chars_label_2;
	if (N > 1)
	{
		label_2_amended[char_index++] = 's';
		label_2_amended[char_index++] = 0;
	}
	else
	{
		label_2_amended[char_index++] = ' ';
		label_2_amended[char_index++] = ' ';
		label_2_amended[char_index++] = 0;
	}

// Show label 2
	x_pos +=
			label_1_width + PRE_NUMBER_SPACE + number_width + POST_NUMBER_SPACE;
	display_text(x_pos, y_pos, label_2_amended, LCD_REGULAR_FONT,
	LCD_COLOR_TEXT);

// Free pointer
	free(label_2_amended);
	label_2_amended = NULL;

	return;
}

// Function to calculate text width
uint16_t calculate_width(const char *text, TFTSTCustomFontData font)
{
	uint16_t width = 0;
	char c;

// Print character until the end
	for (uint8_t i = 0; i < MAX_ITEM_CHAR; i++)
	{
		// Stop if end of string
		if ((c = text[i]) == 0)
			break;

		width += font.charData[c - FONT_INDEX_OFFSET].width;
		width += font.charData[c - FONT_INDEX_OFFSET].left;
	}

	return width;
}

// Function to show number
void show_num(uint16_t N_, uint16_t color, uint16_t x_pos,
		uint16_t number_width)
{
// Calculate y position
	uint16_t y_pos = LCD_TOP_ROW + LCD_ROW_HEIGHT * MSG_ROW + V_ADJUST;

	snprintf(display_buf, N_DISP_MAX, "%2u", N_);
// Clear number
	BSP_LCD_SetTextColor(LCD_COLOR_BCKGND);
	BSP_LCD_FillRect(x_pos, y_pos + LCD_ROW_HEIGHT / 4, number_width,
	LCD_ROW_HEIGHT);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);

	display_text(x_pos, y_pos, display_buf, LCD_BOLD_FONT, color);
}

void display_encoder_icon(icon_code_t icon_code)
{
	ili9488_DrawRGBImage8bit(BUTTON_ICON_X, ENCODER_ICON_Y, ICON_W,
	ICON_H, icon_address(icon_code));
	return;
}

void display_escape_icon(icon_code_t icon_code)
{
	ili9488_DrawRGBImage8bit(BUTTON_ICON_X, ESCAPE_ICON_Y, ICON_W,
	ICON_H, icon_address(icon_code));
	return;
}

void display_sensors(void)
{
	uint16_t row;
	carousel_disable();
	BSP_LCD_SetTextColor(LCD_COLOR_RED_SHFLR);
	prompt_basic_text("     TO EXIT, LONG PRESS ON ESC =>", 0,
	LCD_FIXED_SMALL_FONT);
	BSP_LCD_SetTextColor(LCD_COLOR_TEXT);
	reset_btns();
	while (!escape_btn.permanent_press)
	{
		watchdog_refresh();
		row = 1;
		snprintf(display_buf, N_DISP_MAX,
				"J14 TRAY:       %d   J11 ENTRY 1:    %d",
				read_sensor(TRAY_SENSOR), read_sensor(ENTRY_SENSOR_1));
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX,
				"J8  ENTRY 2:    %d   J15 ENTRY 3:    %d",
				read_sensor(ENTRY_SENSOR_2), read_sensor(ENTRY_SENSOR_3));
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX,
				"J6  HOMING:     %d   J7  SLOT:       %d",
				read_sensor(HOMING_SENSOR), read_sensor(SLOT_SENSOR));
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX,
				"J10 EXIT:       %d   J9  SHOE:       %d",
				read_sensor(EXIT_SENSOR), read_sensor(SHOE_SENSOR));
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX,
				"ESC button:     %d   ENT button:     %d",
				HAL_GPIO_ReadPin(escape_btn.port, escape_btn.pin),
				HAL_GPIO_ReadPin(encoder_btn.port, encoder_btn.pin));
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		snprintf(display_buf, N_DISP_MAX, "ENCODER:    %5d", ENCODER_POSITION);
		prompt_basic_text(display_buf, row++, LCD_FIXED_SMALL_FONT);
		update_btns();
	}
	BSP_LCD_Clear(LCD_COLOR_BCKGND);
	prompt_message("\nHoming Carousel.");
	carousel_enable();
	home_carousel();
	clear_message(TEXT_ERROR);

	return;
}
