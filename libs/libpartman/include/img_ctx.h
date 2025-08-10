#ifndef LIBPARTMAN_IMG_CTX_H
#define LIBPARTMAN_IMG_CTX_H

#include "partman_types.h"
#include "mbr.h"

struct img_ctx {
    /* Logical sector (block) size, in bytes */
    pu64 sec_sz;

    /* Image size, in bytes */
    pu64 img_sz;

    /* Partition alignment, in bytes */
    pu64 align;

    /* Maximum logical number of heads per cylinder (max 255) */
    pu8 hpc;

    /* Maximum logical number of sectors per track (max 63) */
    pu8 spt;

    /* In-memory MBR structure */
    struct mbr mbr;

    /* Pointer to a memory region, which maps image MBR region */
    pu8 *mbr_reg;

    /* Pointer to a memory region, which maps image primary GPT region */
    pu8 *gpt_reg_prim;

    /* Pointer to a memory region, which maps image secondary GPT region */
    pu8 *gpt_reg_sec;
};

pu64 lba_to_byte(const struct img_ctx *ctx, pu64 lba);

pu64 byte_to_lba(const struct img_ctx *ctx, pu64 bytes);

pu32 lba_to_chs(const struct img_ctx *ctx, pu64 lba);

pu64 lba_align(const struct img_ctx *ctx, pu64 lba);

pu32 chs_tuple_to_int(pu32 c, pu32 h, pu32 s);

void chs_int_to_tuple(pu32 chs, pu32 *c, pu32 *h, pu32 *s);

void img_ctx_init(struct img_ctx *ctx, pu64 img_sz);

pres img_ctx_map(struct img_ctx *ctx, int img_fd, pflag map_gpt);

pres img_ctx_unmap(struct img_ctx *ctx);

#endif
