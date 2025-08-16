#ifndef LIBPARTMAN_GPT_H
#define LIBPARTMAN_GPT_H

#include "partman_types.h"
#include "guid.h"
#include "crc32.h"
#include "mbr.h"

/* GUID Partition Table (GPT) header structure */
struct gpt_hdr {
    /* Revision number */
    pu32 rev;

    /* Header size, in bytes */
    pu32 hdr_sz;

    /* Header CRC32 checksum */
    pcrc32 hdr_crc32;

    /* LBA of the primary header */
    plba my_lba;

    /* LBA of the alternate header */
    plba alt_lba;

    /* LBA of the first sector, which can be used by a partition */
    plba first_usable_lba;

    /* LBA of the last sector, which can be used by a partition */
    plba last_usable_lba;

    /* Disk GUID */
    struct guid disk_guid;

    /* LBA of the partition entry array */
    plba part_table_lba;

    /* Number of partition entries in the partition entry array */
    pu32 part_table_entry_cnt;

    /* Partition entry size, in bytes */
    pu32 part_entry_sz;

    /* Partition entry array CRC32 checksum */
    pcrc32 part_table_crc32;
};

/* GPT partition entry structure */
struct gpt_part_ent {
    /* Partition type GUID */
    struct guid type_guid;

    /* Partition GUID, which is unique for every partition entry */
    struct guid unique_guid;

    /* LBA of the first sector, used by a partition */
    plba start_lba;

    /* LBA of the last sector, used by a partition */
    plba end_lba;

    /* Attributes bit field */
    pu64 attr;

    /* Name of the partition, using UCS-2 */
    pchar_ucs name[36];
};

/* GPT partitioning scheme context */
struct schem_ctx_gpt {
    /* Protective MBR partitioning scheme */
    struct schem_ctx_mbr mbr_prot;

    /* In-memory GPT primary header structure */
    struct gpt_hdr hdr_prim;

    /* In-memory GPT primary table structure */
    struct gpt_part_ent *table_prim;

    /* In-memory GPT secondary header structure */
    struct gpt_hdr hdr_sec;

    /* In-memory GPT secondary table structure */
    struct gpt_part_ent *table_sec;
};

enum gpt_load_res {
    gpt_load_ok, gpt_load_hdr_inv, gpt_load_table_inv, gpt_load_fatal
};

enum {
    /* Maximum supported GPT partition count. GPT partition table must
     * be able to contain at least this number of partitions */
    gpt_part_cnt = 128
};

void gpt_init_new(struct gpt_hdr *hdr_prim, struct gpt_hdr *hdr_sec,
                  struct gpt_part_ent table_prim[],
                  struct gpt_part_ent table_sec[],
                  const struct img_ctx *img_ctx);

void gpt_hdr_write(pu8 *buf, const struct gpt_hdr *hdr);

void gpt_hdr_read(const pu8 *buf, struct gpt_hdr *hdr);

void gpt_crc_fill_table(struct gpt_hdr *hdr,
                        const struct gpt_part_ent table[]);

void gpt_crc_fill_hdr(struct gpt_hdr *hdr);

pflag gpt_is_present(const pu8 *buf);

pflag gpt_hdr_is_valid(const struct gpt_hdr *hdr, plba hdr_lba);

pflag gpt_table_is_valid(const struct gpt_hdr *hdr,
                         const struct gpt_part_ent table[]);

void gpt_restore(struct gpt_hdr *hdr_dst, struct gpt_part_ent table_dst[],
                 plba table_dst_lba, const struct gpt_hdr *hdr_src,
                 const struct gpt_part_ent table_src[]);

pflag gpt_is_part_used(const struct gpt_part_ent *part);

enum gpt_load_res gpt_load(struct gpt_hdr *hdr, struct gpt_part_ent table[],
                           const struct img_ctx *img_ctx, plba hdr_lba);

pres gpt_save(const struct gpt_hdr *hdr, const struct gpt_part_ent table[],
              const struct img_ctx *img_ctx);
#endif
