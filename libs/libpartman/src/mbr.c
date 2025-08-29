#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "mbr.h"
#include "log.h"
#include "memutils.h"
#include "rand.h"

enum {
    /* MBR size, in bytes */
    mbr_sz             = 512,

    /* MBR boot signature */
    mbr_boot_sig       = 0xAA55,

    /* Protective MBR partition type */
    mbr_part_type_prot = 0xEE,

    /* Default MBR partition type - Linux */
    mbr_part_type_def  = 0x83
};

/* MBR partition structure */
struct mbr_part {
    /* Boot indicator */
    pu8 boot_ind;

    /* CHS address of first absolute sector in partition */
    pchs start_chs;

    /* Partition type */
    pu8 type;

    /* CHS address of last absolute sector in partition */
    pchs end_chs;

    /* LBA of first absolute sector in the partition */
    plba_mbr start_lba;

    /* Number of sectors in partition */
    plba_mbr sz_lba;
};

/* Master Boot Record (MBR) structure */
struct mbr {
    /* Unique disk signature */
    pu32 disk_sig;

    /* Partition table (4 primary partitions) */
    struct mbr_part partitions[4];
};

static pres mbr_unmap(pu8 *reg, const struct img_ctx *img_ctx)
{
    pu64 len;
    int c;

    /* MBR length, aligned to sectors, in bytes */
    len = lba_to_byte(img_ctx, byte_to_lba(img_ctx, mbr_sz, 1));

    c = munmap(reg, len);
    if(c == -1) {
        perror("munmap()");
        return pres_fail;
    }

    return pres_ok;
}

static pu8 *mbr_map(const struct img_ctx *img_ctx)
{
    pu64 len;
    pu8 *r;

    /* MBR length, aligned to sectors, in bytes.
     * MBR could possibly take less or more than 1 sector */
    len = lba_to_byte(img_ctx, byte_to_lba(img_ctx, mbr_sz, 1));

    /* Map sector(s), containing MBR, located at offset 0 */
    r = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, img_ctx->img_fd, 0);
    if(r == MAP_FAILED) {
        perror("mmap()");
        return NULL;
    }

    return r;
}

static void mbr_part_write(pu8 *buf, const struct mbr_part *mbr_part)
{
    write_pu8 (buf,      mbr_part->boot_ind);
    write_pu24(buf + 1,  mbr_part->start_chs);
    write_pu8 (buf + 4,  mbr_part->type);
    write_pu24(buf + 5,  mbr_part->end_chs);
    write_pu32(buf + 8,  mbr_part->start_lba);
    write_pu32(buf + 12, mbr_part->sz_lba);
}

static void mbr_part_read(const pu8 *buf, struct mbr_part *mbr_part)
{
    mbr_part->boot_ind  = read_pu8 (buf);
    mbr_part->start_chs = read_pu24(buf + 1);
    mbr_part->type      = read_pu8 (buf + 4);
    mbr_part->end_chs   = read_pu24(buf + 5);
    mbr_part->start_lba = read_pu32(buf + 8);
    mbr_part->sz_lba    = read_pu32(buf + 12);
}

static void mbr_write(pu8 *buf, const struct mbr *mbr)
{
    /* Bootstrap code 440 bytes */

    /* Disk signature, 4 bytes */
    write_pu32(buf + 440, mbr->disk_sig);

    /* Unknowm field, 2 bytes */

    /* Partitions, 16 bytes each */
    mbr_part_write(buf + 446, &mbr->partitions[0]);
    mbr_part_write(buf + 462, &mbr->partitions[1]);
    mbr_part_write(buf + 478, &mbr->partitions[2]);
    mbr_part_write(buf + 494, &mbr->partitions[3]);

    /* Boot signature, 2 bytes */
    write_pu16(buf + 510, mbr_boot_sig);
}

static void mbr_read(const pu8 *buf, struct mbr *mbr)
{
    /* Disk signature */
    mbr->disk_sig = read_pu32(buf + 440);

    /* Partitions */
    mbr_part_read(buf + 446, &mbr->partitions[0]);
    mbr_part_read(buf + 462, &mbr->partitions[1]);
    mbr_part_read(buf + 478, &mbr->partitions[2]);
    mbr_part_read(buf + 494, &mbr->partitions[3]);
}

static pflag mbr_is_part_used(const struct mbr_part *part)
{
    return part->type != 0;
}

static pflag mbr_is_present(const pu8 *buf)
{
    return read_pu16(buf + 510) == mbr_boot_sig;
}

static void mbr_init_schem(struct schem *schem, const struct img_ctx *img_ctx)
{
    schem->type = schem_type_mbr;
    schem->first_usable_lba = byte_to_lba(img_ctx, mbr_sz, 1);
    schem->last_usable_lba = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;
    /* Limit last usable LBA, due to MBR using plba_mbr type for storing LBA */
    if(schem->last_usable_lba > 0xFFFFFFFF) {
        schem->last_usable_lba = 0xFFFFFFFF;
    }
    schem->part_cnt = mbr_max_part_cnt;
}

