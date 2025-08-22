#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

#include "partman_types.h"
#include "img_ctx.h"

enum {
    /* MBR size, in bytes */
    mbr_sz = 512
};

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

enum mbr_load_res {
    mbr_load_ok, mbr_load_not_found, mbr_load_fatal
};

void mbr_init_new(struct mbr *mbr);

void mbr_init_protective(struct mbr *mbr, const struct img_ctx *img_ctx);

void mbr_read(const pu8 *buf, struct mbr *mbr);

void mbr_write(pu8 *buf, const struct mbr *mbr);

pflag mbr_is_present(const pu8 *buf);

pflag mbr_is_part_used(const struct mbr_part *part);

enum mbr_load_res mbr_load(struct mbr *mbr, const struct img_ctx *img_ctx);

pres mbr_save(const struct mbr *mbr, const struct img_ctx *img_ctx);

#endif
