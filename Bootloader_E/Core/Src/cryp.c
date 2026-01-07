#include "cryp.h"

CRYP_HandleTypeDef hcryp;

/* Dummy key/IV for initial CRYP setup (will be replaced on actual decrypt) */
static const uint32_t dummy_key[8] = {0};
static const uint32_t dummy_iv[4] = {0};

void MX_CRYP_Init(void)
{
  hcryp.Instance = CRYP;
  hcryp.Init.DataType = CRYP_DATATYPE_8B;
  hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
  hcryp.Init.Algorithm = CRYP_AES_CBC;
  hcryp.Init.pKey = (uint32_t*)dummy_key;
  hcryp.Init.pInitVect = (uint32_t*)dummy_iv;
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
    /* Keep CRYP clock enabled - we need it for subsequent operations */
    /* __HAL_RCC_CRYP_CLK_DISABLE(); */
  }
}
