#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "schem.h"
#include "log.h"
#include "mbr.h"
#include "gpt.h"

static pu32 schem_get_max_part_cnt(enum schem_type type)
{
    switch(type) {
        case schem_type_mbr:
            return PARTMAN_SCHEM_MAX_PART_CNT_MBR;

        case schem_type_gpt:
            return gpt_max_part_cnt;

        case schem_type_none:
            /* (!!!) This should return maximum possible partition count for
             * all schemes */
            return gpt_max_part_cnt;
    }
}

static void schem_map_funcs(struct schem_funcs *funcs, enum schem_type type)
{
    switch(type) {
        case schem_type_mbr:
            funcs->init         = &schem_init_mbr;
            funcs->part_init    = &schem_part_init_mbr;
            funcs->part_is_used = &schem_part_is_used_mbr;
            funcs->load         = &schem_load_mbr;
            funcs->save         = &schem_save_mbr;
            break;

        case schem_type_gpt:
            funcs->init         = &schem_init_gpt;
            funcs->part_init    = &schem_part_init_gpt;
            funcs->part_is_used = &schem_part_is_used_gpt;
            funcs->load         = &schem_load_gpt;
            funcs->save         = &schem_save_gpt;
            break;

        case schem_type_none:
            break;
    }
}

void schem_init(struct schem *schem)
{
    memset(schem, 0, sizeof(*schem));
    schem->type = schem_type_none;
}

pres schem_change_type(struct schem *schem, const struct img_ctx *img_ctx,
                       enum schem_type type)
{
    /* Free previous table */
    if(schem->type != schem_type_none) {
        free(schem->table);
    }

    memset(schem, 0, sizeof(*schem));

    /* Return if scheme type NONE will be used */
    if(type == schem_type_none) {
        return pres_ok;
    }

    /* Map new scheme functions */
    schem_map_funcs(&schem->funcs, type);

    /* Init new scheme */
    schem->funcs.init(schem, img_ctx);

    /* Allocate new scheme table */
    schem->table = calloc(schem_get_max_part_cnt(type),
                          sizeof(struct schem_part));

    return schem->table != NULL;
}

pres schem_load(struct schem *schem, const struct img_ctx *img_ctx)
{
    /* Load GPT first (as loading MBR first will resul in loading
     * protective MBR) */
    static const enum schem_type load_order[] = {
        schem_type_gpt, schem_type_mbr
    };

    int i;
    enum schem_load_res res;

    plog_dbg("Scheme detection and loading started");

    /* Free any previous scheme */
    schem_change_type(schem, img_ctx, schem_type_none);

    /* Allocate maximum possible partition table, as we do not know, which
     * scmeme will be loaded at this point */
    schem->table = calloc(schem_get_max_part_cnt(schem_type_none),
                          sizeof(struct schem_part));

    for(i = 0; i < ARRAY_SIZE(load_order); i++) {
        /* Map loading scheme functions */
        schem_map_funcs(&schem->funcs, load_order[i]);

        res = schem->funcs.load(schem, img_ctx);

        /* Scheme found and loaded or fatal error happened */
        if(res != schem_load_not_found) {
            goto found;
        }
    }

    /* No scheme found */
    return pres_ok;

found:
    return res == schem_load_ok ? pres_ok : pres_fail;
}

/* TODO: FIX */

p32 schem_find_overlap(const struct schem_ctx *schem_ctx, pu32 part_cnt,
                       const struct schem_part *part, p32 part_ign)
{
    pu32 i;
    struct schem_part part_cmp;

    for(i = 0; i < part_cnt; i++) {
        if(i == part_ign) {
            continue;
        }

        if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
            continue;
        }

        schem_ctx->funcs.part_get(&schem_ctx->s, i, &part_cmp);

        /* Start LBA is inside of the partition */
        if(
            part->start_lba >= part_cmp.start_lba &&
            part->start_lba <= part_cmp.end_lba
        ) {
            return i;
        }

        /* End LBA is inside of the partition */
        if(
            part->end_lba >= part_cmp.start_lba &&
            part->end_lba <= part_cmp.end_lba
        ) {
            return i;
        }

        /* Partition is inside of the boundaries */
        if(
            part->start_lba < part_cmp.start_lba &&
            part->end_lba > part_cmp.end_lba
        ) {
            return i;
        }
    }

    /* No overlap detected */
    return -1;
}

