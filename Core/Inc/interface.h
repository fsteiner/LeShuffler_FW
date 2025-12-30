/*
 * screen.h
 *
 *  Created on: Jul 1, 2024
 *      Author: Francois S
 */

#ifndef INC_INTERFACE_H_
#define INC_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <fonts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stm32_adafruit_lcd.h>
#include <string.h>
#include <utilities.h>

#define FONT_INDEX_OFFSET			32  //ASCII
#define LCD_FIXED_FONT        	    Font24
#define LCD_FIXED_SMALL_FONT   	    Font16
#define LCD_TITLE_FONT           	tftstFont_condorBlackRegular_24
#define LCD_TITLE_CAP_FONT       	tftstFont_condorBlackItalic_30
#define LCD_BOLD_FONT       	    tftstFont_helveticaBold_24
#define LCD_REGULAR_FONT       	    tftstFont_helveticaRegular_24
#define LCD_TITLE_HEIGHT			35
#define CAP_TITLE_ADJUSTMENT		(-6)
#define LCD_COLOR_BCKGND				LCD_COLOR_BLACK
#define LCD_COLOR_TEXT				LCD_COLOR_WHITE
#define LCD_X_TEXT 					30
#define LCD_X_MARGIN_MESSAGE_L      LCD_X_TEXT // 50
#define LCD_X_MARGIN_MESSAGE_R      30
#define LCD_FRAME_WEIGHT             2 // TO BE ADJUSTED
#define LCD_X_TITLE 			    LCD_X_TEXT
#define LCD_Y_TITLE					11
#define LCD_TOP_SPACE 				70//60
#define LCD_BOTTOM_SPACE 			38
#define LCD_ROW_ADJUSTMENT			4
#define LCD_ROW_HEIGHT				(LCD_BOLD_FONT.size + LCD_ROW_ADJUSTMENT)
#define LCD_FIXED_TOP_ROW 			(BSP_LCD_GetYSize()-LCD_TOP_SPACE-LCD_ROW_HEIGHT/2)
#define LCD_TOP_ROW					(LCD_TOP_SPACE-LCD_BOLD_FONT.size*3/4)
#define LCD_TOP_DOT_X				(LCD_X_TEXT/2 + 2)
#define LCD_TOP_DOT_Y               (LCD_TOP_ROW + LCD_ROW_HEIGHT/2 + 2)
#define LCD_N_ROWS 					((BSP_LCD_GetYSize()-LCD_TOP_SPACE-LCD_BOTTOM_SPACE)/LCD_ROW_HEIGHT)
#define LCD_BUFFER_LENGTH			35
#define LCD_RIGHT_MARGIN    		15
#define BUTTON_ICON_X       		(BSP_LCD_GetXSize()-ICON_W-LCD_RIGHT_MARGIN)
#define ENCODER_ICON_Y      		(275-ICON_H/2)
#define ESCAPE_ICON_Y       		(75-ICON_H/2)
#define SILH_Y 						(LCD_TOP_ROW + 20)
#define PRE_NUMBER_SPACE 			5
#define POST_NUMBER_SPACE 			5
#define MAX_TITLE_ITEM_CHAR 		25
#define MAX_ITEM_CHAR 				60
#define MAX_MSG_CHAR               240
#define MSG_ROW						2 //(LCD_N_ROWS + 1) // Row number
#define BOTTOM_MSG_ROW 				4
#define V_ADJUST					50
#define BUTTON_X_SPACE				20

// IMAGES SIZES
#define LOGO_N_FRAMES 				36
#define LOGO_W 						340UL
#define LOGO_H 						102UL
#define LOGO_SIZE 					(LOGO_W * LOGO_H * 2)
#define SMALL_LOGO_W 				140UL
#define SMALL_LOGO_H  				24UL
#define SMALL_LOGO_SIZE 			(SMALL_LOGO_W * SMALL_LOGO_H * 2)
#define ICON_W 						40UL
#define ICON_H 						ICON_W
#define ICON_SIZE 					(ICON_W * ICON_H * 2)
#define SILH_W 						200UL  	// Frame is 400
#define SILH_H 						115UL	// Frame is 117
#define SILH_SIZE 					(SILH_W * SILH_H * 2)
#define RESTART_W 					100UL
#define RESTART_H 					RESTART_W
#define RESTART_SIZE 				(RESTART_W * RESTART_H * 2)

