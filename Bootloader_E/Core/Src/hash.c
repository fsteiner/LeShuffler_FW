#include "hash.h"

HASH_HandleTypeDef hhash;

void MX_HASH_Init(void)
{
  hhash.Init.DataType = HASH_DATATYPE_8B;
  if (HAL_HASH_Init(&hhash) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_HASH_MspInit(HASH_HandleTypeDef* hashHandle)
{
  (void)hashHandle;
  __HAL_RCC_HASH_CLK_ENABLE();
}

void HAL_HASH_MspDeInit(HASH_HandleTypeDef* hashHandle)
{
  (void)hashHandle;
  __HAL_RCC_HASH_CLK_DISABLE();
}
