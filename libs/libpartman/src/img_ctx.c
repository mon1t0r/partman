#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "img_ctx.h"

unsigned long long
lba_to_byte(const struct img_ctx *ctx, unsigned long long lba)
{
    return lba * ctx->sec_sz;
}

unsigned long long
byte_to_lba(const struct img_ctx *ctx, unsigned long long bytes)
{
#if 0
    return (bytes + (ctx->sec_sz-1)) / ctx->sec_sz;
#else
    return bytes / ctx->sec_sz + (bytes % ctx->sec_sz ? 1 : 0);
#endif
}

unsigned long lba_to_chs(const struct img_ctx *ctx, unsigned long long lba)
{
    unsigned long long max_lba;
    unsigned long c, h, s;

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

unsigned long long
lba_align(const struct img_ctx *ctx, unsigned long long lba)
{
    /* Next aligned LBA after input LBA */
    return (lba / ctx->align + 1) * ctx->align;
}

unsigned long
chs_tuple_to_int(unsigned long c, unsigned long h, unsigned long s)
{
    return ((c & 0xFF) << 16) | (((c >> 8) & 0x3) << 14) |
           ((s & 0x3F) << 8) | (h & 0xFF);
}

void chs_int_to_tuple(unsigned long chs, unsigned long *c, unsigned long *h,
                      unsigned long *s)
{
    *c = (chs >> 14) & 0x3FF;
    *s = (chs >> 8) & 0x3F;
    *h = (chs >> 0) & 0xFF;
}

int img_ctx_init(struct img_ctx *ctx, unsigned long long img_sz)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->img_sz = img_sz;

    /* Default values */
    ctx->sec_sz = 512;         /* 512 bytes per sector   */
    ctx->align  = 1024*1024*1; /* 1 MiB alignment        */
    ctx->hpc    = 255;         /* 255 heads per cylinder */
    ctx->spt    = 63;          /* 63 sectors per track   */

    return 1;
}

int img_ctx_map(struct img_ctx *ctx, int img_fd, int map_gpt)
{
    /* img_fd size here must be greater or equal to ctx->img_sz,
     * otherwise mmap() behavior will be undefined */

    ctx->mbr_reg = mmap(NULL, lba_to_byte(ctx, mbr_sz_secs),
                        PROT_READ|PROT_WRITE, MAP_SHARED,
                        img_fd, 0);
    if(ctx->mbr_reg == MAP_FAILED) {
        perror("mmap()");
        fprintf(stderr, "Failed to map image MBR\n");
        return 0;
    }

    if(!map_gpt) {
        ctx->gpt_reg_prim = NULL;
        ctx->gpt_reg_sec = NULL;
        return 1;
    }

    /* TODO: Map GPT primary header */

    /* TODO: Map GPT secondary header */

    return 1;
}

int img_ctx_unmap(struct img_ctx *ctx)
{
    int c;

    c = munmap(ctx->mbr_reg, lba_to_byte(ctx, mbr_sz_secs));
    if(c == -1) {
        perror("munmap()");
        fprintf(stderr, "Failed to unmap image MBR\n");
        return 0;
    }

    /* TODO: Unmap GPT primary header */

    /* TODO: Unmap GPT secondary header */

    return 1;
}
