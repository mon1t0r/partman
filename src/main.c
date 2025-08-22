/* lseek64() */
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "partman_types.h"
#include "options.h"
#include "img_ctx.h"
#include "scan.h"
#include "schem.h"

#define PARTMAN_VER "1.0"

enum action_res {
    action_continue, action_exit_ok, action_exit_fatal
};

static void schem_print_mbr(const struct schem_mbr *schem_mbr)
{
    const struct mbr *mbr;
    int i;
    const struct mbr_part *part;
    pu32 c, h, s;

    mbr = &schem_mbr->mbr;

    printf("Disk identifier: 0x%08lx\n", mbr->disk_sig);

    printf("\n===Partitions===\n\n");

    for(i = 0; i < ARRAY_SIZE(mbr->partitions); i++) {
        part = &mbr->partitions[i];

        /* Empty partition */
        if(!mbr_is_part_used(part)) {
            break;
        }

        printf("==Partition #%d\n", i + 1);
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

static void schem_print_gpt(const struct schem_gpt *schem_gpt)
{
    const struct gpt_hdr *hdr;
    const struct gpt_part_ent *part;
    char buf[50];
    int i;

    hdr = &schem_gpt->hdr_prim;

    guid_to_str(buf, &hdr->disk_guid);
    printf("Disk identifier: %s\n", buf);

    printf("\n===Partitions===\n\n");

    for(i = 0; i < hdr->part_table_entry_cnt; i++) {
        part = &schem_gpt->table_prim[i];

        /* Empty partition */
        if(!gpt_is_part_used(part)) {
            break;
        }

        printf("==Partition #%d\n", i + 1);

        guid_to_str(buf, &part->unique_guid);
        printf("Id          %s\n", buf);

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
        case schem_type_mbr:
            printf("Partitioning scheme: MBR\n");
            schem_print_mbr(&schem_ctx->s.s_mbr);
            break;

        case schem_type_gpt:
            printf("Partitioning scheme: GPT\n");
            schem_print_gpt(&schem_ctx->s.s_gpt);
            break;

        case schem_type_none:
            printf("Partitioning scheme: None\n");
            break;
    }
}

static pflag
schem_check_overlap(const struct schem_ctx *schem_ctx,
                    const struct schem_part *part1, pu32 *index)
{
    pu32 part_cnt;
    pu32 i;
    struct schem_part part2;

    part_cnt = schem_ctx->funcs.get_part_cnt(&schem_ctx->s);

    for(i = 0; i < part_cnt; i++) {
        if(i == *index) {
            continue;
        }

        if(!schem_ctx->funcs.is_part_used(&schem_ctx->s, i)) {
            continue;
        }

        schem_ctx->funcs.get_part(&schem_ctx->s, i, &part2);

        /* Start LBA is inside of the partition */
        if(
            part1->start_lba >= part2.start_lba &&
            part1->start_lba <= part2.end_lba
        ) {
            *index = i;
            return 1;
        }

        /* End LBA is inside of the partition */
        if(
            part1->end_lba >= part2.start_lba &&
            part1->end_lba <= part2.end_lba
        ) {
            *index = i;
            return 1;
        }

        /* Partition is inside of the boundaries */
        if(
            part1->start_lba < part2.start_lba &&
            part1->end_lba > part2.end_lba
        ) {
            *index = i;
            return 1;
        }
    }

    return 0;
}

static void
schem_part_alter(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx,
                 pflag is_new)
{
    pu32 part_cnt;
    pu32 part_index;
    pflag part_used;
    struct schem_part part;

    part_cnt = schem_ctx->funcs.get_part_cnt(&schem_ctx->s);

    printf("Partition number: ");

    part_index = scan_pu32();
    if(part_index < 1 || part_index > part_cnt) {
        printf("Invalid value\n");
        return;
    }
    part_index--;

    part_used = schem_ctx->funcs.is_part_used(&schem_ctx->s, part_index);

    if(!is_new && !part_used) {
        printf("Partition is not in use\n");
        return;
    }
    if(is_new && part_used) {
        printf("Partition already in use\n");
        return;
    }

    printf("First sector: ");
    part.start_lba = scan_pu64();

    printf("Last sector: ");
    part.end_lba = scan_pu64();

    if(part.end_lba < part.start_lba) {
        printf("Last sector must be greater, than first sector\n");
        return;
    }

    /* TODO: Add boundaries check */

    if(schem_check_overlap(schem_ctx, &part, &part_index)) {
        printf("Overlap detected with partition #%lu\n", part_index + 1);
        return;
    }

    if(is_new) {
        schem_ctx->funcs.new_part(&schem_ctx->s, part_index);
    }

    schem_ctx->funcs.set_part(&schem_ctx->s, img_ctx, part_index, &part);
}

static enum action_res
action_handle(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx,
              int sym)
{
    pres res;

    res = pres_ok;

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

        /* Add a new partition */
        case 'n':
            schem_part_alter(schem_ctx, img_ctx, 1);
            break;

        /* Resize a partition */
        case 'e':
            schem_part_alter(schem_ctx, img_ctx, 0);
            break;

        /* Write the partition table */
        case 'w':
            if(schem_ctx->funcs.sync) {
                schem_ctx->funcs.sync(&schem_ctx->s);
            }
            res = schem_ctx->funcs.save(&schem_ctx->s, img_ctx);
            break;

        /* Create new MBR scheme */
        case 'o':
            res = schem_change_type(schem_ctx, img_ctx, schem_type_mbr);
            break;

        /* Create new GPT scheme */
        case 'g':
            res = schem_change_type(schem_ctx, img_ctx, schem_type_gpt);
            break;

        /* Unknown */
        default:
            printf("Unknown command\n");
            break;
    }

    return res ? action_continue : action_exit_fatal;
}

static pres
routine_start(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    int c;
    enum action_res res;

    for(;;) {
        printf("\nCommand (m for help): ");

        c = scan_char();
        /* End of file */
        if(c == -1) {
            printf("\n");
            return pres_ok;
        }

        res = action_handle(schem_ctx, img_ctx, c);
        if(res == action_continue) {
            continue;
        }

        return res == action_exit_ok ? pres_ok : pres_fail;
    }
}

static pres
img_init(struct img_ctx *img_ctx, const struct partman_opts *opts, int img_fd)
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

    img_ctx_init(img_ctx, img_fd, sz);

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

    /* Initialize scheme context */
    schem_init(&schem_ctx);

    /* Load schemes, which are present in image */
    res = schem_load(&schem_ctx, &img_ctx);
    if(!res) {
        fprintf(stderr, "Failed to load schemes from image\n");
        goto exit;
    }

    /* Start user routine */
    res = routine_start(&schem_ctx, &img_ctx);

exit:
    /* Free scheme resources, ignore return value at this point */
    schem_change_type(&schem_ctx, &img_ctx, schem_type_none);
    close(img_fd);

    return res;
}
