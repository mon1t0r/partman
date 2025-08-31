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
#include "mbr.h"

#define PARTMAN_VER "1.0"

/* Splitted help message (due to possible string length limitations) on
 * some compilers */
#define PARTMAN_HELP_1                        \
    "Help:\n\n"                               \
    "  MBR\n"                                 \
    "  |-a  toggle a bootable flag\n"         \
    "\n"                                      \
    "  GPT\n"                                 \
    "  |-M  enter Protective MBR\n"           \
    "  |-h  reset Protective MBR\n"           \
    "\n"                                      \
    "  Generic\n"                             \
    "  |-p  print the partitioning scheme\n"  \
    "  |-n  add a new partition\n"            \
    "  |-e  resize a partition\n"             \
    "  |-t  change a partition type\n"        \
    "  |-d  delete a partition\n"             \
    "\n"                                      \

#define PARTMAN_HELP_2                        \
    "  Misc\n"                                \
    "  |-m  print this menu\n"                \
    "  |-r  return from any nested scheme\n"  \
    "\n"                                      \
    "  Save & Exit\n"                         \
    "  |-w  write scheme to image and exit\n" \
    "  |-q  quit without saving changes\n"    \
    "\n"                                      \
    "  Create a new partitioning scheme\n"    \
    "  |-g  create a new GPT scheme\n"        \
    "  |-o  create a new MBR scheme\n"        \
    "\n"                                      \

enum action_res {
    action_continue, action_exit_ok, action_exit_fatal
};

