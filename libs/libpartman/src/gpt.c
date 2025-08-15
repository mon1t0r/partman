#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "gpt.h"
#include "img_ctx.h"
#include "memutils.h"
#include "crc32.h"

#define GPT_SIG "EFI PART"

enum {
    /* GPT header revision number */
    gpt_hdr_rev = 0x00010000
};

static pu8 *map_secs(const struct img_ctx *ctx, int img_fd, pu64 off_lba,
                     pu64 len_lba)
{
    pu8 *reg;

    reg = mmap(NULL, lba_to_byte(ctx, len_lba), PROT_READ|PROT_WRITE,
               MAP_SHARED, img_fd, lba_to_byte(ctx, off_lba));
    if(reg == MAP_FAILED) {
        perror("mmap()");
        return NULL;
    }

    return reg;
}

static pres unmap_secs(pu8 *reg, const struct img_ctx *ctx, pu64 len_lba)
{
    int c;

    c = munmap(reg, lba_to_byte(ctx, len_lba));
    if(c == -1) {
        perror("munmap()");
        return pres_fail;
    }
    reg = NULL;

    return pres_succ;
}

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

static pcrc32 gpt_table_crc_create(const struct gpt_part_ent table[],
                                   pu32 table_len)
{
    pu32 i;
    pcrc32 crc32;

    crc32 = crc32_init();

    for(i = 0; i < table_len; i++) {
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

static void gpt_part_ent_write(pu8 *buf, const struct gpt_part_ent *entry)
{
    /* Partition type GUID */
    guid_write(buf, &entry->type_guid);

    /* Partition GUID */
    guid_write(buf + 16, &entry->unique_guid);

    write_pu64(buf + 32, entry->start_lba);
    write_pu64(buf + 40, entry->end_lba);
    write_pu64(buf + 48, entry->attr);

    /* TODO: Write partition name */
}

static void gpt_part_ent_read(const pu8 *buf, struct gpt_part_ent *entry)
{
    /* Partition type GUID */
    guid_read(buf, &entry->type_guid);

    /* Partition GUID */
    guid_read(buf + 16, &entry->unique_guid);

    entry->start_lba = read_pu64(buf + 32);
    entry->end_lba = read_pu64(buf + 40);
    entry->attr = read_pu64(buf + 48);

    /* TODO: Read partition name */
}

void gpt_init_new(struct gpt_hdr *hdr)
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

void gpt_table_write(pu8 *buf, const struct gpt_part_ent table[],
                     pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    for(i = 0; i < table_len; i++) {
        gpt_part_ent_write(&buf[i * entry_sz], &table[i]);
    }
}

void gpt_table_read(const pu8 *buf, struct gpt_part_ent table[],
                    pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    for(i = 0; i < table_len; i++) {
        gpt_part_ent_read(&buf[i * entry_sz], &table[i]);
    }
}

void gpt_crc_create(struct gpt_hdr *hdr, const struct gpt_part_ent table[])
{
    hdr->part_table_crc32 = gpt_table_crc_create(table,
                                                 hdr->part_table_entry_cnt);
    hdr->hdr_crc32 = gpt_hdr_crc_create(hdr);
}

pflag gpt_is_present(const pu8 *buf)
{
    return 0 == strncmp((const char *) buf, GPT_SIG,
                        sizeof(GPT_SIG) / sizeof(GPT_SIG[0]) - 1);
}

pflag gpt_hdr_is_valid(const struct gpt_hdr *hdr, pu64 hdr_lba)
{
    return hdr->hdr_crc32 == gpt_hdr_crc_create(hdr) &&
           hdr->my_lba == hdr_lba;
}

pflag gpt_table_is_valid(const struct gpt_hdr *hdr,
                         const struct gpt_part_ent table[])
{
    return hdr->part_table_crc32 ==
           gpt_table_crc_create(table, hdr->part_table_entry_cnt);
}

void gpt_restore(struct gpt_hdr *hdr_dst, struct gpt_part_ent table_dst[],
                 struct gpt_hdr *hdr_src, struct gpt_part_ent table_src[])
{
    pu32 i;

    memcpy(hdr_dst, hdr_src, sizeof(*hdr_src));
    hdr_dst->my_lba = hdr_src->alt_lba;
    hdr_dst->alt_lba = hdr_src->my_lba;
    hdr_dst->part_table_lba = 0;

    for(i = 0; i < hdr_src->part_table_entry_cnt; i++) {
        memcpy(&table_dst[i], &table_src[i], sizeof(table_src[i]));
    }
}

enum gpt_load_res gpt_load(struct gpt_hdr *hdr, struct gpt_part_ent table[],
                           const struct img_ctx *img_ctx, int img_fd,
                           pu64 hdr_lba)
{
    enum gpt_load_res load_res;
    pres res;
    pu8 *hdr_reg = NULL;
    pu8 *table_reg = NULL;
    pu64 hdr_sz_secs;
    pu64 table_sz_secs;
    pu64 table_lba;

    /* GPT header size */
    hdr_sz_secs = 1;

    /* Map GPT header sector */
    hdr_reg = map_secs(img_ctx, img_fd, hdr_lba, hdr_sz_secs);
    if(hdr_reg == NULL) {
        fprintf(stderr, "Failed to map image GPT header at LBA %llu\n",
                hdr_lba);
        load_res = gpt_load_fatal;
        goto exit;
    }

    /* Check GPT header signature */
    if(!gpt_is_present(hdr_reg)) {
        load_res = gpt_load_hdr_inv;
        goto exit;
    }

    /* Read GPT header */
    gpt_hdr_read(hdr_reg, hdr);

    /* Check GPT header CRC */
    if(!gpt_hdr_is_valid(hdr, hdr_lba)) {
        load_res = gpt_load_hdr_inv;
        goto exit;
    }

    /* Get GPT table LBA and size */
    table_lba = hdr->part_table_lba;
    table_sz_secs = byte_to_lba(img_ctx, hdr->part_table_entry_cnt *
                                hdr->part_entry_sz, 1);

    /* Map GPT table sectors */
    table_reg = map_secs(img_ctx, img_fd, table_lba, table_sz_secs);
    if(table_reg == NULL) {
        fprintf(stderr, "Failed to map image GPT table at LBA %llu\n",
                table_lba);
        load_res = gpt_load_fatal;
        goto exit;
    }

    /* Read GPT table */
    gpt_table_read(table_reg, table, hdr->part_table_entry_cnt,
                   hdr->part_entry_sz);

    /* Check GPT table CRC */
    if(!gpt_table_is_valid(hdr, table)) {
        load_res = gpt_load_table_inv;
        goto exit;
    }

    /* Loaded successfully */
    load_res = gpt_load_ok;

exit:
    /* If mapped, unmap GPT header sector */
    if(hdr_reg) {
        res = unmap_secs(hdr_reg, img_ctx, hdr_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT header at LBA "
                    "%llu\n", hdr_lba);
            return gpt_load_fatal;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT table at LBA "
                    "%llu\n", table_lba);
            return gpt_load_fatal;
        }
    }

    return load_res;
}

