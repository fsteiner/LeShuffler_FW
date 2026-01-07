/*
 * Definitions.h
 *
 *  Version 1.24
 *  Released to factory 26/11/2025
 *
 *  1)	Changes in roller friction test: duration, staggered cold start @80%
 *  2)	Corrected a bug on 2-deck shuffle. Double-deck state was reset to SINGLE
 *  	by prompt_n_cards_in and warn_n_cards_in if empty carousel (and empty tray),
 *  	leading to unexpected consequences.
 *  	Stopped these 2 functions from resetting double-deck state to 0 when
 *  	shuffler empty.
 *  	get_n_cards_in still does.
 *  3)	Removed initial rollers run-in
 *  4)	Change in TMC_2209 set up, manages power consumption
 *  		- hopefully solves power issues
 *  		- maybe helps multiple card loads seen in factory
*  	5)	Tentative code for USB bootload doesn't work
*  	6)	Simplified test menu, some functions to be removed
*  	7)	Debounced all sensors (see readme)
 * 
 */

#ifndef DECLARATIONS_H_
#define DECLARATIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

// EERAM status
#define FW_LS_TESTED 	0xABBA
#define FW_LS_V1_00  	0xA100 // New format

// Bootloader parameters
#define BOOTLOADER_MAGIC_ADDR  0x24000000  // Start of SRAM1
#define BOOTLOADER_MAGIC_VALUE 0xDEADBEEF

/* Timer allocation */
// Encoder:		htim2 (encoder mode)
#define ENCODER_TIM_CH			&htim2, TIM_CHANNEL_ALL
// Servo motor:	htim15 CH1 50 Hz(pwm mode)
#define SERVO_PWM_CH			&htim15, TIM_CHANNEL_1
// µs:			htim12	1MHZ used for stepper motor velocity through STEP
#define MICROSECONDS_TIM		htim12
// Carousel release (interrupt):
#define CRSL_RLS_TIM			htim14
// TB6612:	htim3 pwm Mode 10kHZ CH1 (solenoid) CH2 (tray), and CH3 (entry)
#define TB6612_TIM				htim3

// Pin allocation
#define ENTRY_SENSOR_1_PIN  	ENTRY_SENSOR_1_GPIO_Port, ENTRY_SENSOR_1_Pin
#define ENTRY_SENSOR_2_PIN 		ENTRY_SENSOR_2_GPIO_Port, ENTRY_SENSOR_2_Pin
#define ENTRY_SENSOR_3_PIN  	ENTRY_SENSOR_3_GPIO_Port, ENTRY_SENSOR_3_Pin
#define SHOE_SENSOR_PIN		  	SHOE_SENSOR_GPIO_Port, SHOE_SENSOR_Pin
#define EXIT_SENSOR_PIN 		EXIT_SENSOR_GPIO_Port, EXIT_SENSOR_Pin
#define TRAY_SENSOR_PIN 		TRAY_SENSOR_GPIO_Port, TRAY_SENSOR_Pin
#define HOMING_SENSOR_PIN    	HE_IN_GPIO_Port, HE_IN_Pin
#define SLOT_SENSOR_PIN 		IR_IN_GPIO_Port, IR_IN_Pin

#define TRAY_MOTOR_PIN_1		MTR_DRV1_AIN1_GPIO_Port, MTR_DRV1_AIN1_Pin
#define TRAY_MOTOR_PIN_2		MTR_DRV1_AIN2_GPIO_Port, MTR_DRV1_AIN2_Pin
#define TRAY_MOTOR_STDBY_PIN	MTR_DRV1_STBY_GPIO_Port, MTR_DRV1_STBY_Pin
#define TRAY_MOTOR_PWM_PIN		MTR_DRV1_PWMA_GPIO_Port, MTR_DRV1_PWMA_Pin
#define TRAY_MOTOR_PWM_CH		&TB6612_TIM, TIM_CHANNEL_2

