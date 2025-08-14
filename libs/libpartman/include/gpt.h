#ifndef LIBPARTMAN_GPT_H
#define LIBPARTMAN_GPT_H

#include "partman_types.h"
#include "guid.h"
#include "mbr.h"

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
    pu32 part_table_entry_cnt;

    /* Partition entry size, in bytes */
    pu32 part_entry_sz;

    /* Partition entry array CRC32 checksum */
    pu32 part_table_crc32;
};

/* GPT partition entry structure */
struct gpt_part_ent {
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

/* GPT partitioning scheme context */
struct schem_ctx_gpt {
    /* Protective MBR partitioning scheme */
    struct schem_ctx_mbr mbr_prot;

    /* In-memory GPT primary header structure */
    struct gpt_hdr gpt_hdr_prim;

    /* In-memory GPT primary table structure */
    struct gpt_part_ent *gpt_table_prim;

    /* In-memory GPT atlernative header structure */
    struct gpt_hdr gpt_hdr_alt;

    /* In-memory GPT alternative table structure */
    struct gpt_part_ent *gpt_table_alt;

    /* Pointer to a memory region, which maps image primary GPT header */
    pu8 *gpt_prim_hdr_reg;

    /* Pointer to a memory region, which maps image primary GPT table */
    pu8 *gpt_prim_table_reg;

    /* Pointer to a memory region, which maps image atlernative GPT header */
    pu8 *gpt_alt_hdr_reg;

    /* Pointer to a memory region, which maps image atlernative GPT table */
    pu8 *gpt_alt_table_reg;
};

enum {
    /* GPT header size, in bytes */
    gpt_hdr_sz      = 92,

    /* GPT partition entry size, in bytes */
    gpt_part_ent_sz = 128
};

void gpt_init(struct gpt_hdr *hdr);

void gpt_hdr_write(pu8 *buf, const struct gpt_hdr *hdr);

void gpt_hdr_read(const pu8 *buf, struct gpt_hdr *hdr);

pflag gpt_is_present(const pu8 *buf);

pflag gpt_is_valid(const struct gpt_hdr *hdr,
                   const struct gpt_part_ent *table, pu64 hdr_lba);

void gpt_crc_create(struct gpt_hdr *hdr, const struct gpt_part_ent *table);

#endif
