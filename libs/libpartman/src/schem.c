#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "schem.h"
#include "log.h"
#include "gpt.h"
#include "img_ctx.h"
#include "mbr.h"

enum schem_load_res {
    schem_load_ok, schem_load_not_found, schem_load_fatal
};

const struct guid guid_linux_fs = {
    0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79,
    { 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 }
};

/* === MBR === */

static pres schem_save_mbr(const union schem *schem,
                           const struct img_ctx *img_ctx)
{
    return mbr_save(&schem->s_mbr.mbr, img_ctx);
}

static enum schem_load_res
schem_load_mbr(struct schem_mbr *schem_mbr, const struct img_ctx *img_ctx)
{
    enum mbr_load_res mbr_res;

    mbr_res = mbr_load(&schem_mbr->mbr, img_ctx);
    if(mbr_res == mbr_load_fatal) {
        plog_err("An error occured while loading MBR");
        return schem_load_fatal;
    }

    if(mbr_res == mbr_load_ok) {
        return schem_load_ok;
    }

    return schem_load_not_found;
}

static void
schem_part_set_mbr(union schem *schem, const struct img_ctx *img_ctx,
                   pu32 index, const struct schem_part *part)
{
    struct mbr_part *part_mbr;

    part_mbr = &schem->s_mbr.mbr.partitions[index];

    part_mbr->start_lba = part->start_lba;
    part_mbr->sz_lba = part->end_lba - part->start_lba + 1;
    part_mbr->start_chs = lba_to_chs(img_ctx, part->start_lba);
    part_mbr->end_chs = lba_to_chs(img_ctx, part->end_lba);
}

static void
schem_part_get_mbr(const union schem *schem, pu32 index,
                   struct schem_part *part)
{
    const struct mbr_part *part_mbr;

    part_mbr = &schem->s_mbr.mbr.partitions[index];

    part->start_lba = part_mbr->start_lba;
    part->end_lba = part_mbr->start_lba + part_mbr->sz_lba - 1;
}

static void schem_part_delete_mbr(union schem *schem, pu32 index)
{
    struct mbr_part *part_mbr;

    part_mbr = &schem->s_mbr.mbr.partitions[index];

    memset(part_mbr, 0, sizeof(*part_mbr));
}

static void schem_part_new_mbr(union schem *schem, pu32 index)
{
    struct mbr_part *part_mbr;

    part_mbr = &schem->s_mbr.mbr.partitions[index];

    part_mbr->type = 0x83;
}

static pflag schem_part_is_used_mbr(const union schem *schem, pu32 index)
{
    return mbr_is_part_used(&schem->s_mbr.mbr.partitions[index]);
}

static void
schem_get_info_mbr(const union schem *schem, const struct img_ctx *img_ctx,
                   struct schem_info *info)
{
    info->first_usable_lba = byte_to_lba(img_ctx, mbr_sz, 1);
    info->last_usable_lba = byte_to_lba(img_ctx, img_ctx->img_sz, 0);
    info->part_cnt = ARRAY_SIZE(schem->s_mbr.mbr.partitions);
}

static pres schem_init_mbr(union schem *schem, const struct img_ctx *img_ctx)
{
    mbr_init_new(&schem->s_mbr.mbr);

    return pres_ok;
}

/* === GPT === */

static void schem_free_gpt(union schem *schem)
{
    struct schem_gpt *gpt;

    gpt = &schem->s_gpt;

    free(gpt->table_prim);
    free(gpt->table_sec);
}

static pres schem_save_gpt(const union schem *schem,
                           const struct img_ctx *img_ctx)
{
    const struct schem_gpt *gpt;
    pres res;

    gpt = &schem->s_gpt;

    /* Protective MBR */
    res = mbr_save(&gpt->mbr_prot.mbr, img_ctx);
    if(!res) {
        return pres_fail;
    }

    /* UEFI specification requires to update secondary GPT first */
    res = gpt_save(&gpt->hdr_sec, gpt->table_sec, img_ctx);
    if(!res) {
        return pres_fail;
    }

    return gpt_save(&gpt->hdr_prim, gpt->table_prim, img_ctx);
}

