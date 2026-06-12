#include "wav.h"

#include <string.h>

static void put_u16_le(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8) & 0xFFU);
  dst[2] = (uint8_t)((value >> 16) & 0xFFU);
  dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

void WAV_BuildHeader(uint8_t header[WAV_HEADER_SIZE],
                     uint32_t sample_rate,
                     uint16_t bits_per_sample,
                     uint16_t channels,
                     uint32_t data_bytes)
{
  uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8U;
  uint16_t block_align = (uint16_t)(channels * bits_per_sample / 8U);

  const uint8_t template_header[44] = {
    0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,
    0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x80, 0x3E, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00,
    0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,
    0x00, 0x00, 0x00, 0x00
  };

  memcpy(header, template_header, WAV_HEADER_SIZE);
  put_u32_le(&header[4], data_bytes + 36U);
  put_u16_le(&header[22], channels);
  put_u32_le(&header[24], sample_rate);
  put_u32_le(&header[28], byte_rate);
  put_u16_le(&header[32], block_align);
  put_u16_le(&header[34], bits_per_sample);
  put_u32_le(&header[40], data_bytes);
}

FRESULT WAV_WriteHeader(FIL *file,
                        uint32_t sample_rate,
                        uint16_t bits_per_sample,
                        uint16_t channels,
                        uint32_t data_bytes)
{
  UINT written = 0;
  FRESULT result;
  uint8_t header[WAV_HEADER_SIZE];

  WAV_BuildHeader(header, sample_rate, bits_per_sample, channels, data_bytes);
  result = f_write(file, header, WAV_HEADER_SIZE, &written);
  if (result != FR_OK)
  {
    return result;
  }

  return (written == WAV_HEADER_SIZE) ? FR_OK : FR_DISK_ERR;
}

FRESULT WAV_UpdateHeader(FIL *file,
                         uint32_t sample_rate,
                         uint16_t bits_per_sample,
                         uint16_t channels,
                         uint32_t data_bytes)
{
  FRESULT result;

  result = f_lseek(file, 0);
  if (result != FR_OK)
  {
    return result;
  }

  result = WAV_WriteHeader(file, sample_rate, bits_per_sample, channels, data_bytes);
  if (result != FR_OK)
  {
    return result;
  }

  return f_lseek(file, f_size(file));
}
