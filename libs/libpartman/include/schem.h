#ifndef LIBPARTMAN_SCHEME_H
#define LIBPARTMAN_SCHEME_H

#include "mbr.h"
#include "gpt.h"
#include "img_ctx.h"

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

/* Abstract partition structure */
struct schem_part {
    /* Partition start LBA */
    plba start_lba;

    /* Partition end LBA */
    plba end_lba;
};

/* Structure, which contains pointers to common scheme functions.
 * Scheme load is not present here, as at the scheme loading time
 * we do not know, which scheme will be loaded */
struct schem_funcs {
    pres (*init    )(union schem *, const struct img_ctx *);
    void (*sync    )(union schem *);
    pres (*set_part)(union schem *, pu8, const struct schem_part *);
    pres (*save    )(const union schem *, const struct img_ctx *);
    void (*free    )(union schem *);
};

struct schem_ctx {
    /* Partitioning scheme type */
    enum schem_type type;

    /* Partitioning scheme used functions, depends on type */
    struct schem_funcs funcs;

    /* Partitioning scheme used, depends on type */
    union schem s;
};

void schem_init(struct schem_ctx *schem_ctx);

pres schem_change_type(struct schem_ctx *schem_ctx,
                       const struct img_ctx *img_ctx, enum schem_type type);

void schem_sync(struct schem_ctx *schem_ctx);

pres
schem_set_part(struct schem_ctx *schem_ctx, pu8 index,
               const struct schem_part *part);

pres schem_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx);

pres schem_save(const struct schem_ctx *schem_ctx,
                const struct img_ctx *img_ctx);

#endif
