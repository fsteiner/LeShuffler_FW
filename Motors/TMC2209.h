/*
 * TCM2209_Stepper_motor.h
 *
 *  Created on: May 31, 2024
 *      Author: Radhika S
 */

#ifndef TCM2209_STEPPER_MOTOR_H_
#define TCM2209_STEPPER_MOTOR_H_

#include "stm32h7xx_hal.h"
#include "stdio.h"
#include "stm32h7xx_hal_def.h"
#include "string.h"
#include <stdbool.h>
#include <utilities.h>

/* Private defines -----------------------------------------------------------*/

#define BYTE_MAX_VALUE 0xFF
#define BITS_PER_BYTE  8

#define READ_REQUEST_DATAGRAM_SIZE 		4
#define WRITE_READ_REPLY_DATAGRAM_SIZE 	8
#define DATA_SIZE                      	4

// Communication codes
#define SYNC                      	 0x05
#define RW_READ                   	  0b0
#define RW_WRITE                  	  0b1
#define READ_REPLY_SERIAL_ADDRESS 	 0xFF

// Addresses
#define ADDRESS_GCONF 		0x00
#define ADDRESS_GSTAT 		0x01
#define ADDRESS_IFCNT 		0x02
#define ADDRESS_NODECONF 	0x03
#define ADDRESS_IOIN 		0x06
#define ADDRESS_IHOLD_IRUN 	0x10
#define ADDRESS_TPOWERDOWN 	0x11
#define ADDRESS_TSTEP 		0x12
#define ADDRESS_TPWMTHRS 	0x13
#define ADDRESS_VACTUAL     0x22
#define ADDRESS_TCOOLTHRS 	0x14
#define ADDRESS_SGTHRS    	0x40
#define ADDRESS_SG_RESULT 	0x41
#define ADDRESS_COOLCONF    0x42
#define ADDRESS_MSCNT    	0x6A
#define ADDRESS_MSCURACT 	0x6B
#define ADDRESS_CHOPCONF    0x6C
#define ADDRESS_DRV_STATUS	0x6F
#define ADDRESS_PWMCONF		0x70
#define ADDRESS_PWM_SCALE	0x71
#define ADDRESS_PWM_AUTO	0x72

// Register defaults
// GCONF							0x1C0 see below
#define TEST_MODE_DEFAULT			0b0			// Normal operation - ALWAYS 0
#define MULTISTEP_FILT_DEFAULT		0b1			// Optimisation enabled (default)
#define MSTEP_REG_SELECT_DEFAULT	0b1			// 0: pins 1: MRES register
#define PDN_DISABLE_DEFAULT			0b1			// 0: UART controls standstill current 1: when using UART interface
#define INDEX_STEP_DEFAULT			0b0			// 0: output as per index_otpw 1: internal step pulses
#define INDEX_OTPW_DEFAULT			0b0			// 0: first µstep position 1: temperature warning
#define SHAFT_DEFAULT				0b0			// 0: normal 1: invert
#define EN_SPREAD_CYCLE_DEFAULT		0b0			// 0: StealthChop enabled 1: SpreadCycle enabled
#define INTERNAL_RSENSE_DEFAULT		0b0			// 0: external sense resistors 1: internal => ALWAYS 0
#define I_SCALE_ANALOG_DEFAULT		0b0			// 0: internal reference 1:VREF (default)

// NODECONF
#define NODECONF_DEFAULT			0x0			// SENDDELAY x bit times

/*---------------------------*/

// IHOLD_IRUN						0xF1900 see below
#define IHOLDDELAY_DEFAULT  		0b1111		// 15 x 2^18 clock cycles (22 x 15 = 330 ms) for ramp down fm IRUN to IHOLD
#define IRUN_DEFAULT     			0b11001		// (25 + 1)/32
#define IHOLD_DEFAULT       		0b00000		// (0 + 1)/32


/*---------------------------*/

