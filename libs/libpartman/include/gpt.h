#ifndef LIBPARTMAN_GPT_H
#define LIBPARTMAN_GPT_H

#include "guid.h"

/* GUID Partition Table (GPT) header structure */
struct gpt_hdr {
    /* Revision number */
    unsigned long rev;

    /* Header size, in bytes */
    unsigned long hdr_sz;

    /* Header CRC32 checksum */
    unsigned long hdr_crc32;

    /* LBA of the primary header */
    unsigned long long my_lba;

    /* LBA of the alternate header */
    unsigned long long alt_lba;

    /* LBA of the first sector, which can be used by a partition */
    unsigned long long first_usable_lba;

    /* LBA of the last sector, which can be used by a partition */
    unsigned long long last_usable_lba;

    /* Disk GUID */
    struct guid disk_guid;

    /* LBA of the partition entry array */
    unsigned long long part_table_lba;

    /* Number of partition entries in the partition entry array */
    unsigned long part_entry_cnt;

    /* Partition entry size, in bytes */
    unsigned long part_entry_sz;

    /* Partition entry array CRC32 checksum */
    unsigned long part_table_crc32;
};

/* GPT partition entry structure */
struct gpt_part {
    /* Partition type GUID */
    struct guid type_guid;

    /* Partition GUID, which is unique for every partition entry */
    struct guid unique_guid;

    /* LBA of the first sector, used by a partition */
    unsigned long long start_lba;

    /* LBA of the last sector, used by a partition */
    unsigned long long end_lba;

    /* Attributes bit field */
    unsigned long long attr;

    /* Name of the partition, using UCS-2 */
    unsigned short name[36];
};

#endif
