/* lseek64() */
#include "mbr.h"
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "img_ctx.h"

enum {
    /* Input buffer size, in bytes */
    input_buf_sz = 256,

    /* Used image size, in bytes. Will be configurable in future */
    used_image_size = 2048
};

static void mbr_print(const struct mbr *mbr)
{
    const struct mbr_part *part;
    int i;
    unsigned long c, h, s;

    printf("Disk signature: %lx\n", mbr->disk_sig);

    for(i = 0; i < sizeof(mbr->partitions) / sizeof(mbr->partitions[0]); i++) {
        part = &mbr->partitions[i];

        /* Empty partition */
        if(!mbr_is_part_used(part)) {
            continue;
        }

        printf("Partition #%d:\n", i);
        printf("Boot indicator %d\n", part->boot_ind);
        printf("Type           %d\n", part->type);

        chs_int_to_tuple(part->start_chs, &c, &h, &s);
        printf("Start C/H/S    %lu/%lu/%lu\n", c, h, s);

        chs_int_to_tuple(part->end_chs, &c, &h, &s);
        printf("End C/H/S      %lu/%lu/%lu\n", c, h, s);

        printf("Start LBA      %ld\n", part->start_lba);
        printf("Sectors        %ld\n\n", part->sz_lba);
    }
}

static int action_handle(struct img_ctx *ctx, int img_fd, int in)
{
    switch(in) {
        /* Help */
        case 'm':
            printf("*help should be here*\n");
            break;

        /* Print the partition table */
        case 'p':
            mbr_print(&ctx->mbr);
            break;
    }

    return 1;
}

static int user_routine(struct img_ctx *ctx, int img_fd)
{
    int c;
    char buf[input_buf_sz];
    char *s;

    for(;;) {
        printf("Command (m for help): ");

        s = fgets(buf, sizeof(buf), stdin);

        printf("\n");

        /* Character + '\n' */
        if(s == NULL || strlen(s) != 2) {
            return EXIT_SUCCESS;
        }

        c = action_handle(ctx, img_fd, s[0]);
        if(!c) {
            return EXIT_FAILURE;
        }
    }
}

static int img_ensure_size(int img_fd, unsigned long long img_sz)
{
    long long s;
    char buf;

    s = lseek64(img_fd, 0, SEEK_END);
    if(s == -1) {
        perror("lseek64()");
        return 0;
    }

    /* If image already has required size, return */
    if(s >= img_sz) {
        return 1;
    }

    /* Seek and write a single byte to ensure image size */
    s = lseek64(img_fd, img_sz - 1, SEEK_SET);
    if(s == -1) {
        perror("lseek64()");
        return 0;
    }

    buf = 0;
    s = write(img_fd, &buf, 1);
    if(s == -1) {
        perror("write()");
        return 0;
    }

    return 1;
}

int main(int argc, const char * const *argv)
{
    int img_fd;
    int code;
    struct img_ctx ctx;

    /* Image file not specified */
    if(argc < 2) {
        fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Open file */
    img_fd = open(argv[1], O_RDWR|O_CREAT, 0666);
    if(img_fd == -1) {
        perror("open()");
        fprintf(stderr, "Unable to open %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* Ensure img_fd has required size */
    code = img_ensure_size(img_fd, used_image_size);
    if(!code) {
        fprintf(stderr, "Unable to ensure image size\n");
        code = EXIT_FAILURE;
        goto exit;
    }

    /* Initialize context */
    img_ctx_init(&ctx, used_image_size);

    /* Map image parts to memory */
    img_ctx_map(&ctx, img_fd, 0);

    printf("partman 1.0\n\n");

    /* If MBR detected, read MBR  */
    if(mbr_is_present(ctx.mbr_reg)) {
        printf("MBR detected!\n");
        mbr_read(ctx.mbr_reg, &ctx.mbr);
    }

#if 0
    /* TODO: DEBUG CODE, REMOVE! */

    ctx.mbr.partitions[0].boot_ind = 0x0;
    ctx.mbr.partitions[0].type = 0xAB;
    ctx.mbr.partitions[0].start_chs = chs_tuple_to_int(666, 11, 22);
    /* Max CHS for normal MBR (should be (1023, 254, 63))*/
    ctx.mbr.partitions[0].end_chs = lba_to_chs(&ctx, 0xFFFFFFFF);
    ctx.mbr.partitions[0].start_lba = 0xAABBCCDD;
    ctx.mbr.partitions[0].sz_lba = 0xCCDDEEFF;

    ctx.mbr.partitions[1].boot_ind = 0x80;
    ctx.mbr.partitions[1].type = 0xCD;
    ctx.mbr.partitions[1].start_chs = chs_tuple_to_int(0, 0, 2);
    /* Max CHS for protective MBR in GPT layout */
    ctx.mbr.partitions[1].end_chs = chs_tuple_to_int(1023, 255, 63);
    ctx.mbr.partitions[1].start_lba = 0xDDCCBBAA;
    ctx.mbr.partitions[1].sz_lba = 0xFFEEDDCC;

    mbr_write(ctx.mbr_reg, &ctx.mbr);

    goto exit;

#endif

    /* Start user routine */
    code = user_routine(&ctx, img_fd);
    if(!code) {
        code = EXIT_FAILURE;
        goto exit;
    }

exit:
    img_ctx_unmap(&ctx);
    close(img_fd);

    return code;
}
