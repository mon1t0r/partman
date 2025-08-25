/* lseek64() */
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "partman_types.h"
#include "options.h"
#include "log.h"
#include "scan.h"
#include "img_ctx.h"
#include "schem.h"

#define PARTMAN_VER "1.0"

enum action_res {
    action_continue, action_exit_ok, action_exit_fatal
};

static void schem_print_mbr(const struct schem_mbr *schem_mbr)
{
    const struct mbr *mbr;
    const struct mbr_part *part;
    int i;
    pu32 c, h, s;

    mbr = &schem_mbr->mbr;

    pprint("Partitioning scheme: MBR\n");
    pprint("Disk identifier: 0x%08lx\n\n", mbr->disk_sig);

    pprint("=== Partitions ===\n");

    for(i = 0; i < ARRAY_SIZE(mbr->partitions); i++) {
        part = &mbr->partitions[i];

        /* Empty partition */
        if(!mbr_is_part_used(part)) {
            continue;
        }

        pprint("Partition #%d\n", i + 1);
        pprint("|-Boot         %u\n", part->boot_ind);
        pprint("|-Type         0x%02x\n", part->type);
        pprint("|-Start LBA    %lu\n", part->start_lba);
        pprint("|-End LBA      %lu\n", part->start_lba + part->sz_lba - 1);
        pprint("|-Sectors      %lu\n", part->sz_lba);

        chs_int_to_tuple(part->start_chs, &c, &h, &s);
        pprint("|-Start C/H/S  %03lu/%03lu/%03lu\n", c, h, s);

        chs_int_to_tuple(part->end_chs, &c, &h, &s);
        pprint("|-End C/H/S    %03lu/%03lu/%03lu\n\n", c, h, s);
    }
}

static void schem_print_gpt(const struct schem_gpt *schem_gpt)
{
    const struct gpt_hdr *hdr;
    const struct gpt_part_ent *part;
    char buf[50];
    int i;

    hdr = &schem_gpt->gpt.hdr_prim;

    pprint("Partitioning scheme: GPT\n");

    guid_to_str(buf, &hdr->disk_guid);
    pprint("Disk identifier: %s\n\n", buf);

    pprint("=== Partitions === \n");

    for(i = 0; i < hdr->part_table_entry_cnt; i++) {
        part = &schem_gpt->gpt.table_prim[i];

        /* Empty partition */
        if(!gpt_is_part_used(part)) {
            continue;
        }

        pprint("Partition #%d\n", i + 1);

        guid_to_str(buf, &part->unique_guid);
        pprint("|-Id          %s\n", buf);

        guid_to_str(buf, &part->type_guid);
        pprint("|-Type        %s\n", buf);

        pprint("|-Start LBA   %llu\n", part->start_lba);
        pprint("|-End LBA     %llu\n", part->end_lba);
        pprint("|-Sectors     %llu\n", part->end_lba - part->start_lba + 1);

        /* TODO: Print partition name */
    }
}

static void schem_print_common(const struct img_ctx *img_ctx)
{
    pprint("Image '%s': %llu bytes, %llu sectors\n", img_ctx->img_name,
           img_ctx->img_sz, byte_to_lba(img_ctx, img_ctx->img_sz, 0));
    pprint("Units: sectors\n");
    pprint("Sector size: %lu bytes\n", img_ctx->sec_sz);
}

static void schem_print(const struct schem_ctx *schem_ctx,
                        const struct img_ctx *img_ctx)
{
    schem_print_common(img_ctx);

    switch(schem_ctx->type) {
        case schem_type_mbr:
            schem_print_mbr(&schem_ctx->s.s_mbr);
            break;

        case schem_type_gpt:
            schem_print_gpt(&schem_ctx->s.s_gpt);
            break;

        case schem_type_none:
            pprint("Partitioning scheme: None\n");
            break;
    }
}

static p32
schem_part_prompt(const struct schem_ctx *schem_ctx,
                  pu32 part_cnt, pflag find_used)
{
    p32 part_index_def;
    pu32 part_index;
    enum scan_res scan_res;
    pflag is_part_used;

    /* Find first used/free partition index */
    part_index_def = schem_find_part_index(schem_ctx, part_cnt, find_used);
    if(part_index_def == -1) {
        if(find_used) {
            pprint("No used partitions found\n");
        } else {
            pprint("No free partitions left\n");
        }
        return -1;
    }

    /* Get user input */
    scan_res = prompt_range_pu32("Partition number", &part_index, 1,
                                 part_cnt, part_index_def + 1);
    if(scan_res != scan_ok) {
        return -1;
    }

    part_index--;

    is_part_used = schem_ctx->funcs.part_is_used(&schem_ctx->s, part_index);

    /* If looking for used partition and partition is not used */
    if(find_used && !is_part_used) {
        pprint("Partition is not in use\n");
        return -1;
    }

    /* If looking for unused partition and partition is in use */
    if(!find_used && is_part_used) {
        pprint("Partition already in use\n");
        return -1;
    }

    return part_index;
}

