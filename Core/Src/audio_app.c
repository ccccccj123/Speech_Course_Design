#include "audio_app.h"

#include <stdio.h>
#include <string.h>
#include "es8388.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "stm32f4xx_hal_i2s_ex.h"
#include "wav.h"

#define AUDIO_SAMPLE_RATE       16000U
#define AUDIO_BITS_PER_SAMPLE  16U
#define AUDIO_WAV_CHANNELS     1U
#define AUDIO_MAX_SECONDS      60U
#define AUDIO_MAX_DATA_BYTES   (AUDIO_SAMPLE_RATE * (AUDIO_BITS_PER_SAMPLE / 8U) * AUDIO_WAV_CHANNELS * AUDIO_MAX_SECONDS)
#define AUDIO_FILE_NAME        "record.wav"
#define DEBUG_FILE_NAME        "debug.txt"
#define DEBUG_LINE_SIZE        96U
#define DEBUG_FW_TAG           "playback-fx-v12"
#define DEBUG_RAW_WORD_COUNT   16U
#define AUDIO_FX_MODE_COUNT    ((uint8_t)AUDIO_FX_FILTER + 1U)
#define AUDIO_FX_DELAY_SAMPLES 1600U
#define AUDIO_FX_PITCH_STEP_PERCENT 150U
#define AUDIO_FX_PITCH_MOD_PERIOD   24U

#define AUDIO_DMA_FRAMES_HALF  512U
#define AUDIO_DMA_FRAMES_TOTAL (AUDIO_DMA_FRAMES_HALF * 2U)
#define AUDIO_I2S_CHANNELS     2U
#define AUDIO_DMA_WORDS        (AUDIO_DMA_FRAMES_TOTAL * AUDIO_I2S_CHANNELS)
#define AUDIO_DMA_HALF_WORDS   (AUDIO_DMA_WORDS / 2U)
#define AUDIO_MONO_HALF_BYTES  (AUDIO_DMA_FRAMES_HALF * sizeof(uint16_t))
#define AUDIO_RECORD_CHANNEL_LEFT  0U
#define AUDIO_RECORD_CHANNEL_RIGHT 1U
#define AUDIO_RECORD_INPUT_CHANNEL AUDIO_RECORD_CHANNEL_LEFT
#define AUDIO_RECORD_DIGITAL_GAIN  3U

#if ((AUDIO_RECORD_INPUT_CHANNEL != AUDIO_RECORD_CHANNEL_LEFT) && \
     (AUDIO_RECORD_INPUT_CHANNEL != AUDIO_RECORD_CHANNEL_RIGHT))
#error "AUDIO_RECORD_INPUT_CHANNEL must be AUDIO_RECORD_CHANNEL_LEFT or AUDIO_RECORD_CHANNEL_RIGHT"
#endif

#define RECORD_KEY_PORT        GPIOA
#define RECORD_KEY_PIN         GPIO_PIN_0
#define PLAY_KEY_PORT          GPIOE
#define PLAY_KEY_PIN           GPIO_PIN_8
#define RESERVED_KEY_PORT      GPIOC
#define RESERVED_KEY_PIN       GPIO_PIN_13
#define KEY_ACTIVE_STATE       GPIO_PIN_RESET
#define KEY_DEBOUNCE_MS        25U
#define CODEC_POWERUP_DELAY_MS 150U
#define CODEC_INIT_RETRY_COUNT 3U
#define CODEC_INIT_RETRY_MS    100U
#define CODEC_CLOCK_WARMUP_MS  20U

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState stable_state;
  GPIO_PinState last_raw_state;
  uint32_t last_change_ms;
} AudioButton;

typedef struct
{
  int16_t left_min;
  int16_t left_max;
  int16_t right_min;
  int16_t right_max;
  int16_t mono_min;
  int16_t mono_max;
  uint32_t left_nonzero;
  uint32_t right_nonzero;
  uint32_t mono_nonzero;
  uint32_t samples;
  uint32_t half_callbacks;
  uint32_t full_callbacks;
  uint16_t raw_words[DEBUG_RAW_WORD_COUNT];
  uint32_t raw_word_count;
} AudioDebugStats;

static AudioButton s_record_key = {RECORD_KEY_PORT, RECORD_KEY_PIN, GPIO_PIN_SET, GPIO_PIN_SET, 0U};
static AudioButton s_play_key = {PLAY_KEY_PORT, PLAY_KEY_PIN, GPIO_PIN_SET, GPIO_PIN_SET, 0U};
static AudioButton s_reserved_key = {RESERVED_KEY_PORT, RESERVED_KEY_PIN, GPIO_PIN_SET, GPIO_PIN_SET, 0U};