#define TPOWERDOWN_DEFAULT			0x2E   		// up to 255 x 2^18 clock cycles (5.6s) wait after standstill, default 20 (440 mS), set to 1s
#define TPWMTHRS_DEFAULT 			0x00000		// 0: SpreadCycle disabled
#define VACTUAL_DEFAULT				0x000000	// STEP DIR operation
// Minimum threshold for StallGuard
#define RPM_MIN_STALL_GUARD			500			// Deactivated
// TSTEP = fCLK/(RPM/60 x nStepsPerTurn)/MS_FACTOR) = 36 x 10^5/MS_FACTOR/RPM for fCLK 12 MHz
#define TCOOLTHRS_DEFAULT			(3600000UL/MS_FACTOR/RPM_MIN_STALL_GUARD)
#define SGTHRS_DEFAULT				0x00		// Deactivated
#define COOLCONF_DEFAULT    		0x0000		// OFF

/*---------------------------*/
// Micro-step resolution
#define MRES_256             0b0000
#define MRES_128             0b0001
#define MRES_064             0b0010
#define MRES_032             0b0011
#define MRES_016             0b0100
#define MRES_008             0b0101
#define MRES_004             0b0110
#define MRES_002             0b0111
#define MRES_001             0b1000

// CHOPCONF 				0x1[MRES]000053
#define DISS2VS_DEFAULT		0b0 			// 0: short protection low side ON 	1: OFF
#define DISS2G_DEFAULT		0b0 			// 0: short protection to GND ON 	1: OFF
#define DEGDE_DEFAULT		0b0 			// 0: double edge OFF 				1: ON
#define INTPOL_DEFAULT		0b1 			// 0: µsteps interpol to 256 OFF	1: ON (DEFAULT)
// [MRES]
#define VSENSE_DEFAULT		0b0 			// 0: low sensitivity, high voltage	1: hi sens. low sense resistor voltage
#define TBL_DEFAULT      	0b00			// 0b00 or 0b01 recommended
#define HEND_DEFAULT        0b0000			// 0 in StealthChop mode
#define HSTRT_DEFAULT		0b101			// default 5 in StealthChop mode
#define TOFF_DEFAULT      	0b011   		// default 3 in StealthChop mode
/*---------------------------*/

// PWMCONF_DEFAULT 	0xC82F----
#define PWM_LIM_DEFAULT		0xC		// default 12
#define PWM_REG_DEFAULT		0x8		// default 8
#define FREEWHEEL_DEFAULT	TMC_STRONG_BRAKING // 0b10
#define PWM_AUTOGRAD_DEFAULT  0b1	// 0: fixed value	1: automatic tuning with autoscale = 1
#define PWM_AUTOSCALE_DEFAULT 0b1	// 0: user defined	1: automatic (default)
#define PWM_FREQ_DEFAULT	0b11	// 00: 2/1024 01: 2/683 10: 2/512 11: 2/410 FClk
#define PWM_GRAD_DEFAULT	0x0E	// calibrated based on test IRUN = 20 MRES = 256 14 @64 MRES with VREF
#define PWM_OFS_DEFAULT		0x8E	// calibrated based on test, 36 is default Ox2F

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
// 4.1.1 Write Access
typedef union
{
  struct
  {
    uint64_t sync : 4;
    uint64_t reserved : 4;
    uint64_t serial_address : 8;
    uint64_t register_address : 7;
    uint64_t rw : 1;
    uint64_t data : 32;
    uint64_t crc : 8;
  };
  uint64_t bytes;
} write_read_reply_datagram_t;

// 4.1.2 Read Access
typedef union
{
  struct
  {
    uint32_t sync : 4;
    uint32_t reserved : 4;
    uint32_t serial_address : 8;
    uint32_t register_address : 7;
    uint32_t rw : 1;
    uint32_t crc : 8;
  };
  uint32_t bytes;
} read_request_datagram_t;

// 5.1 General Registers GCONF
typedef union
{
  struct
  {
    uint32_t i_scale_analog : 1;
    uint32_t internal_rsense : 1;
    uint32_t en_spread_cycle : 1;
    uint32_t shaft : 1;
    uint32_t index_otpw : 1;
    uint32_t index_step : 1;
    uint32_t pdn_disable : 1;
    uint32_t mstep_reg_select : 1;
    uint32_t multistep_filt : 1;
    uint32_t test_mode : 1;
    uint32_t reserved : 22;
  };
  uint32_t bytes;
} tmc2209_gconf_t;