static pres schem_check_available(const struct schem_ctx *schem_ctx)
{
    if(schem_ctx->type == schem_type_none) {
        pprint("No partitioning scheme is present\n");
        return pres_fail;
    }

    return pres_ok;
}

static void schem_part_delete(struct schem_ctx *schem_ctx,
                              const struct img_ctx *img_ctx)
{
    pres check_res;
    struct schem_info info;
    p32 part_index;

    /* Check if scheme is available for editing */
    check_res = schem_check_available(schem_ctx);
    if(!check_res) {
        return;
    }

    schem_ctx->funcs.get_info(&schem_ctx->s, img_ctx, &info);

    /* Prompt partition selection */
    part_index = schem_part_prompt(schem_ctx, info.part_cnt, 1);
    if(part_index == -1) {
        return;
    }

    /* Delete partition */
    schem_ctx->funcs.part_delete(&schem_ctx->s, part_index);
}

static void schem_part_toggle_boot(struct schem_ctx *schem_ctx,
                                   const struct img_ctx *img_ctx)
{
    struct schem_info info;
    p32 part_index;
    struct mbr_part *part;

    if(schem_ctx->type != schem_type_mbr) {
        pprint("Partitioning scheme is not MBR\n");
        return;
    }

    schem_ctx->funcs.get_info(&schem_ctx->s, img_ctx, &info);

    /* Prompt partition selection */
    part_index = schem_part_prompt(schem_ctx, info.part_cnt, 1);
    if(part_index == -1) {
        return;
    }

    part = &schem_ctx->s.s_mbr.mbr.partitions[part_index];

    part->boot_ind = !part->boot_ind;
}

static void
schem_part_change_type_mbr(struct schem_mbr *schem_mbr, pu32 part_index)
{
    pu32 part_type;
    enum scan_res scan_res;

    pprint("Partition type: 0x");

    /* Get user input */
    scan_res = scan_int("%lx", &part_type);
    if(scan_res != scan_ok) {
        if(scan_res != scan_eof) {
            pprint("Invalid value\n");
        }
        return;
    }

    /* Value is out of 1 byte range */
    if(part_type & ~0xFF) {
        pprint("Invalid value\n");
        return;
    }

    schem_mbr->mbr.partitions[part_index].type = part_type;
}

static void
schem_part_change_type_gpt(struct schem_gpt *schem_gpt, pu32 part_index)
{
    struct guid part_type;
    enum scan_res scan_res;

    pprint("Partition type GUID: ");

    /* Get user input */
    scan_res = scan_guid(&part_type);
    if(scan_res != scan_ok) {
        if(scan_res != scan_eof) {
            pprint("Invalid value\n");
        }
        return;
    }

    memcpy(&schem_gpt->gpt.table_prim[part_index].type_guid, &part_type,
           sizeof(part_type));
}

static void schem_part_change_type(struct schem_ctx *schem_ctx,
                                   const struct img_ctx *img_ctx)
{
    pres check_res;
    struct schem_info info;
    p32 part_index;

    /* Check if scheme is available for editing */
    check_res = schem_check_available(schem_ctx);
    if(!check_res) {
        return;
    }

    schem_ctx->funcs.get_info(&schem_ctx->s, img_ctx, &info);

    /* Prompt partition selection */
    part_index = schem_part_prompt(schem_ctx, info.part_cnt, 1);
    if(part_index == -1) {
        return;
    }

    switch(schem_ctx->type) {
        case schem_type_mbr:
            schem_part_change_type_mbr(&schem_ctx->s.s_mbr, part_index);
            break;

        case schem_type_gpt:
            schem_part_change_type_gpt(&schem_ctx->s.s_gpt, part_index);
            break;

        /* We should not get here as we check for it earlier */
        case schem_type_none:
            break;
    }
}

