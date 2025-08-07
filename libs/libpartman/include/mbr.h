#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

/* MBR partition structure */
struct mbr_part {
    /* 1-byte boot indicator */
    unsigned char boot_ind;

    /* 3-byte CHS address of first absolute sector in partition */
    unsigned char start_chs[3];

    /* 1-byte partition type */
    unsigned char type;

    /* 3-byte CHS address of last absolute sector in partition */
    unsigned char end_chs[3];

    /* 4-byte LBA of first absolute sector in the partition */
    unsigned long start_lba;

    /* 4-byte number of sectors in partition */
    unsigned long size_lba;
};

/* Master Boot Record structure */
struct mbr {
    /* 4-byte disk signature */
    unsigned long disk_sig;

    /* Partition table (4 primary partitions) */
    struct mbr_part partitions[4];
};

void mbr_write(unsigned char *buf, const struct mbr *mbr);

#endif
