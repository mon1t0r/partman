#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

#include "partman_types.h"
#include "img_ctx.h"

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

/* MBR partitioning scheme context */
struct schem_ctx_mbr {
    /* In-memory MBR structure */
    struct mbr mbr;

    /* Pointer to a memory region, which maps image MBR region */
    pu8 *mbr_reg;
};

void mbr_write(pu8 *buf, const struct mbr *mbr);

void mbr_read(const pu8 *buf, struct mbr *mbr);

pflag mbr_is_present(const pu8 *buf);

pflag mbr_is_part_used(const struct mbr_part *part);

pres mbr_map(struct schem_ctx_mbr *schem_ctx, const struct img_ctx *img_ctx);

pres mbr_unmap(struct schem_ctx_mbr *schem_ctx, const struct img_ctx *img_ctx);

void mbr_load(struct schem_ctx_mbr *schem_ctx);

void mbr_save(struct schem_ctx_mbr *schem_ctx);

#endif
