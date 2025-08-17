#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "schem.h"
#include "gpt.h"
#include "img_ctx.h"
#include "mbr.h"

enum schem_load_res {
    schem_load_ok, schem_load_not_found, schem_load_fatal
};

static void schem_free_gpt(union schem *schem)
{
    struct schem_gpt *gpt;

    gpt = &schem->s_gpt;

    free(gpt->table_prim);
    free(gpt->table_sec);
}

static pres schem_save_mbr(const union schem *schem,
                           const struct img_ctx *img_ctx)
{
    return mbr_save(&schem->s_mbr.mbr, img_ctx);
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
schem_load_mbr(struct schem_mbr *schem_mbr, const struct img_ctx *img_ctx)
{
    enum mbr_load_res mbr_res;

    mbr_res = mbr_load(&schem_mbr->mbr, img_ctx);
    if(mbr_res == mbr_load_fatal) {
        fprintf(stderr, "An error occured while loading MBR\n");
        return schem_load_fatal;
    }

    if(mbr_res == mbr_load_ok) {
        printf("MBR detected\n");
        return schem_load_ok;
    }

    return schem_load_not_found;
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
        fprintf(stderr, "An error occured while loading primary GPT\n");
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
        fprintf(stderr, "An error occured while loading secondary GPT\n");
        res = schem_load_fatal;
        goto exit;
    }

    /* Both GPTs are not present or corrupted */
    if(gpt_res_prim != gpt_load_ok && gpt_res_sec != gpt_load_ok) {
        res = schem_load_not_found;
        goto exit;
    }

    printf("GPT detected\n");

    /* Primary GPT is ok, secondary GPT is corrupted */
    if(gpt_res_prim == gpt_load_ok && gpt_res_sec != gpt_load_ok) {
        printf("Secondary GPT is corrupted and will be restored on the next "
               "write\n");


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
        printf("Primary GPT is corrupted and will be restored on the next "
               "write\n");

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

static pres schem_set_part_mbr(union schem *schem, pu8 index,
                               const struct schem_part *part)
{
    struct mbr *mbr;

    mbr = &schem->s_mbr.mbr;


}

static pres schem_set_part_gpt(union schem *schem, pu8 index,
                               const struct schem_part *part)
{
    
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

static pres schem_init_mbr(union schem *schem, const struct img_ctx *img_ctx)
{
    mbr_init_new(&schem->s_mbr.mbr);

    return pres_ok;
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

static void schem_map_funcs(struct schem_funcs *funcs, enum schem_type type)
{
    memset(funcs, 0, sizeof(*funcs));

    switch(type) {
        case schem_type_mbr:
            funcs->init = &schem_init_mbr;
            funcs->set_part = &schem_set_part_mbr;
            funcs->save = &schem_save_mbr;
            break;

        case schem_type_gpt:
            funcs->init = &schem_init_gpt;
            funcs->sync = &schem_sync_gpt;
            funcs->set_part = &schem_set_part_gpt;
            funcs->save = &schem_save_gpt;
            funcs->free = &schem_free_gpt;
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
    if(schem_ctx->funcs.free) {
        schem_ctx->funcs.free(&schem_ctx->s);
    }

    schem_ctx->type = type;
    schem_map_funcs(&schem_ctx->funcs, type);

    if(schem_ctx->funcs.init) {
        return schem_ctx->funcs.init(&schem_ctx->s, img_ctx);
    }

    return pres_ok;
}

void schem_sync(struct schem_ctx *schem_ctx)
{
    /* Perform scheme computations, which are dependent on scheme data */

    if(schem_ctx->funcs.sync) {
        schem_ctx->funcs.sync(&schem_ctx->s);
    }
}

pres
schem_set_part(struct schem_ctx *schem_ctx, pu8 index,
               const struct schem_part *part)
{
    if(schem_ctx->funcs.set_part) {
        return schem_ctx->funcs.set_part(&schem_ctx->s, index, part);
    }

    printf("Partitioning scheme is not present\n");
    return pres_fail;
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
        memcpy(&schem_ctx->s.s_gpt, &schem_gpt, sizeof(schem_gpt));

        if(load_res_mbr == schem_load_ok) {
            /* Protective MBR is detected and loaded */
            memcpy(&schem_ctx->s.s_gpt.mbr_prot, &schem_mbr,
                   sizeof(schem_mbr));
        } else {
            /* Initialize new protective MBR */
            mbr_init_protective(&schem_ctx->s.s_mbr.mbr, img_ctx);
            printf("Protective MBR not found and will be created on the next "
                   "write\n");
        }

        schem_ctx->type = schem_type_gpt;
        return pres_ok;
    }

    /* MBR is detected and loaded */
    if(load_res_mbr == schem_load_ok) {
        memcpy(&schem_ctx->s.s_mbr, &schem_mbr, sizeof(schem_mbr));

        schem_ctx->type = schem_type_mbr;
        return pres_ok;
    }

    return pres_ok;
}

pres schem_save(const struct schem_ctx *schem_ctx,
                const struct img_ctx *img_ctx)
{
    if(schem_ctx->funcs.save) {
        return schem_ctx->funcs.save(&schem_ctx->s, img_ctx);
    }

    printf("Partitioning scheme is not present\n");
    return pres_fail;
}