#define ENTRY_MOTOR_PIN_1		MTR_DRV2_AIN1_GPIO_Port, MTR_DRV2_AIN1_Pin
#define ENTRY_MOTOR_PIN_2		MTR_DRV2_AIN2_GPIO_Port, MTR_DRV2_AIN2_Pin
#define ENTRY_MOTOR_STDBY_PIN	MTR_DRV2_STBY_GPIO_Port, MTR_DRV2_STBY_Pin
#define ENTRY_MOTOR_PWM_PIN		MTR_DRV2_PWMA_GPIO_Port, MTR_DRV2_PWMA_Pin
#define ENTRY_MOTOR_PWM_CH		&TB6612_TIM, TIM_CHANNEL_3

#define SOLENOID_PIN_1			MTR_DRV1_BIN1_GPIO_Port, MTR_DRV1_BIN1_Pin
#define SOLENOID_PIN_2			MTR_DRV1_BIN2_GPIO_Port, MTR_DRV1_BIN2_Pin
#define SOLENOID_STDBY_PIN		MTR_DRV1_STBY_GPIO_Port, MTR_DRV1_STBY_Pin
#define SOLENOID_PWM_PIN		MTR_DRV1_PWMB_GPIO_Port, MTR_DRV1_PWMB_Pin
#define SOLENOID_PWM_CH			&TB6612_TIM, TIM_CHANNEL_1

#define ESC_BTN_PIN				ESC_BTN_GPIO_Port, ESC_BTN_Pin
#define ENT_BTN_PIN				ENT_BTN_GPIO_Port, ENT_BTN_Pin

#define STP_ENN_PIN				STP_ENN_GPIO_Port, STP_ENN_Pin
#define STP_MS1_PIN				STP_MS1_GPIO_Port, STP_MS1_Pin
#define STP_MS2_PIN				STP_MS2_GPIO_Port, STP_MS2_Pin
#define STP_DIR_PIN				STP_DIR_GPIO_Port, STP_DIR_Pin
#define STP_STDBY_PIN			STP_STDBY_GPIO_Port, STP_STDBY_Pin

#define BUZZER_PIN				BUZZER_GPIO_Port, BUZZER_Pin

// Non-portable pins of entry sensors 1 and 3 to be read via common port GPIOB->IDR (pins 1 and 11)
#define ENTRY_1_3_PINS			0x0802 // 0b 0000 1000 0000 0010
#define CARD_SEEN_1_OR_3		((GPIOB->IDR & ENTRY_1_3_PINS) != 0)

/* Timer allocation */
// Encoder:		htim2 (encoder mode)
#define ENCODER_TIM_CH			&htim2, TIM_CHANNEL_ALL
// Servo motor:	htim15 CH1 50 Hz(pwm mode)
#define SERVO_PWM_CH			&htim15, TIM_CHANNEL_1
// µs:			htim12	1MHZ used for stepper motor velocity through STEP
#define MICROSECONDS_TIM		htim12
// TB6612:		htim3 pwm Mode 10kHZ CH1 (solenoid) CH2 (tray), and CH3 (entry)
#define TB6612_TIM				htim3

// ENCODER
#define ENCODER_REFERENCE   	TIM2->CNT
#define ENCODER_POSITION	    ((int16_t) ENCODER_REFERENCE)
#define MICRO_SECONDS		    TIM12->CNT

// Shuffler parameters
#define S_WAIT_DELAY			400UL
#define M_WAIT_DELAY			700UL
#define L_WAIT_DELAY		    1000UL
#define SHORT_BEEP				5UL
#define MEDIUM_BEEP				200UL
#define LONG_BEEP				500UL
#define SHOE_DELAY				1000UL
#define DEFAULT_DEAL_GAP_INDEX 	8
#define DEAL_GAP_MULTIPLIER		100
#define DEAL_GAP				1
#define NO_DEAL_GAP				0
#define DEFAULT_CUT_CARD_FLAG	false


// Stepper
#define SEC_PER_MINUTE			60UL
#define MS_FACTOR 			    64UL
#define STEPS_PER_SLOT 	    	10
#define N_SLOTS 				54
#define CRSL_RESOLUTION         (N_SLOTS * STEPS_PER_SLOT)
#define IN_OFFSET 				(-6)
#define OPTICAL_OFFSET	     	0     // Discrepancy between start of light and alignment in full steps (if <= 1, still in light)

