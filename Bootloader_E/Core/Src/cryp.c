#include "cryp.h"

CRYP_HandleTypeDef hcryp;

void MX_CRYP_Init(void)
{
  hcryp.Instance = CRYP;
  hcryp.Init.DataType = CRYP_DATATYPE_8B;
  hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
  hcryp.Init.Algorithm = CRYP_AES_CBC;
  hcryp.Init.pKey = NULL;
  hcryp.Init.pInitVect = NULL;
  if (HAL_CRYP_Init(&hcryp) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_CRYP_MspInit(CRYP_HandleTypeDef* crypHandle)
{
  if(crypHandle->Instance==CRYP)
  {
    __HAL_RCC_CRYP_CLK_ENABLE();
  }
}

void HAL_CRYP_MspDeInit(CRYP_HandleTypeDef* crypHandle)
{
  if(crypHandle->Instance==CRYP)
  {
    __HAL_RCC_CRYP_CLK_DISABLE();
  }
}
