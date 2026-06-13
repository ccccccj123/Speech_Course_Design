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

typedef enum
{
  AUDIO_FX_NORMAL = 0,
  AUDIO_FX_PITCH,
  AUDIO_FX_ECHO,
  AUDIO_FX_MIX,
  AUDIO_FX_SLOW,
  AUDIO_FX_FILTER,
  AUDIO_FX_RECORD2
} AudioFxMode;

void AudioApp_Init(void);
void AudioApp_Task(void);
AudioApp_State AudioApp_GetState(void);
AudioFxMode AudioApp_GetFxMode(void);
FRESULT AudioApp_GetLastFileResult(void);
HAL_StatusTypeDef AudioApp_GetLastHalResult(void);

#ifdef __cplusplus
}
#endif

#endif
