#ifndef LIBPARTMAN_SCHEME_H
#define LIBPARTMAN_SCHEME_H

#include "mbr.h"
#include "gpt.h"
#include "img_ctx.h"

/* === Scheme definitions === */

/* MBR partitioning scheme */
struct schem_mbr {
    /* In-memory MBR structure */
    struct mbr mbr;
};

/* GPT partitioning scheme */
struct schem_gpt {
    /* Protective MBR partitioning scheme */
    struct schem_mbr mbr_prot;

    /* In-memory GPT primary header structure */
    struct gpt_hdr hdr_prim;

    /* In-memory GPT primary table structure */
    struct gpt_part_ent *table_prim;

    /* In-memory GPT secondary header structure */
    struct gpt_hdr hdr_sec;

    /* In-memory GPT secondary table structure */
    struct gpt_part_ent *table_sec;
};

/* Partitioning scheme type */
enum schem_type {
    schem_type_none, schem_type_mbr, schem_type_gpt
};

/* Partitioning scheme union */
union schem {
    struct schem_mbr s_mbr;
    struct schem_gpt s_gpt;
};

/* === Scheme internals === */

/* Abstract partition structure */
struct schem_part {
    /* Partition start LBA */
    plba start_lba;

    /* Partition end LBA */
    plba end_lba;
};

typedef
pres (*schem_func_init) (
    union schem             *schem,
    const struct img_ctx    *img_ctx
);

typedef
void (*schem_func_sync) (
    union schem             *schem
);

typedef
pu32 (*schem_func_get_part_cnt) (
    const union schem       *schem
);

typedef
pflag (*schem_func_is_part_used) (
    const union schem       *schem,
    pu32                    index
);

typedef
void (*schem_func_get_part) (
    const union schem       *schem,
    pu32                    index,
    struct schem_part       *part
);

typedef
void (*schem_func_set_part) (
    union schem             *schem,
    const struct img_ctx    *img_ctx,
    pu32                    index,
    const struct schem_part *part
);

typedef
void (*schem_func_new_part) (
    union schem             *schem,
    pu32                    index
);

typedef
pres (*schem_func_save) (
    const union schem       *schem,
    const struct img_ctx    *img_ctx
);

typedef
void (*schem_func_free) (
    union schem             *schem
);

/* Pointers to common scheme functions, which can be called from outside */
struct schem_funcs {
    schem_func_sync sync;
    schem_func_get_part_cnt get_part_cnt;
    schem_func_is_part_used is_part_used;
    schem_func_set_part set_part;
    schem_func_get_part get_part;
    schem_func_new_part new_part;
    schem_func_save save;
};

/* Pointers to common scheme functions, which are internal to scheme context */
struct schem_funcs_int {
    schem_func_init init;
    schem_func_free free;
};

struct schem_ctx {
    /* Partitioning scheme type */
    enum schem_type type;

    /* Partitioning scheme public used functions, depends on type */
    struct schem_funcs funcs;

    /* Partitioning scheme internal used functions, depends on type */
    struct schem_funcs_int funcs_int;

    /* Partitioning scheme used, depends on type */
    union schem s;
};

void schem_init(struct schem_ctx *schem_ctx);

pres schem_change_type(struct schem_ctx *schem_ctx,
                       const struct img_ctx *img_ctx, enum schem_type type);

pres schem_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx);

#endif
