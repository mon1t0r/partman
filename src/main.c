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

static pres action_handle(const struct img_ctx *img_ctx,
                          struct schem_ctx *schem_ctx, int sym)
{
    switch(sym) {
        /* Help */
        case 'm':
            printf("*help should be here*\n");
            break;

        /* Print the partition table */
        case 'p':
            break;
    }

    return pres_ok;
}

static pres
routine_start(const struct img_ctx *img_ctx, struct schem_ctx *schem_ctx)
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

        r = action_handle(img_ctx, schem_ctx, s[0]);
        if(!r) {
            return pres_fail;
        }
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
