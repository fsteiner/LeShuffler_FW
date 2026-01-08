/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include <basic_operations.h>
#include <main.h>
#include <i2c.h>
#include "memorymap.h"
#include <octospi.h>
#include <rng.h>
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <bootload.h>
#include <buttons.h>
#include <fonts.h>
#include <games.h>
#include <ili9488.h>
#include <interface.h>
#include <inttypes.h>
#include <math.h>
#include "PSRAM.h"
#include <rng.h>
#include <servo_motor.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stm32_adafruit_lcd.h>
#include <string.h>
#include <TB6612FNG.h>
#include <TMC2209.h>
#include "usbd_cdc_if.h"
#include <utilities.h>
#include <version.h>
#include "iwdg.h"

#include "tests.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

extern char display_buf[];
extern union game_state_t game_state;
extern union machine_state_t machine_state;
gen_prefs_t gen_prefs;
context_t context;

bool reset_home = false;
bool fresh_load = true;

servo_motor_t flap;
dc_motor_t tray_motor;
dc_motor_t entry_motor;
dc_motor_t latch;
button encoder_btn;
button escape_btn;

uint8_t n_cards_in;
int16_t prev_encoder_pos;
uint32_t prev_encoder_turn_time;
int8_t carousel_pos;
uint32_t last_deal_time;
bool latch_position;
move_report_t MR;
eject_report_t ER;
load_report_t LR;
menu_t current_menu;