static pres mbr_save(const struct mbr *mbr, const struct img_ctx *img_ctx)
{
    pres res;
    pu8 *reg;

    /* Map MBR sector */
    reg = mbr_map(img_ctx);
    if(reg == NULL) {
        plog_err("Failed to map image MBR");
        return pres_fail;
    }

    /* Write MBR */
    mbr_write(reg, mbr);

    plog_dbg("Saved MBR");

    /* Unmap MBR sector */
    res = mbr_unmap(reg, img_ctx);
    if(!res) {
        plog_err("Failed to unmap image MBR");
        return pres_fail;
    }

    return pres_ok;
}

static enum schem_load_res mbr_load(struct mbr *mbr, const struct img_ctx *img_ctx)
{
    enum schem_load_res load_res;
    pres res;
    pu8 *reg;

    /* Map MBR sector */
    reg = mbr_map(img_ctx);
    if(reg == NULL) {
        plog_err("Failed to map image MBR");
        return schem_load_fatal;
    }

    /* Check signature and read MBR */
    if(mbr_is_present(reg)) {
        mbr_read(reg, mbr);
        load_res = schem_load_ok;

        plog_dbg("Loaded MBR");
    } else {
        load_res = schem_load_not_found;
    }

    /* Unmap MBR sector */
    res = mbr_unmap(reg, img_ctx);
    if(!res) {
        plog_err("Failed to unmap image MBR");
        return schem_load_fatal;
    }

    return load_res;
}

static void mbr_from_schem(const struct schem *schem, struct mbr *mbr,
                           const struct img_ctx *img_ctx)
{
    pu32 i;
    struct mbr_part *part_mbr;
    const struct schem_part *part;

    /* Convert scheme details */
    mbr->disk_sig = schem->id.i;

    /* Convert partitions */
    for(i = 0; i < schem->part_cnt; i++) {
        part = &schem->table[i];

        if(!schem_part_is_used_mbr(part)) {
            continue;
        }

        part_mbr = &mbr->partitions[i];

        part_mbr->type = part->type.i;
        part_mbr->start_lba = part->start_lba;
        part_mbr->sz_lba = part->end_lba - part->start_lba + 1;
        part_mbr->start_chs = lba_to_chs(img_ctx, part->start_lba);
        part_mbr->end_chs = lba_to_chs(img_ctx, part->end_lba);
        part_mbr->boot_ind = part->boot_ind;
    }
}

static void mbr_to_schem(struct schem *schem, const struct mbr *mbr,
                         const struct img_ctx *img_ctx)
{
    pu32 i;
    const struct mbr_part *part_mbr;
    struct schem_part *part;

    /* Convert scheme details */
    mbr_init_schem(schem, img_ctx);
    schem->id.i = mbr->disk_sig;

    /* Convert partitions */
    for(i = 0; i < schem->part_cnt; i++) {
        part_mbr = &mbr->partitions[i];

        if(!mbr_is_part_used(part_mbr)) {
            continue;
        }

        part = &schem->table[i];

        part->type.i = part_mbr->type;
        part->start_lba = part_mbr->start_lba;
        part->end_lba = part_mbr->start_lba + part_mbr->sz_lba - 1;
        part->boot_ind = part_mbr->boot_ind;
    }
}

void schem_init_mbr(struct schem *schem, const struct img_ctx *img_ctx)
{
    mbr_init_schem(schem, img_ctx);
    schem->id.i = rand_32();
}

void schem_part_init_mbr(struct schem_part *part)
{
    memset(part, 0, sizeof(*part));
    part->type.i = mbr_part_type_def;
}

pflag schem_part_is_used_mbr(const struct schem_part *part)
{
    return part->type.i != 0;
}

enum schem_load_res
schem_load_mbr(struct schem *schem, const struct img_ctx *img_ctx)
{
    struct mbr mbr;
    enum schem_load_res res;

    res = mbr_load(&mbr, img_ctx);
    if(res != schem_load_ok) {
        return res;
    }

    mbr_to_schem(schem, &mbr, img_ctx);

    plog_dbg("MBR is detected and loaded");
    return schem_load_ok;
}

pres schem_save_mbr(const struct schem *schem, const struct img_ctx *img_ctx)
{
    struct mbr mbr;

    mbr_from_schem(schem, &mbr, img_ctx);

    return mbr_save(&mbr, img_ctx);
}

void
schem_mbr_set_protective(struct schem *schem, const struct img_ctx *img_ctx)
{
    struct schem_part *part;
    plba img_sz_lba;

    img_sz_lba = byte_to_lba(img_ctx, img_ctx->img_sz, 0);
    if(img_sz_lba > 0xFFFFFFFF) {
        img_sz_lba = 0x100000000;
    }

    memset(schem->table, 0, sizeof(*schem->table) * schem->part_cnt);

    part = &schem->table[0];

    part->boot_ind = 0x0;
    part->type.i = mbr_part_type_prot;
    part->start_lba = 1;
    part->end_lba = img_sz_lba - 1;
}

pflag schem_mbr_is_protective(struct schem *schem)
{
    return schem->table[0].type.i == mbr_part_type_prot;
}

