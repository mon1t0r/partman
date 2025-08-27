#ifndef LIBPARTMAN_GPT_H
#define LIBPARTMAN_GPT_H

#include "partman_types.h"
#include "schem.h"

enum {
    /* Maximum supported partition count for GPT */
    gpt_max_part_cnt = 128
};

void schem_init_gpt(struct schem *schem, const struct img_ctx *img_ctx);

void schem_part_init_gpt(struct schem_part *part);

pflag schem_part_is_used_gpt(const struct schem_part *part);

enum schem_load_res
schem_load_gpt(struct schem *schem, const struct img_ctx *img_ctx);

pres schem_save_gpt(const struct schem *schem, const struct img_ctx *img_ctx);

#endif