// IMAGES ADDRESSES IN FLASH MEMORY, THESE ARE FLASH - NOT EERAM ADDRESSES
#define FLASH_FACTORY_OFFSET 	0UL
#define IMAGE_START_ADDRESS     (OCTOSPI2_BASE + FLASH_FACTORY_OFFSET)

/* FINAL VERSION */
#define LOGO_ANIM_ADDRESS 	    IMAGE_START_ADDRESS
#define SMALL_LOGO_ADDRESS 	    (LOGO_ANIM_ADDRESS + LOGO_N_FRAMES*LOGO_SIZE)	// AFTER ALL THE ANIMATION FRAMES
#define VOID_ADDRESS            (SMALL_LOGO_ADDRESS + SMALL_LOGO_SIZE)
#define BACK_ADDRESS            (VOID_ADDRESS + ICON_SIZE)
#define CARD_ADDRESS		    (BACK_ADDRESS + ICON_SIZE)
#define CHECK_ADDRESS		    (CARD_ADDRESS + ICON_SIZE)
#define CROSS_ADDRESS		    (CHECK_ADDRESS + ICON_SIZE)
#define EDIT_ADDRESS			(CROSS_ADDRESS + ICON_SIZE)
#define FLAME_ADDRESS		    (EDIT_ADDRESS + ICON_SIZE)
#define PLAYER_ADDRESS		    (FLAME_ADDRESS + ICON_SIZE)
#define RED_CARD_ADDRESS        (PLAYER_ADDRESS + ICON_SIZE)
#define RED_CROSS_ADDRESS       (RED_CARD_ADDRESS + ICON_SIZE)
#define SAVE_ADDRESS       		(RED_CROSS_ADDRESS + ICON_SIZE)
#define SILH_ADDRESS		    (SAVE_ADDRESS + ICON_SIZE)
#define CSO_ENTRY_ADDRESS     	(SILH_ADDRESS + SILH_SIZE)
#define CSO_EXIT_ADDRESS     	(CSO_ENTRY_ADDRESS + SILH_SIZE)
#define CHECK_TRAY_ADDRESS     	(CSO_EXIT_ADDRESS + SILH_SIZE)
#define CONTENT_ADDRESS     	(CHECK_TRAY_ADDRESS + SILH_SIZE)
#define PICK_UP_CARDS_ADDRESS   (CONTENT_ADDRESS + SILH_SIZE)
#define SH_EMPTY_ADDRESS    	(PICK_UP_CARDS_ADDRESS + SILH_SIZE)
#define SH_FULL_ADDRESS			(SH_EMPTY_ADDRESS + SILH_SIZE)
#define TRAY_EMPTY_ADDRESS     	(SH_FULL_ADDRESS + SILH_SIZE)
#define RESTART_ADDRESS     	(TRAY_EMPTY_ADDRESS + SILH_SIZE)
#define FLASH_CURRENT_OFFSET    (RESTART_ADDRESS + RESTART_SIZE)

/*
 * FLASH_CURRENT_OFFSET is the expected value of the first free address in the flash memory
 * this should coincide with the value stored in EERAM_FLASH_OFFSET by image_utility()
 *
 * FLASH_CURRENT_OFFSET = 0x7028D890
 * Taken: 0x28D890/0x2000000 (2.676.880/33.554.432 or 32 MBytes)
 * */

// LOGO: TAKE LAST FRAME OF ANIMATION
#define LOGO_ADDRESS		(LOGO_ANIM_ADDRESS + (LOGO_N_FRAMES-1)*LOGO_SIZE)

// context_t
typedef enum
{
	CONTEXT_ROOT,
	CONTEXT_SET_PREFS,
	CONTEXT_SET_CUSTOM_GAMES,
	CONTEXT_DEALERS_CHOICE
} context_t;

// prompt_menu_mode_t
typedef enum
{
	FORCE_MENU, DEFAULT_SELECT
} prompt_menu_mode_t;

// menu_t
typedef struct
{
	item_code_t code;
	char *label;
	uint8_t n_items;
	item_code_t *items;
} menu_t;

