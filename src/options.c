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
    { 0,              0,                 0,    0   }
};

static const char opt_str[] = "L:b:m:";

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

static pres opts_parse_pu64(const char *arg, pu64 *i_ptr)
{
    return sscanf(arg, "%llu", i_ptr) == 1;
}

static void opts_init_default(struct partman_opts *opts)
{
    opts->img_name = NULL;
    opts->log_level = log_info;
    opts->sec_sz = 512;
    opts->img_sz = 0;
}

pres opts_parse(struct partman_opts *opts, int argc, char * const *argv)
{
    extern int optind;
    extern char *optarg;

    char c;

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