/* CAROUSEL SPEED CALCULATIONS
 * T [µs/µsteps] = US_PER_STEP_AT_1_RPM/MS_FACTOR/RPM
 * T [µs/µsteps] = 6 10^7 [µs/mn]/(MS_F [µsteps/step]*200 [steps/turn]*RPM [turns/mn]))
 * T = 3 10^5/MS_FACTOR/RPM
 */
#define US_PER_STEP_AT_1_RPM 	(1000000UL/CRSL_RESOLUTION*SEC_PER_MINUTE)

#define HOMING_SLOW_SPEED       2.2     // CRSL RPM
#define HOMING_SPEED			33.3    // CRSL RPM
#define CRSL_SLOW_SPEED			6.0 	// 7.0 fails on prod model, occasional failure at 6.5    // 5.9  	// CRSL RPM
#define CRSL_FAST_SPEED			90.0 	// 90.0 used to work but now ≥ 75.0 alignment 100% 72.0 OK// CRSL RPM
#define DDECK_SLOW_SPEED		(CRSL_SLOW_SPEED)
#define DDECK_MAX_SPEED         (CRSL_FAST_SPEED*0.75)//60.0 	// 40.0     // CRSL RPM
#define STP_OVERHEAD			1.4     // µs

#define HOM_ACCEL_ZONE          (20*STEPS_PER_SLOT/10)  // steps
#define HOM_DECEL_ZONE          (5*STEPS_PER_SLOT/10)  // steps HOMING ZONE is 8, initial alignment 4 (may vary)
#define HOM_DECEL_COEF          (0.999 * pow((float)HOMING_SLOW_SPEED/HOMING_SPEED, 1.0/HOM_DECEL_ZONE))
#define HOM_ACCEL_COEF          (1.001 * pow((float)HOMING_SPEED/HOMING_SLOW_SPEED, 1.0/ACCEL_ZONE ))

#define WATCH_ZONE		        (4*STEPS_PER_SLOT/10)	// Leave space to see light, 230 µsteps observed
#define TOLERANCE 		        (3*STEPS_PER_SLOT/10)

#define CRSL_FWD				0
#define CRSL_BWD				1
#define ACCELERATE				1
#define DECELERATE				0

#define CRSL_POS_WITH_OFFSET(x)         ((carousel_pos + (x == FULL_SLOT ?  N_SLOTS + IN_OFFSET : 0)) % N_SLOTS)

#define HOMING_POS              0      // If physical label to keep track of position

// Buttons
#define DEBOUNCE_TIME			15UL
#define LONG_PRESS_TIME			700UL
#define PERM_PRESS_TIME     	1500UL

// DC motors and solenoid
#define SOLENOID_IMPULSE_OPEN	25UL    // stalls at 10-100%, 15-85% works, wobbly at 25-70%
#define SOLENOID_PCT_OPEN		80      // % load (PWM)
#define SOLENOID_IMPULSE_CLOSE	25UL //30 35   // 65% absolute minimum, then 25 minimum
#define SOLENOID_PCT_CLOSE	    65      // % load (PWM)


#define TRAY_MOTOR_SPEED		40                          // 30 % load (PWM)
#define ENTRY_MOTOR_MIN_SPEED	(TRAY_MOTOR_SPEED + 5)      // 30
#define ENTRY_MOTOR_MAX_SPEED	(ENTRY_MOTOR_MIN_SPEED + 5) // 35 % load (PWM)
#define TRAY_PRIMING_TIME		500UL   // ms

// Flap
#define FLAP_DELAY		  		1000UL
#define SERVO_MIN	  	   		200UL
#define SERVO_MAX	  	  		1300UL
#define FLAP_CLOSED_DEFAULT		900UL
#define FLAP_OPEN_DEFAULT		450UL
#define FLAP_CLOSED_TO_MID		140UL
#define FLAP_FACTOR				10UL

