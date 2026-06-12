#ifndef __ES8388_H__
#define __ES8388_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#define ES8388_I2C_ADDR_7BIT 0x10U
#define ES8388_I2C_ADDR      (ES8388_I2C_ADDR_7BIT << 1)

HAL_StatusTypeDef ES8388_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef ES8388_WriteReg(uint8_t reg, uint8_t value);
HAL_StatusTypeDef ES8388_ReadReg(uint8_t reg, uint8_t *value);
HAL_StatusTypeDef ES8388_SetVolume(uint8_t volume);
uint8_t ES8388_GetLastVerifyError(uint8_t *reg, uint8_t *expected, uint8_t *actual);

#ifdef __cplusplus
}
#endif

#endif
