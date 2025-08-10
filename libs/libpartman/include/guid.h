#ifndef LIBPARTMAN_GUID_H
#define LIBPARTMAN_GUID_H

#include "partman_types.h"

/* Globally Unique Identifier (GUID) structure */
struct guid {

    pu32 time_lo;
    pu16 time_mid;
    pu16 time_hi_ver;
    pu8 cl_seq_hi_res;
    pu8 cl_seq_lo;
    pu8 node[6];
};

#endif
