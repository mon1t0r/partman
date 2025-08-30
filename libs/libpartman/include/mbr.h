#ifndef LIBPARTMAN_MBR_H
#define LIBPARTMAN_MBR_H

#include "partman_types.h"
#include "schem.h"

enum {
    /* Maximum supported partition count for MBR */
    mbr_max_part_cnt = 4
};

void schem_init_mbr(struct schem *schem, const struct img_ctx *img_ctx);

void schem_part_init_mbr(struct schem_part *part);

pflag schem_part_is_used_mbr(const struct schem_part *part);

enum schem_load_res
schem_load_mbr(struct schem *schem, const struct img_ctx *img_ctx);

pres schem_save_mbr(const struct schem *schem, const struct img_ctx *img_ctx);

pres schem_remove_mbr(const struct img_ctx *img_ctx);

void schem_mbr_set_prot(struct schem *schem);

pflag schem_mbr_is_prot(const struct schem *schem);

pflag schem_mbr_part_is_prot(const struct schem_part *part);

#endif

