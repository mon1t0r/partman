#ifndef LIBPARTMAN_IMG_CTX_H
#define LIBPARTMAN_IMG_CTX_H

#include "mbr.h"

struct img_ctx {
    /* Logical sector size, in bytes */
    unsigned long long sec_sz;

    /* Image size, in bytes */
    unsigned long long img_sz;

    /* In-memory MBR structure */
    struct mbr mbr;

    /* Pointer to a memory region, which maps image MBR region */
    unsigned char *mbr_reg;

    /* Pointer to a memory region, which maps image primary GPT region */
    unsigned char *gpt_reg_prim;

    /* Pointer to a memory region, which maps image secondary GPT region */
    unsigned char *gpt_reg_sec;
};

unsigned long long
secs_to_bytes(const struct img_ctx *ctx, unsigned long long secs);

unsigned long long
bytes_to_secs(const struct img_ctx *ctx, unsigned long long bytes);

int img_ctx_init(struct img_ctx *ctx, unsigned long long sec_sz,
                 unsigned long long img_sz);

int img_ctx_map(struct img_ctx *ctx, int img_fd, int map_gpt);

int img_ctx_unmap(struct img_ctx *ctx);

#endif
