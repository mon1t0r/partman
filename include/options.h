#ifndef PARTMAN_OPTIONS_H
#define PARTMAN_OPTIONS_H

#include "partman_types.h"
#include "log.h"

struct partman_opts {
    enum log_level log_level;
    const char *img_name;
    pu64 sec_sz;
    pu64 img_sz;
};

pres opts_parse(struct partman_opts *opts, int argc, char * const *argv);

#endif

