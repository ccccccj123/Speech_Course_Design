#ifndef __WAV_H__
#define __WAV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ff.h"

#define WAV_HEADER_SIZE 44U

void WAV_BuildHeader(uint8_t header[WAV_HEADER_SIZE],
                     uint32_t sample_rate,
                     uint16_t bits_per_sample,
                     uint16_t channels,
                     uint32_t data_bytes);
FRESULT WAV_WriteHeader(FIL *file,
                        uint32_t sample_rate,
                        uint16_t bits_per_sample,
                        uint16_t channels,
                        uint32_t data_bytes);
FRESULT WAV_UpdateHeader(FIL *file,
                         uint32_t sample_rate,
                         uint16_t bits_per_sample,
                         uint16_t channels,
                         uint32_t data_bytes);

#ifdef __cplusplus
}
#endif

#endif