static enum schem_load_res
schem_load_gpt(struct schem_gpt *schem_gpt, const struct img_ctx *img_ctx)
{
    enum schem_load_res res;
    enum gpt_load_res gpt_res_prim;
    enum gpt_load_res gpt_res_sec;
    plba gpt_lba_sec;
    plba gpt_table_lba;

    schem_gpt->table_prim = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));
    schem_gpt->table_sec = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));

    if(schem_gpt->table_prim == NULL || schem_gpt->table_sec == NULL) {
        return schem_load_fatal;
    }

    /* Load primary GPT, located at LBA 1 */
    gpt_res_prim = gpt_load(&schem_gpt->hdr_prim, schem_gpt->table_prim,
                            img_ctx, 1);
    if(gpt_res_prim == gpt_load_fatal) {
        plog_err("Error while loading primary GPT");
        res = schem_load_fatal;
        goto exit;
    }

    /* If primary GPT loaded successfully, use alt_lba, otherwise use
     * last image LBA to locate secondary GPT */
    if(gpt_res_prim == gpt_load_ok) {
        gpt_lba_sec = schem_gpt->hdr_prim.alt_lba;
    } else {
        gpt_lba_sec = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;
    }

    /* Load secondary GPT */
    gpt_res_sec = gpt_load(&schem_gpt->hdr_sec, schem_gpt->table_sec,
                           img_ctx, gpt_lba_sec);
    if(gpt_res_sec == gpt_load_fatal) {
        plog_err("Error while loading secondary GPT");
        res = schem_load_fatal;
        goto exit;
    }

    /* Both GPTs are not present or corrupted */
    if(gpt_res_prim != gpt_load_ok && gpt_res_sec != gpt_load_ok) {
        res = schem_load_not_found;
        goto exit;
    }

    /* Primary GPT is ok, secondary GPT is corrupted */
    if(gpt_res_prim == gpt_load_ok && gpt_res_sec != gpt_load_ok) {
        plog_info("Secondary GPT is corrupted and will be restored on the "
                  "next write");

        /* Secondary GPT table LBA */
        gpt_table_lba = schem_gpt->hdr_prim.alt_lba -
                        byte_to_lba(img_ctx,
                                    schem_gpt->hdr_prim.part_table_entry_cnt *
                                    schem_gpt->hdr_prim.part_entry_sz, 1);
        /* Restore secondary GPT */
        gpt_restore(&schem_gpt->hdr_sec, schem_gpt->table_sec, gpt_table_lba,
                    &schem_gpt->hdr_prim, schem_gpt->table_prim);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is corrupted, secondary GPT is ok */
    if(gpt_res_prim != gpt_load_ok && gpt_res_sec == gpt_load_ok) {
        plog_info("Primary GPT is corrupted and will be restored on the "
                  "next write");

        /* Primary GPT table LBA */
        gpt_table_lba = schem_gpt->hdr_sec.alt_lba + 1;

        /* Restore primary GPT */
        gpt_restore(&schem_gpt->hdr_prim, schem_gpt->table_prim, gpt_table_lba,
                    &schem_gpt->hdr_sec, schem_gpt->table_sec);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is ok, secondary GPT is ok */
    res = schem_load_ok;

exit:
    if(res != schem_load_ok) {
        free(schem_gpt->table_prim);
        free(schem_gpt->table_sec);
    }

    return res;
}

static void
schem_part_set_gpt(union schem *schem, const struct img_ctx *img_ctx,
                   pu32 index, const struct schem_part *part)
{
    struct gpt_part_ent *part_gpt;

    part_gpt = &schem->s_gpt.table_prim[index];

    part_gpt->start_lba = part->start_lba;
    part_gpt->end_lba = part->end_lba;
}

static void
schem_part_get_gpt(const union schem *schem, pu32 index,
                   struct schem_part *part)
{
    const struct gpt_part_ent *part_gpt;

    part_gpt = &schem->s_gpt.table_prim[index];

    part->start_lba = part_gpt->start_lba;
    part->end_lba = part_gpt->end_lba;
}

static void schem_part_delete_gpt(union schem *schem, pu32 index)
{
    struct gpt_part_ent *part_gpt;

    part_gpt = &schem->s_gpt.table_prim[index];

    memset(part_gpt, 0, sizeof(*part_gpt));
}

static void schem_part_new_gpt(union schem *schem, pu32 index)
{
    struct gpt_part_ent *part_gpt;

    part_gpt = &schem->s_gpt.table_prim[index];

    guid_create(&part_gpt->unique_guid);
    part_gpt->type_guid = guid_linux_fs;
}

static pflag schem_part_is_used_gpt(const union schem *schem, pu32 index)
{
    return gpt_is_part_used(&schem->s_gpt.table_prim[index]);
}

static void
schem_get_info_gpt(const union schem *schem, const struct img_ctx *img_ctx,
                   struct schem_info *info)
{
    const struct gpt_hdr *hdr;

    hdr = &schem->s_gpt.hdr_prim;

    info->first_usable_lba = hdr->first_usable_lba;
    info->last_usable_lba = hdr->last_usable_lba;
    info->part_cnt = hdr->part_table_entry_cnt;
}

static void schem_sync_gpt(union schem *schem)
{
    struct schem_gpt *gpt;

    gpt = &schem->s_gpt;

    /* Compute GPT table CRCs (primary table should be equal to secondary) */
    gpt_crc_fill_table(&gpt->hdr_prim, gpt->table_prim);
    gpt->hdr_sec.part_table_crc32 = gpt->hdr_prim.part_table_crc32;

    /* Compute GPT header CRCs */
    gpt_crc_fill_hdr(&gpt->hdr_prim);
    gpt_crc_fill_hdr(&gpt->hdr_sec);
}

static pres schem_init_gpt(union schem *schem, const struct img_ctx *img_ctx)
{
    struct schem_gpt *gpt;

    gpt = &schem->s_gpt;

    gpt->table_prim = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));
    gpt->table_sec = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));

    if(gpt->table_prim == NULL || gpt->table_sec == NULL) {
        return pres_fail;
    }

    gpt_init_new(&gpt->hdr_prim, &gpt->hdr_sec,
                 gpt->table_prim, gpt->table_sec, img_ctx);

    mbr_init_protective(&gpt->mbr_prot.mbr, img_ctx);

    return pres_ok;
}

