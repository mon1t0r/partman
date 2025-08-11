#ifndef LIBPARTMAN_CRC32_H
#define LIBPARTMAN_CRC32_H

#include "partman_types.h"

typedef pu32 pcrc32;

void crc32_init_table(void);

pcrc32 crc32_init(void);

void crc32_compute8(pcrc32 *crc32, pu8 i);

void crc32_compute16(pcrc32 *crc32, pu16 i);

void crc32_compute32(pcrc32 *crc32, pu32 i);

void crc32_compute64(pcrc32 *crc32, pu64 i);

void crc32_finalize(pcrc32 *crc32);

#endif
