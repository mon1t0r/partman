#include <stdint.h>
#include <stddef.h>

#include "crc32.h"
#include "log.h"

/* Initialized to 0s */
static pcrc32 crc_table[256];

static void crc32_init_table(void)
{
    unsigned int i, j;
    pcrc32 crc32;

    crc32 = 1;

    for(i = 128; i; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xEDB88320 : 0);
        for(j = 0; j < 256; j += 2 * i) {
            crc_table[i + j] = crc32 ^ crc_table[j];
        }
    }

    plog_dbg("CRC32 table generated");
}

pcrc32 crc32_init(void)
{
    /* CRC32 initial reminder */
    return 0xFFFFFFFFu;
}

void crc32_finalize(pcrc32 *crc32)
{
    /* CRC32 final XOR value */
    *crc32 ^= 0xFFFFFFFFu;
}

void crc32_compute8(pcrc32 *crc32, pu8 i)
{
    if(crc_table[0] == 0) {
        crc32_init_table();
    }

    *crc32 ^= i;
    *crc32 = (*crc32 >> 8) ^ crc_table[*crc32 & 0xFF];
}

void crc32_compute16(pcrc32 *crc32, pu16 i)
{
    crc32_compute8(crc32, (i >> 0) & 0xFF);
    crc32_compute8(crc32, (i >> 8) & 0xFF);
}

void crc32_compute32(pcrc32 *crc32, pu32 i)
{
    crc32_compute8(crc32, (i >> 0 ) & 0xFF);
    crc32_compute8(crc32, (i >> 8 ) & 0xFF);
    crc32_compute8(crc32, (i >> 16) & 0xFF);
    crc32_compute8(crc32, (i >> 24) & 0xFF);
}

void crc32_compute64(pcrc32 *crc32, pu64 i)
{
    crc32_compute8(crc32, (i >> 0 ) & 0xFF);
    crc32_compute8(crc32, (i >> 8 ) & 0xFF);
    crc32_compute8(crc32, (i >> 16) & 0xFF);
    crc32_compute8(crc32, (i >> 24) & 0xFF);
    crc32_compute8(crc32, (i >> 32) & 0xFF);
    crc32_compute8(crc32, (i >> 40) & 0xFF);
    crc32_compute8(crc32, (i >> 48) & 0xFF);
    crc32_compute8(crc32, (i >> 56) & 0xFF);
}

