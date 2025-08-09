#ifndef LIBPARTMAN_IMG_CTX_H
#define LIBPARTMAN_IMG_CTX_H

#include "mbr.h"

struct img_ctx {
    /* Logical sector (block) size, in bytes */
    unsigned long long sec_sz;

    /* Image size, in bytes */
    unsigned long long img_sz;

    /* Partition alignment, in bytes */
    unsigned long long align;

    /* Maximum logical number of heads per cylinder */
    unsigned long hpc;

    /* Maximum logical number of sectors per track */
    unsigned long spt;

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
lba_to_byte(const struct img_ctx *ctx, unsigned long long secs);

unsigned long long
byte_to_lba(const struct img_ctx *ctx, unsigned long long bytes);

void lba_to_chs(const struct img_ctx *ctx, unsigned long long lba,
                unsigned char chs[3]);

unsigned long long
lba_align(const struct img_ctx *ctx, unsigned long long lba);

int img_ctx_init(struct img_ctx *ctx, unsigned long long img_sz);

int img_ctx_map(struct img_ctx *ctx, int img_fd, int map_gpt);

int img_ctx_unmap(struct img_ctx *ctx);

#endif