/* === Common === */

static void schem_map_funcs(struct schem_funcs *funcs, enum schem_type type)
{
    memset(funcs, 0, sizeof(*funcs));

    switch(type) {
        case schem_type_mbr:
            funcs->get_info     = &schem_get_info_mbr;
            funcs->part_is_used = &schem_part_is_used_mbr;
            funcs->part_new     = &schem_part_new_mbr;
            funcs->part_delete  = &schem_part_delete_mbr;
            funcs->part_get     = &schem_part_get_mbr;
            funcs->part_set     = &schem_part_set_mbr;
            funcs->save         = &schem_save_mbr;
            break;

        case schem_type_gpt:
            funcs->sync         = &schem_sync_gpt;
            funcs->get_info     = &schem_get_info_gpt;
            funcs->part_is_used = &schem_part_is_used_gpt;
            funcs->part_new     = &schem_part_new_gpt;
            funcs->part_delete  = &schem_part_delete_gpt;
            funcs->part_get     = &schem_part_get_gpt;
            funcs->part_set     = &schem_part_set_gpt;
            funcs->save         = &schem_save_gpt;
            break;

        case schem_type_none:
            break;
    }
}

static void
schem_map_funcs_int(struct schem_funcs_int *funcs_int, enum schem_type type)
{
    memset(funcs_int, 0, sizeof(*funcs_int));

    switch(type) {
        case schem_type_mbr:
            funcs_int->init = &schem_init_mbr;
            break;

        case schem_type_gpt:
            funcs_int->init = &schem_init_gpt;
            funcs_int->free = &schem_free_gpt;
            break;

        case schem_type_none:
            break;
    }
}

