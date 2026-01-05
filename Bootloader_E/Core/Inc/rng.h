#ifndef __RNG_H__
#define __RNG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern RNG_HandleTypeDef hrng;

void MX_RNG_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RNG_H__ */
