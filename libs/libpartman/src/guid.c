#include <stdlib.h>
#include <stdio.h>

#include "guid.h"
#include "memutils.h"

static pu8 rand_8(void)
{
    return rand() / (RAND_MAX / 0x100 + 1);
}

static pu16 rand_16(void)
{
    return (((pu16) rand_8()) << 0 ) |
           (((pu16) rand_8()) << 8 );
}

static pu32 rand_32(void)
{
    return (((pu32) rand_8()) << 0 ) |
           (((pu32) rand_8()) << 8 ) |
           (((pu32) rand_8()) << 16) |
           (((pu32) rand_8()) << 24);
}

void guid_create(struct guid *guid)
{
    /* Create a random Version 4 Variant 2 GUID */
    int i;

    guid->time_lo       = rand_32();
    guid->time_mid      = rand_16();
    guid->time_hi_ver   = rand_16();
    guid->cl_seq_hi_res = rand_8();
    guid->cl_seq_lo     = rand_8();

    for(i = 0; i < ARRAY_SIZE(guid->node); i++) {
        guid->node[i] = rand_8();
    }

    /* Version 4 */
    guid->time_hi_ver &= ~(0xF << 12);
    guid->time_hi_ver |=  (0x4 << 12);

    /* Variant 2 */
    guid->cl_seq_hi_res &= ~(0x7 << 5);
    guid->cl_seq_hi_res |=  (0x6 << 5);
}

void guid_write(pu8 *buf, const struct guid *guid)
{
    int i;

    write_pu32(buf,     guid->time_lo      );
    write_pu16(buf + 4, guid->time_mid     );
    write_pu16(buf + 6, guid->time_hi_ver  );
    write_pu8 (buf + 8, guid->cl_seq_hi_res);
    write_pu8 (buf + 9, guid->cl_seq_lo    );

    for(i = 0; i < ARRAY_SIZE(guid->node); i++) {
        write_pu8(buf + 10 + i, guid->node[i]);
    }
}

void guid_read(const pu8 *buf, struct guid *guid)
{
    int i;

    guid->time_lo       = read_pu32(buf    );
    guid->time_mid      = read_pu16(buf + 4);
    guid->time_hi_ver   = read_pu16(buf + 6);
    guid->cl_seq_hi_res = read_pu8 (buf + 8);
    guid->cl_seq_lo     = read_pu8 (buf + 9);

    for(i = 0; i < ARRAY_SIZE(guid->node); i++) {
        guid->node[i] = read_pu8(buf + 10 + i);
    }
}

void guid_crc_compute(pcrc32 *crc32, const struct guid *guid)
{
    int i;

    crc32_compute32(crc32, guid->time_lo);
    crc32_compute16(crc32, guid->time_mid);
    crc32_compute16(crc32, guid->time_hi_ver);
    crc32_compute8(crc32, guid->cl_seq_hi_res);
    crc32_compute8(crc32, guid->cl_seq_lo);

    for(i = 0; i < ARRAY_SIZE(guid->node); i++) {
        crc32_compute8(crc32, guid->node[i]);
    }
}

pflag guid_is_zero(const struct guid *guid)
{
    int i;
    pu32 flag;

    flag = guid->time_lo | guid->time_mid | guid->time_hi_ver |
           guid->cl_seq_hi_res | guid->cl_seq_lo;

    for(i = 0; i < ARRAY_SIZE(guid->node); i++) {
        flag |= guid->node[i];
    }

    return flag ? 0 : 1;
}

void guid_to_str(char *buf, const struct guid *guid)
{
    /* Registry format GUID string representation */

    sprintf(buf, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            (unsigned int) guid->time_lo, guid->time_mid, guid->time_hi_ver,
            guid->cl_seq_hi_res, guid->cl_seq_lo, guid->node[0], guid->node[1],
            guid->node[2], guid->node[3], guid->node[4], guid->node[5]);
}

