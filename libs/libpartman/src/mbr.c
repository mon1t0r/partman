#include "mbr.h"
#include "memutils.h"

enum {
    mbr_boot_sig = 0xAA55
};

static void mbr_part_write(pu8 *buf, const struct mbr_part *mbr_part)
{
    /* Boot indicator */
    write_pu8(buf, mbr_part->boot_ind);

    /* Start CHS */
    write_pu24(buf + 1, mbr_part->start_chs);

    /* Partition type */
    write_pu8(buf + 4, mbr_part->type);

    /* End CHS */
    write_pu24(buf + 5, mbr_part->end_chs);

    /* Start sector LBA */
    write_pu32(buf + 8, mbr_part->start_lba);

    /* Number of sectors */
    write_pu32(buf + 12, mbr_part->sz_lba);
}

static void mbr_part_read(const pu8 *buf, struct mbr_part *mbr_part)
{
    /* Boot indicator */
    mbr_part->boot_ind = read_pu8(buf);

    /* Start CHS */
    mbr_part->start_chs = read_pu24(buf + 1);

    /* Partition type */
    mbr_part->type = read_pu8(buf + 4);

    /* End CHS */
    mbr_part->end_chs = read_pu24(buf + 5);

    /* Start sector LBA */
    mbr_part->start_lba = read_pu32(buf + 8);

    /* Number of sectors */
    mbr_part->sz_lba = read_pu32(buf + 12);
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

pflag mbr_is_present(const pu8 *buf)
{
    return read_pu16(buf + 510) == mbr_boot_sig;
}

pflag mbr_is_part_used(const struct mbr_part *part)
{
    return part->type != 0 && part->sz_lba != 0;
}

void mbr_init_protective(struct mbr *mbr)
{
    struct mbr_part *part;

    mbr->disk_sig = 0;

    part = &mbr->partitions[0];

    part->boot_ind = 0;
    part->start_chs = 0x000200;

    /* GPT Protective MBR */
    part->type = 0xEE;

    /* TODO: Replace with actual size */
    part->end_chs = 0xFFFFFF;

    /* TODO: Replace with actual size */
    part->start_lba = 0x01;
    part->sz_lba = 0xFFFFFFFF;
}