static void
schem_part_alter(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx,
                 pflag is_new)
{
    pres check_res;
    struct schem_info info;
    pu32 part_index;
    enum scan_res scan_res;
    struct schem_part part;
    plba_res lba_def;
    p32 part_index_res;

    /* Check if scheme is available for editing */
    check_res = schem_check_available(schem_ctx);
    if(!check_res) {
        return;
    }

    schem_ctx->funcs.get_info(&schem_ctx->s, img_ctx, &info);

    /* Prompt partition selection */
    part_index = schem_part_prompt(schem_ctx, info.part_cnt, !is_new);
    if(part_index == -1) {
        return;
    }

    /* Find start sector - ignore current partition */
    lba_def = schem_find_start_sector(schem_ctx, img_ctx, &info, part_index);
    if(lba_def == -1) {
        pprint("Unable to find free start sector\n");
        return;
    }

    /* Get user input */
    scan_res = prompt_range_pu64("First sector", &part.start_lba,
                                 info.first_usable_lba, info.last_usable_lba,
                                 lba_def);
    if(scan_res != scan_ok) {
        return;
    }

    /* Find last sector - ignore current partition */
    lba_def = schem_find_last_sector(schem_ctx, img_ctx, &info, part_index,
                                     part.start_lba);
    if(lba_def == -1) {
        pprint("Unable to find available last sector\n");
        return;
    }

    /* Get user input */
    scan_res = prompt_range_pu64("Last sector", &part.end_lba,
                                 part.start_lba, info.last_usable_lba,
                                 lba_def);
    if(scan_res != scan_ok) {
        return;
    }

    /* Check if current partition overlaps with any other */
    part_index_res = schem_find_overlap(schem_ctx, info.part_cnt, &part,
                                        part_index);
    if(part_index_res >= 0) {
        pprint("Overlap detected with partition #%ld\n", part_index_res + 1);
        return;
    }

    if(is_new) {
        schem_ctx->funcs.part_new(&schem_ctx->s, part_index);
    }

    schem_ctx->funcs.part_set(&schem_ctx->s, img_ctx, part_index, &part);
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
            pprint("*help should be here*\n");
            break;

        /* Print the partition table */
        case 'p':
            schem_print(schem_ctx, img_ctx);
            break;

        /* Add a new partition */
        case 'n':
            schem_part_alter(schem_ctx, img_ctx, 1);
            break;

        /* Create/resize a partition */
        case 'e':
            schem_part_alter(schem_ctx, img_ctx, 0);
            break;

        /* Change partition type */
        case 't':
            schem_part_change_type(schem_ctx, img_ctx);
            break;

        /* Toggle partition bootable flag */
        case 'a':
            schem_part_toggle_boot(schem_ctx, img_ctx);
            break;

        /* Delete a partition */
        case 'd':
            schem_part_delete(schem_ctx, img_ctx);
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
            pprint("Unknown command\n");
            break;
    }

    return res ? action_continue : action_exit_fatal;
}

static pres
routine_start(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    char c;
    enum scan_res scan_res;
    enum action_res action_res;

    for(;;) {
        pprint("Command (m for help): ");
        scan_res = scan_char(&c);

        /* End of file */
        if(scan_res == scan_eof) {
            pprint("\n");
            return pres_ok;
        }

        /* Error - unknown command */
        if(scan_res != scan_ok) {
            c = '\0';
        }

        action_res = action_handle(schem_ctx, img_ctx, c);
        if(action_res == action_continue) {
            pprint("\n");
            continue;
        }

        return action_res == action_exit_ok ? pres_ok : pres_fail;
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

    plog_dbg("Image size is %lld", sz);

    if(sz >= opts->img_sz) {
        goto init;
    }

    /* If image size is less, than required */

    plog_info("Image size (%lld) is less, than required by the "
              "parameter (%lld). Image size will be extended now to match the "
              "required size", sz, opts->img_sz);

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

    plog_dbg("Image size is extended to the required value");

init:
    img_ctx_init(img_ctx, opts->img_name, img_fd, sz);

    img_ctx->sec_sz = opts->sec_sz;

    return img_ctx_validate(img_ctx);
}

int main(int argc, char *const *argv)
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

    /* Set log level */
    plog_set_level(opts.log_level);

    pprint("partman %s\n\n", PARTMAN_VER);

    /* Initialize random */
    srand(time(NULL));

    /* Open file */
    img_fd = open(opts.img_name, O_RDWR|O_CREAT, 0666);
    if(img_fd == -1) {
        perror("open()");
        plog_err("Unable to open %s", opts.img_name);
        return EXIT_FAILURE;
    }

    /* Initialize image context */
    res = img_init(&img_ctx, &opts, img_fd);
    if(!res) {
        plog_err("Failed to prepare image");
        close(img_fd);
        return EXIT_FAILURE;
    }

    /* Initialize scheme context */
    schem_init(&schem_ctx);

    /* Load schemes, which are present in image */
    res = schem_load(&schem_ctx, &img_ctx);
    if(!res) {
        plog_err("Failed to load schemes from image");
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

