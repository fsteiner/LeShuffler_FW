/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TIM2_prescaler (4-1)
#define encoderFilter 15
#define TIM15_prescaler (128-1)
#define TIM3_period (100-1)
#define TIM15_period (10000-1)
#define TIM12_period (65536-1)
#define TIM12_prescaler (64-1)
#define TIM2_period (65536-1)
#define IWDG_window_value (4096-1)
#define TIM3_prescaler (640-1)
#define IWDG_period (4096-1)
#define TIM14_PRESCALER (64000-1)
#define TIM14_PERIOD (5000-1)
#define ESC_BTN_Pin GPIO_PIN_0
#define ESC_BTN_GPIO_Port GPIOC
#define ESC_BTN_EXTI_IRQn EXTI0_IRQn
#define ENT_BTN_Pin GPIO_PIN_1
#define ENT_BTN_GPIO_Port GPIOC
#define ENT_BTN_EXTI_IRQn EXTI1_IRQn
#define ENCD_CW_Pin GPIO_PIN_0
#define ENCD_CW_GPIO_Port GPIOA
#define ENCD_CCW_Pin GPIO_PIN_1
#define ENCD_CCW_GPIO_Port GPIOA
#define BUZZER_Pin GPIO_PIN_2
#define BUZZER_GPIO_Port GPIOA
#define SHOE_SENSOR_A_Pin GPIO_PIN_3
#define SHOE_SENSOR_A_GPIO_Port GPIOA
#define MTR_DRV1_AIN1_Pin GPIO_PIN_4
#define MTR_DRV1_AIN1_GPIO_Port GPIOA
#define MTR_DRV1_AIN2_Pin GPIO_PIN_5
#define MTR_DRV1_AIN2_GPIO_Port GPIOA
#define MTR_DRV1_BIN1_Pin GPIO_PIN_6
#define MTR_DRV1_BIN1_GPIO_Port GPIOA
#define ENTRY_SENSOR_1_A_Pin GPIO_PIN_4
#define ENTRY_SENSOR_1_A_GPIO_Port GPIOC
#define ENTRY_SENSOR_2_A_Pin GPIO_PIN_5
#define ENTRY_SENSOR_2_A_GPIO_Port GPIOC
#define SHOE_SENSOR_Pin GPIO_PIN_0
#define SHOE_SENSOR_GPIO_Port GPIOB
#define ENTRY_SENSOR_1_Pin GPIO_PIN_1
#define ENTRY_SENSOR_1_GPIO_Port GPIOB
#define ENTRY_SENSOR_2_Pin GPIO_PIN_7
#define ENTRY_SENSOR_2_GPIO_Port GPIOE
#define IR_IN_Pin GPIO_PIN_8
#define IR_IN_GPIO_Port GPIOE
#define IR_IN_EXTI_IRQn EXTI9_5_IRQn
#define HE_IN_Pin GPIO_PIN_9
#define HE_IN_GPIO_Port GPIOE
#define STP_DIAG_Pin GPIO_PIN_10
#define STP_DIAG_GPIO_Port GPIOE
#define STP_DIAG_EXTI_IRQn EXTI15_10_IRQn
#define STP_DIR_Pin GPIO_PIN_13
#define STP_DIR_GPIO_Port GPIOE
#define STP_STEP_Pin GPIO_PIN_14
#define STP_STEP_GPIO_Port GPIOE
#define STP_INDEX_Pin GPIO_PIN_15
#define STP_INDEX_GPIO_Port GPIOE
#define ENTRY_SENSOR_3_Pin GPIO_PIN_11
#define ENTRY_SENSOR_3_GPIO_Port GPIOB
#define STM32_RX_Pin GPIO_PIN_12
#define STM32_RX_GPIO_Port GPIOB
#define STM32_TX_Pin GPIO_PIN_13
#define STM32_TX_GPIO_Port GPIOB
#define BACKLIT_PWM_Pin GPIO_PIN_15
#define BACKLIT_PWM_GPIO_Port GPIOB
#define ENTRY_SENSOR_3_A_Pin GPIO_PIN_8
#define ENTRY_SENSOR_3_A_GPIO_Port GPIOD
#define ESP_GPIO0_Pin GPIO_PIN_9
#define ESP_GPIO0_GPIO_Port GPIOD
#define STP_MS2_Pin GPIO_PIN_10
#define STP_MS2_GPIO_Port GPIOD
#define MTR_DRV1_BIN2_Pin GPIO_PIN_14
#define MTR_DRV1_BIN2_GPIO_Port GPIOD
#define STP_MS1_Pin GPIO_PIN_15
#define STP_MS1_GPIO_Port GPIOD
#define MTR_DRV1_PWMB_Pin GPIO_PIN_6
#define MTR_DRV1_PWMB_GPIO_Port GPIOC
#define MTR_DRV1_PWMA_Pin GPIO_PIN_7
#define MTR_DRV1_PWMA_GPIO_Port GPIOC
#define MTR_DRV2_PWMA_Pin GPIO_PIN_8
#define MTR_DRV2_PWMA_GPIO_Port GPIOC
#define I2C_SDA_Pin GPIO_PIN_9
#define I2C_SDA_GPIO_Port GPIOC
#define I2C_SCL_Pin GPIO_PIN_8
#define I2C_SCL_GPIO_Port GPIOA
#define EXIT_SENSOR_A_Pin GPIO_PIN_9
#define EXIT_SENSOR_A_GPIO_Port GPIOA
#define EXIT_SENSOR_Pin GPIO_PIN_10
#define EXIT_SENSOR_GPIO_Port GPIOA
#define STP_UART_TX_Pin GPIO_PIN_10
#define STP_UART_TX_GPIO_Port GPIOC
#define STP_UART_Pin GPIO_PIN_11
#define STP_UART_GPIO_Port GPIOC
#define SERVO_PWM_Pin GPIO_PIN_12
#define SERVO_PWM_GPIO_Port GPIOC
#define TRAY_SENSOR_A_Pin GPIO_PIN_0
#define TRAY_SENSOR_A_GPIO_Port GPIOD
#define MTR_DRV1_STBY_Pin GPIO_PIN_2
#define MTR_DRV1_STBY_GPIO_Port GPIOD
#define STP_ENN_Pin GPIO_PIN_3
#define STP_ENN_GPIO_Port GPIOD
#define MTR_DRV2_AIN2_Pin GPIO_PIN_4
#define MTR_DRV2_AIN2_GPIO_Port GPIOD
#define MTR_DRV2_AIN1_Pin GPIO_PIN_5
#define MTR_DRV2_AIN1_GPIO_Port GPIOD
#define MTR_DRV2_STBY_Pin GPIO_PIN_6
#define MTR_DRV2_STBY_GPIO_Port GPIOD
#define STP_STDBY_Pin GPIO_PIN_7
#define STP_STDBY_GPIO_Port GPIOD
#define TRAY_SENSOR_Pin GPIO_PIN_1
#define TRAY_SENSOR_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
