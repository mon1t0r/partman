#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

#include "partman_types.h"
#include "schem.h"

/* Maximum supported partition count for MBR */
#define PARTMAN_SCHEM_MAX_PART_CNT_MBR ARRAY_SIZE \
    (((struct mbr *) 0)->partitions)

/* MBR partition structure */
struct mbr_part {
    /* Boot indicator */
    pu8 boot_ind;

    /* CHS address of first absolute sector in partition */
    pchs start_chs;

    /* Partition type */
    pu8 type;

    /* CHS address of last absolute sector in partition */
    pchs end_chs;

    /* LBA of first absolute sector in the partition */
    plba_mbr start_lba;

    /* Number of sectors in partition */
    plba_mbr sz_lba;
};

/* Master Boot Record (MBR) structure */
struct mbr {
    /* Unique disk signature */
    pu32 disk_sig;

    /* Partition table (4 primary partitions) */
    struct mbr_part partitions[4];
};

void schem_init_mbr(struct schem *schem, const struct img_ctx *img_ctx);

void schem_part_init_mbr(struct schem_part *part);

pflag schem_part_is_used_mbr(const struct schem_part *part);

enum schem_load_res
schem_load_mbr(struct schem *schem, const struct img_ctx *img_ctx);

pres schem_save_mbr(const struct schem *schem, const struct img_ctx *img_ctx);

pres mbr_save(const struct mbr *mbr, const struct img_ctx *img_ctx);

enum schem_load_res mbr_load(struct mbr *mbr, const struct img_ctx *img_ctx);

void mbr_init_protective(struct mbr *mbr, const struct img_ctx *img_ctx);

pflag mbr_is_protective(const struct mbr *mbr);

#endif

