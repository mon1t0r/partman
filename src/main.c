/* lseek64() */
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "partman_types.h"
#include "img_ctx.h"
#include "mbr.h"
#include "gpt.h"
#include "guid.h"

/* Used image size, in bytes. Will be configurable in future */
#define IMAGE_SIZE 62058921984

enum {
    /* Input buffer size, in bytes */
    input_buf_sz = 256
};

static void mbr_print(const struct mbr *mbr)
{
    int i;
    const struct mbr_part *part;
    pu32 c, h, s;

    printf("\n===MBR info ===\n");

    printf("Disk signature: %lx\n\n", mbr->disk_sig);

    for(i = 0; i < sizeof(mbr->partitions) / sizeof(mbr->partitions[0]); i++) {
        part = &mbr->partitions[i];

        /* Empty partition */
        if(!mbr_is_part_used(part)) {
            continue;
        }

        printf("Partition #%d:\n", i);
        printf("Boot indicator %d\n", part->boot_ind);
        printf("Type           %d\n", part->type);

        chs_int_to_tuple(part->start_chs, &c, &h, &s);
        printf("Start C/H/S    %lu/%lu/%lu\n", c, h, s);

        chs_int_to_tuple(part->end_chs, &c, &h, &s);
        printf("End C/H/S      %lu/%lu/%lu\n", c, h, s);

        printf("Start LBA      %ld\n", part->start_lba);
        printf("Sectors        %ld\n\n", part->sz_lba);
    }
}

static void gpt_print(const struct gpt_hdr *hdr,
                      const struct gpt_part_ent table[])
{
    char buf[50];
    int i;
    const struct gpt_part_ent *part;

    printf("\n===GPT info ===\n");

    printf("Revision:                    %lx\n", hdr->rev);
    printf("Header size:                 %lu\n", hdr->hdr_sz);
    printf("Header CRC32:                %lu\n", hdr->hdr_crc32);
    printf("My LBA:                      %llu\n", hdr->my_lba);
    printf("Alt LBA:                     %llu\n", hdr->alt_lba);
    printf("First usable LBA:            %llu\n", hdr->first_usable_lba);
    printf("Last usable LBA:             %llu\n", hdr->last_usable_lba);

    guid_to_str(buf, &hdr->disk_guid);
    printf("Disk GUID:                   %s\n", buf);

    printf("Partition table LBA:         %llu\n", hdr->part_table_lba);
    printf("Partition table entry count: %lu\n", hdr->part_table_entry_cnt);
    printf("Partition entry size:        %lu\n", hdr->part_entry_sz);
    printf("Partition table CRC32:       %lu\n\n", hdr->part_table_crc32);


    for(i = 0; i < hdr->part_table_entry_cnt; i++) {
        part = &table[i];

        /* Empty partition */
        if(!gpt_is_part_used(part)) {
            continue;
        }

        guid_to_str(buf, &part->unique_guid);
        printf("Partition %s:\n", buf);

        guid_to_str(buf, &part->type_guid);
        printf("Type %s\n", buf);

        printf("Start LBA: %llu\n", part->start_lba);
        printf("End LBA: %llu\n", part->end_lba);
        printf("Attributes: %llu\n\n", part->attr);

        /* TODO: Print partition name */
    }
}

static pres action_handle(struct schem_ctx_mbr *ctx_mbr,
                          struct schem_ctx_gpt *ctx_gpt,
                          const struct img_ctx *img_ctx, int img_fd, int sym)
{
    switch(sym) {
        /* Help */
        case 'm':
            printf("*help should be here*\n");
            break;

        /* Print the partition table */
        case 'p':
            mbr_print(&ctx_mbr->mbr);
            gpt_print(&ctx_gpt->hdr_prim, ctx_gpt->table_prim);
            break;
    }

    return pres_ok;
}

static pres user_routine(struct schem_ctx_mbr *ctx_mbr,
                         struct schem_ctx_gpt *ctx_gpt,
                         const struct img_ctx *img_ctx, int img_fd)
{
    char buf[input_buf_sz];
    char *s;
    pres r;

    for(;;) {
        printf("Command (m for help): ");

        s = fgets(buf, sizeof(buf), stdin);

        printf("\n");

        /* Character + '\n' */
        if(s == NULL || strlen(s) != 2) {
            return pres_ok;
        }

        r = action_handle(ctx_mbr, ctx_gpt, img_ctx, img_fd, s[0]);
        if(!r) {
            return pres_fail;
        }
    }
}

static pres img_ensure_size(int img_fd, pu64 img_sz)
{
    long long s;
    char c;

    s = lseek64(img_fd, 0, SEEK_END);
    if(s == -1) {
        perror("lseek64()");
        return pres_fail;
    }

    /* If image already has required size, return */
    if(s >= img_sz) {
        return pres_ok;
    }

    /* Seek and write a single byte to ensure image size */
    s = lseek64(img_fd, img_sz - 1, SEEK_SET);
    if(s == -1) {
        perror("lseek64()");
        return pres_fail;
    }

    c = 0;
    s = write(img_fd, &c, 1);
    if(s == -1) {
        perror("write()");
        return pres_fail;
    }

    return pres_ok;
}