static void pm_print_mbr(const struct schem *schem, const struct img_ctx *img_ctx)
{
    const struct schem_part *part;
    int i;
    plba part_sz;
    pflag part_is_boot;
    pflag part_is_prot;
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
        part_is_boot = part->boot_ind & 0x80;
        part_is_prot = schem_mbr_part_is_prot(part);

        pprint("Partition #%d\n", i + 1);
        pprint("|-Boot         0x%02x (%s)\n", part->boot_ind,
               part_is_boot ? "Yes" : "No");
        pprint("|-Type         0x%02x\n", part->type.i);
        pprint("|-Start LBA    %llu\n", part->start_lba);
        pprint("|-End LBA      %llu\n", part->end_lba);
        pprint("|-Sectors      %llu\n", part_sz);
        pprint("|-Size         %llu bytes\n", lba_to_byte(img_ctx, part_sz));

        chs_int_to_tuple(lba_to_chs(img_ctx, part->start_lba, part_is_prot),
                         &c, &h, &s);
        pprint("|-Start C/H/S  %lu/%lu/%lu\n", c, h, s);

        chs_int_to_tuple(lba_to_chs(img_ctx, part->end_lba, part_is_prot),
                         &c, &h, &s);
        pprint("|-End C/H/S    %lu/%lu/%lu\n", c, h, s);
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
        pprint("|-Size       %llu bytes\n", lba_to_byte(img_ctx, part_sz));

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

    /* No scheme is present */
    if(!schem) {
        pprint("Partitioning scheme      None\n");
        return;
    }

    switch(schem->type) {
        case schem_type_mbr:
            pm_print_mbr(schem, img_ctx);
            break;

        case schem_type_gpt:
            pm_print_gpt(schem, img_ctx);
            break;

        case schem_cnt:
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

static void pm_part_delete(struct schem *schem, const struct img_ctx *img_ctx)
{
    p32 part_index;

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
    p32 part_index;

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

        case schem_cnt:
            break;
    }
}

static void
pm_part_alter(struct schem *schem, const struct img_ctx *img_ctx, pflag is_new)
{
    enum scan_res scan_res;
    p32 part_index_res;
    pu32 part_index;
    plba_res def_lba;
    plba start_lba;
    plba end_lba;

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
action_handle(struct schem_ctx *schem_ctx, enum schem_type *schem_cur_t,
              const struct img_ctx *img_ctx, int sym)
{
    struct schem *schem_cur;
    pres res;

    if(*schem_cur_t < ARRAY_SIZE(schem_ctx->schemes)) {
        schem_cur = schem_ctx->schemes[*schem_cur_t];
    } else {
        schem_cur = NULL;
    }

    res = pres_ok;

    switch(sym) {
        /* Exit */
        case 'q':
            return action_exit_ok;

        /* Help */
        case 'm':
            pprint(PARTMAN_HELP_1);
            pprint(PARTMAN_HELP_2);
            break;

        /* Print the partition table */
        case 'p':
            pm_print_scheme(schem_cur, img_ctx);
            break;

        /* Add a new partition */
        case 'n':
            if(!schem_cur) {
                goto no_schem;
            }
            pm_part_alter(schem_cur, img_ctx, 1);
            break;

        /* Resize a partition */
        case 'e':
            if(!schem_cur) {
                goto no_schem;
            }
            pm_part_alter(schem_cur, img_ctx, 0);
            break;

        /* Change partition type */
        case 't':
            if(!schem_cur) {
                goto no_schem;
            }
            pm_part_change_type(schem_cur, img_ctx);
            break;

        /* Toggle partition bootable flag */
        case 'a':
            if(!schem_cur) {
                goto no_schem;
            }
            pm_part_toggle_boot(schem_cur, img_ctx);
            break;

        /* Delete a partition */
        case 'd':
            if(!schem_cur) {
                goto no_schem;
            }
            pm_part_delete(schem_cur, img_ctx);
            break;

        /* Write the partition table */
        case 'w':
            res = schem_ctx_save(schem_ctx, img_ctx);
            break;

        /* Create new MBR scheme */
        case 'o':
            res = schem_ctx_new(schem_ctx, img_ctx, schem_type_mbr);
            *schem_cur_t = schem_ctx_get_type(schem_ctx);
            break;

        /* Create new GPT scheme */
        case 'g':
            res = schem_ctx_new(schem_ctx, img_ctx, schem_type_gpt);
            *schem_cur_t = schem_ctx_get_type(schem_ctx);
            break;

        /* Enter Protective MBR */
        case 'M':
            if(
                *schem_cur_t != schem_type_gpt ||
                !schem_ctx->schemes[schem_type_mbr]
            ) {
                pprint("Unable to switch to Protective MBR\n");
                break;
            }
            *schem_cur_t = schem_type_mbr;
            break;

        /* Reset Protective MBR */
        case 'h':
            if(!schem_ctx->schemes[schem_type_gpt]) {
                pprint("Partitioning scheme is not GPT\n");
                break;
            }
            if(!schem_ctx->schemes[schem_type_mbr]) {
                pprint("Unable to reset Protective MBR\n");
                break;
            }
            schem_init_mbr(schem_ctx->schemes[schem_type_mbr], img_ctx);
            schem_mbr_set_prot(schem_ctx->schemes[schem_type_mbr]);
            break;

        /* Exit any nested scheme */
        case 'r':
            *schem_cur_t = schem_ctx_get_type(schem_ctx);
            break;

        /* Unknown */
        default:
            pprint("Unknown command\n");
            break;
    }

    return res ? action_continue : action_exit_fatal;

no_schem:
    pprint("No partitioning scheme is present\n");
    return action_continue;
}

static pres
routine_start(struct schem_ctx *schem_ctx, const struct img_ctx *img_ctx)
{
    enum schem_type schem_cur_t;
    char c;
    enum scan_res scan_res;
    enum action_res action_res;

    /* Determine initial working scheme */
    schem_cur_t = schem_ctx_get_type(schem_ctx);

    /* UI loop */
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

        action_res = action_handle(schem_ctx, &schem_cur_t, img_ctx, c);
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

    if(opts->sec_sz) {
        img_ctx->sec_sz = opts->sec_sz;
    }

    if(opts->align) {
        img_ctx->align = opts->align;
    }

    if(opts->hpc) {
        img_ctx->hpc = opts->hpc;
    }

    if(opts->spt) {
        img_ctx->spt = opts->spt;
    }

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
    schem_ctx_init(&schem_ctx);

    /* Load schemes, which are present in image */
    res = schem_ctx_load(&schem_ctx, &img_ctx);
    if(!res) {
        plog_err("Failed to load schemes from image");
        goto exit;
    }

    /* Start user routine */
    res = routine_start(&schem_ctx, &img_ctx);

exit:
    /* Free scheme context resources */
    schem_ctx_reset(&schem_ctx, 0);
    close(img_fd);

    return res;
}

