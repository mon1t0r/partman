#include <stdio.h>
#include <string.h>

#include "guid.h"
#include "memutils.h"
#include "rand.h"

void guid_create(struct guid *guid)
{
    /* Create a random Version 4 Variant 2 GUID */
    int i;

    guid->time_lo       = rand_32();
    guid->time_mid      = rand_16();
    guid->time_hi_ver   = rand_16();
    guid->cl_seq_hi_res = rand_8();
    guid->cl_seq_lo     = rand_8();

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        guid->nodes[i] = rand_8();
    }

    /* Version 4 */
    guid->time_hi_ver &= ~(0xF << 12);
    guid->time_hi_ver |=  (0x4 << 12);

    /* Variant 2 */
    guid->cl_seq_hi_res &= ~(0x7 << 5);
    guid->cl_seq_hi_res |=  (0x6 << 5);
}

void guid_read(const pu8 *buf, struct guid *guid)
{
    int i;

    guid->time_lo       = read_pu32(buf);
    guid->time_mid      = read_pu16(buf + 4);
    guid->time_hi_ver   = read_pu16(buf + 6);
    guid->cl_seq_hi_res = read_pu8 (buf + 8);
    guid->cl_seq_lo     = read_pu8 (buf + 9);

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        guid->nodes[i] = read_pu8(buf + 10 + i);
    }
}

void guid_write(pu8 *buf, const struct guid *guid)
{
    int i;

    write_pu32(buf,     guid->time_lo);
    write_pu16(buf + 4, guid->time_mid);
    write_pu16(buf + 6, guid->time_hi_ver);
    write_pu8 (buf + 8, guid->cl_seq_hi_res);
    write_pu8 (buf + 9, guid->cl_seq_lo);

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        write_pu8(buf + 10 + i, guid->nodes[i]);
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

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        crc32_compute8(crc32, guid->nodes[i]);
    }
}

pflag guid_is_zero(const struct guid *guid)
{
    int i;
    pu32 flag;

    flag = guid->time_lo | guid->time_mid | guid->time_hi_ver |
           guid->cl_seq_hi_res | guid->cl_seq_lo;

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        flag |= guid->nodes[i];
    }

    return flag ? 0 : 1;
}

void guid_to_str(char *buf, const struct guid *guid)
{
    /* Registry format GUID string representation */

    sprintf(buf, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid->time_lo, guid->time_mid, guid->time_hi_ver,
            guid->cl_seq_hi_res, guid->cl_seq_lo, guid->nodes[0],
            guid->nodes[1], guid->nodes[2], guid->nodes[3], guid->nodes[4],
            guid->nodes[5]);
}

pres str_to_guid(const char *buf, struct guid *guid)
{
    pu32 time_lo, time_mid, time_hi_ver, cl_seq_hi_res, cl_seq_lo;
    pu32 nodes[6];
    int i;

    /* Registry format GUID string representation */

    /* Registry format GUID string is 36 chars long  */
    if(strlen(buf) != 36) {
        return pres_fail;
    }

    i = sscanf(buf,
               " %08lX-%04lX-%04lX-%02lX%02lX-%02lX%02lX%02lX%02lX%02lX%02lX",
               &time_lo, &time_mid, &time_hi_ver, &cl_seq_hi_res, &cl_seq_lo,
               &nodes[0], &nodes[1], &nodes[2], &nodes[3], &nodes[4],
               &nodes[5]);

    if(i != 11) {
        return pres_fail;
    }

    guid->time_lo = time_lo;
    guid->time_mid = time_mid;
    guid->time_hi_ver = time_hi_ver;
    guid->cl_seq_hi_res = cl_seq_hi_res;
    guid->cl_seq_lo = cl_seq_lo;

    for(i = 0; i < ARRAY_SIZE(guid->nodes); i++) {
        guid->nodes[i] = nodes[i];
    }

    return pres_ok;
}