void schem_init(struct schem_ctx *schem_ctx)
{
    memset(schem_ctx, 0, sizeof(*schem_ctx));

    schem_ctx->type = schem_type_none;
}

pres schem_change_type(struct schem_ctx *schem_ctx,
                       const struct img_ctx *img_ctx, enum schem_type type)
{
    CALL_FUNC_NORET1(schem_ctx->funcs_int.free, &schem_ctx->s);

    schem_ctx->type = type;
    schem_map_funcs(&schem_ctx->funcs, type);
    schem_map_funcs_int(&schem_ctx->funcs_int, type);

    CALL_FUNC_NORET2(schem_ctx->funcs_int.init, &schem_ctx->s, img_ctx);

    return pres_ok;
}

pres schem_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    enum schem_load_res load_res_mbr;
    enum schem_load_res load_res_gpt;
    struct schem_mbr schem_mbr;
    struct schem_gpt schem_gpt;

    /* Init temp schemes */
    memset(&schem_mbr, 0, sizeof(schem_mbr));
    memset(&schem_gpt, 0, sizeof(schem_gpt));

    /* Free any previous scheme */
    schem_change_type(schem_ctx, img_ctx, schem_type_none);

    /* Load MBR */
    load_res_mbr = schem_load_mbr(&schem_mbr, img_ctx);
    if(load_res_mbr == schem_load_fatal) {
        return pres_fail;
    }

    /* Load GPT */
    load_res_gpt = schem_load_gpt(&schem_gpt, img_ctx);
    if(load_res_gpt == schem_load_fatal) {
        return pres_fail;
    }

    /* GPT is detected and loaded */
    if(load_res_gpt == schem_load_ok) {
        plog_dbg("GPT detected");

        memcpy(&schem_ctx->s.s_gpt, &schem_gpt, sizeof(schem_gpt));

        if(load_res_mbr == schem_load_ok) {
            plog_dbg("Protective MBR detected");

            /* Protective MBR is detected and loaded */
            memcpy(&schem_ctx->s.s_gpt.mbr_prot, &schem_mbr,
                   sizeof(schem_mbr));
        } else {
            plog_info("Protective MBR not found and will be created on the "
                      "next write");

            /* Initialize new protective MBR */
            mbr_init_protective(&schem_ctx->s.s_mbr.mbr, img_ctx);
        }

        schem_ctx->type = schem_type_gpt;
        goto ok;
    }

    /* MBR is detected and loaded */
    if(load_res_mbr == schem_load_ok) {
        plog_dbg("MBR detected");

        memcpy(&schem_ctx->s.s_mbr, &schem_mbr, sizeof(schem_mbr));

        schem_ctx->type = schem_type_mbr;
        goto ok;
    }

ok:
    schem_map_funcs(&schem_ctx->funcs, schem_ctx->type);
    schem_map_funcs_int(&schem_ctx->funcs_int, schem_ctx->type);
    return pres_ok;
}

pflag schem_check_overlap(const struct schem_ctx *schem_ctx, pu32 part_cnt,
                          const struct schem_part *part1, pu32 *part_index)
{
    pu32 i;
    struct schem_part part2;

    for(i = 0; i < part_cnt; i++) {
        if(part_index && *part_index == i) {
            continue;
        }

        if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
            continue;
        }

        schem_ctx->funcs.part_get(&schem_ctx->s, i, &part2);

        /* Start LBA is inside of the partition */
        if(
            part1->start_lba >= part2.start_lba &&
            part1->start_lba <= part2.end_lba
        ) {
            if(part_index) {
                *part_index = i;
            }
            return 1;
        }

        /* End LBA is inside of the partition */
        if(
            part1->end_lba >= part2.start_lba &&
            part1->end_lba <= part2.end_lba
        ) {
            if(part_index) {
                *part_index = i;
            }
            return 1;
        }

        /* Partition is inside of the boundaries */
        if(
            part1->start_lba < part2.start_lba &&
            part1->end_lba > part2.end_lba
        ) {
            if(part_index) {
                *part_index = i;
            }
            return 1;
        }
    }

    /* No overlap detected */
    return 0;
}

