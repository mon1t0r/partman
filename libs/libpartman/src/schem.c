#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "schem.h"
#include "log.h"
#include "mbr.h"
#include "gpt.h"

static void schem_free(struct schem *schem)
{
    free(schem->table);
}

static pu32 schem_get_max_part_cnt(enum schem_type type)
{
    switch(type) {
        case schem_type_mbr:
            return mbr_max_part_cnt;

        case schem_type_gpt:
            return gpt_max_part_cnt;

        case schem_cnt:
            break;
    }

    /* This should never happen */
    return 0;
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

        case schem_cnt:
            break;
    }
}

static pres schem_new(struct schem *schem, const struct img_ctx *img_ctx,
                      enum schem_type type, pflag init)
{
    memset(schem, 0, sizeof(*schem));

    /* Allocate new scheme table */
    schem->table = calloc(schem_get_max_part_cnt(type),
                          sizeof(struct schem_part));

    if(schem->table == NULL) {
        return pres_fail;
    }

    /* Map new scheme functions */
    schem_map_funcs(&schem->funcs, type);

    /* Init new scheme if requested */
    if(init) {
        schem->funcs.init(schem, img_ctx);
    }

    return pres_ok;
}

static pres schem_ctx_schem_alloc(struct schem_ctx *schem_ctx,
                                  const struct img_ctx *img_ctx,
                                  enum schem_type type, pflag init)
{
    /* Initialize GPT */
    schem_ctx->schemes[type] = malloc(sizeof(struct schem));
    if(!schem_ctx->schemes[type]) {
        return pres_fail;
    }

    return schem_new(schem_ctx->schemes[type], img_ctx, type, init);
}

static pres schem_ctx_new_gpt(struct schem_ctx *schem_ctx,
                              const struct img_ctx *img_ctx)
{
    pres res;

    /* Initialize GPT */
    res = schem_ctx_schem_alloc(schem_ctx, img_ctx, schem_type_gpt, 1);
    if(!res) {
        return pres_fail;
    }

    /* Initialize protective MBR */
    res = schem_ctx_schem_alloc(schem_ctx, img_ctx, schem_type_mbr, 1);
    if(!res) {
        return pres_fail;
    }

    schem_mbr_set_protective(schem_ctx->schemes[schem_type_mbr], img_ctx);

    return pres_ok;
}

void schem_ctx_init(struct schem_ctx *schem_ctx)
{
    memset(schem_ctx, 0, sizeof(*schem_ctx));
}

pres schem_ctx_new(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx,
                   enum schem_type type)
{
    /* Reset context and any previous schemes */
    schem_ctx_reset(schem_ctx);

    switch(type) {
        case schem_type_gpt:
            return schem_ctx_new_gpt(schem_ctx, img_ctx);

        case schem_cnt:
            break;

        default:
            return schem_ctx_schem_alloc(schem_ctx, img_ctx, type, 1);
    }

    return pres_fail;
}

pres schem_ctx_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    int i;
    struct schem schem;
    pres res_new;
    enum schem_load_res res_load;

    plog_dbg("Scheme detection and loading started");

    /* Reset context and any previous schemes */
    schem_ctx_reset(schem_ctx);

    for(i = 0; i < schem_cnt; i++) {
        /* Create new scheme, do not initialize */
        res_new = schem_new(&schem, img_ctx, i, 0);
        if(!res_new) {
            return pres_fail;
        }

        /* Try loading new scheme */
        res_load = schem.funcs.load(&schem, img_ctx);

        if(res_load == schem_load_fatal) {
            return pres_fail;
        }

        if(res_load == schem_load_not_found) {
            schem_free(&schem);
            continue;
        }

        /* Allocate new entry and copy scheme */
        schem_ctx->schemes[i] = malloc(sizeof(struct schem));
        if(!schem_ctx->schemes[i]) {
            return pres_fail;
        }

        memcpy(schem_ctx->schemes[i], &schem, sizeof(schem));

        plog_dbg("Scheme #%d is loaded", i);
    }

    if(!schem_ctx->schemes[schem_type_gpt]) {
        return pres_ok;
    }

    /* Extra checks for GPT - Protective MBR */

    if(!schem_ctx->schemes[schem_type_mbr]) {
        plog_info("Protective MBR not found. A new Protective MBR loaded"
                  "instead");

        /* Allocate new MBR scheme entry */
        res_new = schem_ctx_schem_alloc(schem_ctx, img_ctx, schem_type_mbr, 1);
        if(!res_new) {
            return pres_fail;
        }

        schem_mbr_set_protective(schem_ctx->schemes[schem_type_mbr], img_ctx);
    } else if(!schem_mbr_is_protective(schem_ctx->schemes[schem_type_mbr])) {
        plog_info("MBR is found, but is not recognized as Protective MBR. "
                  "A new Protective MBR loaded instead");

        schem_init_mbr(schem_ctx->schemes[schem_type_mbr], img_ctx);
        schem_mbr_set_protective(schem_ctx->schemes[schem_type_mbr], img_ctx);
    } else {
        plog_dbg("Protective MBR detected and loaded");
    }

    return pres_ok;
}

