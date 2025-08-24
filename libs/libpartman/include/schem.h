#ifndef LIBPARTMAN_SCHEME_H
#define LIBPARTMAN_SCHEME_H

#include "mbr.h"
#include "gpt.h"
#include "img_ctx.h"

/* Macro to be used on scheme functions, which may not be implemented */
#define CALL_FUNC_NORET(func, args) \
do {                          \
    if(func) {                \
        func args;            \
    }                         \
} while(0)

#define CALL_FUNC_NORET1(func, arg1) \
CALL_FUNC_NORET(func, (arg1))

#define CALL_FUNC_NORET2(func, arg1, arg2) \
CALL_FUNC_NORET(func, (arg1, arg2))

#define CALL_FUNC_NORET3(func, arg1, arg2, arg3) \
CALL_FUNC_NORET(func, (arg1, arg2, arg3))

#define CALL_FUNC_NORET4(func, arg1, arg2, arg3, arg4) \
CALL_FUNC_NORET(func, (arg1, arg2, arg3, arg4))

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

/* Scheme info structure */
struct schem_info {
    /* First usable LBA */
    plba first_usable_lba;

    /* Last usable LBA */
    plba last_usable_lba;

    /* Partition count */
    pu32 part_cnt;
};

/* Scheme function types */
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
void (*schem_func_get_info) (
    const union schem       *schem,
    const struct img_ctx    *img_ctx,
    struct schem_info       *info
);

typedef
pflag (*schem_func_part_is_used) (
    const union schem       *schem,
    pu32                    index
);

typedef
void (*schem_func_part_new) (
    union schem             *schem,
    pu32                    index
);

typedef
void (*schem_func_part_delete) (
    union schem             *schem,
    pu32                    index
);

typedef
void (*schem_func_part_get) (
    const union schem       *schem,
    pu32                    index,
    struct schem_part       *part
);

typedef
void (*schem_func_part_set) (
    union schem             *schem,
    const struct img_ctx    *img_ctx,
    pu32                    index,
    const struct schem_part *part
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

/* Functions 'sync', 'init' and 'free' are not required to be implemented */

/* Pointers to common scheme functions, which can be called from outside */
struct schem_funcs {
    schem_func_sync sync;
    schem_func_get_info get_info;
    schem_func_part_is_used part_is_used;
    schem_func_part_new part_new;
    schem_func_part_delete part_delete;
    schem_func_part_get part_get;
    schem_func_part_set part_set;
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

p32 schem_find_overlap(const struct schem_ctx *schem_ctx, pu32 part_cnt,
                       const struct schem_part *part, p32 part_ign);

p32 schem_find_part_index(const struct schem_ctx *schem_ctx, pu32 part_cnt,
                          pflag part_used);

plba_res schem_find_start_sector(const struct schem_ctx *schem_ctx,
                                 const struct img_ctx *img_ctx,
                                 const struct schem_info *info, p32 part_ign);

plba_res schem_find_last_sector(const struct schem_ctx *schem_ctx,
                                const struct img_ctx *img_ctx,
                                const struct schem_info *info, p32 part_ign,
                                plba first_lba);

#endif
