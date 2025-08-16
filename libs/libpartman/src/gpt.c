#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "gpt.h"
#include "img_ctx.h"
#include "memutils.h"
#include "crc32.h"
#include "partman_types.h"

#define GPT_SIG "EFI PART"

enum {
    /* GPT header revision number */
    gpt_hdr_rev     = 0x00010000,

    /* GPT header size, in bytes */
    gpt_hdr_sz      = 92,

    /* GPT partition entry size, in bytes */
    gpt_part_ent_sz = 128
};

static long byte_to_page(pu64 bytes, pflag round_up)
{
    long pg_sz = sysconf(_SC_PAGESIZE);
    return bytes / pg_sz + (round_up && (bytes % pg_sz) ? 1 : 0);
}

static pu64 page_to_byte(long pages)
{
    return pages * sysconf(_SC_PAGESIZE);
}

static pu8 *map_secs(const struct img_ctx *ctx, plba off_lba, plba secs_cnt)
{
    pu8 *reg;
    pu64 off_bytes;
    long off_pages;
    long align_off;
    long len_pages;

    /* Memory region offset, in bytes */
    off_bytes = lba_to_byte(ctx, off_lba);

    /* Memory region offset, in pages (aligned, to lower) */
    off_pages = byte_to_page(off_bytes, 0);

    /* Alignment offset, in bytes (how many bytes offset moved back) */
    align_off = off_bytes - page_to_byte(off_pages);

    /* Memory region length, in pages (aligned, to higher)  */
    len_pages = byte_to_page(lba_to_byte(ctx, secs_cnt) + align_off, 1);

    reg = mmap(NULL, page_to_byte(len_pages), PROT_READ|PROT_WRITE,
               MAP_SHARED, ctx->img_fd, page_to_byte(off_pages));
    if(reg == MAP_FAILED) {
        perror("mmap()");
        return NULL;
    }

    /* Return address moved forward with alignment offset */
    return reg + align_off;
}

static pres unmap_secs(pu8 *reg, const struct img_ctx *ctx, plba off_lba,
                       plba secs_cnt)
{
    int c;
    pu64 off_bytes;
    long off_pages;
    long align_off;
    long len_pages;

    /* Memory region offset, in bytes */
    off_bytes = lba_to_byte(ctx, off_lba);

    /* Memory region offset, in pages (aligned, to lower) */
    off_pages = byte_to_page(off_bytes, 0);

    /* Alignment offset, in bytes (how many bytes offset moved back) */
    align_off = off_bytes - page_to_byte(off_pages);

    /* Memory region length, in pages (aligned, to higher)  */
    len_pages = byte_to_page(lba_to_byte(ctx, secs_cnt) + align_off, 1);

    c = munmap(reg - align_off, len_pages);
    if(c == -1) {
        perror("munmap()");
        return pres_fail;
    }

    return pres_ok;
}