static AudioApp_State s_state = AUDIO_APP_IDLE;
static FRESULT s_last_file_result = FR_OK;
static HAL_StatusTypeDef s_last_hal_result = HAL_OK;
static HAL_StatusTypeDef s_stop_hal_result = HAL_OK;
static FRESULT s_stop_wav_result = FR_OK;
static FRESULT s_stop_sync_result = FR_OK;
static FRESULT s_stop_close_result = FR_OK;
static uint8_t s_sd_mounted;
static FIL s_audio_file;
static uint32_t s_recorded_bytes;
static uint8_t s_stop_after_half;
static uint8_t s_stop_after_full;
static AudioFxMode s_fx_mode = AUDIO_FX_NORMAL;
static uint32_t s_fx_delay_index;
static uint32_t s_fx_pitch_phase;
static int16_t s_fx_delay_buffer[AUDIO_FX_DELAY_SAMPLES];
static int16_t s_fx_filter_last;
static AudioDebugStats s_debug_stats;

static uint16_t s_i2s_tx_buffer[AUDIO_DMA_WORDS];
static uint16_t s_i2s_rx_buffer[AUDIO_DMA_WORDS];
static uint16_t s_mono_buffer[AUDIO_DMA_FRAMES_HALF];
static uint16_t s_effect_buffer[AUDIO_DMA_FRAMES_HALF];

static volatile uint8_t s_dma_half_ready;
static volatile uint8_t s_dma_full_ready;
static volatile uint8_t s_i2s_stop_in_progress;

static uint8_t button_pressed(AudioButton *button);
static HAL_StatusTypeDef init_codec_with_retry(void);
static HAL_StatusTypeDef init_codec_with_i2s_clock(void);
static void prepare_i2s_for_start(void);
static HAL_StatusTypeDef stop_i2s_dma(void);
static FRESULT mount_sd(void);
static void set_error(FRESULT file_result, HAL_StatusTypeDef hal_result);
static void start_recording(void);
static void stop_recording(void);
static void start_playback(void);
static void stop_playback(void);
static void process_recording(void);
static void process_playback(void);
static void write_record_segment(uint32_t offset_words);
static uint8_t fill_playback_segment(uint32_t offset_words);
static uint16_t select_i2s_record_sample(const uint16_t *frame);
static uint16_t apply_record_gain(uint16_t sample);
static void cycle_fx_mode(void);
static void reset_effect_state(void);
static void apply_playback_effects(uint16_t *samples, uint16_t *output, uint32_t count);
static void apply_pitch_effect(uint16_t *samples, uint16_t *output, uint32_t count);
static void apply_echo_effect(uint16_t *samples, uint16_t *output, uint32_t count);
static void apply_mix_effect(uint16_t *samples, uint16_t *output, uint32_t count);
static void apply_slow_effect(uint16_t *samples, uint16_t *output, uint32_t count);
static void apply_filter_effect(uint16_t *samples, uint16_t *output, uint32_t count);
static uint16_t saturate_i16(int32_t sample);
static void debug_reset_stats(void);
static void debug_update_stats(const uint16_t *frame, uint16_t mono);
static void debug_write_log(const char *event);
static void debug_write_line(FIL *file, const char *text);
static void debug_write_es8388_registers(FIL *file);
static void debug_write_raw_words(FIL *file);

void AudioApp_Init(void)
{
  GPIO_PinState raw;

  raw = HAL_GPIO_ReadPin(RECORD_KEY_PORT, RECORD_KEY_PIN);
  s_record_key.stable_state = raw;
  s_record_key.last_raw_state = raw;

  raw = HAL_GPIO_ReadPin(PLAY_KEY_PORT, PLAY_KEY_PIN);
  s_play_key.stable_state = raw;
  s_play_key.last_raw_state = raw;

  raw = HAL_GPIO_ReadPin(RESERVED_KEY_PORT, RESERVED_KEY_PIN);
  s_reserved_key.stable_state = raw;
  s_reserved_key.last_raw_state = raw;

  HAL_Delay(CODEC_POWERUP_DELAY_MS);
  s_last_hal_result = init_codec_with_i2s_clock();
  if (s_last_hal_result != HAL_OK)
  {
    s_state = AUDIO_APP_ERROR;
  }
}

