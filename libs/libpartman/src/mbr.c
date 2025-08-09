#include "mbr.h"
#include "memutils.h"

static void mbr_part_write(unsigned char *buf, const struct mbr_part *mbr_part)
{
    /* Boot indicator */
    write_int8(buf, mbr_part->boot_ind);

    /* Start CHS */
    write_int8(buf + 1, mbr_part->start_chs[0]);
    write_int8(buf + 2, mbr_part->start_chs[1]);
    write_int8(buf + 3, mbr_part->start_chs[2]);

    /* Partition type */
    write_int8(buf + 4, mbr_part->type);

    /* End CHS */
    write_int8(buf + 5, mbr_part->end_chs[0]);
    write_int8(buf + 6, mbr_part->end_chs[1]);
    write_int8(buf + 7, mbr_part->end_chs[2]);

    /* Start sector LBA */
    write_int32(buf + 8, mbr_part->start_lba);

    /* Number of sectors */
    write_int32(buf + 12, mbr_part->sz_lba);
}

static void mbr_part_read(const unsigned char *buf, struct mbr_part *mbr_part)
{
    /* Boot indicator */
    mbr_part->boot_ind = read_int8(buf);

    /* Start CHS */
    mbr_part->start_chs[0] = read_int8(buf + 1);
    mbr_part->start_chs[1] = read_int8(buf + 2);
    mbr_part->start_chs[2] = read_int8(buf + 3);

    /* Partition type */
    mbr_part->type = read_int8(buf + 4);

    /* End CHS */
    mbr_part->end_chs[0] = read_int8(buf + 5);
    mbr_part->end_chs[1] = read_int8(buf + 6);
    mbr_part->end_chs[2] = read_int8(buf + 7);

    /* Start sector LBA */
    mbr_part->start_lba = read_int32(buf + 8);

    /* Number of sectors */
    mbr_part->sz_lba = read_int32(buf + 12);
}

void mbr_write(unsigned char *buf, const struct mbr *mbr)
{
    /* Bootstrap code 440 bytes */

    /* Disk signature, 4 bytes */
    write_int32(buf + 440, mbr->disk_sig);

    /* Unknowm field, 2 bytes */

    /* Partitions, 16 bytes each */
    mbr_part_write(buf + 446, &mbr->partitions[0]);
    mbr_part_write(buf + 462, &mbr->partitions[1]);
    mbr_part_write(buf + 478, &mbr->partitions[2]);
    mbr_part_write(buf + 494, &mbr->partitions[3]);

    /* Boot signature, 2 bytes */
    write_int16(buf + 510, 0x55AA);
}

void mbr_read(const unsigned char *buf, struct mbr *mbr)
{
    /* Disk signature */
    mbr->disk_sig = read_int32(buf + 440);

    /* Partitions */
    mbr_part_read(buf + 446, &mbr->partitions[0]);
    mbr_part_read(buf + 462, &mbr->partitions[1]);
    mbr_part_read(buf + 478, &mbr->partitions[2]);
    mbr_part_read(buf + 494, &mbr->partitions[3]);
}

int mbr_detect(const unsigned char *buf)
{
    return read_int16(buf + 510) == 0x55AA;
}

void mbr_init_protective(struct mbr *mbr)
{
    struct mbr_part *part;

    mbr->disk_sig = 0;

    part = &mbr->partitions[0];

    part->boot_ind = 0;
    part->start_chs[0] = 0x00;
    part->start_chs[1] = 0x02;
    part->start_chs[2] = 0x00;

    /* GPT Protective MBR */
    part->type = 0xEE;

    /* TODO: Replace with actual size */
    part->end_chs[0] = 0xFF;
    part->end_chs[1] = 0xFF;
    part->end_chs[2] = 0xFF;

    /* TODO: Replace with actual size */
    part->start_lba = 0x01;
    part->sz_lba = 0xFFFFFFFF;
}
