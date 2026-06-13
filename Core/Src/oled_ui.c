#include "oled_ui.h"

#include <stdio.h>
#include <string.h>
#include "audio_app.h"

#define OLED_I2C_ADDR       (0x3CU << 1)
#define OLED_WIDTH          128U
#define OLED_HEIGHT         64U
#define OLED_PAGES          (OLED_HEIGHT / 8U)
#define OLED_REFRESH_MS     200U
#define OLED_FONT_WIDTH     5U
#define OLED_FONT_SPACING   1U
#define OLED_SCL_PORT        GPIOD
#define OLED_SCL_PIN         GPIO_PIN_10
#define OLED_SDA_PORT        GPIOE
#define OLED_SDA_PIN         GPIO_PIN_13
#define OLED_I2C_DELAY_COUNT 24U

static uint8_t s_framebuffer[OLED_WIDTH * OLED_PAGES];
static uint8_t s_oled_ready;
static uint32_t s_last_refresh_ms;
static AudioApp_State s_last_state = (AudioApp_State)0xFF;

static HAL_StatusTypeDef oled_write_cmd(uint8_t cmd);
static HAL_StatusTypeDef oled_write_data(const uint8_t *data, uint16_t size);
static void oled_soft_i2c_gpio_init(void);
static HAL_StatusTypeDef oled_soft_i2c_write(uint8_t addr, const uint8_t *data, uint16_t size);
static void oled_i2c_start(void);
static void oled_i2c_stop(void);
static uint8_t oled_i2c_write_byte(uint8_t data);
static void oled_scl_high(void);
static void oled_scl_low(void);
static void oled_sda_high(void);
static void oled_sda_low(void);
static GPIO_PinState oled_sda_read(void);
static void oled_i2c_delay(void);
static void oled_clear_buffer(void);
static void oled_draw_char(uint8_t x, uint8_t page, char ch);
static void oled_draw_string(uint8_t x, uint8_t page, const char *text);
static void oled_render_state(AudioApp_State state);
static HAL_StatusTypeDef oled_flush(void);
static const uint8_t *oled_get_glyph(char ch);

void OLED_UI_Init(void)
{
  static const uint8_t init_cmds[] = {
    0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
    0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
    0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
    0x40, 0x8D, 0x14, 0xAF
  };
  uint32_t i;

  oled_soft_i2c_gpio_init();
  HAL_Delay(50);

  for (i = 0; i < (sizeof(init_cmds) / sizeof(init_cmds[0])); i++)
  {
    if (oled_write_cmd(init_cmds[i]) != HAL_OK)
    {
      s_oled_ready = 0U;
      return;
    }
  }

  s_oled_ready = 1U;
  oled_render_state(AudioApp_GetState());
  (void)oled_flush();
}

void OLED_UI_Task(void)
{
  AudioApp_State state;
  uint32_t now;

  if (s_oled_ready == 0U)
  {
    return;
  }

  now = HAL_GetTick();
  state = AudioApp_GetState();
  if ((state == s_last_state) && ((now - s_last_refresh_ms) < OLED_REFRESH_MS))
  {
    return;
  }

  oled_render_state(state);
  if (oled_flush() == HAL_OK)
  {
    s_last_state = state;
    s_last_refresh_ms = now;
  }
}

static HAL_StatusTypeDef oled_write_cmd(uint8_t cmd)
{
  uint8_t data[2];

  data[0] = 0x00U;
  data[1] = cmd;
  return oled_soft_i2c_write(OLED_I2C_ADDR, data, sizeof(data));
}

static HAL_StatusTypeDef oled_write_data(const uint8_t *data, uint16_t size)
{
  static uint8_t tx[OLED_WIDTH + 1U];

  if (size > OLED_WIDTH)
  {
    return HAL_ERROR;
  }

  tx[0] = 0x40U;
  memcpy(&tx[1], data, size);
  return oled_soft_i2c_write(OLED_I2C_ADDR, tx, (uint16_t)(size + 1U));
}

static void oled_soft_i2c_gpio_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitStruct.Pin = OLED_SCL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(OLED_SCL_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = OLED_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(OLED_SDA_PORT, &GPIO_InitStruct);

  oled_sda_high();
  oled_scl_high();
}