void AudioApp_Task(void)
{
  if ((s_state != AUDIO_APP_RECORDING) && button_pressed(&s_reserved_key))
  {
    cycle_fx_mode();
  }

  if (button_pressed(&s_record_key))
  {
    if (s_state == AUDIO_APP_RECORDING)
    {
      stop_recording();
    }
    else if (s_state == AUDIO_APP_IDLE)
    {
      start_recording();
    }
  }

  if (button_pressed(&s_play_key))
  {
    if (s_state == AUDIO_APP_PLAYING)
    {
      stop_playback();
    }
    else if (s_state == AUDIO_APP_IDLE)
    {
      start_playback();
    }
  }

  if (s_state == AUDIO_APP_RECORDING)
  {
    process_recording();
  }
  else if (s_state == AUDIO_APP_PLAYING)
  {
    process_playback();
  }
}

AudioApp_State AudioApp_GetState(void)
{
  return s_state;
}

AudioFxMode AudioApp_GetFxMode(void)
{
  return s_fx_mode;
}

FRESULT AudioApp_GetLastFileResult(void)
{
  return s_last_file_result;
}

HAL_StatusTypeDef AudioApp_GetLastHalResult(void)
{
  return s_last_hal_result;
}

static uint8_t button_pressed(AudioButton *button)
{
  GPIO_PinState raw = HAL_GPIO_ReadPin(button->port, button->pin);
  uint32_t now = HAL_GetTick();

  if (raw != button->last_raw_state)
  {
    button->last_raw_state = raw;
    button->last_change_ms = now;
  }

  if ((now - button->last_change_ms) < KEY_DEBOUNCE_MS)
  {
    return 0U;
  }

  if (raw != button->stable_state)
  {
    button->stable_state = raw;
    return (raw == KEY_ACTIVE_STATE) ? 1U : 0U;
  }

  return 0U;
}

static HAL_StatusTypeDef init_codec_with_retry(void)
{
  uint32_t retry;
  HAL_StatusTypeDef result = HAL_ERROR;

  for (retry = 0U; retry < CODEC_INIT_RETRY_COUNT; retry++)
  {
    result = ES8388_Init(&hi2c1);
    if (result == HAL_OK)
    {
      return HAL_OK;
    }

    HAL_Delay(CODEC_INIT_RETRY_MS);
  }

  return result;
}

static HAL_StatusTypeDef init_codec_with_i2s_clock(void)
{
  HAL_StatusTypeDef clock_result;
  HAL_StatusTypeDef codec_result;

  memset(s_i2s_tx_buffer, 0, sizeof(s_i2s_tx_buffer));
  memset(s_i2s_rx_buffer, 0, sizeof(s_i2s_rx_buffer));
  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;

  prepare_i2s_for_start();
  clock_result = HAL_I2SEx_TransmitReceive_DMA(&hi2s2,
                                               s_i2s_tx_buffer,
                                               s_i2s_rx_buffer,
                                               AUDIO_DMA_WORDS);
  if (clock_result == HAL_OK)
  {
    HAL_Delay(CODEC_CLOCK_WARMUP_MS);
  }

  codec_result = init_codec_with_retry();

  if (clock_result == HAL_OK)
  {
    (void)stop_i2s_dma();
  }

  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;

  if (clock_result != HAL_OK)
  {
    return clock_result;
  }

  return codec_result;
}

static FRESULT mount_sd(void)
{
  if (s_sd_mounted != 0U)
  {
    return FR_OK;
  }

  s_last_file_result = f_mount(&SDFatFS, SDPath, 1);
  if (s_last_file_result == FR_OK)
  {
    s_sd_mounted = 1U;
  }

  return s_last_file_result;
}

static void set_error(FRESULT file_result, HAL_StatusTypeDef hal_result)
{
  s_last_file_result = file_result;
  s_last_hal_result = hal_result;
  s_state = AUDIO_APP_ERROR;
}

