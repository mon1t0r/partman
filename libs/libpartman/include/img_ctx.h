#ifndef LIBPARTMAN_IMG_CTX_H
#define LIBPARTMAN_IMG_CTX_H

#include "partman_types.h"

struct img_ctx {
    /* Image file name */
    const char *img_name;

    /* Image file descriptor */
    int img_fd;

    /* Logical sector (block) size, in bytes */
    pu64 sec_sz;

    /* Image size, in bytes */
    pu64 img_sz;

    /* Partition alignment, in sectors */
    pu64 align;

    /* Maximum logical number of heads per cylinder (max 255) */
    pu8 hpc;

    /* Maximum logical number of sectors per track (max 63) */
    pu8 spt;
};

void img_ctx_init(struct img_ctx *ctx, const char *img_name, int img_fd,
                  pu64 img_sz);

pres img_ctx_validate(const struct img_ctx *ctx);

pu64 lba_to_byte(const struct img_ctx *ctx, plba lba);

plba byte_to_lba(const struct img_ctx *ctx, pu64 bytes, pflag round_up);

plba lba_align(const struct img_ctx *ctx, plba lba, pflag round_up);

pchs lba_to_chs(const struct img_ctx *ctx, plba lba);

pchs chs_tuple_to_int(pchs c, pchs h, pchs s);

void chs_int_to_tuple(pchs chs, pchs *c, pchs *h, pchs *s);

#endif

