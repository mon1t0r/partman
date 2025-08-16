#ifndef PARTMAN_OPTIONS_H
#define PARTMAN_OPTIONS_H

#include "partman_types.h"

struct partman_opts {
    const char *img_name;
    pu64 sec_sz;
    pu64 img_sz;
};

pres opts_parse(struct partman_opts *opts, int argc, char * const *argv);

#endif
