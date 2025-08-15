#ifndef LIBPARTMAN_GUID_H
#define LIBPARTMAN_GUID_H

#include "partman_types.h"
#include "crc32.h"

/* Globally Unique Identifier (GUID) structure */
struct guid {
    pu32 time_lo;
    pu16 time_mid;
    pu16 time_hi_ver;
    pu8 cl_seq_hi_res;
    pu8 cl_seq_lo;
    pu8 node[6];
};

void guid_create(struct guid *guid);

void guid_write(pu8 *buf, const struct guid *guid);

void guid_read(const pu8 *buf, struct guid *guid);

void guid_crc_compute(pcrc32 *crc32, const struct guid *guid);

pflag guid_is_zero(const struct guid *guid);

#endif
