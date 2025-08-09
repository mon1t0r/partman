#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "img_ctx.h"

enum {
    sec_sz = 512,
    img_sz = 2048
};

int main(void)
{
    /* Testing code */

    int fd;
    char buf[1];
    struct img_ctx ctx;

    fd = open("test.bin", O_RDWR|O_CREAT|O_TRUNC, 0666);

    lseek(fd, img_sz - 1, SEEK_SET);

    buf[0] = 0;
    write(fd, buf, 1);

    img_ctx_init(&ctx, sec_sz, img_sz);
    img_ctx_map(&ctx, fd, 0);

    ctx.mbr.partitions[0].start_lba = 0x00001122;
    ctx.mbr.partitions[0].sz_lba = 0xAABBCCDD;

    mbr_write(ctx.mbr_reg, &ctx.mbr);

    img_ctx_unmap(&ctx);
    close(fd);

    return 0;
}
