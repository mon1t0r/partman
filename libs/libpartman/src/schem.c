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

static pres schem_save_mbr(const struct schem_ctx_mbr *s_ctx_mbr,
                           const struct img_ctx *img_ctx)
{
    return mbr_save(&s_ctx_mbr->mbr, img_ctx);
}

static pres schem_save_gpt(const struct schem_ctx_gpt *s_ctx_gpt,
                           const struct img_ctx *img_ctx)
{
    pres res;

    /* Protective MBR */
    res = mbr_save(&s_ctx_gpt->mbr_prot.mbr, img_ctx);
    if(!res) {
        return pres_fail;
    }

    /* UEFI specification requires to update secondary GPT first */
    res = gpt_save(&s_ctx_gpt->hdr_sec, s_ctx_gpt->table_sec, img_ctx);
    if(!res) {
        return pres_fail;
    }

    return gpt_save(&s_ctx_gpt->hdr_prim, s_ctx_gpt->table_prim, img_ctx);
}

static enum schem_load_res
schem_load_mbr(struct schem_ctx_mbr *s_ctx_mbr, const struct img_ctx *img_ctx)
{
    enum mbr_load_res mbr_res;

    mbr_res = mbr_load(&s_ctx_mbr->mbr, img_ctx);
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
schem_load_gpt(struct schem_ctx_gpt *s_ctx_gpt, const struct img_ctx *img_ctx)
{
    enum schem_load_res res;
    enum gpt_load_res gpt_res_prim;
    enum gpt_load_res gpt_res_sec;
    plba gpt_lba_sec;
    plba gpt_table_lba;

    s_ctx_gpt->table_prim = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));
    s_ctx_gpt->table_sec = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));

    if(s_ctx_gpt->table_prim == NULL || s_ctx_gpt->table_sec == NULL) {
        return schem_load_fatal;
    }

    /* Load primary GPT, located at LBA 1 */
    gpt_res_prim = gpt_load(&s_ctx_gpt->hdr_prim, s_ctx_gpt->table_prim,
                            img_ctx, 1);
    if(gpt_res_prim == gpt_load_fatal) {
        fprintf(stderr, "An error occured while loading primary GPT\n");
        res = schem_load_fatal;
        goto exit;
    }

    /* If primary GPT loaded successfully, use alt_lba, otherwise use
     * last image LBA to locate secondary GPT */
    if(gpt_res_prim == gpt_load_ok) {
        gpt_lba_sec = s_ctx_gpt->hdr_prim.alt_lba;
    } else {
        gpt_lba_sec = byte_to_lba(img_ctx, img_ctx->img_sz, 0) - 1;
    }

    /* Load secondary GPT */
    gpt_res_sec = gpt_load(&s_ctx_gpt->hdr_sec, s_ctx_gpt->table_sec,
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
        gpt_table_lba = s_ctx_gpt->hdr_prim.alt_lba -
                        byte_to_lba(img_ctx,
                                    s_ctx_gpt->hdr_prim.part_table_entry_cnt *
                                    s_ctx_gpt->hdr_prim.part_entry_sz, 1);
        /* Restore secondary GPT */
        gpt_restore(&s_ctx_gpt->hdr_sec, s_ctx_gpt->table_sec, gpt_table_lba,
                    &s_ctx_gpt->hdr_prim, s_ctx_gpt->table_prim);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is corrupted, secondary GPT is ok */
    if(gpt_res_prim != gpt_load_ok && gpt_res_sec == gpt_load_ok) {
        printf("Primary GPT is corrupted and will be restored on the next "
               "write\n");

        /* Primary GPT table LBA */
        gpt_table_lba = s_ctx_gpt->hdr_sec.alt_lba + 1;

        /* Restore primary GPT */
        gpt_restore(&s_ctx_gpt->hdr_prim, s_ctx_gpt->table_prim, gpt_table_lba,
                    &s_ctx_gpt->hdr_sec, s_ctx_gpt->table_sec);

        res = schem_load_ok;
        goto exit;
    }

    /* Primary GPT is ok, secondary GPT is ok */
    res = schem_load_ok;

exit:
    if(res != schem_load_ok) {
        free(s_ctx_gpt->table_prim);
        free(s_ctx_gpt->table_sec);
    }

    return res;
}

