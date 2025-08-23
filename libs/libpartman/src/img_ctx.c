#include <string.h>

#include "img_ctx.h"

void img_ctx_init(struct img_ctx *ctx, int img_fd, pu64 img_sz)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->img_fd = img_fd;
    ctx->img_sz = img_sz;

    /* Default values */
    ctx->sec_sz = 512;         /* 512 bytes per sector   */
    ctx->align  = 1024*1024*1; /* 1 MiB alignment        */
    ctx->hpc    = 255;         /* 255 heads per cylinder */
    ctx->spt    = 63;          /* 63 sectors per track   */
}

pu64 lba_to_byte(const struct img_ctx *ctx, plba lba)
{
    return lba * ctx->sec_sz;
}

plba byte_to_lba(const struct img_ctx *ctx, pu64 bytes, pflag round_up)
{
    return bytes / ctx->sec_sz + (round_up && (bytes % ctx->sec_sz) ? 1 : 0);
}

plba lba_align(const struct img_ctx *ctx, plba lba, pflag round_up)
{
    return (lba / ctx->align + (round_up ? 1 : 0)) * ctx->align;
}

pchs lba_to_chs(const struct img_ctx *ctx, plba lba)
{
    plba max_lba;
    pchs c, h, s;

    /* Max LBA, which can be converted to CHS */
    /* Formula used is to convert from CHS to LBA */
    max_lba = (0x3FF * ctx->hpc + (ctx->hpc-1)) * ctx->spt + (ctx->spt-1);

    if(lba > max_lba) {
        lba = max_lba;
    }

    /* Cylinders */
    c = lba / (ctx->hpc * ctx->spt);

    /* Heads */
    h = (lba / ctx->spt) % ctx->hpc;

    /* Sectors */
    s = (lba % ctx->spt) + 1;

    return chs_tuple_to_int(c, h, s);
}

pchs chs_tuple_to_int(pchs c, pchs h, pchs s)
{
    return ((c & 0xFF) << 16) | (((c >> 8) & 0x3) << 14) |
           ((s & 0x3F) << 8) | (h & 0xFF);
}

void chs_int_to_tuple(pchs chs, pchs *c, pchs *h, pchs *s)
{
    *c = (chs >> 14) & 0x3FF;
    *s = (chs >> 8) & 0x3F;
    *h = (chs >> 0) & 0xFF;
}