// Screen dimensions
#define ENCODER_Y               50
#define ESCAPE_Y                230

// Logical flags
#define	DO_RESET				0       // Default if memory not initialised
#define DONT_RESET				1
#define CAROUSEL_HOMED	        0       // Hall Sensor reads 0 when influenced by a magnet, i.e. when aligned
#define SLOT_DARK		     	1       // Slot sensor reads 1 when dark
#define CARD_SEEN 				0
#define CARD_CLEAR				1
#define CLK_WISE				1
#define C_CLK_WISE			    0
#define DC_MTR_FWD				1
#define DC_MTR_BWD				0
#define LATCH_OPEN 				1
#define LATCH_CLOSED		 	0
#define RANDOM_STATE            1
#define SEQUENTIAL_STATE        0
#define DOUBLE_DECK_STATE       1
#define SINGLE_DECK_STATE       0
#define GAME_IN_PROGRESS        0
#define GAME_ENDED              1
#define CUT_STATE               1
#define NOT_CUT_STATE           0
#define CONTENT_ERROR           1
#define CONTENT_OK          	0

/*
 * EERAM parameters
 */
// General
#define BYTE					8		// EERAM storage unit
#define E_PTR_SIZE				4	    // 32-bit address
#define E_USER_PREFS_SIZE 		3      	// 3 bytes = sizeof(user_prefs_t)
#define E_N_MENUS			  	80 	    // must be ≥ n_menus
#define E_N_PRESETS			  	50 	    // must be ≥ n_presets
// Game parameters
#define N_DEFAULT_DECK 			52
#define N_DEFAULT_DOUBLE_DECK 	108
#define MAX_CC_STAGES 			5
#define N_CUSTOM_GAMES 			5
#define GAME_NAME_MAX_CHAR		16
#define STAGE_NAME_MAX_CHAR 	16
// Implied sizes
#define E_CC_STAGE_SIZE 		(STAGE_NAME_MAX_CHAR + 2)					// add terminaison + n_cards
#define E_GAME_RULES_SIZE 		(8 + E_CC_STAGE_SIZE * MAX_CC_STAGES)		// 56 bytes = sizeof(game_rules_t)
//Storage sizes
#define E_STATUS_SIZE	    	2		// 2 byte code (or string)
#define E_TEST_COUNTER_SIZE     1
#define E_UID_SIZE        		12
#define E_TEST_RESULTS_SIZE     3
#define	E_FLASH_OFFSET_SIZE		E_PTR_SIZE
#define E_MAIN_SERIAL_SIZE      16
#define E_LCD_SERIAL_SIZE       16
#define E_CARDS_TALLY_SIZE		4
#define E_LCD_SCROLL_SIZE		E_N_MENUS    // 1 byte per menu
#define E_LCD_ROW_SIZE			E_N_MENUS	// 1 1 byte per menu
#define E_CLL_MEN_SIZE  		E_N_MENUS	// 1 1 byte per menu(calling menu)
#define E_FAVS_SIZE             ((E_N_PRESETS-1)/BYTE + 1)   // 1 bit per preset rounded up
#define E_DLRS_SIZE			   	2		// 1 bit per game, based on items in poker extended list (14)
#define E_CRSL_SIZE  			8       // map uint64_t = 64 bits ≥ 54
#define E_N_DECK_SIZE			1		// default deck
#define E_MCHN_STATE_SIZE	    1		// stores machine_state_t variable
#define E_FLAP_OPEN_SIZE		1		// 0-255 (to be x 10)
#define E_FLAP_CLOSED_SIZE		1		// 0-255 (to be x 10)
#define E_N_DOUBLE_DECK_SIZE    1       // 0-255 (108 max)
#define E_GAME_STATE_SIZE       4       // stores game_state global variable
#define E_PREFS_STORAGE_SIZE    (E_N_PRESETS * E_USER_PREFS_SIZE)    //  3 bytes per menu
#define E_CUST_GAMES_NM_SIZE	(N_CUSTOM_GAMES * (GAME_NAME_MAX_CHAR + 1))
#define E_CUST_GAMES_RLS_SIZE	(N_CUSTOM_GAMES * E_GAME_RULES_SIZE)
#define E_BOOTLOADER_FLAG_SIZE 	4
#define E_GEN_PREFS_SIZE 		1





