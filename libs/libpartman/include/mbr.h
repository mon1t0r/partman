#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

/* MBR partition structure */
struct mbr_part {
    /* Boot indicator */
    unsigned char boot_ind;

    /* CHS address of first absolute sector in partition */
    unsigned char start_chs[3];

    /* Partition type */
    unsigned char type;

    /* CHS address of last absolute sector in partition */
    unsigned char end_chs[3];

    /* LBA of first absolute sector in the partition */
    unsigned long start_lba;

    /* Number of sectors in partition */
    unsigned long sz_lba;
};

/* Master Boot Record (MBR) structure */
struct mbr {
    /* Unique disk signature */
    unsigned long disk_sig;

    /* Partition table (4 primary partitions) */
    struct mbr_part partitions[4];
};

enum {
    /* MBR size, in sectors */
    mbr_sz_secs = 1
};

void mbr_write(unsigned char *buf, const struct mbr *mbr);

void mbr_read(const unsigned char *buf, struct mbr *mbr);

int mbr_is_present(const unsigned char *buf);

int mbr_is_part_used(const struct mbr_part *part);

void mbr_init_protective(struct mbr *mbr);

#endif