static void start_recording(void)
{
  if (mount_sd() != FR_OK)
  {
    set_error(s_last_file_result, HAL_OK);
    debug_write_log("record_start_error");
    return;
  }

  s_last_file_result = f_open(&s_audio_file, AUDIO_FILE_NAME, FA_CREATE_ALWAYS | FA_WRITE);
  if (s_last_file_result != FR_OK)
  {
    set_error(s_last_file_result, HAL_OK);
    debug_write_log("record_start_error");
    return;
  }

  s_last_file_result = WAV_WriteHeader(&s_audio_file,
                                       AUDIO_SAMPLE_RATE,
                                       AUDIO_BITS_PER_SAMPLE,
                                       AUDIO_WAV_CHANNELS,
                                       0U);
  if (s_last_file_result != FR_OK)
  {
    f_close(&s_audio_file);
    set_error(s_last_file_result, HAL_OK);
    debug_write_log("record_start_error");
    return;
  }

  memset(s_i2s_tx_buffer, 0, sizeof(s_i2s_tx_buffer));
  memset(s_i2s_rx_buffer, 0, sizeof(s_i2s_rx_buffer));
  memset(s_mono_buffer, 0, sizeof(s_mono_buffer));
  debug_reset_stats();
  s_recorded_bytes = 0U;
  s_stop_hal_result = HAL_OK;
  s_stop_wav_result = FR_OK;
  s_stop_sync_result = FR_OK;
  s_stop_close_result = FR_OK;
  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;

  prepare_i2s_for_start();
  s_last_hal_result = HAL_I2SEx_TransmitReceive_DMA(&hi2s2,
                                                    s_i2s_tx_buffer,
                                                    s_i2s_rx_buffer,
                                                    AUDIO_DMA_WORDS);
  if (s_last_hal_result != HAL_OK)
  {
    f_close(&s_audio_file);
    set_error(FR_OK, s_last_hal_result);
    debug_write_log("record_start_error");
    return;
  }

  s_state = AUDIO_APP_RECORDING;
}

static void stop_recording(void)
{
  s_state = AUDIO_APP_IDLE;
  s_stop_hal_result = stop_i2s_dma();
  s_stop_wav_result = WAV_UpdateHeader(&s_audio_file,
                                       AUDIO_SAMPLE_RATE,
                                       AUDIO_BITS_PER_SAMPLE,
                                       AUDIO_WAV_CHANNELS,
                                       s_recorded_bytes);
  s_stop_sync_result = f_sync(&s_audio_file);
  s_stop_close_result = f_close(&s_audio_file);
  if (s_stop_hal_result != HAL_OK)
  {
    s_last_hal_result = s_stop_hal_result;
  }
  if (s_stop_wav_result != FR_OK)
  {
    s_last_file_result = s_stop_wav_result;
  }
  else if (s_stop_sync_result != FR_OK)
  {
    s_last_file_result = s_stop_sync_result;
  }
  else if (s_stop_close_result != FR_OK)
  {
    s_last_file_result = s_stop_close_result;
  }
  debug_write_log("record_stop");
  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;
}

static void start_playback(void)
{
  if (mount_sd() != FR_OK)
  {
    set_error(s_last_file_result, HAL_OK);
    return;
  }

  s_last_file_result = f_open(&s_audio_file, AUDIO_FILE_NAME, FA_READ);
  if (s_last_file_result != FR_OK)
  {
    set_error(s_last_file_result, HAL_OK);
    return;
  }

  s_last_file_result = f_lseek(&s_audio_file, WAV_HEADER_SIZE);
  if (s_last_file_result != FR_OK)
  {
    f_close(&s_audio_file);
    set_error(s_last_file_result, HAL_OK);
    return;
  }

  memset(s_i2s_tx_buffer, 0, sizeof(s_i2s_tx_buffer));
  memset(s_i2s_rx_buffer, 0, sizeof(s_i2s_rx_buffer));
  memset(s_effect_buffer, 0, sizeof(s_effect_buffer));
  s_stop_after_half = 0U;
  s_stop_after_full = 0U;
  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;
  reset_effect_state();

  s_stop_after_half = fill_playback_segment(0U);
  s_stop_after_full = fill_playback_segment(AUDIO_DMA_HALF_WORDS);

  prepare_i2s_for_start();
  s_last_hal_result = HAL_I2SEx_TransmitReceive_DMA(&hi2s2,
                                                    s_i2s_tx_buffer,
                                                    s_i2s_rx_buffer,
                                                    AUDIO_DMA_WORDS);
  if (s_last_hal_result != HAL_OK)
  {
    f_close(&s_audio_file);
    set_error(FR_OK, s_last_hal_result);
    return;
  }

  s_state = AUDIO_APP_PLAYING;
}

static void stop_playback(void)
{
  (void)stop_i2s_dma();
  (void)f_close(&s_audio_file);
  memset(s_i2s_tx_buffer, 0, sizeof(s_i2s_tx_buffer));
  memset(s_effect_buffer, 0, sizeof(s_effect_buffer));
  s_dma_half_ready = 0U;
  s_dma_full_ready = 0U;
  s_stop_after_half = 0U;
  s_stop_after_full = 0U;
  s_state = AUDIO_APP_IDLE;
}

