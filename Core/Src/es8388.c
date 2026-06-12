#include "es8388.h"

typedef struct
{
  uint8_t reg;
  uint8_t value;
} ES8388_RegValue;

static I2C_HandleTypeDef *s_es8388_i2c;
static uint8_t s_verify_error_reg;
static uint8_t s_verify_error_expected;
static uint8_t s_verify_error_actual;

static HAL_StatusTypeDef verify_reg(uint8_t reg, uint8_t expected);

HAL_StatusTypeDef ES8388_WriteReg(uint8_t reg, uint8_t value)
{
  if (s_es8388_i2c == 0)
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Write(s_es8388_i2c,
                           ES8388_I2C_ADDR,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1,
                           100);
}

HAL_StatusTypeDef ES8388_ReadReg(uint8_t reg, uint8_t *value)
{
  if ((s_es8388_i2c == 0) || (value == 0))
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(s_es8388_i2c,
                          ES8388_I2C_ADDR,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1,
                          100);
}

HAL_StatusTypeDef ES8388_SetVolume(uint8_t volume)
{
  uint8_t value = volume;

  if (value > 33U)
  {
    value = 33U;
  }

  if (ES8388_WriteReg(0x2E, value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (ES8388_WriteReg(0x2F, value) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (ES8388_WriteReg(0x30, value) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return ES8388_WriteReg(0x31, value);
}

uint8_t ES8388_GetLastVerifyError(uint8_t *reg, uint8_t *expected, uint8_t *actual)
{
  if (reg != 0)
  {
    *reg = s_verify_error_reg;
  }
  if (expected != 0)
  {
    *expected = s_verify_error_expected;
  }
  if (actual != 0)
  {
    *actual = s_verify_error_actual;
  }

  return (s_verify_error_reg != 0U) ? 1U : 0U;
}

HAL_StatusTypeDef ES8388_Init(I2C_HandleTypeDef *hi2c)
{
  uint32_t i;
  static const ES8388_RegValue init_regs[] = {
    {0x00, 0x12}, /* CONTROL1: enable codec analog reference path after reset. */
    {0x01, 0x50}, /* CONTROL2: VMID on, normal bias. */
    {0x02, 0x00}, /* CHIPPOWER: enable clocks and analog blocks. */
    {0x03, 0x09}, /* ADCPOWER: enable ADC line inputs; board provides external MIC bias. */
    {0x04, 0x00}, /* DACPOWER: power up DAC and analog output path. */
    {0x08, 0x00}, /* MASTERMODE: slave mode, STM32 provides MCLK/BCLK/LRCK. */
    {0x09, 0x66}, /* ADCCONTROL1: moderate left/right microphone PGA gain after ADC power fix. */
    {0x0A, 0x00}, /* ADCCONTROL2: select onboard MIC on LIN1/RIN1 per schematic. */
    {0x0B, 0x02}, /* ADCCONTROL3: ADC normal stereo path. */
    {0x0C, 0x0C}, /* ADCCONTROL4: ADC I2S Philips, 16-bit word length. */
    {0x0D, 0x02}, /* ADCCONTROL5: ADC normal clocking/unmute path. */
    {0x10, 0x00}, /* ADCCONTROL8: keep ADC path in normal mode. */
    {0x11, 0x00}, /* DACCONTROL2: DAC normal polarity and unmute. */
    {0x17, 0x18}, /* DACCONTROL8: deemphasis off, normal DAC path. */
    {0x18, 0x02}, /* DACCONTROL9: DAC LRCK divider for 256fs clocking. */
    {0x19, 0x00}, /* DACCONTROL10: DAC unmute. */
    {0x1A, 0x00}, /* DACCONTROL11: left DAC digital volume, 0 dB. */
    {0x1B, 0x00}, /* DACCONTROL12: right DAC digital volume, 0 dB. */
    {0x26, 0x00}, /* DACCONTROL17: mixer normal. */
    {0x27, 0xB8}, /* DACCONTROL18: route LDAC to left output mixer. */
    {0x2A, 0xB8}, /* DACCONTROL20: route RDAC to right output mixer. */
    {0x2B, 0x80}, /* DACCONTROL21: use ADC LRCK internally for I2S clock alignment. */
    {0x2E, 0x21}, /* LOUT1 volume for headphone left. */
    {0x2F, 0x21}, /* ROUT1 volume for headphone right. */
    {0x30, 0x21}, /* LOUT2 volume for speaker amp path. */
    {0x31, 0x21}  /* ROUT2 volume for speaker amp path. */
  };

  if (hi2c == 0)
  {
    return HAL_ERROR;
  }

  s_es8388_i2c = hi2c;
  s_verify_error_reg = 0U;
  s_verify_error_expected = 0U;
  s_verify_error_actual = 0U;

  if (ES8388_WriteReg(0x00, 0x80) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(20);

  if (ES8388_WriteReg(0x00, 0x00) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(20);

  for (i = 0; i < (sizeof(init_regs) / sizeof(init_regs[0])); i++)
  {
    if (ES8388_WriteReg(init_regs[i].reg, init_regs[i].value) != HAL_OK)
    {
      return HAL_ERROR;
    }
    HAL_Delay(2);
  }

  (void)verify_reg(0x00, 0x12); /* analog reference path */
  (void)verify_reg(0x08, 0x00); /* slave mode */
  (void)verify_reg(0x0B, 0x02); /* ADC I2S format */
  (void)verify_reg(0x03, 0x09); /* ADC power and board MIC bias mode */
  (void)verify_reg(0x09, 0x66); /* ADC PGA gain */
  (void)verify_reg(0x0A, 0x00); /* ADC input select */
  (void)verify_reg(0x0C, 0x0C); /* ADC I2S 16-bit format */
  (void)verify_reg(0x0D, 0x02); /* ADC unmute */
  (void)verify_reg(0x10, 0x00); /* ADC normal mode */
  (void)verify_reg(0x19, 0x00); /* DAC unmute */
  (void)verify_reg(0x1A, 0x00); /* left DAC volume */
  (void)verify_reg(0x1B, 0x00); /* right DAC volume */
  (void)verify_reg(0x27, 0xB8); /* left DAC output route */
  (void)verify_reg(0x2A, 0xB8); /* right DAC output route */
  (void)verify_reg(0x2B, 0x80); /* ADC/DAC LRCK alignment */
  (void)verify_reg(0x2E, 0x21); /* LOUT1 volume */
  (void)verify_reg(0x2F, 0x21); /* ROUT1 volume */
  (void)verify_reg(0x30, 0x21); /* LOUT2 volume */
  (void)verify_reg(0x31, 0x21); /* ROUT2 volume */

  return HAL_OK;
}

static HAL_StatusTypeDef verify_reg(uint8_t reg, uint8_t expected)
{
  uint8_t actual = 0U;

  if (ES8388_ReadReg(reg, &actual) != HAL_OK)
  {
    s_verify_error_reg = reg;
    s_verify_error_expected = expected;
    s_verify_error_actual = 0xFFU;
    return HAL_ERROR;
  }

  if (actual != expected)
  {
    s_verify_error_reg = reg;
    s_verify_error_expected = expected;
    s_verify_error_actual = actual;
    return HAL_ERROR;
  }

  return HAL_OK;
}
