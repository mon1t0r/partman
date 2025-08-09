#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "img_ctx.h"

unsigned long long
secs_to_bytes(const struct img_ctx *ctx, unsigned long long secs)
{
    return secs * ctx->sec_sz;
}

unsigned long long
bytes_to_secs(const struct img_ctx *ctx, unsigned long long bytes)
{
#if 0
    return (bytes + (ctx->sec_zs-1)) / ctx->sec_sz;
#else
    return bytes / ctx->sec_sz + (bytes % ctx->sec_sz ? 1 : 0);
#endif
}

int img_ctx_init(struct img_ctx *ctx, unsigned long long sec_sz,
                 unsigned long long img_sz)
{
    if(sec_sz > img_sz) {
        fprintf(stderr, "Sector/image size invalid: Image must contain at "
                "least 1 sector\n");
        return 0;
    }

    if(sec_sz <= 0) {
        fprintf(stderr, "Sector size invalid: Must be greater than 0\n");
        return 0;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->sec_sz = sec_sz;
    ctx->img_sz = img_sz;

    return 1;
}

int img_ctx_map(struct img_ctx *ctx, int img_fd, int map_gpt)
{
    /* img_fd size here must be greater or equal to ctx->img_sz,
     * otherwise mmap() behavior will be undefined */

    ctx->mbr_reg = mmap(NULL, secs_to_bytes(ctx, mbr_sz_secs),
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

    c = munmap(ctx->mbr_reg, secs_to_bytes(ctx, mbr_sz_secs));
    if(c == -1) {
        perror("munmap()");
        fprintf(stderr, "Failed to unmap image MBR\n");
        return 0;
    }

    /* TODO: Unmap GPT primary header */

    /* TODO: Unmap GPT secondary header */

    return 1;
}