static void gpt_part_ent_crc_compute(pcrc32 *crc32,
                                     const struct gpt_part_ent *entry)
{
    int i;

    guid_crc_compute(crc32, &entry->type_guid);
    guid_crc_compute(crc32, &entry->unique_guid);
    crc32_compute64(crc32, entry->start_lba);
    crc32_compute64(crc32, entry->end_lba);
    crc32_compute64(crc32, entry->attr);

    for(i = 0; i < ARRAY_SIZE(entry->name); i++) {
        crc32_compute16(crc32, entry->name[i]);
    }
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
    for(i = 0; i < ARRAY_SIZE(GPT_SIG) - 1 ; i++) {
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
    int i;

    /* Partition type GUID */
    guid_write(buf, &entry->type_guid);

    /* Partition GUID */
    guid_write(buf + 16, &entry->unique_guid);

    write_pu64(buf + 32, entry->start_lba);
    write_pu64(buf + 40, entry->end_lba  );
    write_pu64(buf + 48, entry->attr     );

    for(i = 0; i < ARRAY_SIZE(entry->name); i++) {
        write_pu16(buf + 56 + i, entry->name[i]);
    }
}

static void gpt_part_ent_read(const pu8 *buf, struct gpt_part_ent *entry)
{
    int i;

    /* Partition type GUID */
    guid_read(buf, &entry->type_guid);

    /* Partition GUID */
    guid_read(buf + 16, &entry->unique_guid);

    entry->start_lba = read_pu64(buf + 32);
    entry->end_lba = read_pu64(buf + 40);
    entry->attr = read_pu64(buf + 48);

    for(i = 0; i < ARRAY_SIZE(entry->name); i++) {
        entry->name[i] = read_pu16(buf + 56 + i);
    }
}

void gpt_init_new(struct gpt_hdr *hdr_prim, struct gpt_hdr *hdr_sec,
                  struct gpt_part_ent table_prim[],
                  struct gpt_part_ent table_sec[],
                  const struct img_ctx *img_ctx)
{
    plba hdr_lba_prim;
    plba hdr_lba_sec;
    plba table_sz;
    plba table_lba_prim;
    plba table_lba_sec;

    memset(hdr_prim, 0, sizeof(*hdr_prim));
    guid_create(&hdr_prim->disk_guid);
    hdr_prim->rev = gpt_hdr_rev;
    hdr_prim->hdr_sz = gpt_hdr_sz;

    /* Primary GPT header is located at LBA 1 */
    hdr_lba_prim = 1;

    /* Secondary GPT header is located at image last LBA */
    hdr_lba_sec = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;

    /* GPT table size */
    table_sz = byte_to_lba(img_ctx, gpt_part_cnt * gpt_part_ent_sz, 1);

    /* Primary GPT table is located after primary GPT header */
    table_lba_prim = hdr_lba_prim + 1;

    /* Secondary GPT table is located before secondary GPT header */
    table_lba_sec = hdr_lba_sec - table_sz;

    hdr_prim->my_lba = hdr_lba_prim;
    hdr_prim->alt_lba = hdr_lba_sec;
    hdr_prim->first_usable_lba = table_lba_prim + table_sz;
    hdr_prim->last_usable_lba = table_lba_sec - 1;
    hdr_prim->part_table_lba = table_lba_prim;
    hdr_prim->part_table_entry_cnt = gpt_part_cnt;
    hdr_prim->part_entry_sz = gpt_part_ent_sz;

    /* Create secondary GPT header */
    gpt_restore(hdr_sec, table_sec, table_lba_sec, hdr_prim, table_prim);
}

void gpt_hdr_write(pu8 *buf, const struct gpt_hdr *hdr)
{
    int i;

    /* Signature */
    for(i = 0; i < ARRAY_SIZE(GPT_SIG) - 1 ; i++) {
        write_pu8(buf + i, GPT_SIG[i]);
    }

    write_pu32(buf + 8,  hdr->rev      );
    write_pu32(buf + 12, hdr->hdr_sz   );
    write_pu32(buf + 16, hdr->hdr_crc32);

    /* Reserved, 4 bytes */
    write_pu32(buf + 20, 0);

    write_pu64(buf + 24, hdr->my_lba          );
    write_pu64(buf + 32, hdr->alt_lba         );
    write_pu64(buf + 40, hdr->first_usable_lba);
    write_pu64(buf + 48, hdr->last_usable_lba );

    /* Disk GUID */
    guid_write(buf + 56, &hdr->disk_guid);

    write_pu64(buf + 72, hdr->part_table_lba      );
    write_pu32(buf + 80, hdr->part_table_entry_cnt);
    write_pu32(buf + 84, hdr->part_entry_sz       );
    write_pu32(buf + 88, hdr->part_table_crc32    );
}

void gpt_hdr_read(const pu8 *buf, struct gpt_hdr *hdr)
{
    hdr->rev       = read_pu32(buf + 8 );
    hdr->hdr_sz    = read_pu32(buf + 12);
    hdr->hdr_crc32 = read_pu32(buf + 16);

    /* Reserved, 4 bytes */

    hdr->my_lba           = read_pu64(buf + 24);
    hdr->alt_lba          = read_pu64(buf + 32);
    hdr->first_usable_lba = read_pu64(buf + 40);
    hdr->last_usable_lba  = read_pu64(buf + 48);

    /* Disk GUID */
    guid_read(buf + 56, &hdr->disk_guid);

    hdr->part_table_lba       = read_pu64(buf + 72);
    hdr->part_table_entry_cnt = read_pu32(buf + 80);
    hdr->part_entry_sz        = read_pu32(buf + 84);
    hdr->part_table_crc32     = read_pu32(buf + 88);
}

void gpt_table_write(pu8 *buf, const struct gpt_part_ent table[],
                     pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    /* Maximum supported number of partitions limit */
    for(i = 0; i < table_len && i < gpt_part_cnt; i++) {
        gpt_part_ent_write(&buf[i * entry_sz], &table[i]);
    }
}

void gpt_table_read(const pu8 *buf, struct gpt_part_ent table[],
                    pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    /* Maximum supported number of partitions limit */
    for(i = 0; i < table_len && i < gpt_part_cnt; i++) {
        gpt_part_ent_read(&buf[i * entry_sz], &table[i]);
    }
}

void gpt_crc_fill_table(struct gpt_hdr *hdr, const struct gpt_part_ent table[])
{
    hdr->part_table_crc32 = gpt_table_crc_create(table,
                                                 hdr->part_table_entry_cnt);
}

void gpt_crc_fill_hdr(struct gpt_hdr *hdr)
{
    hdr->hdr_crc32 = gpt_hdr_crc_create(hdr);
}

pflag gpt_is_present(const pu8 *buf)
{
    return 0 == strncmp((const char *) buf, GPT_SIG, ARRAY_SIZE(GPT_SIG) - 1);
}

pflag gpt_hdr_is_valid(const struct gpt_hdr *hdr, plba hdr_lba)
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
                 plba table_dst_lba, const struct gpt_hdr *hdr_src,
                 const struct gpt_part_ent table_src[])
{
    pu32 i;

    memcpy(hdr_dst, hdr_src, sizeof(*hdr_src));
    hdr_dst->my_lba = hdr_src->alt_lba;
    hdr_dst->alt_lba = hdr_src->my_lba;
    hdr_dst->part_table_lba = table_dst_lba;

    for(i = 0; i < hdr_src->part_table_entry_cnt; i++) {
        memcpy(&table_dst[i], &table_src[i], sizeof(table_src[i]));
    }
}

