#include <string.h>

#include "gpt.h"
#include "guid.h"
#include "memutils.h"
#include "crc32.h"

#define GPT_SIG "EFI PART"

static void gpt_part_ent_crc_compute(pcrc32 *crc32,
                                     const struct gpt_part_ent *entry)
{
    guid_crc_compute(crc32, &entry->type_guid);
    guid_crc_compute(crc32, &entry->unique_guid);
    crc32_compute64(crc32, entry->start_lba);
    crc32_compute64(crc32, entry->end_lba);
    crc32_compute64(crc32, entry->attr);

    /* TODO: Compute CRC of the partition name */
}

static pcrc32 gpt_table_crc_create(const struct gpt_part_ent *table,
                                   pu32 table_sz)
{
    pu32 i;
    pcrc32 crc32;

    crc32 = crc32_init();

    for(i = 0; i < table_sz; i++) {
        gpt_part_ent_crc_compute(&crc32, &table[i]);
    }

    crc32_finalize(&crc32);

    return crc32;
}

static pcrc32 gpt_hdr_crc_create(const struct gpt_hdr *hdr)
{
    pcrc32 crc32;
    int i;

    crc32 = crc32_init();

    /* Signature */
    for(i = 0; i < sizeof(GPT_SIG) / sizeof(GPT_SIG[0]) - 1 ; i++) {
        crc32_compute8(&crc32, GPT_SIG[i]);
    }

    crc32_compute32(&crc32, hdr->rev);
    crc32_compute32(&crc32, hdr->hdr_sz);

    /* Header CRC32 field counts as 0 during CRC32 evaluation */
    crc32_compute32(&crc32, 0);

    /* Reserved, 4 bytes */
    crc32_compute32(&crc32, 0);

    crc32_compute64(&crc32, hdr->my_lba);
    crc32_compute64(&crc32, hdr->alt_lba);
    crc32_compute64(&crc32, hdr->first_usable_lba);
    crc32_compute64(&crc32, hdr->last_usable_lba);

    /* Disk GUID */
    guid_crc_compute(&crc32, &hdr->disk_guid);

    crc32_compute64(&crc32, hdr->part_table_lba);
    crc32_compute32(&crc32, hdr->part_table_entry_cnt);
    crc32_compute32(&crc32, hdr->part_entry_sz);
    crc32_compute32(&crc32, hdr->part_table_crc32);

    crc32_finalize(&crc32);

    return crc32;
}

void gpt_init(struct gpt_hdr *hdr)
{
    hdr->rev = gpt_hdr_rev;
    hdr->hdr_sz = gpt_hdr_sz;
    guid_create(&hdr->disk_guid);
    hdr->part_entry_sz = gpt_part_ent_sz;
}

void gpt_hdr_write(pu8 *buf, const struct gpt_hdr *hdr)
{
    int i;

    /* Signature */
    for(i = 0; i < sizeof(GPT_SIG) / sizeof(GPT_SIG[0]) - 1 ; i++) {
        write_pu8(buf + i, GPT_SIG[i]);
    }

    write_pu32(buf + 8, hdr->rev);
    write_pu32(buf + 12, hdr->hdr_sz);
    write_pu32(buf + 16, hdr->hdr_crc32);

    /* Reserved, 4 bytes */
    write_pu32(buf + 20, 0);

    write_pu64(buf + 24, hdr->my_lba);
    write_pu64(buf + 32, hdr->alt_lba);
    write_pu64(buf + 40, hdr->first_usable_lba);
    write_pu64(buf + 48, hdr->last_usable_lba);

    /* Disk GUID */
    guid_write(buf + 56, &hdr->disk_guid);

    write_pu64(buf + 72, hdr->part_table_lba);
    write_pu32(buf + 80, hdr->part_table_entry_cnt);
    write_pu32(buf + 84, hdr->part_entry_sz);
    write_pu32(buf + 88, hdr->part_table_crc32);
}

void gpt_hdr_read(const pu8 *buf, struct gpt_hdr *hdr)
{
    hdr->rev = read_pu32(buf + 8);
    hdr->hdr_sz = read_pu32(buf + 12);
    hdr->hdr_crc32 = read_pu32(buf + 16);

    /* Reserved, 4 bytes */

    hdr->my_lba = read_pu64(buf + 24);
    hdr->alt_lba = read_pu64(buf + 32);
    hdr->first_usable_lba = read_pu64(buf + 40);
    hdr->last_usable_lba = read_pu64(buf + 48);

    /* Disk GUID */
    guid_read(buf + 56, &hdr->disk_guid);

    hdr->part_table_lba = read_pu64(buf + 72);
    hdr->part_table_entry_cnt = read_pu32(buf + 80);
    hdr->part_entry_sz = read_pu32(buf + 84);
    hdr->part_table_crc32 = read_pu32(buf + 88);
}

pflag gpt_is_present(const pu8 *buf)
{
    return 0 == strncmp((const char *) buf, GPT_SIG,
                        sizeof(GPT_SIG) / sizeof(GPT_SIG[0]) - 1);
}

pflag gpt_is_valid(const struct gpt_hdr *hdr,
                   const struct gpt_part_ent *table, pu64 hdr_lba)
{
    return hdr->hdr_crc32 == gpt_hdr_crc_create(hdr) &&
           hdr->my_lba == hdr_lba &&
           hdr->part_table_crc32 ==
                gpt_table_crc_create(table, hdr->part_table_entry_cnt);
}

void gpt_crc_create(struct gpt_hdr *hdr, const struct gpt_part_ent *table)
{
    hdr->part_table_crc32 = gpt_table_crc_create(table,
                                                 hdr->part_table_entry_cnt);
    hdr->hdr_crc32 = gpt_hdr_crc_create(hdr);
}