static void process_recording(void)
{
  if (s_dma_half_ready != 0U)
  {
    s_dma_half_ready = 0U;
    s_debug_stats.half_callbacks++;
    write_record_segment(0U);
  }

  if (s_dma_full_ready != 0U)
  {
    s_dma_full_ready = 0U;
    s_debug_stats.full_callbacks++;
    write_record_segment(AUDIO_DMA_HALF_WORDS);
  }

  if (s_recorded_bytes >= AUDIO_MAX_DATA_BYTES)
  {
    stop_recording();
  }
}

static HAL_StatusTypeDef stop_i2s_dma(void)
{
  HAL_StatusTypeDef result;

  s_i2s_stop_in_progress = 1U;
  result = HAL_I2S_DMAStop(&hi2s2);
  s_i2s_stop_in_progress = 0U;

  return result;
}

static void prepare_i2s_for_start(void)
{
  __HAL_I2S_CLEAR_OVRFLAG(&hi2s2);
  __HAL_I2S_CLEAR_UDRFLAG(&hi2s2);
  __HAL_I2S_FLUSH_RX_DR(&hi2s2);
  __HAL_I2SEXT_CLEAR_OVRFLAG(&hi2s2);
  __HAL_I2SEXT_CLEAR_UDRFLAG(&hi2s2);
  __HAL_I2SEXT_FLUSH_RX_DR(&hi2s2);
  hi2s2.ErrorCode = HAL_I2S_ERROR_NONE;
}

static void process_playback(void)
{
  if (s_dma_half_ready != 0U)
  {
    s_dma_half_ready = 0U;
    if (s_stop_after_half != 0U)
    {
      stop_playback();
      return;
    }
    s_stop_after_half = fill_playback_segment(0U);
  }

  if (s_dma_full_ready != 0U)
  {
    s_dma_full_ready = 0U;
    if (s_stop_after_full != 0U)
    {
      stop_playback();
      return;
    }
    s_stop_after_full = fill_playback_segment(AUDIO_DMA_HALF_WORDS);
  }
}

static void write_record_segment(uint32_t offset_words)
{
  uint32_t i;
  uint32_t sample_count = AUDIO_DMA_FRAMES_HALF;
  UINT written = 0U;
  uint16_t *src = &s_i2s_rx_buffer[offset_words];

  if ((s_recorded_bytes + AUDIO_MONO_HALF_BYTES) > AUDIO_MAX_DATA_BYTES)
  {
    sample_count = (AUDIO_MAX_DATA_BYTES - s_recorded_bytes) / sizeof(uint16_t);
  }

  for (i = 0; i < sample_count; i++)
  {
    s_mono_buffer[i] = apply_record_gain(select_i2s_record_sample(&src[i * AUDIO_I2S_CHANNELS]));
    debug_update_stats(&src[i * AUDIO_I2S_CHANNELS], s_mono_buffer[i]);
  }

  s_last_file_result = f_write(&s_audio_file,
                               s_mono_buffer,
                               sample_count * sizeof(uint16_t),
                               &written);
  if ((s_last_file_result != FR_OK) || (written != (sample_count * sizeof(uint16_t))))
  {
    if (s_last_file_result == FR_OK)
    {
      s_last_file_result = FR_DISK_ERR;
    }
    stop_recording();
    set_error(s_last_file_result, HAL_OK);
    return;
  }

  s_recorded_bytes += written;
}

static uint8_t fill_playback_segment(uint32_t offset_words)
{
  uint32_t i;
  UINT read_bytes = 0U;
  uint16_t *dst = &s_i2s_tx_buffer[offset_words];
  uint8_t should_stop_after_segment = 0U;

  memset(s_mono_buffer, 0, sizeof(s_mono_buffer));
  s_last_file_result = f_read(&s_audio_file,
                              s_mono_buffer,
                              (s_fx_mode == AUDIO_FX_SLOW) ? (AUDIO_MONO_HALF_BYTES / 2U) : AUDIO_MONO_HALF_BYTES,
                              &read_bytes);
  if ((s_last_file_result != FR_OK) ||
      (read_bytes < ((s_fx_mode == AUDIO_FX_SLOW) ? (AUDIO_MONO_HALF_BYTES / 2U) : AUDIO_MONO_HALF_BYTES)))
  {
    should_stop_after_segment = 1U;
  }

  apply_playback_effects(s_mono_buffer, s_effect_buffer, AUDIO_DMA_FRAMES_HALF);

  for (i = 0; i < AUDIO_DMA_FRAMES_HALF; i++)
  {
    dst[(i * AUDIO_I2S_CHANNELS) + 0U] = s_effect_buffer[i];
    dst[(i * AUDIO_I2S_CHANNELS) + 1U] = s_effect_buffer[i];
  }

  return should_stop_after_segment;
}

