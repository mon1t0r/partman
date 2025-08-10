#include <stdlib.h>

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

    guid->time_lo = rand_32();
    guid->time_mid = rand_16();
    guid->time_hi_ver = rand_16();
    guid->cl_seq_hi_res = rand_8();
    guid->cl_seq_lo = rand_8();

    for(i = 0; i < sizeof(guid->node) / sizeof(guid->node[0]); i++) {
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

    write_pu32(buf, guid->time_lo);
    write_pu16(buf + 4, guid->time_mid);
    write_pu16(buf + 6, guid->time_hi_ver);
    write_pu8(buf + 8, guid->cl_seq_hi_res);
    write_pu8(buf + 9, guid->cl_seq_lo);

    for(i = 0; i < sizeof(guid->node) / sizeof(guid->node[0]); i++) {
        write_pu8(buf + 10 + i, guid->node[i]);
    }
}

void guid_read(const pu8 *buf, struct guid *guid)
{
    int i;

    guid->time_lo = read_pu32(buf);
    guid->time_mid = read_pu16(buf + 4);
    guid->time_hi_ver = read_pu16(buf + 6);
    guid->cl_seq_hi_res = read_pu8(buf + 8);
    guid->cl_seq_lo = read_pu8(buf + 9);

    for(i = 0; i < sizeof(guid->node) / sizeof(guid->node[0]); i++) {
        guid->node[i] = read_pu8(buf + 10 + i);
    }
}