pres schem_ctx_save(const struct schem_ctx *schem_ctx,
                    const struct img_ctx *img_ctx)
{
    int i;
    pres r;

    for(i = 0; i < schem_cnt; i++) {
        if(!schem_ctx->schemes[i]) {
            continue;
        }

        r = schem_ctx->schemes[i]->funcs.save(schem_ctx->schemes[i], img_ctx);
        if(!r) {
            return pres_fail;
        }
    }

    return pres_ok;
}

enum schem_type schem_ctx_get_type(const struct schem_ctx *schem_ctx)
{
    /* Check for GPT first, so we return GPT instead of protected MBR */
    static const enum schem_type check_order[] = {
        schem_type_gpt, schem_type_mbr
    };

    int i;

    for(i = 0; i < ARRAY_SIZE(check_order); i++) {
        if(schem_ctx->schemes[check_order[i]]) {
            return check_order[i];
        }
    }

    return schem_cnt;
}

void schem_ctx_reset(struct schem_ctx *schem_ctx)
{
    int i;
    for(i = 0; i < schem_cnt; i++) {
        if(!schem_ctx->schemes[i]) {
            continue;
        }

        schem_free(schem_ctx->schemes[i]);
        free(schem_ctx->schemes[i]);
        schem_ctx->schemes[i] = NULL;
    }
}

void schem_part_delete(struct schem_part *part)
{
    memset(part, 0, sizeof(*part));
}

p32 schem_find_overlap(const struct schem *schem, plba start_lba, plba end_lba,
                       p32 part_ign)
{
    pu32 i;
    const struct schem_part *part;

    for(i = 0; i < schem->part_cnt; i++) {
        if(i == part_ign) {
            continue;
        }

        part = &schem->table[i];

        if(!schem->funcs.part_is_used(part)) {
            continue;
        }

        /* Start LBA is inside of the partition */
        if(start_lba >= part->start_lba && start_lba <= part->end_lba) {
            return i;
        }

        /* End LBA is inside of the partition */
        if(end_lba >= part->start_lba && end_lba <= part->end_lba) {
            return i;
        }

        /* Partition is inside of the boundaries */
        if(start_lba < part->start_lba && end_lba > part->end_lba) {
            return i;
        }
    }

    /* No overlap detected */
    return -1;
}

p32 schem_find_part_index(const struct schem *schem, pflag part_used)
{
    pu32 i;

    for(i = 0; i < schem->part_cnt; i++) {
        if(schem->funcs.part_is_used(&schem->table[i]) == part_used) {
            return i;
        }
    }

    return -1;
}

plba_res schem_find_start_sector(const struct schem *schem,
                                 const struct img_ctx *img_ctx, p32 part_ign)
{
    plba lba;
    plba lba_no_align;
    pu32 i;
    const struct schem_part *part;
    pflag pos_changed;

    lba_no_align = schem->first_usable_lba;
    lba = lba_align(img_ctx, lba_no_align, 1);

    /* If aligned LBA is after the end of usable space */
    if(lba > schem->last_usable_lba) {
        lba = lba_no_align;
        lba_no_align = 0;
    }

    do {
        pos_changed = 0;

        for(i = 0; i < schem->part_cnt; i++) {
            if(i == part_ign) {
                continue;
            }

            part = &schem->table[i];

            if(!schem->funcs.part_is_used(part)) {
                continue;
            }

            if(lba < part->start_lba || lba > part->end_lba) {
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
            lba_no_align = part->end_lba + 1;

            /* If we reached the end of the usable space */
            if(lba_no_align > schem->last_usable_lba) {
                return -1;
            }

            /* Set aligned LBA for next iteration */
            lba = lba_align(img_ctx, lba_no_align, 1);

            /* If aligned LBA is after the end of usable space */
            if(lba > schem->last_usable_lba) {
                lba = lba_no_align;
                lba_no_align = 0;
            }

            /* Break internal cycle */
            break;
        }
    } while(pos_changed);

    return lba;
}

plba_res schem_find_last_sector(const struct schem *schem,
                                const struct img_ctx *img_ctx, p32 part_ign,
                                plba first_lba)
{
    plba next_lba_bound;
    pu32 i;
    const struct schem_part *part;
    plba test_end_lba;

    next_lba_bound = schem->last_usable_lba;

    for(i = 0; i < schem->part_cnt; i++) {
        if(i == part_ign) {
            continue;
        }

        part = &schem->table[i];

        if(!schem->funcs.part_is_used(part)) {
            continue;
        }

        /* If partition is located further than first LBA and LBA bound
         * is less, than partition start */
        if(part->start_lba > first_lba && part->start_lba < next_lba_bound) {
            next_lba_bound = part->start_lba - 1;
        }
    }

    /* End LBA can not be less, than first LBA */
    if(next_lba_bound < first_lba) {
        return -1;
    }

    /* Check if LBA can be aligned */
    test_end_lba = lba_align(img_ctx, next_lba_bound, 0);

    /* Minus 1 sector to get the end LBA of the previous alignment segment */
    if(test_end_lba > 0) {
        test_end_lba--;
    }

    if(
        test_end_lba >= first_lba &&
        schem_find_overlap(schem, first_lba, test_end_lba, part_ign) == -1
    ) {
        /* Return aligned LBA */
        return test_end_lba;
    }

    /* Return non-aligned LBA */
    return next_lba_bound;
}

