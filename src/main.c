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

static void pm_print_mbr(const struct schem *schem, const struct img_ctx *img_ctx)
{
    const struct schem_part *part;
    int i;
    plba part_sz;
    pflag part_boot;
    pu32 c, h, s;

    pprint("Partitioning scheme      MBR\n");
    pprint("Disk identifier          0x%08lx\n", schem->id.i);
    pprint("Table size (partitions)  %lu\n\n", schem->part_cnt);

    pprint("=== Partitions ===\n");

    for(i = 0; i < schem->part_cnt; i++) {
        part = &schem->table[i];

        /* Empty partition */
        if(!schem->funcs.part_is_used(part)) {
            continue;
        }

        part_sz = part->end_lba - part->start_lba + 1;
        part_boot = part->boot_ind & 0x80;

        pprint("Partition #%d\n", i + 1);
        pprint("|-Boot         0x%02x (%s)\n", part->boot_ind,
               part_boot ? "Yes" : "No");
        pprint("|-Type         0x%02x\n", part->type.i);
        pprint("|-Start LBA    %llu\n", part->start_lba);
        pprint("|-End LBA      %llu\n", part->end_lba);
        pprint("|-Sectors      %llu\n", part_sz);
        pprint("|-Size         %llu bytes\n", lba_to_byte(img_ctx, part_sz));

        chs_int_to_tuple(lba_to_chs(img_ctx, part->start_lba), &c, &h, &s);
        pprint("|-Start C/H/S  %03lu/%03lu/%03lu\n", c, h, s);

        chs_int_to_tuple(lba_to_chs(img_ctx, part->end_lba), &c, &h, &s);
        pprint("|-End C/H/S    %03lu/%03lu/%03lu\n\n", c, h, s);
    }
}

static void pm_print_gpt(const struct schem *schem, const struct img_ctx *img_ctx)
{
    char buf[50];
    const struct schem_part *part;
    int i;
    plba part_sz;

    pprint("Partitioning scheme      GPT\n");

    guid_to_str(buf, &schem->id.guid);
    pprint("Disk identifier          %s\n", buf);
    pprint("Table size (partitions)  %lu\n\n", schem->part_cnt);

    pprint("=== Partitions === \n");

    for(i = 0; i < schem->part_cnt; i++) {
        part = &schem->table[i];

        /* Empty partition */
        if(!schem->funcs.part_is_used(part)) {
            continue;
        }

        part_sz = part->end_lba - part->start_lba + 1;

        pprint("Partition #%d\n", i + 1);

        guid_to_str(buf, &part->unique_guid);
        pprint("|-Id         %s\n", buf);

        guid_to_str(buf, &part->type.guid);
        pprint("|-Type       %s\n", buf);

        pprint("|-Start LBA  %llu\n", part->start_lba);
        pprint("|-End LBA    %llu\n", part->end_lba);
        pprint("|-Sectors    %llu\n", part_sz);
        pprint("|-Size       %llu bytes\n\n", lba_to_byte(img_ctx, part_sz));

        /* GPT partition name can be printed here */
    }
}

static void pm_print_common(const struct img_ctx *img_ctx)
{
    pprint("Image '%s': %llu bytes, %llu sectors\n", img_ctx->img_name,
           img_ctx->img_sz, byte_to_lba(img_ctx, img_ctx->img_sz, 0));
    pprint("Units                    sectors\n");
    pprint("Sector size              %lu bytes\n", img_ctx->sec_sz);
}

static void
pm_print_scheme(const struct schem *schem, const struct img_ctx *img_ctx)
{
    pm_print_common(img_ctx);

    switch(schem->type) {
        case schem_type_mbr:
            pm_print_mbr(schem, img_ctx);
            break;

        case schem_type_gpt:
            pm_print_gpt(schem, img_ctx);
            break;

        case schem_type_none:
            pprint("Partitioning scheme      None\n");
            break;
    }
}

static p32 pm_part_prompt(const struct schem *schem, pflag find_used)
{
    p32 part_index_def;
    pu32 part_index;
    enum scan_res scan_res;
    pflag is_part_used;

    /* Find first used/free partition index */
    part_index_def = schem_find_part_index(schem, find_used);
    if(part_index_def == -1) {
        if(find_used) {
            pprint("No used partitions found\n");
        } else {
            pprint("No free partitions left\n");
        }
        return -1;
    }

    /* Get user input */
    scan_res = prompt_range_pu32("Partition number", 1, schem->part_cnt,
                                 part_index_def + 1, &part_index);
    if(scan_res != scan_ok) {
        return -1;
    }

    part_index--;

    is_part_used = schem->funcs.part_is_used(&schem->table[part_index]);

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

static pres pm_check_available(const struct schem *schem)
{
    if(schem->type == schem_type_none) {
        pprint("No partitioning scheme is present\n");
        return pres_fail;
    }

    return pres_ok;
}

static void pm_part_delete(struct schem *schem, const struct img_ctx *img_ctx)
{
    pres check_res;
    p32 part_index;

    /* Check if scheme is available for editing */
    check_res = pm_check_available(schem);
    if(!check_res) {
        return;
    }

    /* Prompt partition selection */
    part_index = pm_part_prompt(schem, 1);
    if(part_index == -1) {
        return;
    }

    /* Delete partition */
    schem_part_delete(&schem->table[part_index]);
}

static void
pm_part_toggle_boot(struct schem *schem, const struct img_ctx *img_ctx)
{
    p32 part_index;
    struct schem_part *part;

    if(schem->type != schem_type_mbr) {
        pprint("Partitioning scheme is not MBR\n");
        return;
    }

    /* Prompt partition selection */
    part_index = pm_part_prompt(schem, 1);
    if(part_index == -1) {
        return;
    }

    part = &schem->table[part_index];

    part->boot_ind = (~part->boot_ind) & 0x80;
}

static void pm_part_change_type_mbr(struct schem *schem, pu32 part_index)
{
    pu32 part_type;
    enum scan_res scan_res;

    pprint("Partition type: 0x");

    /* Get user input */
    scan_res = scan_int(" %lx", &part_type);
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

    schem->table[part_index].type.i = part_type;
}

static void pm_part_change_type_gpt(struct schem *schem, pu32 part_index)
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

    memcpy(&schem->table[part_index].type.guid, &part_type, sizeof(part_type));
}