static HAL_StatusTypeDef oled_soft_i2c_write(uint8_t addr, const uint8_t *data, uint16_t size)
{
  uint16_t i;

  oled_i2c_start();
  if (oled_i2c_write_byte(addr) == 0U)
  {
    oled_i2c_stop();
    return HAL_ERROR;
  }

  for (i = 0U; i < size; i++)
  {
    if (oled_i2c_write_byte(data[i]) == 0U)
    {
      oled_i2c_stop();
      return HAL_ERROR;
    }
  }

  oled_i2c_stop();
  return HAL_OK;
}

static void oled_i2c_start(void)
{
  oled_sda_high();
  oled_scl_high();
  oled_i2c_delay();
  oled_sda_low();
  oled_i2c_delay();
  oled_scl_low();
}

static void oled_i2c_stop(void)
{
  oled_sda_low();
  oled_i2c_delay();
  oled_scl_high();
  oled_i2c_delay();
  oled_sda_high();
  oled_i2c_delay();
}

static uint8_t oled_i2c_write_byte(uint8_t data)
{
  uint8_t bit;
  uint8_t ack;

  for (bit = 0U; bit < 8U; bit++)
  {
    if ((data & 0x80U) != 0U)
    {
      oled_sda_high();
    }
    else
    {
      oled_sda_low();
    }
    oled_i2c_delay();
    oled_scl_high();
    oled_i2c_delay();
    oled_scl_low();
    data <<= 1;
  }

  oled_sda_high();
  oled_i2c_delay();
  oled_scl_high();
  oled_i2c_delay();
  ack = (oled_sda_read() == GPIO_PIN_RESET) ? 1U : 0U;
  oled_scl_low();

  return ack;
}

static void oled_scl_high(void)
{
  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
}

static void oled_scl_low(void)
{
  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
}

static void oled_sda_high(void)
{
  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
}

static void oled_sda_low(void)
{
  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
}

static GPIO_PinState oled_sda_read(void)
{
  return HAL_GPIO_ReadPin(OLED_SDA_PORT, OLED_SDA_PIN);
}

static void oled_i2c_delay(void)
{
  volatile uint32_t i;

  for (i = 0U; i < OLED_I2C_DELAY_COUNT; i++)
  {
    __NOP();
  }
}

