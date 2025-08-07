#include <string.h>

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
    write_int32(buf + 12, mbr_part->size_lba);
}

void mbr_write(unsigned char *buf, const struct mbr *mbr)
{
    /* Bootstrap code */
    memset(buf, 0, 440);

    /* Disk signature */
    write_int32(buf + 440, mbr->disk_sig);

    /* Unknowm 2-byte field */
    memset(buf + 444, 0, 2);

    /* Partitions, 16 bytes each */
    mbr_part_write(buf + 446, &mbr->partitions[0]);
    mbr_part_write(buf + 462, &mbr->partitions[1]);
    mbr_part_write(buf + 478, &mbr->partitions[2]);
    mbr_part_write(buf + 494, &mbr->partitions[3]);

    /* Boot signature */
    write_int16(buf + 510, 0x55AA);
}