pres gpt_save(const struct gpt_hdr *hdr, const struct gpt_part_ent table[],
              const struct img_ctx *img_ctx, int img_fd)
{
    pres save_res;
    pres res;
    pu8 *hdr_reg = NULL;
    pu8 *table_reg = NULL;
    pu64 hdr_sz_secs;
    pu64 table_sz_secs;
    pu64 hdr_lba;
    pu64 table_lba;

    /* Get GPT header LBA and size */
    hdr_lba = hdr->my_lba;
    hdr_sz_secs = 1;

    /* Map GPT header sector */
    hdr_reg = map_secs(img_ctx, img_fd, hdr_lba, hdr_sz_secs);
    if(hdr_reg == NULL) {
        fprintf(stderr, "Failed to map image GPT header at LBA %llu\n",
                hdr_lba);
        save_res = pres_fail;
        goto exit;
    }

    /* Write GPT header */
    gpt_hdr_write(hdr_reg, hdr);

    /* Get GPT table LBA and size */
    table_lba = hdr->part_table_lba;
    table_sz_secs = byte_to_lba(img_ctx, hdr->part_table_entry_cnt *
                                hdr->part_entry_sz, 1);

    /* Map GPT table sectors */
    table_reg = map_secs(img_ctx, img_fd, table_lba, table_sz_secs);
    if(table_reg == NULL) {
        fprintf(stderr, "Failed to map image GPT table at LBA %llu\n",
                table_lba);
        save_res = pres_fail;
        goto exit;
    }

    /* Write GPT table */
    gpt_table_write(table_reg, table, hdr->part_table_entry_cnt,
                    hdr->part_entry_sz);

    /* Saved successfully */
    save_res = pres_succ;

exit:
    /* If mapped, unmap GPT header sector */
    if(hdr_reg) {
        res = unmap_secs(hdr_reg, img_ctx, hdr_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT header at LBA "
                    "%llu\n", hdr_lba);
            return pres_fail;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT table at LBA "
                    "%llu\n", table_lba);
            return pres_fail;
        }
    }

    return save_res;
}

