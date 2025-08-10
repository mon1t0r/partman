#ifndef LIBPARTMAN_GPT_H
#define LIBPARTMAN_GPT_H

#include "partman_types.h"
#include "guid.h"

/* GUID Partition Table (GPT) header structure */
struct gpt_hdr {
    /* Revision number */
    pu32 rev;

    /* Header size, in bytes */
    pu32 hdr_sz;

    /* Header CRC32 checksum */
    pu32 hdr_crc32;

    /* LBA of the primary header */
    pu64 my_lba;

    /* LBA of the alternate header */
    pu64 alt_lba;

    /* LBA of the first sector, which can be used by a partition */
    pu64 first_usable_lba;

    /* LBA of the last sector, which can be used by a partition */
    pu64 last_usable_lba;

    /* Disk GUID */
    struct guid disk_guid;

    /* LBA of the partition entry array */
    pu64 part_table_lba;

    /* Number of partition entries in the partition entry array */
    pu32 part_entry_cnt;

    /* Partition entry size, in bytes */
    pu32 part_entry_sz;

    /* Partition entry array CRC32 checksum */
    pu32 part_table_crc32;
};

/* GPT partition entry structure */
struct gpt_part {
    /* Partition type GUID */
    struct guid type_guid;

    /* Partition GUID, which is unique for every partition entry */
    struct guid unique_guid;

    /* LBA of the first sector, used by a partition */
    pu64 start_lba;

    /* LBA of the last sector, used by a partition */
    pu64 end_lba;

    /* Attributes bit field */
    pu64 attr;

    /* Name of the partition, using UCS-2 */
    pu32 name[36];
};

#endif