pres schem_calc_first_part(const struct schem_ctx *schem_ctx, pu32 *index,
                           pu32 part_cnt, pflag part_used)
{
    pu32 i;

    for(i = 0; i < part_cnt; i++) {
        if(schem_ctx->funcs.part_is_used(&schem_ctx->s, i) == part_used) {
            *index = i;
            return pres_ok;
        }
    }

    return pres_fail;
}

pres schem_calc_start_sector(const struct schem_ctx *schem_ctx,
                             const struct img_ctx *img_ctx, pu64 *lba,
                             const struct schem_info *info)
{
    pflag pos_changed;
    pu64 lba_no_align;
    pu32 i;
    struct schem_part part;

    lba_no_align = info->first_usable_lba;
    *lba = lba_align(img_ctx, lba_no_align, 1);

    do {
        pos_changed = 0;

        for(i = 0; i < info->part_cnt; i++) {
            if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
                continue;
            }

            schem_ctx->funcs.part_get(&schem_ctx->s, i, &part);

            if(*lba < part.start_lba || *lba > part.end_lba) {
                continue;
            }

            /* If current LBA intersects with partition */

            pos_changed = 1;

            /* If there is no align LBA from previous iteration, test it */
            if(lba_no_align != 0) {
                *lba = lba_no_align;
                lba_no_align = 0;
                break;
            }

            /* Set no align LBA in case aligned LBA will intersect */
            lba_no_align = part.end_lba + 1;

            /* If we reached the end of the usable space */
            if(lba_no_align > info->last_usable_lba) {
                return pres_fail;
            }

            /* Set aligned LBA for next iteration */
            *lba = lba_align(img_ctx, lba_no_align, 1);

            /* If aligned LBA is after the end of usable space */
            if(*lba > info->last_usable_lba) {
                *lba = lba_no_align;
                lba_no_align = 0;
            }

            /* Break internal cycle */
            break;
        }
    } while(pos_changed);

    return pres_ok;
}

pres schem_calc_last_sector(const struct schem_ctx *schem_ctx,
                            const struct img_ctx *img_ctx, pu64 *lba,
                            const struct schem_info *info)
{
    pu64 next_lba_bound;
    pu32 i;
    struct schem_part part;

    next_lba_bound = info->last_usable_lba;

    for(i = 0; i < info->part_cnt; i++) {
        if(!schem_ctx->funcs.part_is_used(&schem_ctx->s, i)) {
            continue;
        }

        schem_ctx->funcs.part_get(&schem_ctx->s, i, &part);

        /* If partition is located further than start LBA and LBA bound
         * is less, than partition start */
        if(part.start_lba > *lba && part.start_lba < next_lba_bound) {
            next_lba_bound = part.start_lba - 1;
        }
    }

    /* End LBA can not be less, than start LBA */
    if(next_lba_bound < *lba) {
        return pres_fail;
    }

    part.start_lba = *lba;
    /* Minus 1 sector to get the end LBA of the previous alignment segment */
    part.end_lba = lba_align(img_ctx, next_lba_bound, 0) - 1;
    /* Check if LBA can be aligned */
    if(
        part.end_lba >= info->first_usable_lba &&
        !schem_check_overlap(schem_ctx, info->part_cnt, &part, NULL)
    ) {
        next_lba_bound = part.end_lba;
    }

    *lba = next_lba_bound;

    return pres_ok;
}

