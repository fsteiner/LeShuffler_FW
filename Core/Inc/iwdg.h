/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    iwdg.h
  * @brief   This file contains all the function prototypes for
  *          the iwdg.c file
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
#ifndef __IWDG_H__
#define __IWDG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern IWDG_HandleTypeDef hiwdg1;

/* USER CODE BEGIN Private defines */

// Watchdog timeout configuration
// LSI = 32 kHz, Prescaler = 256, Reload = 624
// Timeout = (256 * 625) / 32000 = 5 seconds
#define IWDG_PRESCALER      IWDG_PRESCALER_256
#define IWDG_RELOAD_VALUE   624

/* USER CODE END Private defines */

void MX_IWDG1_Init(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief Refresh the watchdog timer
 * Call this function in all waiting loops to prevent reset
 */
void watchdog_refresh(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __IWDG_H__ */