static uint16_t select_i2s_record_sample(const uint16_t *frame)
{
  int16_t left = (int16_t)frame[0];
  int16_t right = (int16_t)frame[1];

#if (AUDIO_RECORD_INPUT_CHANNEL == AUDIO_RECORD_CHANNEL_RIGHT)
  (void)left;
  return (uint16_t)right;
#else
  (void)right;
  return (uint16_t)left;
#endif
}

static uint16_t apply_record_gain(uint16_t sample)
{
  int32_t amplified = (int32_t)(int16_t)sample * AUDIO_RECORD_DIGITAL_GAIN;

  if (amplified > 32767)
  {
    amplified = 32767;
  }
  else if (amplified < -32768)
  {
    amplified = -32768;
  }

  return (uint16_t)(int16_t)amplified;
}

static void cycle_fx_mode(void)
{
  uint8_t next_mode = (uint8_t)s_fx_mode + 1U;

  if (next_mode >= AUDIO_FX_MODE_COUNT)
  {
    next_mode = 0U;
  }

  s_fx_mode = (AudioFxMode)next_mode;
  reset_effect_state();
}

static void reset_effect_state(void)
{
  s_fx_delay_index = 0U;
  s_fx_pitch_phase = 0U;
  s_fx_filter_last = 0;
  memset(s_fx_delay_buffer, 0, sizeof(s_fx_delay_buffer));
}

static void apply_playback_effects(uint16_t *samples, uint16_t *output, uint32_t count)
{
  switch (s_fx_mode)
  {
    case AUDIO_FX_PITCH:
      apply_pitch_effect(samples, output, count);
      break;

    case AUDIO_FX_ECHO:
      apply_echo_effect(samples, output, count);
      break;

    case AUDIO_FX_MIX:
      apply_mix_effect(samples, output, count);
      break;

    case AUDIO_FX_SLOW:
      apply_slow_effect(samples, output, count);
      break;

    case AUDIO_FX_FILTER:
      apply_filter_effect(samples, output, count);
      break;

    case AUDIO_FX_NORMAL:
    default:
      memcpy(output, samples, count * sizeof(output[0]));
      break;
  }
}

static void apply_pitch_effect(uint16_t *samples, uint16_t *output, uint32_t count)
{
  uint32_t i;
  uint32_t phase = s_fx_pitch_phase;

  for (i = 0U; i < count; i++)
  {
    uint32_t src_index = ((phase + i) * AUDIO_FX_PITCH_STEP_PERCENT) / 100U;
    int16_t sample;

    if (count != 0U)
    {
      src_index %= count;
    }

    sample = (int16_t)samples[src_index];
    if ((((phase + i) / AUDIO_FX_PITCH_MOD_PERIOD) & 1U) != 0U)
    {
      sample = -sample;
    }

    output[i] = saturate_i16(((int32_t)sample * 5) / 4);
  }

  s_fx_pitch_phase = (phase + count) % (AUDIO_FX_PITCH_MOD_PERIOD * 2U);
}

static void apply_echo_effect(uint16_t *samples, uint16_t *output, uint32_t count)
{
  uint32_t i;

  for (i = 0U; i < count; i++)
  {
    int16_t sample = (int16_t)samples[i];
    int16_t delayed = s_fx_delay_buffer[s_fx_delay_index];

    output[i] = saturate_i16((int32_t)sample + ((int32_t)delayed / 2));
    s_fx_delay_buffer[s_fx_delay_index] = (int16_t)saturate_i16((int32_t)sample + ((int32_t)delayed / 2));
    s_fx_delay_index = (s_fx_delay_index + 1U) % AUDIO_FX_DELAY_SAMPLES;
  }
}

static void apply_mix_effect(uint16_t *samples, uint16_t *output, uint32_t count)
{
  uint32_t i;

  for (i = 0U; i < count; i++)
  {
    uint32_t second_index = (s_fx_delay_index + (AUDIO_FX_DELAY_SAMPLES / 2U)) % AUDIO_FX_DELAY_SAMPLES;
    int16_t sample = (int16_t)samples[i];
    int16_t delayed_one = s_fx_delay_buffer[s_fx_delay_index];
    int16_t delayed_two = s_fx_delay_buffer[second_index];

    output[i] = saturate_i16((int32_t)sample + ((int32_t)delayed_one / 3) + ((int32_t)delayed_two / 4));
    s_fx_delay_buffer[s_fx_delay_index] = (int16_t)saturate_i16((int32_t)sample + ((int32_t)delayed_one / 2));
    s_fx_delay_index = (s_fx_delay_index + 1U) % AUDIO_FX_DELAY_SAMPLES;
  }
}

