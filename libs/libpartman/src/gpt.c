#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "gpt.h"
#include "log.h"
#include "memutils.h"
#include "crc32.h"

#define GPT_SIG "EFI PART"

enum {
    /* GPT header revision number */
    gpt_hdr_rev     = 0x00010000,

    /* GPT header size, in bytes */
    gpt_hdr_sz      = 92,

    /* GPT partition entry size, in bytes */
    gpt_part_ent_sz = 128
};

/* Default GPT partition type - Linux filesystem */
const struct guid gpt_part_type_def = {
    0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79,
    { 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 }
};

enum gpt_pair_load_res {
    gpt_pair_load_ok,
    gpt_pair_load_hdr_inv,
    gpt_pair_load_table_inv,
    gpt_pair_load_fatal
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

/* GPT header structure */
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

/* GUID Partition Table (GPT) structure */
struct gpt {
    /* In-memory GPT primary header structure */
    struct gpt_hdr hdr_prim;

    /* In-memory GPT primary table structure */
    struct gpt_part_ent *table_prim;

    /* In-memory GPT secondary header structure */
    struct gpt_hdr hdr_sec;

    /* In-memory GPT secondary table structure */
    struct gpt_part_ent *table_sec;
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
    write_pu64(buf + 40, entry->end_lba);
    write_pu64(buf + 48, entry->attr);

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
    entry->end_lba   = read_pu64(buf + 40);
    entry->attr      = read_pu64(buf + 48);