// 5.1 General Registers NODECONF
typedef union
{
  struct
  {
    uint32_t reserved_0 : 8;
    uint32_t senddelay : 4;
    uint32_t reserved_1 : 20;
  };
  uint32_t bytes;
} tmc2209_node_conf_t;

// 5.1 General Registers IOIN (read only pin status and version)
typedef union
{
  struct
  {
    uint32_t enn : 1;
    uint32_t reserved_0 : 1;
    uint32_t ms1 : 1;
    uint32_t ms2 : 1;
    uint32_t diag : 1;
    uint32_t reserved_1 : 1;
    uint32_t pdn_uart : 1;
    uint32_t step : 1;
    uint32_t spread_en : 1;
    uint32_t dir : 1;
    uint32_t reserved_2 : 14;
    uint32_t version : 8;
  };
  uint32_t bytes;
} tmc2209_input_t;

// 5.2 Velocity Dependent Control IHOLD_IRUN
typedef union
{
  struct
  {
    uint32_t ihold : 5;
    uint32_t reserved_0 : 3;
    uint32_t irun : 5;
    uint32_t reserved_1 : 3;
    uint32_t iholddelay : 4;
    uint32_t reserved_2 : 12;
  };
  uint32_t bytes;
} tmc2209_ihold_irun_t;

// 5.3.1 COOLCONF
typedef union
{
  struct
  {
    uint32_t semin : 4;
    uint32_t reserved_0 : 1;
    uint32_t seup : 2;
    uint32_t reserved_1 : 1;
    uint32_t semax : 4;
    uint32_t reserved_2 : 1;
    uint32_t sedn : 2;
    uint32_t seimin : 1;
    uint32_t reserved_3 : 16;
  };
  uint32_t bytes;
} tmc2209_coolconf_t;

// 5.5 PWM_AUTO
typedef union
{
  struct
  {
    uint32_t pwm_ofs_auto : 8;
    uint32_t reserved_0 : 8;
    uint32_t pwm_grad_auto : 8;
    uint32_t reserved_1 : 8;
  };
  uint32_t bytes;
} tmc2209_pwm_auto_t;

// 5.5.1 CHOPCONF
typedef union
{
  struct
  {
    uint32_t toff : 4;
    uint32_t hstrt : 3;
    uint32_t hend : 4;
    uint32_t reserved_0 : 4;
    uint32_t tbl : 2;
    uint32_t vsense : 1;
    uint32_t reserved_1 : 6;
    uint32_t mres : 4;
    uint32_t intpol : 1;
    uint32_t dedge : 1;
    uint32_t diss2g : 1;
    uint32_t diss2vs : 1;
  };
  uint32_t bytes;
} tmc2209_chopconf_t;

// 5.5.2 PWMCONF
typedef union
{
  struct
  {
    uint32_t pwm_ofs : 8;
    uint32_t pwm_grad : 8;
    uint32_t pwm_freq : 2;
    uint32_t pwm_autoscale : 1;
    uint32_t pwm_autograd : 1;
    uint32_t freewheel : 2;
    uint32_t reserved : 2;
    uint32_t pwm_reg : 4;
    uint32_t pwm_lim : 4;
  };
  uint32_t bytes;
} tmc2209_pwmconf_t;

// 5.5.3 DRV_STATUS
typedef struct
{
  uint32_t over_temperature_warning : 1;
  uint32_t over_temperature_shutdown : 1;
  uint32_t short_to_ground_a : 1;
  uint32_t short_to_ground_b : 1;
  uint32_t low_side_short_a : 1;
  uint32_t low_side_short_b : 1;
  uint32_t open_load_a : 1;
  uint32_t open_load_b : 1;
  uint32_t over_temperature_120c : 1;
  uint32_t over_temperature_143c : 1;
  uint32_t over_temperature_150c : 1;
  uint32_t over_temperature_157c : 1;
  uint32_t reserved0 : 4;
  uint32_t current_scaling : 5;
  uint32_t reserved1 : 9;
  uint32_t stealth_chop_mode : 1;
  uint32_t standstill : 1;
} tmc2209_status_t; //

typedef union
{
  struct
  {
    tmc2209_status_t status;
  };
  uint32_t bytes;
} tmc2209_drive_status_t;

/***********************/