static void apply_slow_effect(uint16_t *samples, uint16_t *output, uint32_t count)
{
  uint32_t i;

  for (i = 0U; i < count; i++)
  {
    output[i] = samples[i / 2U];
  }
}

static void apply_filter_effect(uint16_t *samples, uint16_t *output, uint32_t count)
{
  uint32_t i;

  for (i = 0U; i < count; i++)
  {
    int16_t sample = (int16_t)samples[i];
    s_fx_filter_last = (int16_t)(((int32_t)s_fx_filter_last * 3 + sample) / 4);
    output[i] = (uint16_t)s_fx_filter_last;
  }
}

static uint16_t saturate_i16(int32_t sample)
{
  if (sample > 32767)
  {
    sample = 32767;
  }
  else if (sample < -32768)
  {
    sample = -32768;
  }

  return (uint16_t)(int16_t)sample;
}

static void debug_reset_stats(void)
{
  s_debug_stats.left_min = 32767;
  s_debug_stats.left_max = -32768;
  s_debug_stats.right_min = 32767;
  s_debug_stats.right_max = -32768;
  s_debug_stats.mono_min = 32767;
  s_debug_stats.mono_max = -32768;
  s_debug_stats.left_nonzero = 0U;
  s_debug_stats.right_nonzero = 0U;
  s_debug_stats.mono_nonzero = 0U;
  s_debug_stats.samples = 0U;
  s_debug_stats.half_callbacks = 0U;
  s_debug_stats.full_callbacks = 0U;
  s_debug_stats.raw_word_count = 0U;
  memset(s_debug_stats.raw_words, 0, sizeof(s_debug_stats.raw_words));
}

static void debug_update_stats(const uint16_t *frame, uint16_t mono)
{
  int16_t left = (int16_t)frame[0];
  int16_t right = (int16_t)frame[1];
  int16_t mono_sample = (int16_t)mono;

  if ((s_debug_stats.raw_word_count < DEBUG_RAW_WORD_COUNT) &&
      ((frame[0] != 0U) || (frame[1] != 0U)))
  {
    s_debug_stats.raw_words[s_debug_stats.raw_word_count++] = frame[0];
    if (s_debug_stats.raw_word_count < DEBUG_RAW_WORD_COUNT)
    {
      s_debug_stats.raw_words[s_debug_stats.raw_word_count++] = frame[1];
    }
  }

  if (left < s_debug_stats.left_min)
  {
    s_debug_stats.left_min = left;
  }
  if (left > s_debug_stats.left_max)
  {
    s_debug_stats.left_max = left;
  }
  if (right < s_debug_stats.right_min)
  {
    s_debug_stats.right_min = right;
  }
  if (right > s_debug_stats.right_max)
  {
    s_debug_stats.right_max = right;
  }
  if (mono_sample < s_debug_stats.mono_min)
  {
    s_debug_stats.mono_min = mono_sample;
  }
  if (mono_sample > s_debug_stats.mono_max)
  {
    s_debug_stats.mono_max = mono_sample;
  }

  if (left != 0)
  {
    s_debug_stats.left_nonzero++;
  }
  if (right != 0)
  {
    s_debug_stats.right_nonzero++;
  }
  if (mono_sample != 0)
  {
    s_debug_stats.mono_nonzero++;
  }

  s_debug_stats.samples++;
}

