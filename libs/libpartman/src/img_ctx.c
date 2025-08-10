#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "img_ctx.h"

pu64 lba_to_byte(const struct img_ctx *ctx, pu64 lba)
{
    return lba * ctx->sec_sz;
}

pu64 byte_to_lba(const struct img_ctx *ctx, pu64 bytes)
{
#if 0
    return (bytes + (ctx->sec_sz-1)) / ctx->sec_sz;
#else
    return bytes / ctx->sec_sz + (bytes % ctx->sec_sz ? 1 : 0);
#endif
}

pu32 lba_to_chs(const struct img_ctx *ctx, pu64 lba)
{
    pu64 max_lba;
    pu32 c, h, s;

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

pu64 lba_align(const struct img_ctx *ctx, pu64 lba)
{
    /* Next aligned LBA after input LBA */
    return (lba / ctx->align + 1) * ctx->align;
}

pu32 chs_tuple_to_int(pu32 c, pu32 h, pu32 s)
{
    return ((c & 0xFF) << 16) | (((c >> 8) & 0x3) << 14) |
           ((s & 0x3F) << 8) | (h & 0xFF);
}

void chs_int_to_tuple(pu32 chs, pu32 *c, pu32 *h, pu32 *s)
{
    *c = (chs >> 14) & 0x3FF;
    *s = (chs >> 8) & 0x3F;
    *h = (chs >> 0) & 0xFF;
}

void img_ctx_init(struct img_ctx *ctx, pu64 img_sz)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->img_sz = img_sz;

    /* Default values */
    ctx->sec_sz = 512;         /* 512 bytes per sector   */
    ctx->align  = 1024*1024*1; /* 1 MiB alignment        */
    ctx->hpc    = 255;         /* 255 heads per cylinder */
    ctx->spt    = 63;          /* 63 sectors per track   */
}

pres img_ctx_map(struct img_ctx *ctx, int img_fd, pflag map_gpt)
{
    /* img_fd size here must be greater or equal to ctx->img_sz,
     * otherwise mmap() behavior will be undefined */

    ctx->mbr_reg = mmap(NULL, mbr_sz,
                        PROT_READ|PROT_WRITE, MAP_SHARED,
                        img_fd, 0);
    if(ctx->mbr_reg == MAP_FAILED) {
        perror("mmap()");
        fprintf(stderr, "Failed to map image MBR\n");
        return pres_fail;
    }

    if(!map_gpt) {
        ctx->gpt_reg_prim = NULL;
        ctx->gpt_reg_sec = NULL;
        return pres_succ;
    }

    /* TODO: Map GPT primary header */

    /* TODO: Map GPT secondary header */

    return pres_succ;
}

pres img_ctx_unmap(struct img_ctx *ctx)
{
    int c;

    c = munmap(ctx->mbr_reg, mbr_sz);
    if(c == -1) {
        perror("munmap()");
        fprintf(stderr, "Failed to unmap image MBR\n");
        return pres_fail;
    }

    /* TODO: Unmap GPT primary header */

    /* TODO: Unmap GPT secondary header */

    return pres_succ;
}
