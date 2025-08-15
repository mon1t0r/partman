#ifndef LIBPARTMAN_PART_SCHEM_H
#define LIBPARTMAN_PART_SCHEM_H

#include "mbr.h"
#include "gpt.h"

enum schem_type {
    schem_mbr, schem_gpt
};

struct schem_ctx {
    /* Partitioning scheme type */
    enum schem_type type;

    /* Partitioning scheme used, depends on type */
    union {
        struct schem_ctx_mbr s_mbr;
        struct schem_ctx_gpt s_gpt;
    } s;
};

#endif