    for(i = 0; i < ARRAY_SIZE(entry->name); i++) {
        entry->name[i] = read_pu16(buf + 56 + i);
    }
}

static void gpt_table_write(pu8 *buf, const struct gpt_part_ent table[],
                            pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    /* Maximum supported number of partitions limit */
    for(i = 0; i < table_len && i < gpt_max_part_cnt; i++) {
        gpt_part_ent_write(&buf[i * entry_sz], &table[i]);
    }
}

static void gpt_table_read(const pu8 *buf, struct gpt_part_ent table[],
                           pu32 table_len, pu32 entry_sz)
{
    pu32 i;

    /* Maximum supported number of partitions limit */
    for(i = 0; i < table_len && i < gpt_max_part_cnt; i++) {
        gpt_part_ent_read(&buf[i * entry_sz], &table[i]);
    }
}

static void gpt_hdr_write(pu8 *buf, const struct gpt_hdr *hdr)
{
    int i;

    /* Signature */
    for(i = 0; i < ARRAY_SIZE(GPT_SIG) - 1 ; i++) {
        write_pu8(buf + i, GPT_SIG[i]);
    }

    write_pu32(buf + 8,  hdr->rev);
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

static void gpt_hdr_read(const pu8 *buf, struct gpt_hdr *hdr)
{
    hdr->rev       = read_pu32(buf + 8);
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

static void gpt_free(struct gpt *gpt)
{
    free(gpt->table_prim);
    free(gpt->table_sec);
}

static void gpt_restore(struct gpt *gpt, plba table_dst_lba,
                        pflag restore_primary)
{
    pu32 i;

    struct gpt_hdr *hdr_dst, *hdr_src;
    struct gpt_part_ent *table_dst, *table_src;

    if(restore_primary) {
        hdr_dst = &gpt->hdr_prim;
        hdr_src = &gpt->hdr_sec;
        table_dst = gpt->table_prim;
        table_src = gpt->table_sec;
    } else {
        hdr_dst = &gpt->hdr_sec;
        hdr_src = &gpt->hdr_prim;
        table_dst = gpt->table_sec;
        table_src = gpt->table_prim;
    }

    memcpy(hdr_dst, hdr_src, sizeof(*hdr_src));
    hdr_dst->my_lba = hdr_src->alt_lba;
    hdr_dst->alt_lba = hdr_src->my_lba;
    hdr_dst->part_table_lba = table_dst_lba;

    for(i = 0; i < hdr_src->part_table_entry_cnt; i++) {
        memcpy(&table_dst[i], &table_src[i], sizeof(table_src[i]));
    }

    hdr_dst->hdr_crc32 = gpt_hdr_crc_create(hdr_dst);
}

static pflag gpt_table_is_valid(const struct gpt_hdr *hdr,
                         const struct gpt_part_ent table[])
{
    return hdr->part_table_crc32 ==
           gpt_table_crc_create(table, hdr->part_table_entry_cnt);
}

static pflag gpt_hdr_is_valid(const struct gpt_hdr *hdr, plba hdr_lba)
{
    return hdr->hdr_crc32 == gpt_hdr_crc_create(hdr) &&
           hdr->my_lba == hdr_lba;
}

static pflag gpt_is_part_used(const struct gpt_part_ent *part)
{
    return !guid_is_zero(&part->type_guid);
}

static pflag gpt_is_present(const pu8 *buf)
{
    return 0 == strncmp((const char *) buf, GPT_SIG, ARRAY_SIZE(GPT_SIG) - 1);
}

static pres
gpt_pair_save(const struct gpt_hdr *hdr, const struct gpt_part_ent table[],
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
        plog_err("Failed to map GPT header at sector %llu", hdr_lba);
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
        plog_err("Failed to map GPT table at sector %llu", table_lba);
        save_res = pres_fail;
        goto exit;
    }

    /* Write GPT table */
    gpt_table_write(table_reg, table, hdr->part_table_entry_cnt,
                    hdr->part_entry_sz);

    plog_dbg("Saved GPT: header/table %llu/%llu", hdr_lba, table_lba);

    /* Saved successfully */
    save_res = pres_ok;

exit:
    /* If mapped, unmap GPT header sector */
    if(hdr_reg) {
        res = unmap_secs(hdr_reg, img_ctx, hdr_lba, hdr_sz_secs);
        if(!res) {
            plog_err("Failed to unmap GPT header at sector %llu", hdr_lba);
            return pres_fail;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_lba, table_sz_secs);
        if(!res) {
            plog_err("Failed to unmap GPT table at sector %llu", table_lba);
            return pres_fail;
        }
    }

    return save_res;
}

static enum gpt_pair_load_res
gpt_pair_load(struct gpt_hdr *hdr, struct gpt_part_ent table[],
              const struct img_ctx *img_ctx, plba hdr_lba)
{
    enum gpt_pair_load_res load_res;
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
        plog_err("Failed to map GPT header at sector %llu", hdr_lba);
        load_res = gpt_pair_load_fatal;
        goto exit;
    }

    /* Check GPT header signature */
    if(!gpt_is_present(hdr_reg)) {
        load_res = gpt_pair_load_hdr_inv;
        goto exit;
    }

    /* Read GPT header */
    gpt_hdr_read(hdr_reg, hdr);

    /* Check GPT header CRC */
    if(!gpt_hdr_is_valid(hdr, hdr_lba)) {
        load_res = gpt_pair_load_hdr_inv;
        goto exit;
    }

    if(hdr->part_table_entry_cnt > gpt_max_part_cnt) {
        plog_err("GPT table partition count (%lu) is greater, than "
                 "maximum supported (%lu)", hdr->part_table_entry_cnt,
                 gpt_max_part_cnt);
        load_res = gpt_pair_load_fatal;
        goto exit;
    }

    /* Get GPT table LBA and size */
    table_lba = hdr->part_table_lba;
    table_sz_secs = byte_to_lba(img_ctx, hdr->part_table_entry_cnt *
                                hdr->part_entry_sz, 1);

    /* Map GPT table sectors */
    table_reg = map_secs(img_ctx, table_lba, table_sz_secs);
    if(table_reg == NULL) {
        plog_err("Failed to map GPT table at sector %llu", table_lba);
        load_res = gpt_pair_load_fatal;
        goto exit;
    }

    /* Read GPT table */
    gpt_table_read(table_reg, table, hdr->part_table_entry_cnt,
                   hdr->part_entry_sz);

    /* Check GPT table CRC */
    if(!gpt_table_is_valid(hdr, table)) {
        load_res = gpt_pair_load_table_inv;
        goto exit;
    }

    plog_dbg("Loaded GPT: header/table %llu/%llu", hdr_lba, table_lba);

    /* Loaded successfully */
    load_res = gpt_pair_load_ok;

exit:
    /* If mapped, unmap GPT header sector */
    if(hdr_reg) {
        res = unmap_secs(hdr_reg, img_ctx, hdr_lba, hdr_sz_secs);
        if(!res) {
            plog_err("Failed to unmap GPT header at sector %llu", hdr_lba);
            return gpt_pair_load_fatal;
        }
    }

    /* If mapped, unmap GPT table sectors */
    if(table_reg) {
        res = unmap_secs(table_reg, img_ctx, table_lba, table_sz_secs);
        if(!res) {
            plog_err("Failed to unmap GPT table at sector %llu", table_lba);
            return gpt_pair_load_fatal;
        }
    }

    return load_res;
}

static pres gpt_save(const struct gpt *gpt, const struct img_ctx *img_ctx)
{
    pres res;

    /* UEFI specification requires to update secondary GPT first */
    res = gpt_pair_save(&gpt->hdr_sec, gpt->table_sec, img_ctx);
    if(!res) {
        return pres_fail;
    }

    res = gpt_pair_save(&gpt->hdr_prim, gpt->table_prim, img_ctx);
    if(!res) {
        return pres_fail;
    }

    plog_dbg("Saved GPT");
    return pres_ok;
}

static enum schem_load_res
gpt_load(struct gpt *gpt, const struct img_ctx *img_ctx)
{
    enum schem_load_res res;
    enum gpt_pair_load_res gpt_res_prim;
    enum gpt_pair_load_res gpt_res_sec;
    plba gpt_lba_sec;
    plba gpt_table_lba;

    /* Load primary GPT, located at LBA 1 */
    gpt_res_prim = gpt_pair_load(&gpt->hdr_prim, gpt->table_prim, img_ctx, 1);
    if(gpt_res_prim == gpt_pair_load_fatal) {
        plog_err("Error while loading primary GPT");
        res = schem_load_fatal;
        goto exit;
    }

    /* If primary GPT loaded successfully, use alt_lba, otherwise use
     * last image LBA to locate secondary GPT */
    if(gpt_res_prim == gpt_pair_load_ok) {
        gpt_lba_sec = gpt->hdr_prim.alt_lba;
    } else {
        gpt_lba_sec = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;
    }

    /* Load secondary GPT */
    gpt_res_sec = gpt_pair_load(&gpt->hdr_sec, gpt->table_sec, img_ctx,
                                gpt_lba_sec);
    if(gpt_res_sec == gpt_pair_load_fatal) {
        plog_err("Error while loading secondary GPT");
        res = schem_load_fatal;
        goto exit;
    }

    /* Both GPTs are not present or are corrupted */
    if(gpt_res_prim != gpt_pair_load_ok && gpt_res_sec != gpt_pair_load_ok) {
        res = schem_load_not_found;
        goto exit;
    }

    /* Primary GPT is ok, secondary GPT is corrupted */
    if(gpt_res_prim == gpt_pair_load_ok && gpt_res_sec != gpt_pair_load_ok) {
        plog_info("Secondary GPT is corrupted and will be restored on the "
                  "next write");

        /* Secondary GPT table LBA */
        gpt_table_lba = gpt->hdr_prim.alt_lba -
                        byte_to_lba(img_ctx,
                                    gpt->hdr_prim.part_table_entry_cnt *
                                    gpt->hdr_prim.part_entry_sz, 1);
        /* Restore secondary GPT */
        gpt_restore(gpt, gpt_table_lba, 0);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is corrupted, secondary GPT is ok */
    if(gpt_res_prim != gpt_pair_load_ok && gpt_res_sec == gpt_pair_load_ok) {
        plog_info("Primary GPT is corrupted and will be restored on the "
                  "next write");

        /* Primary GPT table LBA */
        gpt_table_lba = gpt->hdr_sec.alt_lba + 1;

        /* Restore primary GPT */
        gpt_restore(gpt, gpt_table_lba, 1);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is ok, secondary GPT is ok */

    res = schem_load_ok;

exit:
    if(res == schem_load_ok) {
        plog_dbg("Loaded GPT");
    }

    return res;
}

static void gpt_calc_pos(const struct img_ctx *img_ctx,
                         plba *hdr_lba_prim, plba *hdr_lba_sec,
                         plba *table_lba_prim, plba *table_lba_sec,
                         plba *table_sz)
{
    /* Primary GPT header is located at LBA 1 */
    *hdr_lba_prim = 1;

    /* Secondary GPT header is located at image last LBA */
    *hdr_lba_sec = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;

    /* GPT table size */
    *table_sz = byte_to_lba(img_ctx, gpt_max_part_cnt * gpt_part_ent_sz, 1);

    /* Primary GPT table is located after primary GPT header */
    *table_lba_prim = *hdr_lba_prim + 1;

    /* Secondary GPT table is located before secondary GPT header */
    *table_lba_sec = *hdr_lba_sec - *table_sz;
}

static void gpt_from_schem(const struct schem *schem, struct gpt *gpt,
                           const struct img_ctx *img_ctx)
{
    plba hdr_lba_prim;
    plba hdr_lba_sec;
    plba table_lba_prim;
    plba table_lba_sec;
    plba table_sz;
    pu32 i;
    struct gpt_part_ent *part_gpt;
    const struct schem_part *part;

    gpt_calc_pos(img_ctx, &hdr_lba_prim, &hdr_lba_sec, &table_lba_prim,
                 &table_lba_sec, &table_sz);

    /* Convert scheme details */
    gpt->hdr_prim.rev = gpt_hdr_rev;
    gpt->hdr_prim.hdr_sz = gpt_hdr_sz;
    gpt->hdr_prim.my_lba = hdr_lba_prim;
    gpt->hdr_prim.alt_lba = hdr_lba_sec;
    gpt->hdr_prim.first_usable_lba = schem->first_usable_lba;
    gpt->hdr_prim.last_usable_lba = schem->last_usable_lba;
    gpt->hdr_prim.part_table_lba = table_lba_prim;
    gpt->hdr_prim.part_table_entry_cnt = schem->part_cnt;
    gpt->hdr_prim.part_entry_sz = gpt_part_ent_sz;

    memcpy(&gpt->hdr_prim.disk_guid, &schem->id.guid, sizeof(struct guid));

    /* Convert partitions */
    for(i = 0; i < schem->part_cnt; i++) {
        part = &schem->table[i];

        if(!schem_part_is_used_gpt(part)) {
            continue;
        }

        part_gpt = &gpt->table_prim[i];

        memcpy(&part_gpt->type_guid, &part->type.guid, sizeof(struct guid));
        memcpy(&part_gpt->unique_guid, &part->unique_guid,
               sizeof(struct guid));

        part_gpt->start_lba = part->start_lba;
        part_gpt->end_lba = part->end_lba;
        part_gpt->attr = part->attr;
        memcpy(&part_gpt->name, &part->name, sizeof(part_gpt->name));
    }

    /* Compute GPT table CRC */
    gpt->hdr_prim.part_table_crc32 =
        gpt_table_crc_create(gpt->table_prim,
                             gpt->hdr_prim.part_table_entry_cnt);

    /* Compute GPT header CRC */
    gpt->hdr_prim.hdr_crc32 = gpt_hdr_crc_create(&gpt->hdr_prim);

    /* Restore GPT secondary header and table */
    gpt_restore(gpt, table_lba_sec, 0);
}

static void gpt_to_schem(struct schem *schem, const struct gpt *gpt)
{
    pu32 i;
    const struct gpt_part_ent *part_gpt;
    struct schem_part *part;

    /* Convert scheme details */
    schem->type = schem_type_gpt;
    memcpy(&schem->id.guid, &gpt->hdr_prim.disk_guid, sizeof(struct guid));
    schem->first_usable_lba = gpt->hdr_prim.first_usable_lba;
    schem->last_usable_lba = gpt->hdr_prim.last_usable_lba;
    schem->part_cnt = gpt->hdr_prim.part_table_entry_cnt;

    memset(schem->table, 0, sizeof(*schem->table) * schem->part_cnt);

    /* Convert partitions */
    for(i = 0; i < schem->part_cnt; i++) {
        part_gpt = &gpt->table_prim[i];

        if(!gpt_is_part_used(part_gpt)) {
            continue;
        }

        part = &schem->table[i];

        memcpy(&part->type.guid, &part_gpt->type_guid, sizeof(struct guid));
        memcpy(&part->unique_guid, &part_gpt->unique_guid,
               sizeof(struct guid));

        part->start_lba = part_gpt->start_lba;
        part->end_lba = part_gpt->end_lba;
        part->attr = part_gpt->attr;

        memcpy(&part->name, &part_gpt->name, sizeof(part_gpt->name));
    }
}

void schem_init_gpt(struct schem *schem, const struct img_ctx *img_ctx)
{
    plba hdr_lba_prim;
    plba hdr_lba_sec;
    plba table_lba_prim;
    plba table_lba_sec;
    plba table_sz;

    gpt_calc_pos(img_ctx, &hdr_lba_prim, &hdr_lba_sec, &table_lba_prim,
                 &table_lba_sec, &table_sz);

    schem->type = schem_type_gpt;
    schem->first_usable_lba = table_lba_prim + table_sz;
    schem->last_usable_lba = table_lba_sec - 1;
    schem->part_cnt = gpt_max_part_cnt;
    guid_create(&schem->id.guid);

    memset(schem->table, 0, sizeof(*schem->table) * schem->part_cnt);
}

void schem_part_init_gpt(struct schem_part *part)
{
    memset(part, 0, sizeof(*part));
    guid_create(&part->unique_guid);
    memcpy(&part->type.guid, &gpt_part_type_def, sizeof(gpt_part_type_def));
}

pflag schem_part_is_used_gpt(const struct schem_part *part)
{
    return !guid_is_zero(&part->type.guid);
}

enum schem_load_res
schem_load_gpt(struct schem *schem, const struct img_ctx *img_ctx)
{
    struct gpt gpt;
    enum schem_load_res res;

    memset(&gpt, 0, sizeof(gpt));

    /* Allocate tables */
    gpt.table_prim = calloc(gpt_max_part_cnt, sizeof(struct gpt_part_ent));
    gpt.table_sec = calloc(gpt_max_part_cnt, sizeof(struct gpt_part_ent));

    if(gpt.table_prim == NULL || gpt.table_sec == NULL) {
        return schem_load_fatal;
    }

    /* Load GPT */
    res = gpt_load(&gpt, img_ctx);
    if(res != schem_load_ok) {
        goto exit;
    }

    /* Convert GPT to general scheme */
    gpt_to_schem(schem, &gpt);

    res = schem_load_ok;

exit:
    /* Free tables */
    gpt_free(&gpt);

    return res;
}

pres schem_save_gpt(const struct schem *schem, const struct img_ctx *img_ctx)
{
    struct gpt gpt;
    pres res;

    memset(&gpt, 0, sizeof(gpt));

    /* Allocate tables */
    gpt.table_prim = calloc(gpt_max_part_cnt, sizeof(struct gpt_part_ent));
    gpt.table_sec = calloc(gpt_max_part_cnt, sizeof(struct gpt_part_ent));

    if(gpt.table_prim == NULL || gpt.table_sec == NULL) {
        return pres_fail;
    }

    /* Convert general scheme to GPT */
    gpt_from_schem(schem, &gpt, img_ctx);

    /* Save GPT */
    res = gpt_save(&gpt, img_ctx);

    /* Free tables */
    gpt_free(&gpt);

    return res;
}