extern game_rules_t void_game_rules;
extern user_prefs_t void_user_pref;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void EnterBootloaderMode(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	uint16_t n_loaded;
	bool sel_rand_mode;
	bool neutralise_errors = false;

	/* USER CODE END 1 */

	/* Enable the CPU Cache */

	/* Enable I-Cache---------------------------------------------------------*/

	SCB_EnableICache();

	/* Enable D-Cache---------------------------------------------------------*/
	SCB_EnableDCache();

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	/*
	 * RECAP OF TIMER USE
	 * TIM2		rotary encoder
	 * TIM3		DC motors PWM (speed) and solenoid (unused)
	 * TIM12	microseconds
	 * TIM15	servo motor PWM (position)
	 */
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C3_Init();
	MX_OCTOSPI2_Init();
	MX_RNG_Init();
	MX_TIM2_Init();
	MX_TIM3_Init();
	MX_TIM12_Init();
	MX_TIM15_Init();
	MX_UART5_Init();
	MX_USB_DEVICE_Init();
	MX_USART3_UART_Init();
	MX_IWDG1_Init();
	/* USER CODE BEGIN 2 */

	/**
	 * @brief Call this in main() as soon as possible AFTER MX_I2C3_Init()
	 * because it needs I2C3 to communicate with the external 47C16 chip
	 */
	EarlyBootloaderCheck();
	// Initialise Status
	return_code_t status = LS_OK;
	return_code_t local_status;

	//Set up LCD
	HAL_GPIO_WritePin(BACKLIT_PWM_GPIO_Port, BACKLIT_PWM_Pin, GPIO_PIN_SET);
	BSP_LCD_Init();
	BSP_LCD_Clear(LCD_COLOR_BCKGND);

	//Initialise menu system
	interface_init();

	// Enable flash memory map
	W25Q64_OSPI_EnableMemoryMappedMode(&hospi2);

	// Start encoder mode on TIM2 (encoder)
	HAL_TIM_Encoder_Start(ENCODER_TIM_CH);
	// Start TIM12 (microseconds)
	HAL_TIM_Base_Start(&MICROSECONDS_TIM);

	// Initialise buttons
	button_init(&encoder_btn, ENT_BTN_GPIO_Port, ENT_BTN_Pin, GPIO_PIN_RESET);
	button_init(&escape_btn, ESC_BTN_GPIO_Port, ESC_BTN_Pin, GPIO_PIN_RESET);

	// Initialise actuators
	DCmotorInit(&tray_motor, 1, TRAY_MOTOR_PIN_1, TRAY_MOTOR_PIN_2,
	TRAY_MOTOR_STDBY_PIN, TRAY_MOTOR_PWM_CH);
	DCmotorInit(&entry_motor, 2, ENTRY_MOTOR_PIN_1, ENTRY_MOTOR_PIN_2,
	ENTRY_MOTOR_STDBY_PIN, ENTRY_MOTOR_PWM_CH);
	DCmotorInit(&latch, 3, SOLENOID_PIN_1, SOLENOID_PIN_2, SOLENOID_STDBY_PIN,
	SOLENOID_PWM_CH);
	flap_init(SERVO_PWM_CH);

	// Activate EERAM AutoStore
	if (EERAM_ActivateAutoStore(&hi2c3) != HAL_OK)
	{
		status = EERAM_ERROR;
		goto _ERROR_CATCH;
	}

	// Check code consistency of games/EERAM
	extern uint16_t n_presets;
	extern uint16_t n_user_prefs;
	extern uint16_t n_menus;
	extern uint16_t n_games;
	if ((n_presets != n_user_prefs) || n_presets != n_games + 1 // +1 is  DEAL_N_CARDS
	|| sizeof(game_rules_t) != E_GAME_RULES_SIZE
			|| sizeof(cc_stage_t) != E_CC_STAGE_SIZE
			|| sizeof(user_prefs_t) != E_USER_PREFS_SIZE || E_N_MENUS < n_menus
			|| E_N_PRESETS < n_presets)
	{
		status = LS_ERROR;
		goto _ERROR_CATCH;
	}

	// Check code consistency of error codes
	extern uint16_t n_non_errors;
	extern uint16_t n_text_errors;
	extern uint16_t n_graphic_errors;
	extern uint16_t n_fatal_errors;
	if (n_non_errors + n_text_errors + n_graphic_errors + n_fatal_errors
			!= TMC2209_ERROR + 1)
	{
		status = LS_ERROR;
		goto _ERROR_CATCH;
	}

	// Prompt animated logo
	prompt_animated_logo();

	// If ESC pressed during logo prompt, wait for buttons after logo
	if (escape_btn.interrupt_press)
	{
		wait_btns();
		reset_btns();
	}

	// Force latch closed
	latch_position = LATCH_OPEN;
	set_latch(LATCH_CLOSED);

	// Initialise carousel
	if ((status = carousel_init()) != LS_OK)
		goto _ERROR_CATCH;

	// Home carousel just after initialisation
	if ((status = home_carousel()) != LS_OK)
		goto _ERROR_CATCH;

	// Check EERAM status and initialise if necessary
	// (will prompt soft reset down the line via DO_RESET
	if ((status = check_eeram()) != LS_OK)
	{
		set_current_menu(ROOT_MENU);
		goto _ERROR_CATCH;
	}

	// Get machine state
	if ((status = read_machine_state()) != LS_OK)
		goto _ERROR_CATCH;

	if (machine_state.reset == DO_RESET)
	{
		if ((status = reset_user_prefs()) != LS_OK
				|| (status = reset_content()) != LS_OK || (status =
						reset_custom_games()) != LS_OK)
			goto _ERROR_CATCH;
	}

	// Read general preferences
	if ((status = read_gen_prefs()) != LS_OK)
		goto _ERROR_CATCH;

	// Get nb cards in
	if ((status = get_n_cards_in()) != LS_OK)
		goto _ERROR_CATCH;

	// Flag start_up
	bool start_up = true;

	// Signal end of sequence
	beep(1);
	// If cards in shoe, ask to pick them up
	wait_pickup_shoe_or_ESC();

	/* USER CODE BEGIN WHILE */
	/* USER CODE END 2 */
	/* Infinite loop */
	while (1)
	{
		watchdog_refresh();

		// Force empty if error content
		if (machine_state.content_error == CONTENT_ERROR)
			if ((status = force_empty(
					"Content error.\nShuffler will be emptied", TIME_LAG))
					!= LS_OK)
				goto _ERROR_CATCH;;

		// Reset homing position if needed
		if (reset_home)
		{
			prompt_message("Resetting carousel...");
			// Home carousel
			if ((local_status = home_carousel()) != LS_OK)
			{
				status = local_status;
				clear_message(TEXT_ERROR);
				goto _ERROR_CATCH;
			}

			clear_message(TEXT_ERROR);

			// Reset flag
			reset_home = false;

			goto _ERROR_CATCH;
		}

		// Initialise n_loaded
		n_loaded = 0;

		// GET GAME STATE
		if ((status = read_game_state()) != LS_OK)
			goto _ERROR_CATCH;

		// Check if last game was interrupted
		// If stuck at END_GAME, or not proper game and interrupted, reset
		if (game_state.current_stage == END_GAME
				|| (!is_proper_game(game_state.game_code)
						&& game_state.current_stage != NO_GAME_RUNNING))
		{
			game_state.current_stage = NO_GAME_RUNNING;
			if ((status = write_game_state()) != LS_OK)
				goto _ERROR_CATCH;
		}
		// Else, if suspended proper game, ask
		else if (game_state.current_stage != NO_GAME_RUNNING)
		{
			// Get label of game_code and prompt user
			menu_t temp_menu;
			return_code_t user_input;
			extern icon_set_t icon_set_check;
			if ((status = set_menu(&temp_menu, game_state.game_code)) != LS_OK)
				goto _ERROR_CATCH;
			snprintf(display_buf, N_DISP_MAX, "%s in progress\nContinue?",
					temp_menu.label);
			clear_text();
			user_input = prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
					icon_set_check, ICON_RED_CROSS, BUTTON_PRESS);
			if (user_input == LS_ESC)
			{
				if ((status = reset_game_state()) != LS_OK)
					goto _ERROR_CATCH;
				if ((status = prompt_menu(DEFAULT_SELECT, ROOT_MENU)) != LS_OK)
					goto _ERROR_CATCH;
			}
			else
			{
				if ((status = set_current_menu(game_state.game_code)) != LS_OK)
					goto _ERROR_CATCH;
				prompt_title(current_menu.label);
			}
		}

		/********************************************
		 * Cycle through menus if no game is running
		 ********************************************/
		if (game_state.current_stage == NO_GAME_RUNNING)
		{

			if (start_up)
			{
				// Prompt root menu
				prompt_menu(DEFAULT_SELECT, ROOT_MENU);
				if (machine_state.content_error == CONTENT_OK)
					prompt_n_cards_in();
				else
					hide_n_cards_in();
				start_up = false;
			}

			if ((status = menu_cycle()) != LS_OK)
				goto _ERROR_CATCH;
			// Update game_state
			game_state.game_code = current_menu.code;

		}

		/********************************************
		 * Exiting menu cycle
		 ********************************************/

		/*
		 * If double deck shuffling was interrupted, and we are attempting an
		 * action - other than shuffling 2 decks or emptying - with more than 1 card
		 * in any slots, empty carousel
		 */

		if ((current_menu.n_items == 0)
				&& (machine_state.double_deck == DOUBLE_DECK_STATE)
				&& (current_menu.code != DOUBLE_DECK_SHUFFLE)
				&& (current_menu.code != EMPTY))
		{
			// Assert if some slots have 2 cards
			bool SDS;
			if ((status = some_double_slots(&SDS)) != LS_OK)
				goto _ERROR_CATCH;
			// If so force empty
			if (SDS)
			{
				if ((status = force_empty(
						"Some slots contain 2 cards.\nShuffler will be emptied",
						TIME_LAG)) != LS_OK)
					goto _ERROR_CATCH;
			}
			// If not, switch to single deck mode
			else
			{
				machine_state.double_deck = SINGLE_DECK_STATE;
				if ((status = write_machine_state()) != LS_OK)
					goto _ERROR_CATCH;
				hide_n_cards_in();

			}
		}

		// If proper game, run game
		if (is_proper_game(game_state.game_code))
		{
			if (context == CONTEXT_SET_PREFS)
			{
				if ((status = set_user_prefs(game_state.game_code)) != LS_OK)
					goto _ERROR_CATCH;
			}
			else if (context == CONTEXT_SET_CUSTOM_GAMES)
			{
				if ((status = set_custom_game_rules(game_state.game_code))
						!= LS_OK)
					goto _ERROR_CATCH;
			}
			else
				while ((status = run_game()) == LS_OK)
					;
		}
		// Else if action menu, find associated action if any
		else if (current_menu.n_items == 0)
			switch ((uint8_t) current_menu.code)
			{
				case RESET_PREFS:
					clear_text();
					extern icon_set_t icon_set_check;
					return_code_t user_input = prompt_interface(WARNING,
							CUSTOM_MESSAGE, "\nReset preferences?",
							icon_set_check, ICON_BACK, BUTTON_PRESS);
					if (user_input == LS_OK)
					{
						if ((status = reset_user_prefs()) != LS_OK)
							goto _ERROR_CATCH;
						clear_message(TEXT_ERROR);
					}
					reset_btns();
					break;

				case RESET_CONTENT:
					clear_text();
					extern icon_set_t icon_set_check;
					user_input = prompt_interface(WARNING, CUSTOM_MESSAGE,
							"\nReset shuffler content?", icon_set_check,
							ICON_BACK, BUTTON_PRESS);
					if (user_input == LS_OK)
					{
						if ((status = reset_content()) != LS_OK)
							goto _ERROR_CATCH;
						clear_message(TEXT_ERROR);
					}
					// Safely close latch
					latch_position = LATCH_OPEN;
					set_latch(LATCH_CLOSED);
					reset_btns();
					break;

				case RESET_CUSTOM_GAMES:
					clear_text();
					extern icon_set_t icon_set_check;
					user_input = prompt_interface(WARNING, CUSTOM_MESSAGE,
							"\nReset custom games?", icon_set_check, ICON_BACK,
							BUTTON_PRESS);
					if (user_input == LS_OK)
					{
						if ((status = reset_custom_games()) != LS_OK)
							goto _ERROR_CATCH;
						clear_message(TEXT_ERROR);
					}
					reset_btns();
					break;

				case SHUFFLE:
					// Shuffle
					if (!cards_in_shoe())
						flap_close();
					game_state.current_stage = SHUFFLING;
					if ((status = write_game_state()) != LS_OK)
						goto _ERROR_CATCH;
					status = shuffle(&n_loaded);
					break;

				case DOUBLE_DECK_SHUFFLE:
					// Shuffle 2 decks
					if ((status = load_double_deck(&n_loaded)) != LS_OK)
						goto _ERROR_CATCH;
					// Safe empty afterwards
					game_state.current_stage = SAFE_EMPTYING;
					if ((status = write_game_state()) != LS_OK)
						goto _ERROR_CATCH;
					clear_text();
					if ((status = discharge_cards(void_game_rules)) != LS_OK)
						goto _ERROR_CATCH;
					// If OK wait for pick-up
					wait_pickup_shoe(0);
					break;

				case LOAD:
					// Close flap
					flap_close();

					// Get machine state
					if ((status = read_machine_state()) != LS_OK)
						break;

					// Select mode: random if long press and no previous sequential operation, seq otherwise
					if (encoder_btn.long_press
							&& machine_state.random_in == RANDOM_STATE)
						sel_rand_mode = RAND_MODE;
					else
						sel_rand_mode = SEQ_MODE;

					// Load cards and prompt if OK
					if ((status = load_carousel(sel_rand_mode, &n_loaded))
							== LS_OK)
					{
						snprintf(display_buf, N_DISP_MAX, "Loaded %d card%s",
								n_loaded, n_loaded > 1 ? "s" : "");
						extern icon_set_t icon_set_void;
						prompt_interface(MESSAGE, CUSTOM_MESSAGE, display_buf,
								icon_set_void, ICON_VOID, TIME_LAG);
					}
					break;

				case DEAL_N_CARDS:
					status = deal_n_cards();
					break;

				case SHUFFLERS_CHOICE:
					status = shufflers_choice();
					break;

				case EMPTY:

					// Update game_state
					game_state.game_code = EMPTY;

					// Set up safe random mode (PERMANENT safe + seq, other seq)
					if (encoder_btn.permanent_press)
						game_state.current_stage = SAFE_EMPTYING;
					else
						game_state.current_stage = EMPTYING;

					if ((status = write_game_state()) != LS_OK)
						goto _ERROR_CATCH;

					// Close flap (option)
					wait_pickup_shoe_or_ESC();

					// Empty carousel
					if ((status = discharge_cards(void_game_rules))
							!= CARD_STUCK_ON_EXIT)
						set_latch(LATCH_CLOSED);

					// Update game_state
					game_state.current_stage = NO_GAME_RUNNING;
					if ((local_status = write_game_state()) != LS_OK)
						status = local_status;
					break;

				case ADJUST_DEAL_GAP:
					status = adjust_deal_gap();
					break;

				case SET_CUT_CARD_FLAG:
					status = set_cut_card_flag();
					break;

				case ROTATE_TRAY_ROLLER:
					clean_roller(tray_motor);
					break;

				case ROTATE_ENTRY_ROLLER:
					clean_roller(entry_motor);
					break;

				case DC_MOTORS_RUN_IN:
					dc_motors_run_in();
					break;

				case TEST_CAROUSEL:
					status = test_carousel_motor();
					break;

				case TEST_CAROUSEL_DRIVER:
					test_carousel_driver();
					break;

				case TEST_EXIT_LATCH:
					test_exit_latch();
					break;

				case ACCESS_EXIT_CHUTE:
					access_exit_chute();
					break;

				case ADJUST_CARD_FLAP:
					status = adjust_card_flap_full();
					break;

				case ADJUST_CARD_FLAP_LIMITED:
					status = adjust_card_flap_limited();
					break;

				case TEST_BUZZER:
					test_buzzer();
					break;

				case DISPLAY_SENSORS:
					display_sensors();
					break;

				case ABOUT:
					status = prompt_about();
					break;

				case IMAGE_UTILITY:
					carousel_disable();
					image_utility();
					carousel_enable();
					status = LS_OK;
					break;

				case FIRMWARE_UPDATE:
					// General warning with current version
					clear_text();
					extern icon_set_t icon_set_check;
					char fw_warning[80];
					snprintf(fw_warning, sizeof(fw_warning),
							"Current version: %s\nWill be erased. Continue?",
							GetFirmwareVersionString());
					user_input = prompt_interface(WARNING, CUSTOM_MESSAGE,
							fw_warning,
							icon_set_check, ICON_CROSS, BUTTON_PRESS);
					if (user_input == LS_OK)
					{
						// Check safety code
						const uint16_t code_len = 5;
						char safety_code[] = "AKQJT";
						char code_input[] =  "@@@@@";
						int16_t m_row = 1;

						clear_message(TEXT_ERROR);
						prompt_text("Enter safety code:", m_row, LCD_BOLD_FONT);
						name_input(code_input, code_len);
						if (strcmp(code_input, safety_code) == 0)
							EnterBootloaderMode();
					}
					reset_btns();
					break;

				case TEST_IMAGES:
					show_images();
					status = LS_OK;
					break;

				case TEST_WATCHDOG:
					test_watchdog();
					status = LS_OK;
					break;

					// _menu with no sub-menus and no affected action (yet)
				default:
					status = INVALID_CHOICE;
			}

		// ERROR CATCH and reset screen
		_ERROR_CATCH:

		bool is_standard_error(return_code_t return_code)
		{
			bool found = false;
			extern return_code_t standard_errors[];
			extern const uint16_t n_standard_errors;
			for (uint16_t i = 0; i < n_standard_errors; i++)
			{
				if (return_code == standard_errors[i])
				{
					found = true;
					break;
				}
			}

			return found;
		}

		// If all OK and action menu (no sub-menus) return to calling menu
		if (status == LS_OK)
		{
			if (current_menu.n_items == 0)
				if ((status = prompt_calling_menu()) != LS_OK)
					goto _ERROR_CATCH;
		}
		// Manage standard errors (display standard message)
		else if (is_standard_error(status))
		{
			extern icon_set_t icon_set_check;
			prompt_interface(ALERT, status, "", icon_set_check, ICON_VOID,
					BUTTON_PRESS);
			if (current_menu.code != ROOT_MENU)
			{
				if ((status = prompt_calling_menu()) != LS_OK)
					goto _ERROR_CATCH;
			}
			else
				prompt_menu(DEFAULT_SELECT, ROOT_MENU);
		}
		// Manage non-standard errors
		else
			switch (status)
			{
				case LS_ESC:
					// Go back to calling menu, inactive if in root menu
					if ((status = prompt_calling_menu()) != LS_OK)
						goto _ERROR_CATCH;
					break;

				case DO_EMPTY:
					status = force_empty("Empty Shuffler?", BUTTON_PRESS);
					start_up = true;
					break;

				case SLOT_IS_EMPTY:
					machine_state.content_error = CONTENT_ERROR;
					if ((local_status = write_machine_state()) != LS_OK)
					{
						status = local_status;
						goto _ERROR_CATCH;
					}

					reset_home = true;
					break;

				case NO_FAVORITES_SELECTED:
					extern icon_set_t icon_set_check;
					prompt_interface(ALERT, status, "", icon_set_check,
							ICON_VOID, BUTTON_PRESS);
					set_param_sub_menu(SETTINGS, SET_FAVORITES);
					prompt_menu(FORCE_MENU, SETTINGS);

					break;

				case NO_DEALERS_CHOICE_SELECTED:
					extern icon_set_t icon_set_check;
					prompt_interface(ALERT, status, "", icon_set_check,
							ICON_VOID, BUTTON_PRESS);
					set_param_sub_menu(SETTINGS, SET_DEALERS_CHOICE);
					prompt_menu(FORCE_MENU, SETTINGS);

					break;

				default:
					if (neutralise_errors)
					{
						extern icon_set_t icon_set_check;
						prompt_interface(ALERT, status, "", icon_set_check,
								ICON_VOID, BUTTON_PRESS);
						BSP_LCD_Clear(LCD_COLOR_BCKGND);
						if (current_menu.code != ROOT_MENU)
						{
							if ((status = prompt_calling_menu()) != LS_OK)
								goto _ERROR_CATCH;
						}
						else
						{
							prompt_current_title();
							prompt_menu(DEFAULT_SELECT, ROOT_MENU);
						}
					}
					else
					{

						// All other errors cause termination
						LS_error_handler(status);
					}
			}

		status = LS_OK;
		reset_btns();

		if (machine_state.content_error == CONTENT_OK)
			prompt_n_cards_in();
		else
			hide_n_cards_in();

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct =
	{ 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct =
	{ 0 };

	/** Supply configuration update enable
	 */
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
	{
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48
			| RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = 64;
	RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 12;
	RCC_OscInitStruct.PLL.PLLP = 3;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1
			| RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

int _write(int file, char *ptr, int len)
{
	CDC_Transmit_HS((uint8_t*) ptr, len);
	return (len);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

	if (GPIO_Pin == ESC_BTN_Pin)
		escape_btn.interrupt_press = true;
	else if (GPIO_Pin == ENT_BTN_Pin)
		encoder_btn.interrupt_press = true;
//	else if (GPIO_Pin == STP_DIAG_Pin)
//		stallFlag = true;

	return;
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
		;
	/* USER CODE END Error_Handler_Debug */
}

/**
 * @brief Enter bootloader mode for firmware update
 * Sets RTC backup register flag and resets.
 * Bootloader will erase flash before initializing USB.
 */
void EnterBootloaderMode(void)
{
	RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
	__DSB();
	PWR->CR1 |= PWR_CR1_DBP;
	while ((PWR->CR1 & PWR_CR1_DBP) == 0)
	{
	}
	RTC->BKP0R = BOOTLOADER_MAGIC_VALUE;
	NVIC_SystemReset();
}