static void debug_write_log(const char *event)
{
  FIL debug_file;
  char line[DEBUG_LINE_SIZE];
  uint8_t verify_reg = 0U;
  uint8_t verify_expected = 0U;
  uint8_t verify_actual = 0U;
  uint8_t has_verify_error;

  if (f_open(&debug_file, DEBUG_FILE_NAME, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
  {
    return;
  }

  debug_write_line(&debug_file, "audio debug\r\n");
  snprintf(line, sizeof(line), "fw=%s\r\n", DEBUG_FW_TAG);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "event=%s\r\n", event);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "recorded_bytes=%lu\r\n", (unsigned long)s_recorded_bytes);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "record_channel=%s\r\n",
           (AUDIO_RECORD_INPUT_CHANNEL == AUDIO_RECORD_CHANNEL_RIGHT) ? "right" : "left");
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "fx_mode=%d\r\n", (int)s_fx_mode);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "hal_result=%d file_result=%d state=%d\r\n",
           (int)s_last_hal_result,
           (int)s_last_file_result,
           (int)s_state);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "i2s_state=%lu i2s_error=0x%08lX\r\n",
           (unsigned long)hi2s2.State,
           (unsigned long)hi2s2.ErrorCode);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "stop_hal_result=%d wav_result=%d sync_result=%d close_result=%d\r\n",
           (int)s_stop_hal_result,
           (int)s_stop_wav_result,
           (int)s_stop_sync_result,
           (int)s_stop_close_result);
  debug_write_line(&debug_file, line);

  snprintf(line, sizeof(line), "samples=%lu half_callbacks=%lu full_callbacks=%lu\r\n",
           (unsigned long)s_debug_stats.samples,
           (unsigned long)s_debug_stats.half_callbacks,
           (unsigned long)s_debug_stats.full_callbacks);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "left_min=%d left_max=%d left_nonzero=%lu\r\n",
           (int)s_debug_stats.left_min,
           (int)s_debug_stats.left_max,
           (unsigned long)s_debug_stats.left_nonzero);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "right_min=%d right_max=%d right_nonzero=%lu\r\n",
           (int)s_debug_stats.right_min,
           (int)s_debug_stats.right_max,
           (unsigned long)s_debug_stats.right_nonzero);
  debug_write_line(&debug_file, line);
  snprintf(line, sizeof(line), "mono_min=%d mono_max=%d mono_nonzero=%lu\r\n",
           (int)s_debug_stats.mono_min,
           (int)s_debug_stats.mono_max,
           (unsigned long)s_debug_stats.mono_nonzero);
  debug_write_line(&debug_file, line);
  debug_write_raw_words(&debug_file);

  has_verify_error = ES8388_GetLastVerifyError(&verify_reg, &verify_expected, &verify_actual);
  snprintf(line, sizeof(line), "verify_error=%u reg=0x%02X expected=0x%02X actual=0x%02X\r\n",
           (unsigned int)has_verify_error,
           (unsigned int)verify_reg,
           (unsigned int)verify_expected,
           (unsigned int)verify_actual);
  debug_write_line(&debug_file, line);

  debug_write_es8388_registers(&debug_file);

  (void)f_sync(&debug_file);
  (void)f_close(&debug_file);
}

static void debug_write_line(FIL *file, const char *text)
{
  UINT written = 0U;
  (void)f_write(file, text, strlen(text), &written);
}

static void debug_write_es8388_registers(FIL *file)
{
  uint32_t i;
  char line[DEBUG_LINE_SIZE];
  static const uint8_t regs[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x17, 0x18, 0x19,
    0x1A, 0x1B, 0x26, 0x27, 0x2A, 0x2B, 0x2E, 0x2F, 0x30, 0x31
  };

  debug_write_line(file, "ES8388 registers\r\n");
  for (i = 0U; i < (sizeof(regs) / sizeof(regs[0])); i++)
  {
    uint8_t value = 0U;
    HAL_StatusTypeDef result = ES8388_ReadReg(regs[i], &value);
    snprintf(line, sizeof(line), "reg_0x%02X=%s0x%02X\r\n",
             (unsigned int)regs[i],
             (result == HAL_OK) ? "" : "ERR/",
             (unsigned int)value);
    debug_write_line(file, line);
  }
}

static void debug_write_raw_words(FIL *file)
{
  uint32_t i;
  char line[DEBUG_LINE_SIZE];

  snprintf(line, sizeof(line), "raw_words_count=%lu\r\n",
           (unsigned long)s_debug_stats.raw_word_count);
  debug_write_line(file, line);

  for (i = 0U; i < s_debug_stats.raw_word_count; i += 4U)
  {
    snprintf(line,
             sizeof(line),
             "raw_%02lu=%04X %04X %04X %04X\r\n",
             (unsigned long)i,
             (unsigned int)s_debug_stats.raw_words[i],
             (unsigned int)((i + 1U) < s_debug_stats.raw_word_count ? s_debug_stats.raw_words[i + 1U] : 0U),
             (unsigned int)((i + 2U) < s_debug_stats.raw_word_count ? s_debug_stats.raw_words[i + 2U] : 0U),
             (unsigned int)((i + 3U) < s_debug_stats.raw_word_count ? s_debug_stats.raw_words[i + 3U] : 0U));
    debug_write_line(file, line);
  }
}

void HAL_I2SEx_TxRxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s2)
  {
    s_dma_half_ready = 1U;
  }
}

void HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s2)
  {
    s_dma_full_ready = 1U;
  }
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s2)
  {
    if (s_i2s_stop_in_progress != 0U)
    {
      return;
    }

    s_last_hal_result = HAL_ERROR;
    s_dma_half_ready = 0U;
    s_dma_full_ready = 0U;
    s_state = AUDIO_APP_ERROR;
  }
}