static void oled_clear_buffer(void)
{
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

static void oled_draw_char(uint8_t x, uint8_t page, char ch)
{
  uint32_t i;
  const uint8_t *glyph;
  uint32_t offset;

  if ((x >= OLED_WIDTH) || (page >= OLED_PAGES))
  {
    return;
  }

  glyph = oled_get_glyph(ch);
  offset = ((uint32_t)page * OLED_WIDTH) + x;
  for (i = 0; (i < OLED_FONT_WIDTH) && ((x + i) < OLED_WIDTH); i++)
  {
    s_framebuffer[offset + i] = glyph[i];
  }
}

static void oled_draw_string(uint8_t x, uint8_t page, const char *text)
{
  uint8_t cursor = x;

  while ((*text != '\0') && (cursor < OLED_WIDTH))
  {
    oled_draw_char(cursor, page, *text);
    cursor = (uint8_t)(cursor + OLED_FONT_WIDTH + OLED_FONT_SPACING);
    text++;
  }
}

static void oled_render_state(AudioApp_State state)
{
  char line[16];

  oled_clear_buffer();
  oled_draw_string(0U, 0U, "AUDIO");

  switch (state)
  {
    case AUDIO_APP_RECORDING:
      oled_draw_string(0U, 2U, "REC: ON");
      oled_draw_string(0U, 4U, "PLAY: STOP");
      break;

    case AUDIO_APP_PLAYING:
      oled_draw_string(0U, 2U, "REC: STOP");
      oled_draw_string(0U, 4U, "PLAY: ON");
      break;

    case AUDIO_APP_ERROR:
      oled_draw_string(0U, 2U, "ERROR");
      snprintf(line, sizeof(line), "SD:%d", (int)AudioApp_GetLastFileResult());
      oled_draw_string(0U, 4U, line);
      snprintf(line, sizeof(line), "H:%d", (int)AudioApp_GetLastHalResult());
      oled_draw_string(64U, 4U, line);
      break;

    case AUDIO_APP_IDLE:
    default:
      oled_draw_string(0U, 2U, "REC: STOP");
      oled_draw_string(0U, 4U, "PLAY: STOP");
      break;
  }

  if (AudioApp_GetFxMode() == AUDIO_FX_RECORD2)
  {
    oled_draw_string(0U, 6U, "REC2");
  }
  else
  {
    snprintf(line, sizeof(line), "FX:%d", (int)AudioApp_GetFxMode());
    oled_draw_string(0U, 6U, line);
  }
}

static HAL_StatusTypeDef oled_flush(void)
{
  uint8_t page;

  for (page = 0U; page < OLED_PAGES; page++)
  {
    if (oled_write_cmd((uint8_t)(0xB0U + page)) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (oled_write_cmd(0x00U) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (oled_write_cmd(0x10U) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (oled_write_data(&s_framebuffer[(uint32_t)page * OLED_WIDTH], OLED_WIDTH) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}

static const uint8_t *oled_get_glyph(char ch)
{
  static const uint8_t glyph_space[OLED_FONT_WIDTH] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t glyph_colon[OLED_FONT_WIDTH] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t glyph_slash[OLED_FONT_WIDTH] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static const uint8_t glyph_0[OLED_FONT_WIDTH] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t glyph_1[OLED_FONT_WIDTH] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t glyph_2[OLED_FONT_WIDTH] = {0x62, 0x51, 0x49, 0x49, 0x46};
  static const uint8_t glyph_3[OLED_FONT_WIDTH] = {0x22, 0x41, 0x49, 0x49, 0x36};
  static const uint8_t glyph_4[OLED_FONT_WIDTH] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t glyph_5[OLED_FONT_WIDTH] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t glyph_6[OLED_FONT_WIDTH] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t glyph_7[OLED_FONT_WIDTH] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t glyph_8[OLED_FONT_WIDTH] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t glyph_9[OLED_FONT_WIDTH] = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t glyph_a[OLED_FONT_WIDTH] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t glyph_c[OLED_FONT_WIDTH] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t glyph_d[OLED_FONT_WIDTH] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t glyph_e[OLED_FONT_WIDTH] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t glyph_f[OLED_FONT_WIDTH] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t glyph_h[OLED_FONT_WIDTH] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static const uint8_t glyph_i[OLED_FONT_WIDTH] = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t glyph_k[OLED_FONT_WIDTH] = {0x7F, 0x08, 0x14, 0x22, 0x41};
  static const uint8_t glyph_l[OLED_FONT_WIDTH] = {0x7F, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t glyph_n[OLED_FONT_WIDTH] = {0x7F, 0x02, 0x04, 0x08, 0x7F};
  static const uint8_t glyph_o[OLED_FONT_WIDTH] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t glyph_p[OLED_FONT_WIDTH] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t glyph_r[OLED_FONT_WIDTH] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t glyph_s[OLED_FONT_WIDTH] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t glyph_t[OLED_FONT_WIDTH] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t glyph_u[OLED_FONT_WIDTH] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t glyph_x[OLED_FONT_WIDTH] = {0x63, 0x14, 0x08, 0x14, 0x63};
  static const uint8_t glyph_y[OLED_FONT_WIDTH] = {0x07, 0x08, 0x70, 0x08, 0x07};

  switch (ch)
  {
    case ' ': return glyph_space;
    case ':': return glyph_colon;
    case '/': return glyph_slash;
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'A': return glyph_a;
    case 'C': return glyph_c;
    case 'D': return glyph_d;
    case 'E': return glyph_e;
    case 'F': return glyph_f;
    case 'H': return glyph_h;
    case 'I': return glyph_i;
    case 'K': return glyph_k;
    case 'L': return glyph_l;
    case 'N': return glyph_n;
    case 'O': return glyph_o;
    case 'P': return glyph_p;
    case 'R': return glyph_r;
    case 'S': return glyph_s;
    case 'T': return glyph_t;
    case 'U': return glyph_u;
    case 'X': return glyph_x;
    case 'Y': return glyph_y;
    default: return glyph_space;
  }
}