static pres schem_init_mbr(struct schem_ctx_mbr *s_ctx_mbr,
                           const struct img_ctx *img_ctx)
{
    mbr_init_new(&s_ctx_mbr->mbr);

    return pres_ok;
}

static pres schem_init_gpt(struct schem_ctx_gpt *s_ctx_gpt,
                           const struct img_ctx *img_ctx)
{
    s_ctx_gpt->table_prim = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));
    s_ctx_gpt->table_sec = calloc(gpt_part_cnt, sizeof(struct gpt_part_ent));

    if(s_ctx_gpt->table_prim == NULL || s_ctx_gpt->table_sec == NULL) {
        return pres_fail;
    }

    gpt_init_new(&s_ctx_gpt->hdr_prim, &s_ctx_gpt->hdr_sec,
                 s_ctx_gpt->table_prim, s_ctx_gpt->table_sec, img_ctx);

    mbr_init_protective(&s_ctx_gpt->mbr_prot.mbr, img_ctx);

    return pres_ok;
}

static void schem_free(struct schem_ctx *schem_ctx)
{
    if(schem_ctx->type == schem_gpt) {
        free(schem_ctx->s.s_gpt.table_prim);
        free(schem_ctx->s.s_gpt.table_sec);
    }
}

pres schem_change_type(struct schem_ctx *schem_ctx,
                       const struct img_ctx *img_ctx, enum schem_type type)
{
    schem_free(schem_ctx);

    schem_ctx->type = type;

    switch(type) {
        case schem_mbr:
            return schem_init_mbr(&schem_ctx->s.s_mbr, img_ctx);
            break;

        case schem_gpt:
            return schem_init_gpt(&schem_ctx->s.s_gpt, img_ctx);
            break;

        case schem_none:
            break;
    }

    return pres_ok;
}

pres schem_load(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    enum schem_load_res load_res_mbr;
    enum schem_load_res load_res_gpt;
    struct schem_ctx_mbr s_ctx_mbr;
    struct schem_ctx_gpt s_ctx_gpt;

    /* Init temp schemes */
    memset(&s_ctx_mbr, 0, sizeof(s_ctx_mbr));
    memset(&s_ctx_gpt, 0, sizeof(s_ctx_gpt));

    /* Free any previous scheme */
    schem_change_type(schem_ctx, img_ctx, schem_none);

    /* Load MBR */
    load_res_mbr = schem_load_mbr(&s_ctx_mbr, img_ctx);
    if(load_res_mbr == schem_load_fatal) {
        return pres_fail;
    }

    /* Load GPT */
    load_res_gpt = schem_load_gpt(&s_ctx_gpt, img_ctx);
    if(load_res_gpt == schem_load_fatal) {
        return pres_fail;
    }

    /* GPT is detected and loaded */
    if(load_res_gpt == schem_load_ok) {
        memcpy(&schem_ctx->s.s_gpt, &s_ctx_gpt, sizeof(s_ctx_gpt));

        if(load_res_mbr == schem_load_ok) {
            /* Protective MBR is detected and loaded */
            memcpy(&schem_ctx->s.s_gpt.mbr_prot, &s_ctx_mbr,
                   sizeof(s_ctx_mbr));
        } else {
            /* Initialize new protective MBR */
            mbr_init_protective(&schem_ctx->s.s_mbr.mbr, img_ctx);
            printf("Protective MBR will be created on the next write\n");
        }

        schem_ctx->type = schem_gpt;
        return pres_ok;
    }

    /* MBR is detected and loaded */
    if(load_res_mbr == schem_load_ok) {
        memcpy(&schem_ctx->s.s_mbr, &s_ctx_mbr, sizeof(s_ctx_mbr));

        schem_ctx->type = schem_mbr;
        return pres_ok;
    }

    return pres_ok;
}

pres schem_save(const struct schem_ctx *schem_ctx,
                const struct img_ctx *img_ctx)
{
    switch(schem_ctx->type) {
        case schem_mbr:
            return schem_save_mbr(&schem_ctx->s.s_mbr, img_ctx);

        case schem_gpt:
            return schem_save_gpt(&schem_ctx->s.s_gpt, img_ctx);

        case schem_none:
            printf("Partitioning scheme is not present");
            return pres_fail;
    }

    return pres_fail;
}