p32 schem_find_part_index(const struct schem_ctx *schem_ctx, pu32 part_cnt,
                          pflag part_used)
{
    pu32 i;

    for(i = 0; i < part_cnt; i++) {
        if(schem_ctx->funcs.part_is_used(&schem_ctx->s, i) == part_used) {
            return i;
        }
    }

    return -1;
}

plba_res schem_find_start_sector(const struct schem_ctx *schem_ctx,
                                 const struct img_ctx *img_ctx,
                                 const struct schem_info *info, p32 part_ign)
{
    plba lba;
    plba lba_no_align;
    pu32 i;
    struct schem_part part;
    pflag pos_changed;

    lba_no_align = info->first_usable_lba;
    lba = lba_align(img_ctx, lba_no_align, 1);

    /* If aligned LBA is after the end of usable space */
    if(lba > info->last_usable_lba) {
        lba = lba_no_align;
        lba_no_align = 0;
    }

    do {
        pos_changed = 0;

        for(i = 0; i < info->part_cnt; i++) {
            if(i == part_ign) {
                continue;
            }

            if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
                continue;
            }

            schem_ctx->funcs.part_get(&schem_ctx->s, i, &part);

            if(lba < part.start_lba || lba > part.end_lba) {
                continue;
            }

            /* If current LBA intersects with partition */

            pos_changed = 1;

            /* If there is no align LBA from previous iteration, test it */
            if(lba_no_align != 0) {
                lba = lba_no_align;
                lba_no_align = 0;
                break;
            }

            /* Set no align LBA in case aligned LBA will intersect */
            lba_no_align = part.end_lba + 1;

            /* If we reached the end of the usable space */
            if(lba_no_align > info->last_usable_lba) {
                return -1;
            }

            /* Set aligned LBA for next iteration */
            lba = lba_align(img_ctx, lba_no_align, 1);

            /* If aligned LBA is after the end of usable space */
            if(lba > info->last_usable_lba) {
                lba = lba_no_align;
                lba_no_align = 0;
            }

            /* Break internal cycle */
            break;
        }
    } while(pos_changed);

    return lba;
}

plba_res schem_find_last_sector(const struct schem_ctx *schem_ctx,
                                const struct img_ctx *img_ctx,
                                const struct schem_info *info, p32 part_ign,
                                plba first_lba)
{
    plba next_lba_bound;
    pu32 i;
    struct schem_part part;

    next_lba_bound = info->last_usable_lba;

    for(i = 0; i < info->part_cnt; i++) {
        if(i == part_ign) {
            continue;
        }

        if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
            continue;
        }

        schem_ctx->funcs.part_get(&schem_ctx->s, i, &part);

        /* If partition is located further than first LBA and LBA bound
         * is less, than partition start */
        if(part.start_lba > first_lba && part.start_lba < next_lba_bound) {
            next_lba_bound = part.start_lba - 1;
        }
    }

    /* End LBA can not be less, than first LBA */
    if(next_lba_bound < first_lba) {
        return -1;
    }

    /* Check if LBA can be aligned */
    part.start_lba = first_lba;
    part.end_lba = lba_align(img_ctx, next_lba_bound, 0);

    /* Minus 1 sector to get the end LBA of the previous alignment segment */
    if(part.end_lba > 0) {
        part.end_lba--;
    }

    if(
        part.end_lba >= info->first_usable_lba &&
        schem_find_overlap(schem_ctx, info->part_cnt, &part, part_ign) == -1
    ) {
        /* Return aligned LBA */
        return part.end_lba;
    }

    /* Return non-aligned LBA */
    return next_lba_bound;
}