static void
pm_part_change_type(struct schem *schem, const struct img_ctx *img_ctx)
{
    pres check_res;
    p32 part_index;

    /* Check if scheme is available for editing */
    check_res = pm_check_available(schem);
    if(!check_res) {
        return;
    }

    /* Prompt partition selection */
    part_index = pm_part_prompt(schem, 1);
    if(part_index == -1) {
        return;
    }

    switch(schem->type) {
        case schem_type_mbr:
            pm_part_change_type_mbr(schem, part_index);
            break;

        case schem_type_gpt:
            pm_part_change_type_gpt(schem, part_index);
            break;

        /* We should not get here as we check for it earlier */
        case schem_type_none:
            break;
    }
}

static void
pm_part_alter(struct schem *schem, const struct img_ctx *img_ctx, pflag is_new)
{
    pres check_res;
    enum scan_res scan_res;
    p32 part_index_res;
    pu32 part_index;
    plba_res def_lba;
    plba start_lba;
    plba end_lba;

    /* Check if scheme is available for editing */
    check_res = pm_check_available(schem);
    if(!check_res) {
        return;
    }

    /* Prompt partition selection */
    part_index = pm_part_prompt(schem, !is_new);
    if(part_index == -1) {
        return;
    }

    /* Find start sector - ignore current partition */
    def_lba = schem_find_start_sector(schem, img_ctx, part_index);
    if(def_lba == -1) {
        pprint("Unable to find free start sector\n");
        return;
    }

    /* Get user input */
    scan_res = prompt_range_pu64("First sector", schem->first_usable_lba,
                                 schem->last_usable_lba, def_lba, &start_lba);
    if(scan_res != scan_ok) {
        return;
    }

    /* Find last sector - ignore current partition */
    def_lba = schem_find_last_sector(schem, img_ctx, part_index, start_lba);
    if(def_lba == -1) {
        pprint("Unable to find available last sector\n");
        return;
    }

    /* Get user input */
    scan_res = prompt_sector_ext("Last sector", start_lba,
                                 schem->last_usable_lba, def_lba,
                                 img_ctx->sec_sz, &end_lba);
    if(scan_res != scan_ok) {
        return;
    }

    /* Check if current partition overlaps with any other */
    part_index_res = schem_find_overlap(schem, start_lba, end_lba, part_index);
    if(part_index_res >= 0) {
        pprint("Overlap detected with partition #%ld\n", part_index_res + 1);
        return;
    }

    /* Initialize partition, if creating new */
    if(is_new) {
        schem->funcs.part_init(&schem->table[part_index]);
    }

    schem->table[part_index].start_lba = start_lba;
    schem->table[part_index].end_lba = end_lba;
}

static enum action_res
action_handle(struct schem *schem, const struct img_ctx *img_ctx, int sym)
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
            pm_print_scheme(schem, img_ctx);
            break;

        /* Add a new partition */
        case 'n':
            pm_part_alter(schem, img_ctx, 1);
            break;

        /* Create/resize a partition */
        case 'e':
            pm_part_alter(schem, img_ctx, 0);
            break;

        /* Change partition type */
        case 't':
            pm_part_change_type(schem, img_ctx);
            break;

        /* Toggle partition bootable flag */
        case 'a':
            pm_part_toggle_boot(schem, img_ctx);
            break;

        /* Delete a partition */
        case 'd':
            pm_part_delete(schem, img_ctx);
            break;

        /* Write the partition table */
        case 'w':
            res = schem->funcs.save(schem, img_ctx);
            break;

        /* Create new MBR scheme */
        case 'o':
            res = schem_change_type(schem, img_ctx, schem_type_mbr);
            break;

        /* Create new GPT scheme */
        case 'g':
            res = schem_change_type(schem, img_ctx, schem_type_gpt);
            break;

        /* Unknown */
        default:
            pprint("Unknown command\n");
            break;
    }

    return res ? action_continue : action_exit_fatal;
}

static pres routine_start(struct schem *schem, const struct img_ctx *img_ctx)
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

        action_res = action_handle(schem, img_ctx, c);
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
    struct schem schem;

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
    schem_init(&schem);

    /* Load schemes, which are present in image */
    res = schem_load(&schem, &img_ctx);
    if(!res) {
        plog_err("Failed to load schemes from image");
        goto exit;
    }

    /* Start user routine */
    res = routine_start(&schem, &img_ctx);

exit:
    /* Free scheme resources, ignore return value at this point */
    schem_change_type(&schem, &img_ctx, schem_type_none);
    close(img_fd);

    return res;
}

