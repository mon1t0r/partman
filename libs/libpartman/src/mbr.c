#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "mbr.h"
#include "memutils.h"

enum {
    /* MBR size, in bytes */
    mbr_sz        = 512,

    /* MBR boot signature */
    mbr_boot_sig  = 0xAA55,

    /* Protective MBR partition type */
    mbr_prot_type = 0xEE
};

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

static void mbr_part_write(pu8 *buf, const struct mbr_part *mbr_part)
{
    write_pu8 (buf,      mbr_part->boot_ind );
    write_pu24(buf + 1,  mbr_part->start_chs);
    write_pu8 (buf + 4,  mbr_part->type     );
    write_pu24(buf + 5,  mbr_part->end_chs  );
    write_pu32(buf + 8,  mbr_part->start_lba);
    write_pu32(buf + 12, mbr_part->sz_lba   );
}

static void mbr_part_read(const pu8 *buf, struct mbr_part *mbr_part)
{
    mbr_part->boot_ind  = read_pu8 (buf     );
    mbr_part->start_chs = read_pu24(buf + 1 );
    mbr_part->type      = read_pu8 (buf + 4 );
    mbr_part->end_chs   = read_pu24(buf + 5 );
    mbr_part->start_lba = read_pu32(buf + 8 );
    mbr_part->sz_lba    = read_pu32(buf + 12);
}

void mbr_write(pu8 *buf, const struct mbr *mbr)
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

void mbr_read(const pu8 *buf, struct mbr *mbr)
{
    /* Disk signature */
    mbr->disk_sig = read_pu32(buf + 440);

    /* Partitions */
    mbr_part_read(buf + 446, &mbr->partitions[0]);
    mbr_part_read(buf + 462, &mbr->partitions[1]);
    mbr_part_read(buf + 478, &mbr->partitions[2]);
    mbr_part_read(buf + 494, &mbr->partitions[3]);
}

void mbr_init_protective(struct mbr *mbr, const struct img_ctx *img_ctx)
{
    struct mbr_part *part;
    plba img_sz_lba;

    img_sz_lba = byte_to_lba(img_ctx, img_ctx->img_sz, 0);
    if(img_sz_lba > 0xFFFFFFFF) {
        img_sz_lba = 0x100000000;
    }

    memset(mbr, 0, sizeof(*mbr));

    part = &mbr->partitions[0];

    part->boot_ind = 0x0;
    part->type = mbr_prot_type;
    part->start_lba = 1;
    part->sz_lba = img_sz_lba - part->start_lba;
    part->start_chs = lba_to_chs(img_ctx, part->start_lba);
    part->end_chs = lba_to_chs(img_ctx, part->start_lba + part->sz_lba - 1);
}

pflag mbr_is_present(const pu8 *buf)
{
    return read_pu16(buf + 510) == mbr_boot_sig;
}

pflag mbr_is_part_used(const struct mbr_part *part)
{
    return part->type != 0 && part->sz_lba != 0;
}

enum mbr_load_res mbr_load(struct mbr *mbr, const struct img_ctx *img_ctx)
{
    enum mbr_load_res load_res;
    pres res;
    pu8 *reg;

    /* Map MBR sector */
    reg = mbr_map(img_ctx);
    if(reg == NULL) {
        fprintf(stderr, "Failed to map image MBR\n");
        return mbr_load_fatal;
    }

    /* Check signature and read MBR */
    if(mbr_is_present(reg)) {
        mbr_read(reg, mbr);
        load_res = mbr_load_ok;
    } else {
        load_res = mbr_load_not_found;
    }

    /* Unmap MBR sector */
    res = mbr_unmap(reg, img_ctx);
    if(!res) {
        fprintf(stderr, "Failed to unmap image MBR\n");
        return mbr_load_fatal;
    }

    return load_res;
}

pres mbr_save(const struct mbr *mbr, const struct img_ctx *img_ctx)
{
    pres res;
    pu8 *reg;

    /* Map MBR sector */
    reg = mbr_map(img_ctx);
    if(reg == NULL) {
        fprintf(stderr, "Failed to map image MBR\n");
        return pres_fail;
    }

    /* Write MBR */
    mbr_write(reg, mbr);

    /* Unmap MBR sector */
    res = mbr_unmap(reg, img_ctx);
    if(!res) {
        fprintf(stderr, "Failed to unmap image MBR\n");
        return pres_fail;
    }

    return pres_ok;
}