int main(int argc, const char * const *argv)
{
    int img_fd;
    struct img_ctx img_ctx;
    struct schem_ctx_mbr ctx_mbr;
    struct schem_ctx_gpt ctx_gpt;
    pres r;

#if 1
    enum gpt_load_res gpt_res;
#endif

    printf("partman 1.0\n\n");

    /* Image file not specified */
    if(argc < 2) {
        fprintf(stderr, "Usage: %s [FILE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Open file */
    img_fd = open(argv[1], O_RDWR|O_CREAT, 0666);
    if(img_fd == -1) {
        perror("open()");
        fprintf(stderr, "Unable to open %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    memset(&ctx_gpt, 0, sizeof(ctx_gpt));

    /* Allocate space for GPT tables */
    ctx_gpt.table_prim = calloc(128, sizeof(ctx_gpt.table_prim[0]));
    ctx_gpt.table_sec = calloc(128, sizeof(ctx_gpt.table_prim[0]));

    /* Ensure img_fd has required size */
    r = img_ensure_size(img_fd, IMAGE_SIZE);
    if(!r) {
        fprintf(stderr, "Unable to ensure image size\n");
        goto exit;
    }

    /* Initialize context */
    img_ctx_init(&img_ctx, img_fd, IMAGE_SIZE);

    /* Map MBR */
    r = mbr_map(&ctx_mbr, &img_ctx);
    if(!r) {
        goto exit;
    }

    /* If MBR detected, read MBR  */
    if(mbr_is_present(ctx_mbr.mbr_reg)) {
        printf("MBR detected!\n");
        mbr_load(&ctx_mbr);
    }

#if 0
    /* TODO: DEBUG CODE, REMOVE! */

    ctx_mbr.mbr.partitions[0].boot_ind = 0x0;
    ctx_mbr.mbr.partitions[0].type = 0xAB;
    ctx_mbr.mbr.partitions[0].start_chs = chs_tuple_to_int(666, 11, 22);
    /* Max CHS for normal MBR (should be (1023, 254, 63))*/
    ctx_mbr.mbr.partitions[0].end_chs = lba_to_chs(&img_ctx, 0xFFFFFFFF);
    ctx_mbr.mbr.partitions[0].start_lba = 0xAABBCCDD;
    ctx_mbr.mbr.partitions[0].sz_lba = 0xCCDDEEFF;

    ctx_mbr.mbr.partitions[1].boot_ind = 0x80;
    ctx_mbr.mbr.partitions[1].type = 0xCD;
    ctx_mbr.mbr.partitions[1].start_chs = chs_tuple_to_int(0, 0, 2);
    /* Max CHS for protective MBR in GPT layout */
    ctx_mbr.mbr.partitions[1].end_chs = chs_tuple_to_int(1023, 255, 63);
    ctx_mbr.mbr.partitions[1].start_lba = 0xDDCCBBAA;
    ctx_mbr.mbr.partitions[1].sz_lba = 0xFFEEDDCC;

    mbr_save(&ctx_mbr);

    goto exit;

#elif 0
    /* Primary GPT */
    gpt_init_new(&ctx_gpt.hdr_prim);
    ctx_gpt.hdr_prim.my_lba = 1;
    ctx_gpt.hdr_prim.alt_lba = byte_to_lba(&img_ctx, IMAGE_SIZE, 0) - 1;
    ctx_gpt.hdr_prim.first_usable_lba = 2048;
    ctx_gpt.hdr_prim.last_usable_lba = 121208798;
    guid_create(&ctx_gpt.hdr_prim.disk_guid);
    ctx_gpt.hdr_prim.part_table_lba = 2;
    ctx_gpt.hdr_prim.part_table_entry_cnt = 1;
    ctx_gpt.hdr_prim.part_entry_sz = 128;

    guid_create(&ctx_gpt.table_prim[0].type_guid);
    guid_create(&ctx_gpt.table_prim[0].unique_guid);
    ctx_gpt.table_prim[0].start_lba = 2048;
    ctx_gpt.table_prim[0].end_lba = 121206783;

    gpt_crc_create(&ctx_gpt.hdr_prim, ctx_gpt.table_prim);

    /* Secondary GPT */
    gpt_restore(&ctx_gpt.hdr_sec, ctx_gpt.table_sec, &ctx_gpt.hdr_prim, ctx_gpt.table_prim);
    ctx_gpt.hdr_sec.part_table_lba = ctx_gpt.hdr_sec.my_lba - 32;

    gpt_crc_create(&ctx_gpt.hdr_sec, ctx_gpt.table_sec);

    /* Save GPTs */
    r = gpt_save(&ctx_gpt.hdr_prim, ctx_gpt.table_prim, &img_ctx);
    if(!r) {
        fprintf(stderr, "Failed to write primary GPT\n");
        goto exit;
    }

    r = gpt_save(&ctx_gpt.hdr_sec, ctx_gpt.table_sec, &img_ctx);
    if(!r) {
        fprintf(stderr, "Failed to write secondary GPT\n");
        goto exit;
    }

    goto exit;

#else

    /* Load GPT primary header */
    gpt_res = gpt_load(&ctx_gpt.hdr_prim, ctx_gpt.table_prim, &img_ctx, 1);
    if(gpt_res != gpt_load_ok) {
        fprintf(stderr, "Failed to load primary GPT %d\n", gpt_res);
        goto exit;
    }

    /* Load GPT secondary header */
    gpt_res = gpt_load(&ctx_gpt.hdr_sec, ctx_gpt.table_sec, &img_ctx,
                       ctx_gpt.hdr_prim.alt_lba);
    if(gpt_res != gpt_load_ok) {
        fprintf(stderr, "Failed to load secondary GPT %d\n", gpt_res);
        goto exit;
    }

#endif

    /* Start user routine */
    r = user_routine(&ctx_mbr, &ctx_gpt, &img_ctx, img_fd);
    if(!r) {
        goto exit;
    }

    /* No error branch */
    r = pres_ok;

exit:
    mbr_unmap(&ctx_mbr, &img_ctx);
    close(img_fd);
    free(ctx_gpt.table_prim);
    free(ctx_gpt.table_sec);

    return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
