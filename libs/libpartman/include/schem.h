#ifndef LIBPARTMAN_SCHEME_H
#define LIBPARTMAN_SCHEME_H

#include "mbr.h"
#include "gpt.h"
#include "img_ctx.h"

/* Partitioning scheme type */
enum schem_type {
    schem_none, schem_mbr, schem_gpt
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

pres schem_change_type(struct schem_ctx *schem_ctx,
                       const struct img_ctx *img_ctx, enum schem_type type);

void schem_sync(struct schem_ctx *schem_ctx);

pres schem_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx);

pres schem_save(const struct schem_ctx *schem_ctx,
                const struct img_ctx *img_ctx);

#endif
