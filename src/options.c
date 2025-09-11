#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "options.h"

static const char msg_err[] =
    "Usage: %s [OPTION]... [IMG_FILE]\n(%s)\n";

static const struct option opts_long[] = {
    { "log-level",    required_argument, NULL, 'L' },
    { "sector-size",  required_argument, NULL, 'b' },
    { "min-img-size", required_argument, NULL, 'm' },
    { "alignment",    required_argument, NULL, 'a' },
    { "heads",        required_argument, NULL, 'H' },
    { "sectors",      required_argument, NULL, 'S' },
    { 0,              0,                 0,    0   }
};

static const char opt_str[] = "L:b:m:a:H:S:";

static void opts_err(const char *exec_name, const char *reason)
{
    pprint(msg_err, exec_name, reason);
}

static pres opts_parse_log_level(const char *arg, enum log_level *level_ptr)
{
    if(0 == strcmp(arg, LOG_STR_DBG)) {
        *level_ptr = log_debug;
        return pres_ok;
    }

    if(0 == strcmp(arg, LOG_STR_INFO)) {
        *level_ptr = log_info;
        return pres_ok;
    }

    if(0 == strcmp(arg, LOG_STR_WARN)) {
        *level_ptr = log_warn;
        return pres_ok;
    }

    if(0 == strcmp(arg, LOG_STR_ERR)) {
        *level_ptr = log_error;
        return pres_ok;
    }

    return pres_fail;
}

static pres opts_parse_pu8(const char *arg, pu8 *i_ptr)
{
    pu32 i;
    pres res;

    res = sscanf(arg, "%lu", &i) == 1;
    if(!res) {
        return pres_fail;
    }

    /* Out of bounds */
    if((i & ~0xFF) != 0) {
        return pres_fail;
    }

    *i_ptr = i;

    return pres_ok;
}

static pres opts_parse_pu64(const char *arg, pu64 *i_ptr)
{
    return sscanf(arg, "%llu", i_ptr) == 1;
}

static void opts_init_default(struct partman_opts *opts)
{
    memset(opts, 0, sizeof(*opts));

    /* Default logging for debug build */
#ifdef DEBUG
    opts->log_level = log_debug;
#else
    opts->log_level = log_info;
#endif
}

pres opts_parse(struct partman_opts *opts, int argc, char * const *argv)
{
    extern int optind;
    extern char *optarg;

    int c;

    opts_init_default(opts);

    if(argc <= 0) {
        opts_err("partman", "argc <= 0");
        return pres_fail;
    }

    do {
        c = getopt_long(argc, argv, opt_str, opts_long, NULL);

        switch(c) {
            case -1:
                break;
            case 'L':
                if(!opts_parse_log_level(optarg, &opts->log_level)) {
                    opts_err(argv[0], "log-level - invalid value");
                    return pres_fail;
                }
                break;
            case 'b':
                if(!opts_parse_pu64(optarg, &opts->sec_sz)) {
                    opts_err(argv[0], "sector-size - invalid value");
                    return pres_fail;
                }
                break;
            case 'm':
                if(!opts_parse_pu64(optarg, &opts->img_sz)) {
                    opts_err(argv[0], "min-img-size - invalid value");
                    return pres_fail;
                }
                break;
            case 'a':
                if(!opts_parse_pu64(optarg, &opts->align)) {
                    opts_err(argv[0], "alignment - invalid value");
                    return pres_fail;
                }
                break;
            case 'H':
                if(!opts_parse_pu8(optarg, &opts->hpc)) {
                    opts_err(argv[0], "heads - invalid value");
                    return pres_fail;
                }
                break;
            case 'S':
                if(!opts_parse_pu8(optarg, &opts->spt)) {
                    opts_err(argv[0], "sectors - invalid value");
                    return pres_fail;
                }
                break;
            default:
                opts_err(argv[0], "Unknown option");
                return pres_fail;
        }
    } while(c != -1);

    if(optind + 1 != argc) {
        opts_err(argv[0], "Missing image file name");
        return pres_fail;
    }

    opts->img_name = argv[optind];
    return pres_ok;
}

