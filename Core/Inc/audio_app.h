#ifndef __AUDIO_APP_H__
#define __AUDIO_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "ff.h"

typedef enum
{
  AUDIO_APP_IDLE = 0,
  AUDIO_APP_RECORDING,
  AUDIO_APP_PLAYING,
  AUDIO_APP_ERROR
} AudioApp_State;

void AudioApp_Init(void);
void AudioApp_Task(void);
AudioApp_State AudioApp_GetState(void);
FRESULT AudioApp_GetLastFileResult(void);
HAL_StatusTypeDef AudioApp_GetLastHalResult(void);

#ifdef __cplusplus
}
#endif

#endif
