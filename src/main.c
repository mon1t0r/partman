/* lseek64() */
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "partman_types.h"
#include "options.h"
#include "img_ctx.h"
#include "schem.h"

#define PARTMAN_VER "1.0"

enum {
    /* Input buffer size, in bytes */
    input_buf_sz = 256
};

enum action_res {
    action_continue, action_exit_ok, action_exit_fatal
};

static void schem_print_mbr(const struct schem_ctx_mbr *s_ctx_mbr)
{
    const struct mbr *mbr;
    int i;
    const struct mbr_part *part;
    pu32 c, h, s;

    mbr = &s_ctx_mbr->mbr;

    printf("Disk identifier: 0x%08lx\n", mbr->disk_sig);

    printf("\n===Partitions===\n\n");

    for(i = 0; i < ARRAY_SIZE(mbr->partitions); i++) {
        part = &mbr->partitions[i];

        /* Empty partition */
        if(!mbr_is_part_used(part)) {
            break;
        }

        printf("=Partition №%d\n", i + 1);
        printf("Boot         %u\n", part->boot_ind);
        printf("Type         %u\n", part->type);
        printf("Start LBA    %lu\n", part->start_lba);
        printf("End LBA      %lu\n", part->start_lba + part->sz_lba - 1);
        printf("Sectors      %lu\n", part->sz_lba);

        chs_int_to_tuple(part->start_chs, &c, &h, &s);
        printf("Start C/H/S  %03lu/%03lu/%03lu\n", c, h, s);

        chs_int_to_tuple(part->end_chs, &c, &h, &s);
        printf("End C/H/S    %03lu/%03lu/%03lu\n\n", c, h, s);
    }
}

static void schem_print_gpt(const struct schem_ctx_gpt *s_ctx_gpt)
{
    const struct gpt_hdr *hdr;
    const struct gpt_part_ent *part;
    char buf[50];
    int i;

    hdr = &s_ctx_gpt->hdr_prim;

    guid_to_str(buf, &hdr->disk_guid);
    printf("Disk identifier: %s\n", buf);

    printf("\n===Partitions===\n\n");

    for(i = 0; i < hdr->part_table_entry_cnt; i++) {
        part = &s_ctx_gpt->table_prim[i];

        /* Empty partition */
        if(!gpt_is_part_used(part)) {
            break;
        }

        guid_to_str(buf, &part->unique_guid);
        printf("=Partition %s\n", buf);

        guid_to_str(buf, &part->type_guid);
        printf("Type        %s\n", buf);

        printf("Start LBA   %llu\n", part->start_lba);
        printf("End LBA     %llu\n", part->end_lba);
        printf("Sectors     %llu\n", part->end_lba - part->start_lba + 1);

        /* TODO: Print partition name */
    }
}

static void schem_print(const struct schem_ctx *schem_ctx)
{
    switch(schem_ctx->type) {
        case schem_mbr:
            printf("Partitioning scheme: MBR\n");
            schem_print_mbr(&schem_ctx->s.s_mbr);
            break;

        case schem_gpt:
            printf("Partitioning scheme: GPT\n");
            schem_print_gpt(&schem_ctx->s.s_gpt);
            break;

        case schem_none:
            printf("Partitioning scheme: None\n");
            break;
    }
}

static enum action_res
action_handle(const struct img_ctx *img_ctx, struct schem_ctx *schem_ctx,
              int sym)
{
    switch(sym) {
        /* Exit */
        case 'q':
            return action_exit_ok;

        /* Help */
        case 'm':
            printf("*help should be here*\n");
            break;

        /* Print the partition table */
        case 'p':
            schem_print(schem_ctx);
            break;

        /* Unknown */
        default:
            printf("Unknown command\n");
            break;
    }

    return action_continue;
}

static pres
routine_start(const struct img_ctx *img_ctx, struct schem_ctx *schem_ctx)
{
    char buf[input_buf_sz];
    char *s;
    enum action_res res;

    for(;;) {
        printf("\nCommand (m for help): ");

        s = fgets(buf, sizeof(buf), stdin);

        /* End of file */
        if(s == NULL) {
            printf("\n");
            return pres_ok;
        }

        if(strlen(s) != 2) {
            continue;
        }

        res = action_handle(img_ctx, schem_ctx, s[0]);
        if(res == action_continue) {
            continue;
        }

        return res == action_exit_ok ? pres_ok : pres_fail;
    }
}

static pres
img_init(struct img_ctx *ctx, const struct partman_opts *opts, int img_fd)
{
    long long sz;
    char c;

    /* Get current image size */
    sz = lseek64(img_fd, 0, SEEK_END);
    if(sz == -1) {
        perror("lseek64()");
        return pres_fail;
    }

    /* If image size is less, than required */
    if(sz < opts->img_sz) {
        /* Seek and write a single byte to ensure image size */
        sz = lseek64(img_fd, opts->img_sz - 1, SEEK_SET);
        if(sz == -1) {
            perror("lseek64()");
            return pres_fail;
        }

        c = 0;
        sz = write(img_fd, &c, 1);
        if(sz == -1) {
            perror("write()");
            return pres_fail;
        }

        /* Now image size is equal to opts->img_sz */
        sz = opts->img_sz;
    }

    img_ctx_init(ctx, img_fd, sz);

    return pres_ok;
}

int main(int argc, char * const *argv)
{
    struct partman_opts opts;
    pres res;

    int img_fd;
    struct img_ctx img_ctx;
    struct schem_ctx schem_ctx;

    /* Parse program options */
    res = opts_parse(&opts, argc, argv);
    if(!res) {
        return EXIT_FAILURE;
    }

    printf("partman %s\n\n", PARTMAN_VER);

    /* Open file */
    img_fd = open(opts.img_name, O_RDWR|O_CREAT, 0666);
    if(img_fd == -1) {
        perror("open()");
        fprintf(stderr, "Unable to open %s\n", opts.img_name);
        return EXIT_FAILURE;
    }

    /* Initialize image context */
    res = img_init(&img_ctx, &opts, img_fd);
    if(!res) {
        fprintf(stderr, "Failed to prepare image\n");
        goto exit;
    }

    /* Load schemes, which are present in image */
    res = schem_load(&schem_ctx, &img_ctx);
    if(!res) {
        fprintf(stderr, "Failed to load schemes from image\n");
        goto exit;
    }

    /* Start user routine */
    res = routine_start(&img_ctx, &schem_ctx);

exit:
    schem_free(&schem_ctx);
    close(img_fd);

    return res;
}