// Addresses allocated to permanent variables
#define EERAM_BASE			     0x0000	   		// capacity 2048 going from 0x000 to 0x7FF
#define EERAM_STATUS	 	     EERAM_BASE
#define EERAM_TEST_COUNTER      (EERAM_STATUS + E_STATUS_SIZE)
#define EERAM_UID			    (EERAM_TEST_COUNTER + E_TEST_COUNTER_SIZE)
#define EERAM_TEST_RESULTS      (EERAM_UID + E_UID_SIZE)
#define EERAM_FLASH_OFFSET 	    (EERAM_TEST_RESULTS + E_TEST_RESULTS_SIZE)
#define EERAM_MAIN_SERIAL       (EERAM_FLASH_OFFSET + E_FLASH_OFFSET_SIZE)
#define EERAM_LCD_SERIAL        (EERAM_MAIN_SERIAL + E_MAIN_SERIAL_SIZE)
#define EERAM_CARDS_TALLY	    (EERAM_LCD_SERIAL + E_LCD_SERIAL_SIZE)
#define EERAM_FLAP_OPEN         (EERAM_CARDS_TALLY + E_CARDS_TALLY_SIZE)
#define EERAM_FLAP_CLOSED       (EERAM_FLAP_OPEN + E_FLAP_OPEN_SIZE)
#define EERAM_LCD_SCROLL	    (EERAM_FLAP_CLOSED + E_FLAP_CLOSED_SIZE)
#define EERAM_LCD_ROW		    (EERAM_LCD_SCROLL + E_LCD_SCROLL_SIZE)
#define EERAM_CLL_MEN		    (EERAM_LCD_ROW + E_LCD_ROW_SIZE)
#define EERAM_FAVS			    (EERAM_CLL_MEN + E_CLL_MEN_SIZE)
#define EERAM_DLRS			    (EERAM_FAVS + E_FAVS_SIZE)
#define EERAM_CRSL		    	(EERAM_DLRS + E_DLRS_SIZE)
#define EERAM_CRSL_2		    (EERAM_CRSL + E_CRSL_SIZE)
#define EERAM_N_DECK			(EERAM_CRSL_2 + E_CRSL_SIZE)
#define EERAM_N_DOUBLE_DECK 	(EERAM_N_DECK + E_N_DECK_SIZE)
#define EERAM_MCHN_STATE		(EERAM_N_DOUBLE_DECK + E_MCHN_STATE_SIZE)
#define EERAM_GAME_STATE        (EERAM_MCHN_STATE + E_N_DOUBLE_DECK_SIZE)
#define EERAM_USER_PREFS        (EERAM_GAME_STATE + E_GAME_STATE_SIZE)
#define EERAM_CUST_GAMES_NM     (EERAM_USER_PREFS + E_PREFS_STORAGE_SIZE)
#define EERAM_CUST_GAMES_RLS    (EERAM_CUST_GAMES_NM + E_CUST_GAMES_NM_SIZE)
#define EERAM_BOOTLOADER_FLAG   (EERAM_CUST_GAMES_RLS + E_CUST_GAMES_RLS_SIZE)
#define EERAM_GEN_PREFS        	(EERAM_BOOTLOADER_FLAG + E_BOOTLOADER_FLAG_SIZE)
#define EERAM_FIRST_FREE        (EERAM_GEN_PREFS + E_GEN_PREFS_SIZE)


// Various
#define max(a, b)               (((a) > (b)) ? (a) : (b))
#define min(a, b)               (((a) < (b)) ? (a) : (b))
#define SAFE_EXEC(x)            if ((ret_val = x)) != LS_OK) goto _EXIT;
#define N_DISP_MAX              80

#ifdef __cplusplus
}
#endif

#endif /* DECLARATIONS_H_ */
