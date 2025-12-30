/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BUZZER_Pin|MTR_DRV1_AIN1_Pin|MTR_DRV1_AIN2_Pin|MTR_DRV1_BIN1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, STP_DIR_Pin|STP_STEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BACKLIT_PWM_GPIO_Port, BACKLIT_PWM_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, ESP_GPIO0_Pin|STP_MS2_Pin|MTR_DRV1_BIN2_Pin|STP_MS1_Pin
                          |MTR_DRV1_STBY_Pin|STP_ENN_Pin|MTR_DRV2_AIN2_Pin|MTR_DRV2_AIN1_Pin
                          |MTR_DRV2_STBY_Pin|STP_STDBY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : ESC_BTN_Pin ENT_BTN_Pin */
  GPIO_InitStruct.Pin = ESC_BTN_Pin|ENT_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BUZZER_Pin MTR_DRV1_AIN1_Pin MTR_DRV1_AIN2_Pin MTR_DRV1_BIN1_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin|MTR_DRV1_AIN1_Pin|MTR_DRV1_AIN2_Pin|MTR_DRV1_BIN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SHOE_SENSOR_A_Pin EXIT_SENSOR_A_Pin */
  GPIO_InitStruct.Pin = SHOE_SENSOR_A_Pin|EXIT_SENSOR_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : ENTRY_SENSOR_1_A_Pin ENTRY_SENSOR_2_A_Pin */
  GPIO_InitStruct.Pin = ENTRY_SENSOR_1_A_Pin|ENTRY_SENSOR_2_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : SHOE_SENSOR_Pin ENTRY_SENSOR_1_Pin ENTRY_SENSOR_3_Pin */
  GPIO_InitStruct.Pin = SHOE_SENSOR_Pin|ENTRY_SENSOR_1_Pin|ENTRY_SENSOR_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : ENTRY_SENSOR_2_Pin HE_IN_Pin STP_INDEX_Pin TRAY_SENSOR_Pin */
  GPIO_InitStruct.Pin = ENTRY_SENSOR_2_Pin|HE_IN_Pin|STP_INDEX_Pin|TRAY_SENSOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_IN_Pin STP_DIAG_Pin */
  GPIO_InitStruct.Pin = IR_IN_Pin|STP_DIAG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : STP_DIR_Pin STP_STEP_Pin */
  GPIO_InitStruct.Pin = STP_DIR_Pin|STP_STEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BACKLIT_PWM_Pin */
  GPIO_InitStruct.Pin = BACKLIT_PWM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BACKLIT_PWM_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ENTRY_SENSOR_3_A_Pin TRAY_SENSOR_A_Pin */
  GPIO_InitStruct.Pin = ENTRY_SENSOR_3_A_Pin|TRAY_SENSOR_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : ESP_GPIO0_Pin STP_MS2_Pin MTR_DRV1_BIN2_Pin STP_MS1_Pin
                           MTR_DRV1_STBY_Pin STP_ENN_Pin MTR_DRV2_AIN2_Pin MTR_DRV2_AIN1_Pin
                           MTR_DRV2_STBY_Pin STP_STDBY_Pin */
  GPIO_InitStruct.Pin = ESP_GPIO0_Pin|STP_MS2_Pin|MTR_DRV1_BIN2_Pin|STP_MS1_Pin
                          |MTR_DRV1_STBY_Pin|STP_ENN_Pin|MTR_DRV2_AIN2_Pin|MTR_DRV2_AIN1_Pin
                          |MTR_DRV2_STBY_Pin|STP_STDBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : EXIT_SENSOR_Pin */
  GPIO_InitStruct.Pin = EXIT_SENSOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(EXIT_SENSOR_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
