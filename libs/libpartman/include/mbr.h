#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

#include "partman_types.h"

/* MBR partition structure */
struct mbr_part {
    /* Boot indicator */
    pu8 boot_ind;

    /* CHS address of first absolute sector in partition */
    pu32 start_chs;

    /* Partition type */
    pu8 type;

    /* CHS address of last absolute sector in partition */
    pu32 end_chs;

    /* LBA of first absolute sector in the partition */
    pu32 start_lba;

    /* Number of sectors in partition */
    pu32 sz_lba;
};

/* Master Boot Record (MBR) structure */
struct mbr {
    /* Unique disk signature */
    pu32 disk_sig;

    /* Partition table (4 primary partitions) */
    struct mbr_part partitions[4];
};

enum {
    /* MBR size, in sectors */
    mbr_sz_secs = 1
};

void mbr_write(pu8 *buf, const struct mbr *mbr);

void mbr_read(const pu8 *buf, struct mbr *mbr);

pflag mbr_is_present(const pu8 *buf);

pflag mbr_is_part_used(const struct mbr_part *part);

void mbr_init_protective(struct mbr *mbr);

#endif