// icon_code_t
typedef enum
{
	ICON_VOID,
	ICON_BACK,
	ICON_CARD,
	ICON_CHECK,
	ICON_CROSS,
	ICON_EDIT,
	ICON_FLAME,
	ICON_PLAYER,
	ICON_RED_CARD,
	ICON_RED_CROSS,
	ICON_SAVE,
	ICON_WHITE_FLAME
} icon_code_t;

// icon_t
typedef struct
{
	icon_code_t code;
	uint8_t *address;
} icon_t;

// icon_set_t
typedef struct
{
	icon_code_t *icon_codes;
	uint16_t n_items;
} icon_set_t;

// alert_mode_t
typedef enum
{
	MESSAGE, WARNING, ALERT
} alert_mode_t;

// prompt_mode_t
typedef enum
{
	NO_HOLD,
	BUTTON_PRESS,
	TIME_LAG,
	SHOE_CLEAR,
	TRAY_LOAD,
	SHOE_CLEAR_OR_TRAY_LOAD
} prompt_mode_t;

// hole_change_t
typedef enum
{
	NO_CHANGE, CARD_CHANGE, PLAYER_CHANGE, BOTH_CHANGE
} hole_change_t;

// tImage necessary for image converter
typedef struct
{
	const uint8_t *data;
	uint16_t width;
	uint16_t height;
	uint8_t dataSize;
} tImage;

//
return_code_t set_dynamic_menu(menu_t*);
void prompt_animated_logo(void);
return_code_t interface_init(void);
void atomic_prompt(return_code_t, char*, uint8_t, prompt_mode_t,
		TFTSTCustomFontData, uint16_t, bool);
void prompt_hole_cards(uint16_t, uint16_t, uint16_t, uint16_t, hole_change_t,
bool);
void prompt_com_cards(uint16_t, uint16_t);
void prompt_message(char*);
void prompt_bottom_message(char*);
return_code_t prompt_interface(alert_mode_t, return_code_t, char*, icon_set_t,
		icon_code_t, prompt_mode_t);
void prompt_final_error(char*);
void atomic_clear_message(error_type_t);
void clear_message(error_type_t);
void clear_bottom_message(void);
void clear_icons(void);
void prompt_basic_item(char*, uint8_t);
void prompt_menu_item(char*, uint8_t);
void prompt_text(char*, uint8_t, TFTSTCustomFontData);
void display_text(uint16_t, uint16_t, const char*, TFTSTCustomFontData,
		uint16_t);
void prompt_basic_text(char*, uint8_t, sFONT);
void display_basic_text(char*, uint16_t, uint16_t, sFONT, uint16_t);
void prompt_title(char*);
void prompt_current_title(void);
void prompt_dot(uint8_t);
void clear_dot(uint8_t);
return_code_t set_menu(menu_t*, item_code_t);
return_code_t set_current_menu(item_code_t);
return_code_t prompt_menu(prompt_menu_mode_t, item_code_t);
return_code_t get_calling_menu(item_code_t*);
return_code_t prompt_calling_menu(void);
item_code_t update_menu(void);
return_code_t update_favorites(void);
return_code_t menu_cycle(void);
bool is_proper_game(item_code_t);
return_code_t get_game_index(uint16_t*, item_code_t);
void clear_text(void);
void clear_picture(void);
void draw_double_circle(uint16_t, uint16_t, uint16_t);
void warn_n_cards_in(void);
void prompt_n_cards_in(void);
void prompt_double_deck(void);
void hide_n_cards_in(void);
void hide_double_deck(void);
return_code_t set_param_sub_menu(item_code_t, item_code_t);
uint8_t* icon_address(icon_code_t);
void prompt_uid(void);
void prompt_firmware_version(void);
return_code_t prompt_tally(void);
return_code_t prompt_about(void);
void name_input(char name_buffer[], uint16_t);
void prompt_labels(const char*, const char*, uint16_t, uint16_t);
uint16_t calculate_width(const char*, TFTSTCustomFontData);
void show_num(uint16_t, uint16_t, uint16_t, uint16_t);
void display_encoder_icon(icon_code_t);
void display_escape_icon(icon_code_t);
void display_sensors(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_INTERFACE_H_ */