pflag gpt_is_part_used(const struct gpt_part_ent *part)
{
    return !guid_is_zero(&part->type_guid);
}

enum gpt_load_res gpt_load(struct gpt_hdr *hdr, struct gpt_part_ent table[],
                           const struct img_ctx *img_ctx, plba hdr_lba)
{
    enum gpt_load_res load_res;
    pres res;
    pu8 *hdr_reg = NULL;
    pu8 *table_reg = NULL;
    plba hdr_sz_secs;
    plba table_sz_secs;
    plba table_lba;

    /* GPT header size */
    hdr_sz_secs = 1;

    /* Map GPT header sector */
    hdr_reg = map_secs(img_ctx, hdr_lba, hdr_sz_secs);
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
    table_reg = map_secs(img_ctx, table_lba, table_sz_secs);
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
        res = unmap_secs(hdr_reg, img_ctx, hdr_lba, hdr_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT header at LBA "
                    "%llu\n", hdr_lba);
            return gpt_load_fatal;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_lba, table_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT table at LBA "
                    "%llu\n", table_lba);
            return gpt_load_fatal;
        }
    }

    return load_res;
}

pres gpt_save(const struct gpt_hdr *hdr, const struct gpt_part_ent table[],
              const struct img_ctx *img_ctx)
{
    pres save_res;
    pres res;
    pu8 *hdr_reg = NULL;
    pu8 *table_reg = NULL;
    plba hdr_sz_secs;
    plba table_sz_secs;
    plba hdr_lba;
    plba table_lba;

    /* Get GPT header LBA and size */
    hdr_lba = hdr->my_lba;
    hdr_sz_secs = 1;

    /* Map GPT header sector */
    hdr_reg = map_secs(img_ctx, hdr_lba, hdr_sz_secs);
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
    table_reg = map_secs(img_ctx, table_lba, table_sz_secs);
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
    save_res = pres_ok;

exit:
    /* If mapped, unmap GPT header sector */
    if(hdr_reg) {
        res = unmap_secs(hdr_reg, img_ctx, hdr_lba, hdr_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT header at LBA "
                    "%llu\n", hdr_lba);
            return pres_fail;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_lba, table_sz_secs);
        if(!res) {
            fprintf(stderr, "Failed to unmap image GPT table at LBA "
                    "%llu\n", table_lba);
            return pres_fail;
        }
    }

    return save_res;
}

