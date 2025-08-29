#ifndef LIBPARTMAN_SCHEM_H
#define LIBPARTMAN_SCHEM_H

#include "img_ctx.h"
#include "guid.h"

/* Partitioning scheme type */
enum schem_type {
    schem_type_none, schem_type_mbr, schem_type_gpt
};

/* Partitioning scheme load result */
enum schem_load_res {
    schem_load_ok, schem_load_not_found, schem_load_fatal
};

struct schem;

struct schem_part;

/* Partitioning scheme function type definitions */
typedef void (*schem_func_init) (
    struct schem            *schem,
    const struct img_ctx    *img_ctx
);

typedef void (*schem_func_part_init) (
    struct schem_part       *part
);

typedef pflag (*schem_func_part_is_used) (
    const struct schem_part *part
);

typedef enum schem_load_res (*schem_func_load) (
    struct schem            *schem,
    const struct img_ctx    *img_ctx
);

typedef pres (*schem_func_save) (
    const struct schem      *schem,
    const struct img_ctx    *img_ctx
);

/* Unified scheme functions structure (contains function pointers) */
struct schem_funcs {
    schem_func_init         init;
    schem_func_part_init    part_init;
    schem_func_part_is_used part_is_used;
    schem_func_load         load;
    schem_func_save         save;
};

/* Unified scheme structure */
struct schem {
    /* Scheme type */
    enum schem_type type;

    /* Disk identifier: integer (MBR) or GUID (GPT) */
    union {
        pu32 i;
        struct guid guid;
    } id;

    /* First usable LBA */
    plba first_usable_lba;

    /* Last usable LBA */
    plba last_usable_lba;

    /* Total partition count (with unsued partitions) */
    pu32 part_cnt;

    /* Partition table */
    struct schem_part *table;

    struct schem_funcs funcs;
};

/* Unified scheme partition structure */
struct schem_part {
    /* Partition type: integer (MBR) or GUID (GPT) */
    union {
        pu8 i;
        struct guid guid;
    } type;

    /* GPT: Partition GUID */
    struct guid unique_guid;

    /* Partition start LBA */
    plba start_lba;

    /* Partition end LBA */
    plba end_lba;

    /* GPT: Partition attributes */
    pu64 attr;

    /* GPT: Name of the partition, using UCS-2 */
    pchar_ucs name[36];

    /* MBR: Partition boot indicator */
    pu8 boot_ind;
};

void schem_init(struct schem *schem);

pres schem_change_type(struct schem *schem, const struct img_ctx *img_ctx,
                       enum schem_type type);

pres schem_load(struct schem *schem, const struct img_ctx *img_ctx);

void schem_part_delete(struct schem_part *part);

p32 schem_find_overlap(const struct schem *schem,
                       const struct schem_part *part, p32 part_ign);

p32 schem_find_part_index(const struct schem *schem, pflag part_used);

plba_res schem_find_start_sector(const struct schem *schem,
                                 const struct img_ctx *img_ctx, p32 part_ign);

plba_res schem_find_last_sector(const struct schem *schem,
                                const struct img_ctx *img_ctx, p32 part_ign,
                                plba first_lba);

#endif