typedef enum
{
  CURRENT_INCREMENT_1 = 0,
  CURRENT_INCREMENT_2 = 1,
  CURRENT_INCREMENT_4 = 2,
  CURRENT_INCREMENT_8 = 3,
} current_increment_t; // NOT USED

typedef enum
{
  MEASUREMENT_COUNT_32 = 0,
  MEASUREMENT_COUNT_8  = 1,
  MEASUREMENT_COUNT_2  = 2,
  MEASUREMENT_COUNT_1  = 3,
} measurement_count_t; // NOT USED

typedef union
{
  struct
  {
    uint32_t reset : 1;
    uint32_t drv_err : 1;
    uint32_t uv_cp : 1;
    uint32_t reserved : 29;
  };
  uint32_t bytes;
} tmc2209_global_status_t; // NOT USED

typedef union
{
  struct
  {
    uint32_t pwm_scale_sum : 8;
    uint32_t reserved_0 : 8;
    int16_t pwm_scale_auto : 9;
    uint32_t reserved_1 : 7;
  };
  uint32_t bytes;
} tmc2209_pwm_scale_t;

typedef enum
{
  SERIAL_ADDRESS_0 = 0,
  SERIAL_ADDRESS_1 = 1,
  SERIAL_ADDRESS_2 = 2,
  SERIAL_ADDRESS_3 = 3,
} tmc2209_serial_address_t;

//tmc2209_standstill_mode_t
typedef enum
{
  TMC_NORMAL         = 0b00,
  TMC_FREEWHEELING   = 0b01,
  TMC_STRONG_BRAKING = 0b10,
  TMC_BRAKING        = 0b11,
} tmc2209_standstill_mode_t;

//tmc2209_hold_current_mode_t
typedef enum
{
  TMC_HOLD_DEFAULT_CURRENT, TMC_HOLD_FULL_CURRENT
} tmc2209_hold_current_mode_t;



// Stepper driver structure
typedef struct
{
  tmc2209_gconf_t          	gconf_;
  uint8_t					nodeconf_;
  tmc2209_ihold_irun_t 		ihold_irun_;
  uint8_t					tpower_down_;
  uint32_t					tpwmthrs_;
  uint32_t					tcoolthrs_;
  uint8_t					sgthrs_;
  tmc2209_coolconf_t       	coolconf_;
  tmc2209_chopconf_t        chopconf_;
  tmc2209_pwmconf_t         pwmconf_;
  tmc2209_pwm_auto_t		pwm_auto_;


} tmc2209_stepper_driver_t;

// Set MS resolution via pins
void setMSpins();

// Init tmc2209 via UART
return_code_t tmc2209_init();

return_code_t tmc2209_set_standstill_mode(tmc2209_standstill_mode_t);
return_code_t tmc2209_set_stepper_direction(bool);

/**
 * @brief Write the given data to the given register address.
 *
 * @param register_address
 * @param data
 */
return_code_t tmc2209_write(uint8_t, uint32_t) __attribute__((weak));
/**
 * @brief Read the data from the given register address.
 *
 * @param register_address
 * @return uint32_t
 */
uint32_t tmc2209_read(uint8_t) __attribute__((weak));


/*---------------------------------------------*/
/**
 * @brief Helper function to calculate the crc for the given datagram for writing to the stepper driver.
 *
 * @param stepper_driver
 * @param datagram
 * @param datagram_size
 * @return uint8_t
 */
uint8_t calculate_crc_write(write_read_reply_datagram_t *datagram, uint8_t datagram_size);

/**
 * @brief Helper function to calculate the crc for the given datagram for reading from the stepper driver.
 *
 * @param stepper_driver
 * @param datagram
 * @param datagram_size
 * @return uint8_t
 */
uint8_t calculate_crc_read(read_request_datagram_t *datagram, uint8_t datagram_size);

/**
 * @brief Set the operation mode to serial, this is used if you want to dynamically switch between step/dir and uart interface.
 *
 * @param stepper_driver
 * @param serial_address
 */

/**
 * @brief Helper function to reverse data while reading/writing to the stepper driver.
 *
 * @param stepper_driver
 * @param data
 * @return uint32_t
 */
uint32_t reverseData(uint32_t data);

return_code_t test_carousel_motor (void);
void test_carousel_driver(void);

#endif /* TCM2209_STEPPER_MOTOR_H_ */
